/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_BindGroup_H_
#define GPU_BindGroup_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class BindGroup final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(BindGroup)
  GPU_DECL_JS_WRAP(BindGroup)

 private:
  BindGroup() = delete;
  virtual ~BindGroup();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_BindGroup_H_
