/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerInterceptController.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/StorageAccess.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "ServiceWorkerManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ServiceWorkerInterceptController,
                  nsINetworkInterceptController)

NS_IMETHODIMP
ServiceWorkerInterceptController::ShouldPrepareForIntercept(
    nsIURI* aURI, nsIChannel* aChannel, bool* aShouldIntercept) {
  *aShouldIntercept = false;

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  // For subresource requests we base our decision solely on the client's
  // controller value.  Any settings that would have blocked service worker
  // access should have been set before the initial navigation created the
  // window.
  if (!nsContentUtils::IsNonSubresourceRequest(aChannel)) {
    const Maybe<ServiceWorkerDescriptor>& controller =
        loadInfo->GetController();
    *aShouldIntercept = controller.isSome();
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal = BasePrincipal::CreateCodebasePrincipal(
      aURI, loadInfo->GetOriginAttributes());

  // First check with the ServiceWorkerManager for a matching service worker.
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm || !swm->IsAvailable(principal, aURI)) {
    return NS_OK;
  }

  // Then check to see if we are allowed to control the window.
  // It is important to check for the availability of the service worker first
  // to avoid showing warnings about the use of third-party cookies in the UI
  // unnecessarily when no service worker is being accessed.
  if (StorageAllowedForChannel(aChannel) != StorageAccess::eAllow) {
    return NS_OK;
  }

  *aShouldIntercept = true;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInterceptController::ChannelIntercepted(
    nsIInterceptedChannel* aChannel) {
  // Note, do not cancel the interception here.  The caller will try to
  // ResetInterception() on error.

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  swm->DispatchFetchEvent(aChannel, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
