/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollStyles_h
#define mozilla_ScrollStyles_h

#include <stdint.h>
#include "nsStyleConsts.h"
#include "mozilla/dom/WindowBinding.h"

// Forward declarations
struct nsStyleDisplay;

namespace mozilla {

// NOTE: Only styles that are propagated from the <body> should end up in this
// class.
struct ScrollStyles {
  // Always one of Scroll, Hidden, or Auto
  StyleOverflow mHorizontal;
  StyleOverflow mVertical;

  ScrollStyles(StyleOverflow aH, StyleOverflow aV)
      : mHorizontal(aH), mVertical(aV) {}

  explicit ScrollStyles(const nsStyleDisplay&);
  bool operator==(const ScrollStyles& aStyles) const {
    return aStyles.mHorizontal == mHorizontal && aStyles.mVertical == mVertical;
  }
  bool operator!=(const ScrollStyles& aStyles) const {
    return !(*this == aStyles);
  }
  bool IsHiddenInBothDirections() const {
    return mHorizontal == StyleOverflow::Hidden &&
           mVertical == StyleOverflow::Hidden;
  }
};

}  // namespace mozilla

#endif  // mozilla_ScrollStyles_h
