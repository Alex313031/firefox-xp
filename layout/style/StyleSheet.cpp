/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleSheet.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/css/ErrorReporter.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/css/Loader.h"
#include "mozilla/dom/CSSImportRule.h"
#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/ShadowRootBinding.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/SheetLoadData.h"

#include "mozAutoDocUpdate.h"
#include "SheetLoadData.h"

namespace mozilla {

using namespace dom;

StyleSheet::StyleSheet(css::SheetParsingMode aParsingMode, CORSMode aCORSMode,
                       const dom::SRIMetadata& aIntegrity)
    : mParent(nullptr),
      mConstructorDocument(nullptr),
      mDocumentOrShadowRoot(nullptr),
      mOwningNode(nullptr),
      mOwnerRule(nullptr),
      mParsingMode(aParsingMode),
      mState(static_cast<State>(0)),
      mAssociationMode(NotOwnedByDocumentOrShadowRoot),
      mInner(new StyleSheetInfo(aCORSMode, aIntegrity, aParsingMode)) {
  mInner->AddSheet(this);
}

StyleSheet::StyleSheet(const StyleSheet& aCopy, StyleSheet* aParentToUse,
                       dom::CSSImportRule* aOwnerRuleToUse,
                       dom::DocumentOrShadowRoot* aDocumentOrShadowRoot,
                       nsINode* aOwningNodeToUse)
    : mParent(aParentToUse),
      mConstructorDocument(aCopy.mConstructorDocument),
      mTitle(aCopy.mTitle),
      mDocumentOrShadowRoot(aDocumentOrShadowRoot),
      mOwningNode(aOwningNodeToUse),
      mOwnerRule(aOwnerRuleToUse),
      mParsingMode(aCopy.mParsingMode),
      mState(aCopy.mState),
      // We only use this constructor during cloning.  It's the cloner's
      // responsibility to notify us if we end up being owned by a document.
      mAssociationMode(NotOwnedByDocumentOrShadowRoot),
      // Shallow copy, but concrete subclasses will fix up.
      mInner(aCopy.mInner) {
  MOZ_ASSERT(mInner, "Should only copy StyleSheets with an mInner.");
  mInner->AddSheet(this);

  if (HasForcedUniqueInner()) {  // CSSOM's been there, force full copy now
    MOZ_ASSERT(IsComplete(),
               "Why have rules been accessed on an incomplete sheet?");
    // FIXME: handle failure?
    EnsureUniqueInner();
  }

  if (aCopy.mMedia) {
    // XXX This is wrong; we should be keeping @import rules and
    // sheets in sync!
    mMedia = aCopy.mMedia->Clone();
  }
}

/* static */
// https://wicg.github.io/construct-stylesheets/#dom-cssstylesheet-cssstylesheet
already_AddRefed<StyleSheet> StyleSheet::Constructor(
    const dom::GlobalObject& aGlobal, const dom::CSSStyleSheetInit& aOptions,
    ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());

  if (!window) {
    aRv.ThrowNotSupportedError("Not supported when there is no document");
    return nullptr;
  }

  Document* constructorDocument = window->GetExtantDoc();
  if (!constructorDocument) {
    aRv.ThrowNotSupportedError("Not supported when there is no document");
    return nullptr;
  }

  // 1. Construct a sheet and set its properties (see spec).
  // TODO(nordzilla): Various issues with the spec's handling of aOptions:
  // * https://github.com/WICG/construct-stylesheets/issues/113
  // * https://github.com/WICG/construct-stylesheets/issues/105
  auto sheet =
      MakeRefPtr<StyleSheet>(css::SheetParsingMode::eAuthorSheetFeatures,
                             CORSMode::CORS_NONE, dom::SRIMetadata());

  nsIURI* baseURI = constructorDocument->GetBaseURI();
  nsIURI* sheetURI = constructorDocument->GetDocumentURI();
  nsIURI* originalURI = nullptr;
  sheet->SetURIs(sheetURI, originalURI, baseURI);

  sheet->SetTitle(aOptions.mTitle);
  sheet->SetPrincipal(constructorDocument->NodePrincipal());
  sheet->SetReferrerInfo(constructorDocument->GetReferrerInfo());
  sheet->mConstructorDocument = constructorDocument;

  // 2. Set the sheet's media according to aOptions.
  if (aOptions.mMedia.IsString()) {
    sheet->SetMedia(MediaList::Create(aOptions.mMedia.GetAsString()));
  } else {
    sheet->SetMedia(aOptions.mMedia.GetAsMediaList()->Clone());
  }

  // 3. Set the sheet's disabled flag according to aOptions.
  sheet->SetDisabled(aOptions.mDisabled);
  sheet->SetComplete();

  // 4. Return sheet.
  return sheet.forget();
}

StyleSheet::~StyleSheet() {
  MOZ_ASSERT(!mInner, "Inner should have been dropped in LastRelease");
}

bool StyleSheet::HasRules() const {
  return Servo_StyleSheet_HasRules(Inner().mContents);
}

Document* StyleSheet::GetAssociatedDocument() const {
  auto* associated = GetAssociatedDocumentOrShadowRoot();
  return associated ? associated->AsNode().OwnerDoc() : nullptr;
}

dom::DocumentOrShadowRoot* StyleSheet::GetAssociatedDocumentOrShadowRoot()
    const {
  // FIXME(nordzilla) This will not work for children of adtoped sheets.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1613748
  return IsConstructed() ? mConstructorDocument : mDocumentOrShadowRoot;
}

Document* StyleSheet::GetComposedDoc() const {
  return mDocumentOrShadowRoot
             ? mDocumentOrShadowRoot->AsNode().GetComposedDoc()
             : nullptr;
}

bool StyleSheet::IsKeptAliveByDocument() const {
  if (mAssociationMode != OwnedByDocumentOrShadowRoot) {
    return false;
  }

  return !!GetComposedDoc();
}

