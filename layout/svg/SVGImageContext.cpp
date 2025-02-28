/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "SVGImageContext.h"

// Keep others in (case-insensitive) order:
#include "gfxUtils.h"
#include "mozilla/Preferences.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsStyleStruct.h"

namespace mozilla {

/* static */
void SVGImageContext::MaybeStoreContextPaint(Maybe<SVGImageContext>& aContext,
                                             nsIFrame* aFromFrame,
                                             imgIContainer* aImgContainer) {
  return MaybeStoreContextPaint(aContext, aFromFrame->Style(), aImgContainer);
}

/* static */
void SVGImageContext::MaybeStoreContextPaint(Maybe<SVGImageContext>& aContext,
                                             ComputedStyle* aFromComputedStyle,
                                             imgIContainer* aImgContainer) {
  const nsStyleSVG* style = aFromComputedStyle->StyleSVG();

  if (!style->ExposesContextProperties()) {
    // Content must have '-moz-context-properties' set to the names of the
    // properties it wants to expose to images it links to.
    return;
  }

  if (aImgContainer->GetType() != imgIContainer::TYPE_VECTOR) {
    // Avoid this overhead for raster images.
    return;
  }

  bool haveContextPaint = false;

  RefPtr<SVGEmbeddingContextPaint> contextPaint =
      new SVGEmbeddingContextPaint();

  if ((style->mMozContextProperties.bits & StyleContextPropertyBits::FILL) &&
      style->mFill.kind.IsColor()) {
    haveContextPaint = true;
    contextPaint->SetFill(
        style->mFill.kind.AsColor().CalcColor(*aFromComputedStyle));
  }
  if ((style->mMozContextProperties.bits & StyleContextPropertyBits::STROKE) &&
      style->mStroke.kind.IsColor()) {
    haveContextPaint = true;
    contextPaint->SetStroke(
        style->mStroke.kind.AsColor().CalcColor(*aFromComputedStyle));
  }
  if (style->mMozContextProperties.bits &
      StyleContextPropertyBits::FILL_OPACITY) {
    haveContextPaint = true;
    contextPaint->SetFillOpacity(style->mFillOpacity);
  }
  if (style->mMozContextProperties.bits &
      StyleContextPropertyBits::STROKE_OPACITY) {
    haveContextPaint = true;
    contextPaint->SetStrokeOpacity(style->mStrokeOpacity);
  }

  if (haveContextPaint) {
    if (!aContext) {
      aContext.emplace();
    }
    aContext->mContextPaint = std::move(contextPaint);
  }
}

}  // namespace mozilla
