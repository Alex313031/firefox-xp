/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentBlocker.h"
#include "nsIContent.h"
#include "nsIURI.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIDocShell.h"
#include "nsString.h"
#include "nsContentPolicyUtils.h"
#include "mozilla/ArrayUtils.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

// Possible behavior pref values
// Those map to the nsIPermissionManager values where possible
#define BEHAVIOR_ACCEPT nsIPermissionManager::ALLOW_ACTION
#define BEHAVIOR_REJECT nsIPermissionManager::DENY_ACTION
#define BEHAVIOR_NOFOREIGN 3

// From nsIContentPolicy
static const nsLiteralCString kTypeString[] = {
    NS_LITERAL_CSTRING("other"),
    NS_LITERAL_CSTRING("script"),
    NS_LITERAL_CSTRING("image"),
    NS_LITERAL_CSTRING("stylesheet"),
    NS_LITERAL_CSTRING("object"),
    NS_LITERAL_CSTRING("document"),
    NS_LITERAL_CSTRING("subdocument"),
    NS_LITERAL_CSTRING("refresh"),
    NS_LITERAL_CSTRING("xbl"),
    NS_LITERAL_CSTRING("ping"),
    NS_LITERAL_CSTRING("xmlhttprequest"),
    NS_LITERAL_CSTRING("objectsubrequest"),
    NS_LITERAL_CSTRING("dtd"),
    NS_LITERAL_CSTRING("font"),
    NS_LITERAL_CSTRING("media"),
    NS_LITERAL_CSTRING("websocket"),
    NS_LITERAL_CSTRING("csp_report"),
    NS_LITERAL_CSTRING("xslt"),
    NS_LITERAL_CSTRING("beacon"),
    NS_LITERAL_CSTRING("fetch"),
    NS_LITERAL_CSTRING("image"),
    NS_LITERAL_CSTRING("manifest"),
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_SCRIPT
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_WORKER
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_SHARED_WORKER
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_EMBED
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_OBJECT
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_FRAME
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_IFRAME
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_AUDIO
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_VIDEO
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_TRACK
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_XMLHTTPREQUEST
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_EVENTSOURCE
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_SERVICE_WORKER
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_SCRIPT_PRELOAD
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_IMAGE
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_IMAGE_PRELOAD
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_STYLESHEET
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_STYLESHEET_PRELOAD
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_IMAGE_FAVICON
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_WORKERS_IMPORT_SCRIPTS
    NS_LITERAL_CSTRING("saveas_download"),
    NS_LITERAL_CSTRING("speculative"),
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_MODULE
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_MODULE_PRELOAD
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_DTD
    NS_LITERAL_CSTRING(""),  // TYPE_INTERNAL_FORCE_ALLOWED_DTD
};

#define NUMBER_OF_TYPES MOZ_ARRAY_LENGTH(kTypeString)
uint8_t nsContentBlocker::mBehaviorPref[NUMBER_OF_TYPES];

NS_IMPL_ISUPPORTS(nsContentBlocker, nsIContentPolicy, nsIObserver,
                  nsISupportsWeakReference)

nsContentBlocker::nsContentBlocker() {
  memset(mBehaviorPref, BEHAVIOR_ACCEPT, NUMBER_OF_TYPES);
}