void StyleSheet::LastRelease() {
  MOZ_ASSERT(mInner, "Should have an mInner at time of destruction.");
  MOZ_ASSERT(mInner->mSheets.Contains(this), "Our mInner should include us.");
  MOZ_DIAGNOSTIC_ASSERT(mAdopters.IsEmpty(),
                        "Should have no adopters at time of destruction.");

  UnparentChildren();

  mInner->RemoveSheet(this);
  mInner = nullptr;

  DropMedia();
  DropRuleList();
}

void StyleSheet::UnlinkInner() {
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  for (StyleSheet* child : ChildSheets()) {
    MOZ_ASSERT(child->mParent == this, "We have a unique inner!");
    child->mParent = nullptr;
    // We (and child) might still think we're owned by a document, because
    // unlink order is non-deterministic, so the document's unlink, which would
    // tell us it doesn't own us anymore, may not have happened yet.  But if
    // we're being unlinked, clearly we're not owned by a document anymore
    // conceptually!
    child->ClearAssociatedDocumentOrShadowRoot();
  }
  Inner().mChildren.Clear();
}

void StyleSheet::TraverseInner(nsCycleCollectionTraversalCallback& cb) {
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  for (StyleSheet* child : ChildSheets()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "child sheet");
    cb.NoteXPCOMChild(child);
  }
}

// QueryInterface implementation for StyleSheet
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StyleSheet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(StyleSheet)
// We want to disconnect from our inner as soon as our refcount drops to zero,
// without waiting for async deletion by the cycle collector.  Otherwise we
// might end up cloning the inner if someone mutates another sheet that shares
// it with us, even though there is only one such sheet and we're about to go
// away.  This situation arises easily with sheet preloading.
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(StyleSheet, LastRelease())

NS_IMPL_CYCLE_COLLECTION_CLASS(StyleSheet)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(StyleSheet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMedia)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConstructorDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mReplacePromise)
  tmp->TraverseInner(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(StyleSheet)
  tmp->DropMedia();
  tmp->UnlinkInner();
  tmp->DropRuleList();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConstructorDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mReplacePromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(StyleSheet)

dom::CSSStyleSheetParsingMode StyleSheet::ParsingModeDOM() {
#define CHECK_MODE(X, Y)                            \
  static_assert(                                    \
      static_cast<int>(X) == static_cast<int>(Y),   \
      "mozilla::dom::CSSStyleSheetParsingMode and " \
      "mozilla::css::SheetParsingMode should have identical values");

  CHECK_MODE(dom::CSSStyleSheetParsingMode::Agent, css::eAgentSheetFeatures);
  CHECK_MODE(dom::CSSStyleSheetParsingMode::User, css::eUserSheetFeatures);
  CHECK_MODE(dom::CSSStyleSheetParsingMode::Author, css::eAuthorSheetFeatures);

#undef CHECK_MODE

  return static_cast<dom::CSSStyleSheetParsingMode>(mParsingMode);
}

void StyleSheet::SetComplete() {
  // HasForcedUniqueInner() is okay if the sheet is constructed, because
  // constructed sheets are always unique and they may be set to complete
  // multiple times if their rules are replaced via Replace()
  MOZ_ASSERT(IsConstructed() || !HasForcedUniqueInner(),
             "Can't complete a sheet that's already been forced unique.");
  MOZ_ASSERT(!IsComplete(), "Already complete?");
  mState |= State::Complete;
  if (!Disabled()) {
    ApplicableStateChanged(true);
  }
  MaybeResolveReplacePromise();
}

void StyleSheet::ApplicableStateChanged(bool aApplicable) {
  MOZ_ASSERT(aApplicable == IsApplicable());
  auto Notify = [this](DocumentOrShadowRoot& target) {
    nsINode& node = target.AsNode();
    if (ShadowRoot* shadow = ShadowRoot::FromNode(node)) {
      shadow->StyleSheetApplicableStateChanged(*this);
    } else {
      node.AsDocument()->StyleSheetApplicableStateChanged(*this);
    }
  };

  if (mDocumentOrShadowRoot) {
    Notify(*mDocumentOrShadowRoot);
  }

  for (DocumentOrShadowRoot* adopter : mAdopters) {
    MOZ_ASSERT(adopter, "adopters should never be null");
    Notify(*adopter);
  }
}

void StyleSheet::SetDisabled(bool aDisabled) {
  if (IsReadOnly()) {
    return;
  }

  if (aDisabled == Disabled()) {
    return;
  }

  if (aDisabled) {
    mState |= State::Disabled;
  } else {
    mState &= ~State::Disabled;
  }

  if (IsComplete()) {
    ApplicableStateChanged(!aDisabled);
  }
}

void StyleSheet::SetURLExtraData() {
  Inner().mURLData =
      new URLExtraData(GetBaseURI(), GetReferrerInfo(), Principal());
}

StyleSheetInfo::StyleSheetInfo(CORSMode aCORSMode,
                               const SRIMetadata& aIntegrity,
                               css::SheetParsingMode aParsingMode)
    : mPrincipal(NullPrincipal::CreateWithoutOriginAttributes()),
      mCORSMode(aCORSMode),
      mReferrerInfo(new ReferrerInfo(nullptr)),
      mIntegrity(aIntegrity),
      mContents(Servo_StyleSheet_Empty(aParsingMode).Consume()),
      mURLData(URLExtraData::Dummy())
#ifdef DEBUG
      ,
      mPrincipalSet(false)
#endif
{
  if (!mPrincipal) {
    MOZ_CRASH("NullPrincipal::Init failed");
  }
  MOZ_COUNT_CTOR(StyleSheetInfo);
}

StyleSheetInfo::StyleSheetInfo(StyleSheetInfo& aCopy, StyleSheet* aPrimarySheet)
    : mSheetURI(aCopy.mSheetURI),
      mOriginalSheetURI(aCopy.mOriginalSheetURI),
      mBaseURI(aCopy.mBaseURI),
      mPrincipal(aCopy.mPrincipal),
      mCORSMode(aCopy.mCORSMode),
      mReferrerInfo(aCopy.mReferrerInfo),
      mIntegrity(aCopy.mIntegrity),
      // We don't rebuild the child because we're making a copy without
      // children.
      mSourceMapURL(aCopy.mSourceMapURL),
      mSourceMapURLFromComment(aCopy.mSourceMapURLFromComment),
      mSourceURL(aCopy.mSourceURL),
      mContents(Servo_StyleSheet_Clone(aCopy.mContents.get(), aPrimarySheet)
                    .Consume()),
      mURLData(aCopy.mURLData)
#ifdef DEBUG
      ,
      mPrincipalSet(aCopy.mPrincipalSet)
#endif
{
  AddSheet(aPrimarySheet);

  // Our child list is fixed up by our parent.
  MOZ_COUNT_CTOR(StyleSheetInfo);
}

StyleSheetInfo::~StyleSheetInfo() { MOZ_COUNT_DTOR(StyleSheetInfo); }

StyleSheetInfo* StyleSheetInfo::CloneFor(StyleSheet* aPrimarySheet) {
  return new StyleSheetInfo(*this, aPrimarySheet);
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoStyleSheetMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoStyleSheetMallocEnclosingSizeOf)

size_t StyleSheetInfo::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  n += Servo_StyleSheet_SizeOfIncludingThis(
      ServoStyleSheetMallocSizeOf, ServoStyleSheetMallocEnclosingSizeOf,
      mContents);

  return n;
}

