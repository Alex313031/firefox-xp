/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FocusTarget_h
#define mozilla_layers_FocusTarget_h

#include <stdint.h>  // for int32_t, uint32_t

#include "mozilla/DefineEnum.h"                  // for MOZ_DEFINE_ENUM
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ViewID
#ifdef MOZ_BUILD_WEBRENDER
#  include "mozilla/webrender/WebRenderTypes.h"  // for RenderRoot
#endif
#include "mozilla/Variant.h"  // for Variant

namespace mozilla {

class PresShell;

namespace layers {

/**
 * This class is used for communicating information about the currently focused
 * element of a document and the scrollable frames to use when keyboard
 * scrolling it. It is created on the main thread at paint-time, but is then
 * passed over IPC to the compositor/APZ code.
 */
class FocusTarget final {
 public:
  struct ScrollTargets {
    ScrollableLayerGuid::ViewID mHorizontal;
    ScrollableLayerGuid::ViewID mVertical;
#ifdef MOZ_BUILD_WEBRENDER
    wr::RenderRoot mHorizontalRenderRoot;
    wr::RenderRoot mVerticalRenderRoot;
#endif

    bool operator==(const ScrollTargets& aRhs) const {
      bool ret =
          (mHorizontal == aRhs.mHorizontal && mVertical == aRhs.mVertical);
#ifdef MOZ_BUILD_WEBRENDER
      if (ret) {
        // The render root is a function of where the scrollable frame is in
        // the DOM/layout tree, so if the ViewIDs match then the render roots
        // should also match.
        MOZ_ASSERT(mHorizontalRenderRoot == aRhs.mHorizontalRenderRoot &&
                   mVerticalRenderRoot == aRhs.mVerticalRenderRoot);
      }
#endif
      return ret;
    }
  };

  // We need this to represent the case where mData has no focus target data
  // because we can't have an empty variant
  struct NoFocusTarget {
    bool operator==(const NoFocusTarget& aRhs) const { return true; }
  };

  FocusTarget();

  /**
   * Construct a focus target for the specified top level PresShell
   */
  FocusTarget(PresShell* aRootPresShell, uint64_t aFocusSequenceNumber);

  bool operator==(const FocusTarget& aRhs) const;

  const char* Type() const;

 public:
  // The content sequence number recorded at the time of this class's creation
  uint64_t mSequenceNumber;

  // Whether there are keydown, keypress, or keyup event listeners
  // in the event target chain of the focused element
  bool mFocusHasKeyEventListeners;

  mozilla::Variant<LayersId, ScrollTargets, NoFocusTarget> mData;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_FocusTarget_h
