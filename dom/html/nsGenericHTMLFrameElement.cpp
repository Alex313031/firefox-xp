/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLFrameElement.h"

#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/XULFrameElement.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/ErrorResult.h"
#include "GeckoProfiler.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "nsIScrollable.h"
#include "nsPresContext.h"
#include "nsServiceManagerUtils.h"
#include "nsSubDocumentFrame.h"
#include "nsAttrValueOrString.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericHTMLFrameElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGenericHTMLFrameElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOpenerWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBrowserElementAPI)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsGenericHTMLFrameElement,
                                                nsGenericHTMLElement)
  if (tmp->mFrameLoader) {
    tmp->mFrameLoader->Destroy();
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOpenerWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBrowserElementAPI)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(
    nsGenericHTMLFrameElement, nsGenericHTMLElement, nsFrameLoaderOwner,
    nsIDOMMozBrowserFrame, nsIMozBrowserFrame, nsGenericHTMLFrameElement)

NS_IMETHODIMP
nsGenericHTMLFrameElement::GetMozbrowser(bool* aValue) {
  *aValue = GetBoolAttr(nsGkAtoms::mozbrowser);
  return NS_OK;
}
NS_IMETHODIMP
nsGenericHTMLFrameElement::SetMozbrowser(bool aValue) {
  return SetBoolAttr(nsGkAtoms::mozbrowser, aValue);
}

int32_t nsGenericHTMLFrameElement::TabIndexDefault() { return 0; }

nsGenericHTMLFrameElement::~nsGenericHTMLFrameElement() {
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}

Document* nsGenericHTMLFrameElement::GetContentDocument(
    nsIPrincipal& aSubjectPrincipal) {
  RefPtr<BrowsingContext> bc = GetContentWindowInternal();
  if (!bc) {
    return nullptr;
  }

  nsPIDOMWindowOuter* window = bc->GetDOMWindow();
  if (!window) {
    // Either our browsing context contents are out-of-process (in which case
    // clearly this is a cross-origin call and we should return null), or our
    // browsing context is torn-down enough to no longer have a window or a
    // document, and we should still return null.
    return nullptr;
  }
  Document* doc = window->GetDoc();
  if (!doc) {
    return nullptr;
  }

  // Return null for cross-origin contentDocument.
  if (!aSubjectPrincipal.SubsumesConsideringDomain(doc->NodePrincipal())) {
    return nullptr;
  }
  return doc;
}

BrowsingContext* nsGenericHTMLFrameElement::GetContentWindowInternal() {
  EnsureFrameLoader();

  if (!mFrameLoader) {
    return nullptr;
  }

  if (mFrameLoader->DepthTooGreat()) {
    // Claim to have no contentWindow
    return nullptr;
  }

  RefPtr<BrowsingContext> bc = mFrameLoader->GetBrowsingContext();
  return bc;
}

Nullable<WindowProxyHolder> nsGenericHTMLFrameElement::GetContentWindow() {
  RefPtr<BrowsingContext> bc = GetContentWindowInternal();
  if (!bc) {
    return nullptr;
  }
  return WindowProxyHolder(bc);
}

void nsGenericHTMLFrameElement::EnsureFrameLoader() {
  if (!IsInComposedDoc() || mFrameLoader || mFrameLoaderCreationDisallowed) {
    // If frame loader is there, we just keep it around, cached
    return;
  }

  // Strangely enough, this method doesn't actually ensure that the
  // frameloader exists.  It's more of a best-effort kind of thing.
  mFrameLoader = nsFrameLoader::Create(this, mOpenerWindow, mNetworkCreated);
}

nsresult nsGenericHTMLFrameElement::CreateRemoteFrameLoader(
    nsIRemoteTab* aBrowserParent) {
  MOZ_ASSERT(!mFrameLoader);
  EnsureFrameLoader();
  NS_ENSURE_STATE(mFrameLoader);
  mFrameLoader->SetRemoteBrowser(aBrowserParent);

  if (nsSubDocumentFrame* subdocFrame = do_QueryFrame(GetPrimaryFrame())) {
    // The reflow for this element already happened while we were waiting
    // for the iframe creation. Therefore the subdoc frame didn't have a
    // frameloader when UpdatePositionAndSize was supposed to be called in
    // ReflowFinished, and we need to do it properly now.
    mFrameLoader->UpdatePositionAndSize(subdocFrame);
  }
  return NS_OK;
}

void nsGenericHTMLFrameElement::PresetOpenerWindow(
    const Nullable<WindowProxyHolder>& aOpenerWindow, ErrorResult& aRv) {
  MOZ_ASSERT(!mFrameLoader);
  mOpenerWindow =
      aOpenerWindow.IsNull() ? nullptr : aOpenerWindow.Value().get();
}

