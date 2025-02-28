/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "mozilla/WritingModes.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay & nsStyleBackground::Position

namespace mozilla {

ScrollStyles::ScrollStyles(const nsStyleDisplay& aDisplay)
    : mHorizontal(aDisplay.mOverflowX), mVertical(aDisplay.mOverflowY) {}

}  // namespace mozilla
