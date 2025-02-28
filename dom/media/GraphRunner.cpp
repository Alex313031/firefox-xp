/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "GraphRunner.h"

#include "GraphDriver.h"
#include "MediaTrackGraph.h"
#include "MediaTrackGraphImpl.h"
#include "mozilla/dom/WorkletThread.h"
#include "nsISupportsImpl.h"
#include "prthread.h"
#include "Tracing.h"

namespace mozilla {

GraphRunner::GraphRunner(MediaTrackGraphImpl* aGraph,
                         already_AddRefed<nsIThread> aThread)
    : Runnable("GraphRunner"),
      mMonitor("GraphRunner::mMonitor"),
      mGraph(aGraph),
      mStateEnd(0),
      mStillProcessing(true),
      mThreadState(ThreadState::Wait),
      mThread(aThread) {
  mThread->Dispatch(do_AddRef(this));
}

GraphRunner::~GraphRunner() {
  MOZ_ASSERT(mThreadState == ThreadState::Shutdown);
}

/* static */
already_AddRefed<GraphRunner> GraphRunner::Create(MediaTrackGraphImpl* aGraph) {
  nsCOMPtr<nsIThread> thread;
  if (NS_WARN_IF(NS_FAILED(
          NS_NewNamedThread("GraphRunner", getter_AddRefs(thread))))) {
    return nullptr;
  }
  nsCOMPtr<nsISupportsPriority> supportsPriority = do_QueryInterface(thread);
  MOZ_ASSERT(supportsPriority);
  MOZ_ALWAYS_SUCCEEDS(
      supportsPriority->SetPriority(nsISupportsPriority::PRIORITY_HIGHEST));

  return do_AddRef(new GraphRunner(aGraph, thread.forget()));
}

void GraphRunner::Shutdown() {
  {
    Monitor2AutoLock lock(mMonitor);
    MOZ_ASSERT(mThreadState == ThreadState::Wait);
    mThreadState = ThreadState::Shutdown;
    mMonitor.Signal();
  }
  mThread->Shutdown();
}

bool GraphRunner::OneIteration(GraphTime aStateEnd) {
  TRACE_AUDIO_CALLBACK();

  Monitor2AutoLock lock(mMonitor);
  MOZ_ASSERT(mThreadState == ThreadState::Wait);
  mStateEnd = aStateEnd;

#ifdef DEBUG
  if (auto audioDriver = mGraph->CurrentDriver()->AsAudioCallbackDriver()) {
    mAudioDriverThreadId = audioDriver->ThreadId();
  } else if (auto clockDriver =
                 mGraph->CurrentDriver()->AsSystemClockDriver()) {
    mClockDriverThread = clockDriver->Thread();
  } else {
    MOZ_CRASH("Unknown GraphDriver");
  }
#endif
  // Signal that mStateEnd was updated
  mThreadState = ThreadState::Run;
  mMonitor.Signal();
  // Wait for mStillProcessing to update
  do {
    mMonitor.Wait();
  } while (mThreadState == ThreadState::Run);

#ifdef DEBUG
  mAudioDriverThreadId = std::thread::id();
  mClockDriverThread = nullptr;
#endif

  return mStillProcessing;
}

NS_IMETHODIMP GraphRunner::Run() {
  Monitor2AutoLock lock(mMonitor);
  while (true) {
    while (mThreadState == ThreadState::Wait) {
      mMonitor.Wait();  // Wait for mStateEnd to update or for shutdown
    }
    if (mThreadState == ThreadState::Shutdown) {
      break;
    }
    TRACE();
    mStillProcessing = mGraph->OneIterationImpl(mStateEnd);
    // Signal that mStillProcessing was updated
    mThreadState = ThreadState::Wait;
    mMonitor.Signal();
  }

  dom::WorkletThread::DeleteCycleCollectedJSContext();

  return NS_OK;
}

bool GraphRunner::OnThread() {
  return mThread->EventTarget()->IsOnCurrentThread();
}

#ifdef DEBUG
bool GraphRunner::RunByGraphDriver(GraphDriver* aDriver) {
  if (!OnThread()) {
    return false;
  }

  if (auto audioDriver = aDriver->AsAudioCallbackDriver()) {
    return audioDriver->ThreadId() == mAudioDriverThreadId;
  }

  if (auto clockDriver = aDriver->AsSystemClockDriver()) {
    return clockDriver->Thread() == mClockDriverThread;
  }

  MOZ_CRASH("Unknown driver");
}
#endif

}  // namespace mozilla
