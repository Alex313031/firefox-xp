/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceVolatileData.h"

#include "gfxAlphaRecovery.h"
#include "mozilla/Likely.h"
#include "mozilla/Types.h"  // for decltype

namespace mozilla {
namespace gfx {

bool SourceSurfaceVolatileData::Init(const IntSize& aSize, int32_t aStride,
                                     SurfaceFormat aFormat) {
  mSize = aSize;
  mStride = aStride;
  mFormat = aFormat;

  size_t alignment = size_t(1) << gfxAlphaRecovery::GoodAlignmentLog2();
  mVBuf = new VolatileBuffer();
  if (MOZ_UNLIKELY(!mVBuf->Init(aStride * aSize.height, alignment))) {
    mVBuf = nullptr;
    return false;
  }

  return true;
}

void SourceSurfaceVolatileData::GuaranteePersistance() {
  MOZ_ASSERT_UNREACHABLE("Should use SourceSurfaceRawData wrapper!");
}

void SourceSurfaceVolatileData::AddSizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf, size_t& aHeapSizeOut, size_t& aNonHeapSizeOut,
    size_t& aExtHandlesOut
#ifdef MOZ_BUILD_WEBRENDER
    ,
    uint64_t& aExtIdOut
#endif
) const {
  if (mVBuf) {
    aHeapSizeOut += mVBuf->HeapSizeOfExcludingThis(aMallocSizeOf);
    aNonHeapSizeOut += mVBuf->NonHeapSizeOfExcludingThis();
#ifdef ANDROID
    if (!mVBuf->OnHeap()) {
      // Volatile buffers keep a file handle open on Android.
      ++aExtHandlesOut;
    }
#endif
  }
}

}  // namespace gfx
}  // namespace mozilla