void StyleSheetInfo::AddSheet(StyleSheet* aSheet) {
  mSheets.AppendElement(aSheet);
}

void StyleSheetInfo::RemoveSheet(StyleSheet* aSheet) {
  if (aSheet == mSheets[0] && mSheets.Length() > 1) {
    StyleSheet* newParent = mSheets[1];
    for (StyleSheet* child : mChildren) {
      child->mParent = newParent;
      child->SetAssociatedDocumentOrShadowRoot(newParent->mDocumentOrShadowRoot,
                                               newParent->mAssociationMode);
    }
  }

  if (1 == mSheets.Length()) {
    NS_ASSERTION(aSheet == mSheets.ElementAt(0), "bad parent");
    delete this;
    return;
  }

  mSheets.RemoveElement(aSheet);
}

void StyleSheet::GetType(nsAString& aType) { aType.AssignLiteral("text/css"); }

void StyleSheet::GetHref(nsAString& aHref, ErrorResult& aRv) {
  if (nsIURI* sheetURI = Inner().mOriginalSheetURI) {
    nsAutoCString str;
    nsresult rv = sheetURI->GetSpec(str);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
    CopyUTF8toUTF16(str, aHref);
  } else {
    SetDOMStringToNull(aHref);
  }
}

void StyleSheet::GetTitle(nsAString& aTitle) {
  // From https://drafts.csswg.org/cssom/#dom-stylesheet-title:
  //
  //    The title attribute must return the title or null if title is the empty
  //    string.
  //
  if (!mTitle.IsEmpty()) {
    aTitle.Assign(mTitle);
  } else {
    SetDOMStringToNull(aTitle);
  }
}

void StyleSheet::WillDirty() {
  MOZ_ASSERT(!IsReadOnly());

  if (IsComplete()) {
    EnsureUniqueInner();
  }
}

void StyleSheet::AddStyleSet(ServoStyleSet* aStyleSet) {
  MOZ_DIAGNOSTIC_ASSERT(!mStyleSets.Contains(aStyleSet),
                        "style set already registered");
  mStyleSets.AppendElement(aStyleSet);
}

void StyleSheet::DropStyleSet(ServoStyleSet* aStyleSet) {
  bool found = mStyleSets.RemoveElement(aStyleSet);
  MOZ_DIAGNOSTIC_ASSERT(found, "didn't find style set");
#ifndef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  Unused << found;
#endif
}

// NOTE(emilio): Composed doc and containing shadow root are set in child sheets
// too, so no need to do it for each ancestor.
#define NOTIFY(function_, args_)                                      \
  do {                                                                \
    if (auto* shadow = GetContainingShadow()) {                       \
      shadow->function_ args_;                                        \
    }                                                                 \
    if (auto* doc = GetComposedDoc()) {                               \
      doc->function_ args_;                                           \
    }                                                                 \
    StyleSheet* current = this;                                       \
    do {                                                              \
      for (ServoStyleSet * set : current->mStyleSets) {               \
        set->function_ args_;                                         \
      }                                                               \
      for (auto* adopter : mAdopters) {                               \
        if (auto* shadow = ShadowRoot::FromNode(adopter->AsNode())) { \
          shadow->function_ args_;                                    \
        } else {                                                      \
          adopter->AsNode().AsDocument()->function_ args_;            \
        }                                                             \
      }                                                               \
      current = current->mParent;                                     \
    } while (current);                                                \
  } while (0)

void StyleSheet::EnsureUniqueInner() {
  MOZ_ASSERT(mInner->mSheets.Length() != 0, "unexpected number of outers");

  if (IsReadOnly()) {
    // Sheets that can't be modified don't need a unique inner.
    return;
  }

  mState |= State::ForcedUniqueInner;

  if (HasUniqueInner()) {
    // already unique
    return;
  }

  StyleSheetInfo* clone = mInner->CloneFor(this);
  MOZ_ASSERT(clone);
  mInner->RemoveSheet(this);
  mInner = clone;

  // Fixup the child lists and parent links in the Servo sheet. This is done
  // here instead of in StyleSheetInner::CloneFor, because it's just more
  // convenient to do so instead.
  BuildChildListAfterInnerClone();

  // let our containing style sets know that if we call
  // nsPresContext::EnsureSafeToHandOutCSSRules we will need to restyle the
  // document
  NOTIFY(SheetCloned, (*this));
}

// WebIDL CSSStyleSheet API

dom::CSSRuleList* StyleSheet::GetCssRules(nsIPrincipal& aSubjectPrincipal,
                                          ErrorResult& aRv) {
  if (!AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return nullptr;
  }
  return GetCssRulesInternal();
}

