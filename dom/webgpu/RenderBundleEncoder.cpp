/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "RenderBundleEncoder.h"

#include "RenderBundle.h"

namespace mozilla {
namespace webgpu {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RenderBundleEncoder, RenderEncoderBase,
                                   mParent)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(RenderBundleEncoder,
                                               RenderEncoderBase)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(RenderBundleEncoder,
                                               RenderEncoderBase)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
GPU_IMPL_JS_WRAP(RenderBundleEncoder)

}  // namespace webgpu
}  // namespace mozilla
