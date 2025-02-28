/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "BaseProfilingStack.h"

#  include "mozilla/IntegerRange.h"
#  include "mozilla/UniquePtr.h"
#  include "mozilla/UniquePtrExtensions.h"

#  include <algorithm>

namespace mozilla {
namespace baseprofiler {

ProfilingStack::~ProfilingStack() {
  // The label macros keep a reference to the ProfilingStack to avoid a TLS
  // access. If these are somehow not all cleared we will get a
  // use-after-free so better to crash now.
  MOZ_RELEASE_ASSERT(stackPointer == 0);

  delete[] frames;
}

void ProfilingStack::ensureCapacitySlow() {
  MOZ_ASSERT(stackPointer >= capacity);
  const uint32_t kInitialCapacity = 128;

  uint32_t sp = stackPointer;
  auto newCapacity =
      std::max(sp + 1, capacity ? capacity * 2 : kInitialCapacity);

  auto* newFrames = new ProfilingStackFrame[newCapacity];

  // It's important that `frames` / `capacity` / `stackPointer` remain
  // consistent here at all times.
  for (auto i : IntegerRange(capacity)) {
    newFrames[i] = frames[i];
  }

  ProfilingStackFrame* oldFrames = frames;
  frames = newFrames;
  capacity = newCapacity;
  delete[] oldFrames;
}

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // MOZ_BASE_PROFILER