void StyleSheet::GetSourceMapURL(nsAString& aSourceMapURL) {
  if (mInner->mSourceMapURL.IsEmpty()) {
    aSourceMapURL = mInner->mSourceMapURLFromComment;
  } else {
    aSourceMapURL = mInner->mSourceMapURL;
  }
}

void StyleSheet::SetSourceMapURL(const nsAString& aSourceMapURL) {
  mInner->mSourceMapURL = aSourceMapURL;
}

void StyleSheet::SetSourceMapURLFromComment(
    const nsAString& aSourceMapURLFromComment) {
  mInner->mSourceMapURLFromComment = aSourceMapURLFromComment;
}

void StyleSheet::GetSourceURL(nsAString& aSourceURL) {
  aSourceURL = mInner->mSourceURL;
}

void StyleSheet::SetSourceURL(const nsAString& aSourceURL) {
  mInner->mSourceURL = aSourceURL;
}

css::Rule* StyleSheet::GetDOMOwnerRule() const { return mOwnerRule; }

uint32_t StyleSheet::InsertRule(const nsAString& aRule, uint32_t aIndex,
                                nsIPrincipal& aSubjectPrincipal,
                                ErrorResult& aRv) {
  if (IsReadOnly() || !AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return 0;
  }
  return InsertRuleInternal(aRule, aIndex, aRv);
}

void StyleSheet::DeleteRule(uint32_t aIndex, nsIPrincipal& aSubjectPrincipal,
                            ErrorResult& aRv) {
  if (IsReadOnly() || !AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return;
  }
  return DeleteRuleInternal(aIndex, aRv);
}

int32_t StyleSheet::AddRule(const nsAString& aSelector, const nsAString& aBlock,
                            const Optional<uint32_t>& aIndex,
                            nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  if (IsReadOnly() || !AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return -1;
  }

  nsAutoString rule;
  rule.Append(aSelector);
  rule.AppendLiteral(" { ");
  if (!aBlock.IsEmpty()) {
    rule.Append(aBlock);
    rule.Append(' ');
  }
  rule.Append('}');

  auto index =
      aIndex.WasPassed() ? aIndex.Value() : GetCssRulesInternal()->Length();

  InsertRuleInternal(rule, index, aRv);
  // Always return -1.
  return -1;
}

void StyleSheet::MaybeResolveReplacePromise() {
  MOZ_ASSERT(!!mReplacePromise == ModificationDisallowed());
  if (!mReplacePromise) {
    return;
  }

  SetModificationDisallowed(false);
  mReplacePromise->MaybeResolve(this);
  mReplacePromise = nullptr;
}

void StyleSheet::MaybeRejectReplacePromise() {
  MOZ_ASSERT(!!mReplacePromise == ModificationDisallowed());
  if (!mReplacePromise) {
    return;
  }

  SetModificationDisallowed(false);
  mReplacePromise->MaybeRejectWithNetworkError(
      "@import style sheet load failed");
  mReplacePromise = nullptr;
}

// https://wicg.github.io/construct-stylesheets/#dom-cssstylesheet-replace
already_AddRefed<dom::Promise> StyleSheet::Replace(const nsACString& aText,
                                                   ErrorResult& aRv) {
  nsIGlobalObject* globalObject = mConstructorDocument->GetScopeObject();
  RefPtr<dom::Promise> promise = dom::Promise::Create(globalObject, aRv);
  if (!promise) {
    return nullptr;
  }

  // Step 1 and 4 are variable declarations

  // 2.1 Check if sheet is constructed, else reject promise.
  if (!mConstructorDocument) {
    promise->MaybeRejectWithNotAllowedError(
        "This method can only be called on "
        "constructed style sheets");
    return promise.forget();
  }

  // 2.2 Check if sheet is modifiable, else throw.
  if (ModificationDisallowed()) {
    promise->MaybeRejectWithNotAllowedError(
        "This method can only be called on "
        "modifiable style sheets");
    return promise.forget();
  }

  // 3. Disallow modifications until finished.
  SetModificationDisallowed(true);

  auto* loader = mConstructorDocument->CSSLoader();
  auto loadData = MakeRefPtr<css::SheetLoadData>(
      loader, GetBaseURI(), this, /* aSyncLoad */ false,
      /* aUseSystemPrincipal */ false, /* aPreloadEncoding */ nullptr,
      /* aObserver */ nullptr, /* aLoaderPrincipal */ nullptr,
      GetReferrerInfo(),
      /* aRequestingNode */ nullptr);

  // In parallel
  // 5.1 Parse aText into rules.
  // 5.2 Load import rules, throw NetworkError if failed.
  // 5.3 Set sheet's rules to new rules.
  nsCOMPtr<nsISerialEventTarget> target =
      mConstructorDocument->EventTargetFor(TaskCategory::Other);
  loadData->mIsBeingParsed = true;
  MOZ_ASSERT(!mReplacePromise);
  mReplacePromise = promise;
  ParseSheet(*loader, aText, *loadData)
      ->Then(
          target, __func__,
          [loadData] { loadData->SheetFinishedParsingAsync(); },
          [] { MOZ_CRASH("This MozPromise should never be rejected."); });

  // 6. Return the promise
  return promise.forget();
}