void nsGenericHTMLFrameElement::SwapFrameLoaders(
    HTMLIFrameElement& aOtherLoaderOwner, ErrorResult& rv) {
  if (&aOtherLoaderOwner == this) {
    // nothing to do
    return;
  }

  aOtherLoaderOwner.SwapFrameLoaders(this, rv);
}

void nsGenericHTMLFrameElement::SwapFrameLoaders(
    XULFrameElement& aOtherLoaderOwner, ErrorResult& rv) {
  aOtherLoaderOwner.SwapFrameLoaders(this, rv);
}

void nsGenericHTMLFrameElement::SwapFrameLoaders(
    nsFrameLoaderOwner* aOtherLoaderOwner, mozilla::ErrorResult& rv) {
  if (RefPtr<Document> doc = GetComposedDoc()) {
    // SwapWithOtherLoader relies on frames being up-to-date.
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  RefPtr<nsFrameLoader> loader = GetFrameLoader();
  RefPtr<nsFrameLoader> otherLoader = aOtherLoaderOwner->GetFrameLoader();
  if (!loader || !otherLoader) {
    rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  rv = loader->SwapWithOtherLoader(otherLoader, this, aOtherLoaderOwner);
}

void nsGenericHTMLFrameElement::LoadSrc() {
  EnsureFrameLoader();

  if (!mFrameLoader) {
    return;
  }

  bool origSrc = !mSrcLoadHappened;
  mSrcLoadHappened = true;
  mFrameLoader->LoadFrame(origSrc);
}

nsresult nsGenericHTMLFrameElement::BindToTree(BindContext& aContext,
                                               nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInComposedDoc()) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Missing a script blocker!");

    AUTO_PROFILER_LABEL("nsGenericHTMLFrameElement::BindToTree", OTHER);

    // We're in a document now.  Kick off the frame load.
    LoadSrc();
  }

  // We're now in document and scripts may move us, so clear
  // the mNetworkCreated flag.
  mNetworkCreated = false;
  return rv;
}

void nsGenericHTMLFrameElement::UnbindFromTree(bool aNullParent) {
  if (mFrameLoader) {
    // This iframe is being taken out of the document, destroy the
    // iframe's frame loader (doing that will tear down the window in
    // this iframe).
    // XXXbz we really want to only partially destroy the frame
    // loader... we don't want to tear down the docshell.  Food for
    // later bug.
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

/* static */
int32_t nsGenericHTMLFrameElement::MapScrollingAttribute(
    const nsAttrValue* aValue) {
  int32_t mappedValue = nsIScrollable::Scrollbar_Auto;
  if (aValue && aValue->Type() == nsAttrValue::eEnum) {
    switch (aValue->GetEnumValue()) {
      case NS_STYLE_FRAME_OFF:
      case NS_STYLE_FRAME_NOSCROLL:
      case NS_STYLE_FRAME_NO:
        mappedValue = nsIScrollable::Scrollbar_Never;
        break;
    }
  }
  return mappedValue;
}

static bool PrincipalAllowsBrowserFrame(nsIPrincipal* aPrincipal) {
  nsCOMPtr<nsIPermissionManager> permMgr =
      mozilla::services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);
  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permMgr->TestPermissionFromPrincipal(
      aPrincipal, NS_LITERAL_CSTRING("browser"), &permission);
  NS_ENSURE_SUCCESS(rv, false);
  return permission == nsIPermissionManager::ALLOW_ACTION;
}

/* virtual */
nsresult nsGenericHTMLFrameElement::AfterSetAttr(
    int32_t aNameSpaceID, nsAtom* aName, const nsAttrValue* aValue,
    const nsAttrValue* aOldValue, nsIPrincipal* aMaybeScriptedPrincipal,
    bool aNotify) {
  if (aValue) {
    nsAttrValueOrString value(aValue);
    AfterMaybeChangeAttr(aNameSpaceID, aName, &value, aMaybeScriptedPrincipal,
                         aNotify);
  } else {
    AfterMaybeChangeAttr(aNameSpaceID, aName, nullptr, aMaybeScriptedPrincipal,
                         aNotify);
  }

  if (aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::scrolling) {
      if (mFrameLoader) {
        nsIDocShell* docshell = mFrameLoader->GetExistingDocShell();
        nsCOMPtr<nsIScrollable> scrollable = do_QueryInterface(docshell);
        if (scrollable) {
          int32_t cur;
          scrollable->GetDefaultScrollbarPreferences(
              nsIScrollable::ScrollOrientation_X, &cur);
          int32_t val = MapScrollingAttribute(aValue);
          if (cur != val) {
            scrollable->SetDefaultScrollbarPreferences(
                nsIScrollable::ScrollOrientation_X, val);
            scrollable->SetDefaultScrollbarPreferences(
                nsIScrollable::ScrollOrientation_Y, val);
            RefPtr<nsPresContext> presContext = docshell->GetPresContext();
            PresShell* presShell =
                presContext ? presContext->GetPresShell() : nullptr;
            nsIFrame* rootScroll =
                presShell ? presShell->GetRootScrollFrame() : nullptr;
            if (rootScroll) {
              presShell->FrameNeedsReflow(
                  rootScroll, IntrinsicDirty::StyleChange, NS_FRAME_IS_DIRTY);
            }
          }
        }
      }
    } else if (aName == nsGkAtoms::mozbrowser) {
      mReallyIsBrowser = !!aValue &&
                         StaticPrefs::dom_mozBrowserFramesEnabled() &&
                         PrincipalAllowsBrowserFrame(NodePrincipal());
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aMaybeScriptedPrincipal, aNotify);
}

nsresult nsGenericHTMLFrameElement::OnAttrSetButNotChanged(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    bool aNotify) {
  AfterMaybeChangeAttr(aNamespaceID, aName, &aValue, nullptr, aNotify);

  return nsGenericHTMLElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                      aValue, aNotify);
}