nsresult nsContentBlocker::Init() {
  mPermissionManager = nsPermissionManager::GetInstance();
  if (!mPermissionManager) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch("permissions.default.",
                              getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  // Migrate old image blocker pref
  nsCOMPtr<nsIPrefBranch> oldPrefBranch;
  oldPrefBranch = do_QueryInterface(prefService);
  int32_t oldPref;
  rv = oldPrefBranch->GetIntPref("network.image.imageBehavior", &oldPref);
  if (NS_SUCCEEDED(rv) && oldPref) {
    int32_t newPref;
    switch (oldPref) {
      default:
        newPref = BEHAVIOR_ACCEPT;
        break;
      case 1:
        newPref = BEHAVIOR_NOFOREIGN;
        break;
      case 2:
        newPref = BEHAVIOR_REJECT;
        break;
    }
    prefBranch->SetIntPref("image", newPref);
    oldPrefBranch->ClearUserPref("network.image.imageBehavior");
  }

  // The branch is not a copy of the prefservice, but a new object, because
  // it is a non-default branch. Adding obeservers to it will only work if
  // we make sure that the object doesn't die. So, keep a reference to it.
  mPrefBranchInternal = prefBranch;

  rv = mPrefBranchInternal->AddObserver("", this, true);
  PrefChanged(prefBranch, nullptr);

  return rv;
}

#undef LIMIT
#define LIMIT(x, low, high, default) \
  ((x) >= (low) && (x) <= (high) ? (x) : (default))

void nsContentBlocker::PrefChanged(nsIPrefBranch* aPrefBranch,
                                   const char* aPref) {
  int32_t val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  for (uint32_t i = 0; i < NUMBER_OF_TYPES; ++i) {
    if (!kTypeString[i].IsEmpty() && PREF_CHANGED(kTypeString[i].get()) &&
        NS_SUCCEEDED(aPrefBranch->GetIntPref(kTypeString[i].get(), &val))) {
      mBehaviorPref[i] = LIMIT(val, 1, 3, 1);
    }
  }
}

// nsIContentPolicy Implementation
NS_IMETHODIMP
nsContentBlocker::ShouldLoad(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                             const nsACString& aMimeGuess, int16_t* aDecision) {
  uint32_t contentType = aLoadInfo->GetExternalContentPolicyType();
  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadInfo->LoadingPrincipal();
  nsCOMPtr<nsIURI> requestingLocation;
  if (loadingPrincipal) {
    loadingPrincipal->GetURI(getter_AddRefs(requestingLocation));
  }

  MOZ_ASSERT(contentType == nsContentUtils::InternalContentPolicyTypeToExternal(
                                contentType),
             "We should only see external content policy types here.");

  *aDecision = nsIContentPolicy::ACCEPT;
  nsresult rv;

  // Ony support NUMBER_OF_TYPES content types. that all there is at the
  // moment, but you never know...
  if (contentType > NUMBER_OF_TYPES) return NS_OK;

  // we can't do anything without this
  if (!aContentLocation) return NS_OK;

  // The final type of an object tag may mutate before it reaches
  // shouldProcess, so we cannot make any sane blocking decisions here
  if (contentType == nsIContentPolicy::TYPE_OBJECT) return NS_OK;

  // we only want to check http, https, ftp
  // for chrome:// and resources and others, no need to check.
  nsAutoCString scheme;
  aContentLocation->GetScheme(scheme);
  if (!scheme.LowerCaseEqualsLiteral("ftp") &&
      !scheme.LowerCaseEqualsLiteral("http") &&
      !scheme.LowerCaseEqualsLiteral("https"))
    return NS_OK;

  bool shouldLoad, fromPrefs;
  rv = TestPermission(aContentLocation, requestingLocation, contentType,
                      &shouldLoad, &fromPrefs);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!shouldLoad) {
    NS_SetRequestBlockingReason(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_CONTENT_BLOCKED);

    if (fromPrefs) {
      *aDecision = nsIContentPolicy::REJECT_TYPE;
    } else {
      *aDecision = nsIContentPolicy::REJECT_SERVER;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentBlocker::ShouldProcess(nsIURI* aContentLocation,
                                nsILoadInfo* aLoadInfo,
                                const nsACString& aMimeGuess,
                                int16_t* aDecision) {
  uint32_t contentType = aLoadInfo->GetExternalContentPolicyType();
  nsCOMPtr<nsISupports> requestingContext = aLoadInfo->GetLoadingContext();
  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadInfo->LoadingPrincipal();
  nsCOMPtr<nsIURI> requestingLocation;
  if (loadingPrincipal) {
    loadingPrincipal->GetURI(getter_AddRefs(requestingLocation));
  }

  MOZ_ASSERT(contentType == nsContentUtils::InternalContentPolicyTypeToExternal(
                                contentType),
             "We should only see external content policy types here.");

  // For loads where requesting context is chrome, we should just
  // accept.  Those are most likely toplevel loads in windows, and
  // chrome generally knows what it's doing anyway.
  nsCOMPtr<nsIDocShellTreeItem> item =
      NS_CP_GetDocShellFromContext(requestingContext);

  if (item && item->ItemType() == nsIDocShellTreeItem::typeChrome) {
    *aDecision = nsIContentPolicy::ACCEPT;
    return NS_OK;
  }

  // For objects, we only check policy in shouldProcess, as the final type isn't
  // determined until the channel is open -- We don't want to block images in
  // object tags because plugins are disallowed.
  // NOTE that this bypasses the aContentLocation checks in ShouldLoad - this is
  // intentional, as aContentLocation may be null for plugins that load by type
  // (e.g. java)
  if (contentType == nsIContentPolicy::TYPE_OBJECT) {
    *aDecision = nsIContentPolicy::ACCEPT;

    bool shouldLoad, fromPrefs;
    nsresult rv = TestPermission(aContentLocation, requestingLocation,
                                 contentType, &shouldLoad, &fromPrefs);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!shouldLoad) {
      NS_SetRequestBlockingReason(
          aLoadInfo,
          nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_CONTENT_BLOCKED);

      if (fromPrefs) {
        *aDecision = nsIContentPolicy::REJECT_TYPE;
      } else {
        *aDecision = nsIContentPolicy::REJECT_SERVER;
      }
    }
    return NS_OK;
  }

  // This isn't a load from chrome or an object tag - Just do a ShouldLoad()
  // check -- we want the same answer here
  return ShouldLoad(aContentLocation, aLoadInfo, aMimeGuess, aDecision);
}

nsresult nsContentBlocker::TestPermission(nsIURI* aCurrentURI,
                                          nsIURI* aFirstURI,
                                          int32_t aContentType,
                                          bool* aPermission, bool* aFromPrefs) {
  *aFromPrefs = false;
  nsresult rv;

  if (kTypeString[aContentType - 1].IsEmpty()) {
    // Disallow internal content policy types, they should not be used here.
    *aPermission = false;
    return NS_OK;
  }

  // This default will also get used if there is an unknown value in the
  // permission list, or if the permission manager returns unknown values.
  *aPermission = true;

  // check the permission list first; if we find an entry, it overrides
  // default prefs.
  // Don't forget the aContentType ranges from 1..8, while the
  // array is indexed 0..7
  // All permissions tested by this method are preload permissions, so don't
  // bother actually checking with the permission manager unless we have a
  // preload permission.
  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  if (mPermissionManager->HasPreloadPermissions()) {
    rv = mPermissionManager->LegacyTestPermissionFromURI(
        aCurrentURI, nullptr, kTypeString[aContentType - 1], &permission);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If there is nothing on the list, use the default.
  if (!permission) {
    permission = mBehaviorPref[aContentType - 1];
    *aFromPrefs = true;
  }

  // Use the fact that the nsIPermissionManager values map to
  // the BEHAVIOR_* values above.
  switch (permission) {
    case BEHAVIOR_ACCEPT:
      *aPermission = true;
      break;
    case BEHAVIOR_REJECT:
      *aPermission = false;
      break;

    case BEHAVIOR_NOFOREIGN:
      // Third party checking

      // Need a requesting uri for third party checks to work.
      if (!aFirstURI) return NS_OK;

      // chrome: and resource: are always trusted.
      if (aFirstURI->SchemeIs("chrome") || aFirstURI->SchemeIs("resource")) {
        return NS_OK;
      }

      // compare tails of names checking to see if they have a common domain
      // we do this by comparing the tails of both names where each tail
      // includes at least one dot

      // A more generic method somewhere would be nice

      nsAutoCString currentHost;
      rv = aCurrentURI->GetAsciiHost(currentHost);
      NS_ENSURE_SUCCESS(rv, rv);

      // Search for two dots, starting at the end.
      // If there are no two dots found, ++dot will turn to zero,
      // that will return the entire string.
      int32_t dot = currentHost.RFindChar('.');
      dot = currentHost.RFindChar('.', dot - 1);
      ++dot;

      // Get the domain, ie the last part of the host (www.domain.com ->
      // domain.com) This will break on co.uk
      const nsACString& tail =
          Substring(currentHost, dot, currentHost.Length() - dot);

      nsAutoCString firstHost;
      rv = aFirstURI->GetAsciiHost(firstHost);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the tail is longer then the whole firstHost, it will never match
      if (firstHost.Length() < tail.Length()) {
        *aPermission = false;
        return NS_OK;
      }

      // Get the last part of the firstUri with the same length as |tail|
      const nsACString& firstTail = Substring(
          firstHost, firstHost.Length() - tail.Length(), tail.Length());

      // Check that both tails are the same, and that just before the tail in
      // |firstUri| there is a dot. That means both url are in the same domain
      if ((firstHost.Length() > tail.Length() &&
           firstHost.CharAt(firstHost.Length() - tail.Length() - 1) != '.') ||
          !tail.Equals(firstTail)) {
        *aPermission = false;
      }
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentBlocker::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  NS_ASSERTION(!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (mPrefBranchInternal)
    PrefChanged(mPrefBranchInternal, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