// https://wicg.github.io/construct-stylesheets/#dom-cssstylesheet-replacesync
void StyleSheet::ReplaceSync(const nsACString& aText, ErrorResult& aRv) {
  // Step 1 is a variable declaration

  // 2.1 Check if sheet is constructed, else throw.
  if (!mConstructorDocument) {
    return aRv.ThrowNotAllowedError(
        "Can only be called on constructed style sheets");
  }

  // 2.2 Check if sheet is modifiable, else throw.
  if (ModificationDisallowed()) {
    return aRv.ThrowNotAllowedError(
        "Can only be called on modifiable style sheets");
  }

  // 3. Parse aText into rules.
  // We need to create a disabled loader so that the parser will
  // accept @import rules, but we do not want to trigger any loads.
  auto disabledLoader = MakeRefPtr<css::Loader>();
  disabledLoader->SetEnabled(false);
  SetURLExtraData();
  RefPtr<const RawServoStyleSheetContents> rawContent =
      Servo_StyleSheet_FromUTF8Bytes(
          disabledLoader, this,
          /* load_data = */ nullptr, &aText, mParsingMode, Inner().mURLData,
          /* line_number_offset = */ 0,
          mConstructorDocument->GetCompatibilityMode(),
          /* reusable_sheets = */ nullptr,
          StyleSanitizationKind::None,
          /* sanitized_output = */ nullptr)
          .Consume();

  // 4. If rules contain @imports, make no changes and throw NotAllowedError.
  // FIXME(nordzilla): Checking for @import rules after parse time means that
  // the document's use counters will be affected even if this function throws.
  // Consider changing this to detect @import rules during parse time.
  if (Servo_StyleSheet_HasImportRules(rawContent)) {
    return aRv.ThrowNotAllowedError(
        "@import rules are not allowed. Use the async replace() method "
        "instead.");
  }

  // 5. Set sheet's rules to the new rules.
  DropRuleList();
  Inner().mContents = std::move(rawContent);
  FinishParse();
  RuleChanged(nullptr);
}

