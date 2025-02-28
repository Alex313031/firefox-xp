/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyConfigLookupChild.h"

namespace mozilla {
namespace net {

ProxyConfigLookupChild::ProxyConfigLookupChild(
    std::function<void(bool)>&& aCallback)
    : mCallback(std::move(aCallback)) {}

ProxyConfigLookupChild::~ProxyConfigLookupChild() = default;

mozilla::ipc::IPCResult ProxyConfigLookupChild::Recv__delete__(
    const bool& aProxied) {
  mCallback(aProxied);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