void nsGenericHTMLFrameElement::AfterMaybeChangeAttr(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString* aValue,
    nsIPrincipal* aMaybeScriptedPrincipal, bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src) {
      mSrcTriggeringPrincipal = nsContentUtils::GetAttrTriggeringPrincipal(
          this, aValue ? aValue->String() : EmptyString(),
          aMaybeScriptedPrincipal);
      if (!IsHTMLElement(nsGkAtoms::iframe) ||
          !HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc)) {
        // Don't propagate error here. The attribute was successfully
        // set or removed; that's what we should reflect.
        LoadSrc();
      }
    } else if (aName == nsGkAtoms::name) {
      // Propagate "name" to the docshell to make browsing context names live,
      // per HTML5.
      nsIDocShell* docShell =
          mFrameLoader ? mFrameLoader->GetExistingDocShell() : nullptr;
      if (docShell) {
        if (aValue) {
          docShell->SetName(aValue->String());
        } else {
          docShell->SetName(EmptyString());
        }
      }
    }
  }
}

void nsGenericHTMLFrameElement::DestroyContent() {
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsGenericHTMLElement::DestroyContent();
}

nsresult nsGenericHTMLFrameElement::CopyInnerTo(Element* aDest) {
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  Document* doc = aDest->OwnerDoc();
  if (doc->IsStaticDocument() && mFrameLoader) {
    nsGenericHTMLFrameElement* dest =
        static_cast<nsGenericHTMLFrameElement*>(aDest);
    nsFrameLoader* fl = nsFrameLoader::Create(dest, nullptr, false);
    NS_ENSURE_STATE(fl);
    dest->mFrameLoader = fl;
    mFrameLoader->CreateStaticClone(fl);
  }

  return rv;
}

bool nsGenericHTMLFrameElement::IsHTMLFocusable(bool aWithMouse,
                                                bool* aIsFocusable,
                                                int32_t* aTabIndex) {
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable,
                                            aTabIndex)) {
    return true;
  }

  *aIsFocusable = nsContentUtils::IsSubDocumentTabbable(this);

  if (!*aIsFocusable && aTabIndex) {
    *aTabIndex = -1;
  }

  return false;
}

/**
 * Return true if this frame element really is a mozbrowser.  (It
 * needs to have the right attributes, and its creator must have the right
 * permissions.)
 */
/* [infallible] */
nsresult nsGenericHTMLFrameElement::GetReallyIsBrowser(bool* aOut) {
  *aOut = mReallyIsBrowser;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::DisallowCreateFrameLoader() {
  MOZ_ASSERT(!mFrameLoader);
  MOZ_ASSERT(!mFrameLoaderCreationDisallowed);
  mFrameLoaderCreationDisallowed = true;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::AllowCreateFrameLoader() {
  MOZ_ASSERT(!mFrameLoader);
  MOZ_ASSERT(mFrameLoaderCreationDisallowed);
  mFrameLoaderCreationDisallowed = false;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::InitializeBrowserAPI() {
  MOZ_ASSERT(mFrameLoader);
  InitBrowserElementAPI();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::DestroyBrowserFrameScripts() {
  MOZ_ASSERT(mFrameLoader);
  DestroyBrowserElementFrameScripts();
  return NS_OK;
}