nsresult StyleSheet::DeleteRuleFromGroup(css::GroupRule* aGroup,
                                         uint32_t aIndex) {
  NS_ENSURE_ARG_POINTER(aGroup);
  NS_ASSERTION(IsComplete(), "No deleting from an incomplete sheet!");
  RefPtr<css::Rule> rule = aGroup->GetStyleRuleAt(aIndex);
  NS_ENSURE_TRUE(rule, NS_ERROR_ILLEGAL_VALUE);

  // check that the rule actually belongs to this sheet!
  if (this != rule->GetStyleSheet()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (IsReadOnly()) {
    return NS_OK;
  }

  WillDirty();

  nsresult result = aGroup->DeleteStyleRuleAt(aIndex);
  NS_ENSURE_SUCCESS(result, result);

  rule->DropReferences();

  RuleRemoved(*rule);
  return NS_OK;
}

ShadowRoot* StyleSheet::GetContainingShadow() const {
  auto* docOrShadow = GetAssociatedDocumentOrShadowRoot();
  if (!docOrShadow) {
    return nullptr;
  }
  return ShadowRoot::FromNode(docOrShadow->AsNode());
}

void StyleSheet::RuleAdded(css::Rule& aRule) {
  mState |= State::ModifiedRules;
  NOTIFY(RuleAdded, (*this, aRule));
}

void StyleSheet::RuleRemoved(css::Rule& aRule) {
  mState |= State::ModifiedRules;
  NOTIFY(RuleRemoved, (*this, aRule));
}

void StyleSheet::RuleChanged(css::Rule* aRule) {
  mState |= State::ModifiedRules;
  NOTIFY(RuleChanged, (*this, aRule));
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
StyleSheet::StyleSheetLoaded(StyleSheet* aSheet, bool aWasDeferred,
                             nsresult aStatus) {
  if (!aSheet->GetParentSheet()) {
    return NS_OK;  // ignore if sheet has been detached already
  }
  MOZ_ASSERT(this == aSheet->GetParentSheet(),
             "We are being notified of a sheet load for a sheet that is not "
             "our child!");
  if (NS_FAILED(aStatus)) {
    return NS_OK;
  }

  MOZ_ASSERT(aSheet->GetOwnerRule());
  NOTIFY(ImportRuleLoaded, (*aSheet->GetOwnerRule(), *aSheet));
  return NS_OK;
}

#undef NOTIFY

nsresult StyleSheet::InsertRuleIntoGroup(const nsAString& aRule,
                                         css::GroupRule* aGroup,
                                         uint32_t aIndex) {
  NS_ASSERTION(IsComplete(), "No inserting into an incomplete sheet!");
  // check that the group actually belongs to this sheet!
  if (this != aGroup->GetStyleSheet()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (IsReadOnly()) {
    return NS_OK;
  }

  WillDirty();

  nsresult result = InsertRuleIntoGroupInternal(aRule, aGroup, aIndex);
  NS_ENSURE_SUCCESS(result, result);
  RuleAdded(*aGroup->GetStyleRuleAt(aIndex));
  return NS_OK;
}

uint64_t StyleSheet::FindOwningWindowInnerID() const {
  uint64_t windowID = 0;
  if (Document* doc = GetAssociatedDocument()) {
    windowID = doc->InnerWindowID();
  }

  if (windowID == 0 && mOwningNode) {
    windowID = mOwningNode->OwnerDoc()->InnerWindowID();
  }

  RefPtr<css::Rule> ownerRule;
  if (windowID == 0 && (ownerRule = GetDOMOwnerRule())) {
    RefPtr<StyleSheet> sheet = ownerRule->GetStyleSheet();
    if (sheet) {
      windowID = sheet->FindOwningWindowInnerID();
    }
  }

  if (windowID == 0 && mParent) {
    windowID = mParent->FindOwningWindowInnerID();
  }

  return windowID;
}

void StyleSheet::RemoveFromParent() {
  if (!mParent) {
    return;
  }

  MOZ_ASSERT(mParent->ChildSheets().Contains(this));
  mParent->Inner().mChildren.RemoveElement(this);
  mParent = nullptr;
  ClearAssociatedDocumentOrShadowRoot();
}

void StyleSheet::UnparentChildren() {
  // XXXbz this is a little bogus; see the XXX comment where we
  // declare mFirstChild in StyleSheetInfo.
  for (StyleSheet* child : ChildSheets()) {
    if (child->mParent == this) {
      child->mParent = nullptr;
      MOZ_ASSERT(child->mAssociationMode == NotOwnedByDocumentOrShadowRoot,
                 "How did we get to the destructor, exactly, if we're owned "
                 "by a document?");
      child->mDocumentOrShadowRoot = nullptr;
    }
  }
}

void StyleSheet::SubjectSubsumesInnerPrincipal(nsIPrincipal& aSubjectPrincipal,
                                               ErrorResult& aRv) {
  StyleSheetInfo& info = Inner();

  if (aSubjectPrincipal.Subsumes(info.mPrincipal)) {
    return;
  }

  // Allow access only if CORS mode is not NONE and the security flag
  // is not turned off.
  if (GetCORSMode() == CORS_NONE && !nsContentUtils::BypassCSSOMOriginCheck()) {
    aRv.ThrowSecurityError("Not allowed to access cross-origin stylesheet");
    return;
  }

  // Now make sure we set the principal of our inner to the subjectPrincipal.
  // We do this because we're in a situation where the caller would not normally
  // be able to access the sheet, but the sheet has opted in to being read.
  // Unfortunately, that means it's also opted in to being _edited_, and if the
  // caller now makes edits to the sheet we want the resulting resource loads,
  // if any, to look as if they are coming from the caller's principal, not the
  // original sheet principal.
  //
  // That means we need a unique inner, of course.  But we don't want to do that
  // if we're not complete yet.  Luckily, all the callers of this method throw
  // anyway if not complete, so we can just do that here too.
  if (!IsComplete()) {
    aRv.ThrowInvalidAccessError(
        "Not allowed to access still-loading stylesheet");
    return;
  }

  WillDirty();

  info.mPrincipal = &aSubjectPrincipal;
}

bool StyleSheet::AreRulesAvailable(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv) {
  // Rules are not available on incomplete sheets.
  if (!IsComplete()) {
    aRv.ThrowInvalidAccessError(
        "Can't access rules of still-loading stylsheet");
    return false;
  }
  //-- Security check: Only scripts whose principal subsumes that of the
  //   style sheet can access rule collections.
  SubjectSubsumesInnerPrincipal(aSubjectPrincipal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return false;
  }
  return true;
}

void StyleSheet::SetAssociatedDocumentOrShadowRoot(
    DocumentOrShadowRoot* aDocOrShadowRoot, AssociationMode aAssociationMode) {
  MOZ_ASSERT(aDocOrShadowRoot ||
             aAssociationMode == NotOwnedByDocumentOrShadowRoot);

  // not ref counted
  mDocumentOrShadowRoot = aDocOrShadowRoot;
  mAssociationMode = aAssociationMode;

  // Now set the same document on all our child sheets....
  // XXXbz this is a little bogus; see the XXX comment where we
  // declare mFirstChild.
  for (StyleSheet* child : ChildSheets()) {
    if (child->mParent == this) {
      child->SetAssociatedDocumentOrShadowRoot(aDocOrShadowRoot,
                                               aAssociationMode);
    }
  }
}

void StyleSheet::AppendStyleSheet(StyleSheet& aSheet) {
  WillDirty();
  AppendStyleSheetSilently(aSheet);
}

void StyleSheet::AppendStyleSheetSilently(StyleSheet& aSheet) {
  MOZ_ASSERT(!IsReadOnly());

  Inner().mChildren.AppendElement(&aSheet);

  // This is not reference counted. Our parent tells us when
  // it's going away.
  aSheet.mParent = this;
  aSheet.SetAssociatedDocumentOrShadowRoot(mDocumentOrShadowRoot,
                                           mAssociationMode);
}

size_t StyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  n += aMallocSizeOf(this);

  // We want to measure the inner with only one of the children, and it makes
  // sense for it to be the latest as it is the most likely to be reachable.
  if (Inner().mSheets.LastElement() == this) {
    n += Inner().SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mTitle
  // - mMedia
  // - mStyleSets
  // - mRuleList

  return n;
}

#ifdef DEBUG
void StyleSheet::List(FILE* out, int32_t aIndent) const {
  int32_t index;

  // Indent
  nsAutoCString str;
  for (index = aIndent; --index >= 0;) {
    str.AppendLiteral("  ");
  }

  str.AppendLiteral("CSS Style Sheet: ");
  nsAutoCString urlSpec;
  nsresult rv = GetSheetURI()->GetSpec(urlSpec);
  if (NS_SUCCEEDED(rv) && !urlSpec.IsEmpty()) {
    str.Append(urlSpec);
  }

  if (mMedia) {
    str.AppendLiteral(" media: ");
    nsAutoString buffer;
    mMedia->GetText(buffer);
    AppendUTF16toUTF8(buffer, str);
  }
  str.Append('\n');
  fprintf_stderr(out, "%s", str.get());

  for (const StyleSheet* child : ChildSheets()) {
    child->List(out, aIndent + 1);
  }
}
#endif

void StyleSheet::SetMedia(already_AddRefed<dom::MediaList> aMedia) {
  mMedia = aMedia;
  if (mMedia) {
    mMedia->SetStyleSheet(this);
  }
}

void StyleSheet::DropMedia() {
  if (mMedia) {
    mMedia->SetStyleSheet(nullptr);
    mMedia = nullptr;
  }
}

dom::MediaList* StyleSheet::Media() {
  if (!mMedia) {
    mMedia = dom::MediaList::Create(nsString());
    mMedia->SetStyleSheet(this);
  }

  return mMedia;
}

// nsWrapperCache

JSObject* StyleSheet::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return dom::CSSStyleSheet_Binding::Wrap(aCx, this, aGivenProto);
}

void StyleSheet::BuildChildListAfterInnerClone() {
  MOZ_ASSERT(Inner().mSheets.Length() == 1, "Should've just cloned");
  MOZ_ASSERT(Inner().mSheets[0] == this);
  MOZ_ASSERT(Inner().mChildren.IsEmpty());

  auto* contents = Inner().mContents.get();
  RefPtr<ServoCssRules> rules = Servo_StyleSheet_GetRules(contents).Consume();

  uint32_t index = 0;
  while (true) {
    uint32_t line, column;  // Actually unused.
    RefPtr<RawServoImportRule> import =
        Servo_CssRules_GetImportRuleAt(rules, index, &line, &column).Consume();
    if (!import) {
      // Note that only @charset rules come before @import rules, and @charset
      // rules are parsed but skipped, so we can stop iterating as soon as we
      // find something that isn't an @import rule.
      break;
    }
    auto* sheet = const_cast<StyleSheet*>(Servo_ImportRule_GetSheet(import));
    MOZ_ASSERT(sheet);
    AppendStyleSheetSilently(*sheet);
    index++;
  }
}

already_AddRefed<StyleSheet> StyleSheet::CreateEmptyChildSheet(
    already_AddRefed<dom::MediaList> aMediaList) const {
  RefPtr<StyleSheet> child =
      new StyleSheet(ParsingMode(), CORSMode::CORS_NONE, SRIMetadata());

  child->mMedia = aMediaList;
  return child.forget();
}

// We disable parallel stylesheet parsing if any of the following three
// conditions hold:
//
// (1) The pref is off.
// (2) The browser is recording CSS errors (which parallel parsing can't
//     handle).
// (3) The stylesheet is a chrome stylesheet, since those can use
//     -moz-bool-pref, which needs to access the pref service, which is not
//     threadsafe.
static bool AllowParallelParse(css::Loader& aLoader, nsIURI* aSheetURI) {
  // If the browser is recording CSS errors, we need to use the sequential path
  // because the parallel path doesn't support that.
  Document* doc = aLoader.GetDocument();
  if (doc && css::ErrorReporter::ShouldReportErrors(*doc)) {
    return false;
  }

  // If this is a chrome stylesheet, it might use -moz-bool-pref, which needs to
  // access the pref service, which is not thread-safe. We could probably expose
  // the relevant booleans as thread-safe var caches if we needed to, but
  // parsing chrome stylesheets in parallel is unlikely to be a win anyway.
  //
  // Note that UA stylesheets can also use -moz-bool-pref, but those are always
  // parsed sync.
  if (dom::IsChromeURI(aSheetURI)) {
    return false;
  }

  return true;
}

RefPtr<StyleSheetParsePromise> StyleSheet::ParseSheet(
    css::Loader& aLoader, const nsACString& aBytes,
    css::SheetLoadData& aLoadData) {
  MOZ_ASSERT(mParsePromise.IsEmpty());
  RefPtr<StyleSheetParsePromise> p = mParsePromise.Ensure(__func__);
  SetURLExtraData();

  if (!AllowParallelParse(aLoader, GetSheetURI())) {
    RefPtr<RawServoStyleSheetContents> contents =
        Servo_StyleSheet_FromUTF8Bytes(
            &aLoader, this, &aLoadData, &aBytes, mParsingMode, Inner().mURLData,
            aLoadData.mLineNumber, aLoader.GetCompatibilityMode(),
            /* reusable_sheets = */ nullptr,
            StyleSanitizationKind::None,
            /* sanitized_output = */ nullptr)
            .Consume();
    FinishAsyncParse(contents.forget());
  } else {
    auto holder = MakeRefPtr<css::SheetLoadDataHolder>(__func__, &aLoadData);
    Servo_StyleSheet_FromUTF8BytesAsync(
        holder, Inner().mURLData, &aBytes, mParsingMode, aLoadData.mLineNumber,
        aLoader.GetCompatibilityMode());
  }

  return p;
}

void StyleSheet::FinishAsyncParse(
    already_AddRefed<RawServoStyleSheetContents> aSheetContents) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mParsePromise.IsEmpty());
  Inner().mContents = aSheetContents;
  FinishParse();
  mParsePromise.Resolve(true, __func__);
}

