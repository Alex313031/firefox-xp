/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* An opaque integral type supporting only comparison operators. */

#ifndef mozilla_Opaque_h
#define mozilla_Opaque_h

#include "mozilla/TypeTraits.h"

namespace mozilla {

/**
 * Opaque<T> is a replacement for integral T in cases where only comparisons
 * must be supported, and it's desirable to prevent accidental dependency on
 * exact values.
 */
template <typename T>
class Opaque final {
  static_assert(mozilla::IsIntegral<T>::value,
                "mozilla::Opaque only supports integral types");

  T mValue;

 public:
  Opaque() = default;
  explicit Opaque(T aValue) : mValue(aValue) {}

  bool operator==(const Opaque& aOther) const {
    return mValue == aOther.mValue;
  }

  bool operator!=(const Opaque& aOther) const { return !(*this == aOther); }
};

}  // namespace mozilla

#endif /* mozilla_Opaque_h */
