/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "ProfilerBacktrace.h"

#  include "ProfileBuffer.h"
#  include "ProfiledThreadData.h"
#  include "BaseProfileJSONWriter.h"
#  include "ThreadInfo.h"

namespace mozilla {
namespace baseprofiler {

ProfilerBacktrace::ProfilerBacktrace(const char* aName, int aThreadId,
                                     UniquePtr<ProfileBuffer> aBuffer)
    : mName(strdup(aName)), mThreadId(aThreadId), mBuffer(std::move(aBuffer)) {}

ProfilerBacktrace::~ProfilerBacktrace() {}

void ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                                   const TimeStamp& aProcessStartTime,
                                   UniqueStacks& aUniqueStacks) {
  // Unlike ProfiledThreadData::StreamJSON, we don't need to call
  // ProfileBuffer::AddJITInfoForRange because mBuffer does not contain any
  // JitReturnAddr entries. For synchronous samples, JIT frames get expanded
  // at sample time.
  StreamSamplesAndMarkers(mName.get(), mThreadId, *mBuffer.get(), aWriter, "",
                          aProcessStartTime,
                          /* aRegisterTime */ TimeStamp(),
                          /* aUnregisterTime */ TimeStamp(),
                          /* aSinceTime */ 0, aUniqueStacks);
}

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // MOZ_BASE_PROFILER