void StyleSheet::ParseSheetSync(
    css::Loader* aLoader, const nsACString& aBytes,
    css::SheetLoadData* aLoadData, uint32_t aLineNumber,
    css::LoaderReusableStyleSheets* aReusableSheets) {
  nsCompatibility compatMode =
      aLoader ? aLoader->GetCompatibilityMode() : eCompatibility_FullStandards;

  SetURLExtraData();
  Inner().mContents =
      Servo_StyleSheet_FromUTF8Bytes(aLoader, this, aLoadData, &aBytes,
                                     mParsingMode, Inner().mURLData,
                                     aLineNumber, compatMode, aReusableSheets,
                                     StyleSanitizationKind::None,
                                     /* sanitized_output = */ nullptr)
          .Consume();

  FinishParse();
}

void StyleSheet::FinishParse() {
  nsString sourceMapURL;
  Servo_StyleSheet_GetSourceMapURL(Inner().mContents, &sourceMapURL);
  SetSourceMapURLFromComment(sourceMapURL);

  nsString sourceURL;
  Servo_StyleSheet_GetSourceURL(Inner().mContents, &sourceURL);
  SetSourceURL(sourceURL);
}

void StyleSheet::ReparseSheet(const nsACString& aInput, ErrorResult& aRv) {
  if (!IsComplete()) {
    return aRv.ThrowInvalidAccessError("Cannot reparse still-loading sheet");
  }

  // Allowing to modify UA sheets is dangerous (in the sense that C++ code
  // relies on rules in those sheets), plus they're probably going to be shared
  // across processes in which case this is directly a no-go.
  if (IsReadOnly()) {
    return;
  }

  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  RefPtr<css::Loader> loader;
  if (Document* doc = GetAssociatedDocument()) {
    loader = doc->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  } else {
    loader = new css::Loader;
  }

  WillDirty();

  // cache child sheets to reuse
  css::LoaderReusableStyleSheets reusableSheets;
  for (StyleSheet* child : ChildSheets()) {
    if (child->GetOriginalURI()) {
      reusableSheets.AddReusableSheet(child);
    }
  }

  // Clean up child sheets list.
  for (StyleSheet* child : ChildSheets()) {
    child->mParent = nullptr;
    child->ClearAssociatedDocumentOrShadowRoot();
  }
  Inner().mChildren.Clear();

  uint32_t lineNumber = 1;
  if (mOwningNode) {
    nsCOMPtr<nsIStyleSheetLinkingElement> link = do_QueryInterface(mOwningNode);
    if (link) {
      lineNumber = link->GetLineNumber();
    }
  }

  // Notify to the stylesets about the old rules going away.
  {
    ServoCSSRuleList* ruleList = GetCssRulesInternal();
    MOZ_ASSERT(ruleList);

    uint32_t ruleCount = ruleList->Length();
    for (uint32_t i = 0; i < ruleCount; ++i) {
      css::Rule* rule = ruleList->GetRule(i);
      MOZ_ASSERT(rule);
      RuleRemoved(*rule);
    }
  }

  DropRuleList();

  ParseSheetSync(loader, aInput, /* aLoadData = */ nullptr, lineNumber,
                 &reusableSheets);

  // Notify the stylesets about the new rules.
  {
    // Get the rule list (which will need to be regenerated after ParseSheet).
    ServoCSSRuleList* ruleList = GetCssRulesInternal();
    MOZ_ASSERT(ruleList);

    uint32_t ruleCount = ruleList->Length();
    for (uint32_t i = 0; i < ruleCount; ++i) {
      css::Rule* rule = ruleList->GetRule(i);
      MOZ_ASSERT(rule);
      RuleAdded(*rule);
    }
  }

  // Our rules are no longer considered modified.
  ClearModifiedRules();
}

