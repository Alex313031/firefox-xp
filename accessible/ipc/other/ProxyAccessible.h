/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ProxyAccessible_h
#define mozilla_a11y_ProxyAccessible_h

#include "Accessible.h"
#include "mozilla/a11y/ProxyAccessibleBase.h"
#include "mozilla/a11y/Role.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"

namespace mozilla {
namespace a11y {

class ProxyAccessible : public ProxyAccessibleBase<ProxyAccessible> {
 public:
  ProxyAccessible(uint64_t aID, ProxyAccessible* aParent,
                  DocAccessibleParent* aDoc, role aRole, uint32_t aInterfaces)
      : ProxyAccessibleBase(aID, aParent, aDoc, aRole, aInterfaces)

  {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }

  ~ProxyAccessible() { MOZ_COUNT_DTOR(ProxyAccessible); }

#include "mozilla/a11y/ProxyAccessibleShared.h"

 protected:
  explicit ProxyAccessible(DocAccessibleParent* aThisAsDoc)
      : ProxyAccessibleBase(aThisAsDoc) {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }
};

}  // namespace a11y
}  // namespace mozilla

#endif