void StyleSheet::DropRuleList() {
  if (mRuleList) {
    mRuleList->DropReferences();
    mRuleList = nullptr;
  }
}

already_AddRefed<StyleSheet> StyleSheet::Clone(
    StyleSheet* aCloneParent, dom::CSSImportRule* aCloneOwnerRule,
    dom::DocumentOrShadowRoot* aCloneDocumentOrShadowRoot,
    nsINode* aCloneOwningNode) const {
  RefPtr<StyleSheet> clone =
      new StyleSheet(*this, aCloneParent, aCloneOwnerRule,
                     aCloneDocumentOrShadowRoot, aCloneOwningNode);
  return clone.forget();
}

ServoCSSRuleList* StyleSheet::GetCssRulesInternal() {
  if (!mRuleList) {
    EnsureUniqueInner();

    RefPtr<ServoCssRules> rawRules =
        Servo_StyleSheet_GetRules(Inner().mContents).Consume();
    MOZ_ASSERT(rawRules);
    mRuleList = new ServoCSSRuleList(rawRules.forget(), this, nullptr);
  }
  return mRuleList;
}

uint32_t StyleSheet::InsertRuleInternal(const nsAString& aRule, uint32_t aIndex,
                                        ErrorResult& aRv) {
  MOZ_ASSERT(!IsReadOnly());

  // Ensure mRuleList is constructed.
  GetCssRulesInternal();

  aRv = mRuleList->InsertRule(aRule, aIndex);
  if (aRv.Failed()) {
    return 0;
  }

  // XXX We may not want to get the rule when stylesheet change event
  // is not enabled.
  css::Rule* rule = mRuleList->GetRule(aIndex);
  RuleAdded(*rule);

  return aIndex;
}

void StyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv) {
  MOZ_ASSERT(!IsReadOnly());

  // Ensure mRuleList is constructed.
  GetCssRulesInternal();
  if (aIndex >= mRuleList->Length()) {
    aRv.ThrowIndexSizeError(
        nsPrintfCString("Cannot delete rule at index %u"
                        " because the number of rules is only %u",
                        aIndex, mRuleList->Length()));
    return;
  }

  // Hold a strong ref to the rule so it doesn't die when we remove it
  // from the list. XXX We may not want to hold it if stylesheet change
  // event is not enabled.
  RefPtr<css::Rule> rule = mRuleList->GetRule(aIndex);
  aRv = mRuleList->DeleteRule(aIndex);
  if (!aRv.Failed()) {
    RuleRemoved(*rule);
  }
}

nsresult StyleSheet::InsertRuleIntoGroupInternal(const nsAString& aRule,
                                                 css::GroupRule* aGroup,
                                                 uint32_t aIndex) {
  MOZ_ASSERT(!IsReadOnly());

  auto rules = static_cast<ServoCSSRuleList*>(aGroup->CssRules());
  MOZ_ASSERT(rules->GetParentRule() == aGroup);
  return rules->InsertRule(aRule, aIndex);
}

StyleOrigin StyleSheet::GetOrigin() const {
  return Servo_StyleSheet_GetOrigin(Inner().mContents);
}

void StyleSheet::SetSharedContents(const ServoCssRules* aSharedRules) {
  MOZ_ASSERT(!IsComplete());

  SetURLExtraData();

  Inner().mContents =
      Servo_StyleSheet_FromSharedData(Inner().mURLData, aSharedRules).Consume();

  // Don't call FinishParse(), since that tries to set source map URLs,
  // which we don't have.
}

const ServoCssRules* StyleSheet::ToShared(
    RawServoSharedMemoryBuilder* aBuilder) {
  // Assert some things we assume when creating a StyleSheet using shared
  // memory.
  MOZ_ASSERT(GetReferrerInfo()->ReferrerPolicy() == ReferrerPolicy::_empty);
  MOZ_ASSERT(GetReferrerInfo()->GetSendReferrer());
  MOZ_ASSERT(!nsCOMPtr<nsIURI>(GetReferrerInfo()->GetComputedReferrer()));
  MOZ_ASSERT(GetCORSMode() == CORS_NONE);
  MOZ_ASSERT(Inner().mIntegrity.IsEmpty());
  MOZ_ASSERT(Principal()->IsSystemPrincipal());

  return Servo_SharedMemoryBuilder_AddStylesheet(aBuilder, Inner().mContents);
}

bool StyleSheet::IsReadOnly() const {
  return IsComplete() && GetOrigin() == StyleOrigin::UserAgent;
}

}  // namespace mozilla
