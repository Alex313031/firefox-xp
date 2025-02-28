/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <MediaTrackGraphImpl.h>
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/AudioDeviceInfo.h"
#include "mozilla/dom/WorkletThread.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "mozilla/MathAlgorithms.h"
#include "CubebDeviceEnumerator.h"
#include "Tracing.h"

#ifdef MOZ_WEBRTC
#  include "webrtc/MediaEngineWebRTC.h"
#endif

#ifdef XP_MACOSX
#  include <sys/sysctl.h>
#endif

extern mozilla::LazyLogModule gMediaTrackGraphLog;
#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(type, msg) MOZ_LOG(gMediaTrackGraphLog, type, msg)

namespace mozilla {

GraphDriver::GraphDriver(MediaTrackGraphImpl* aGraphImpl)
    : mIterationStart(0),
      mIterationEnd(0),
      mGraphImpl(aGraphImpl),
      mPreviousDriver(nullptr),
      mNextDriver(nullptr) {}

void GraphDriver::SetGraphTime(GraphDriver* aPreviousDriver,
                               GraphTime aLastSwitchNextIterationStart,
                               GraphTime aLastSwitchNextIterationEnd) {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  // We set mIterationEnd here, because the first thing a driver do when it
  // does an iteration is to update graph times, so we are in fact setting
  // mIterationStart of the next iteration by setting the end of the previous
  // iteration.
  mIterationStart = aLastSwitchNextIterationStart;
  mIterationEnd = aLastSwitchNextIterationEnd;

  MOZ_ASSERT(!PreviousDriver());
  MOZ_ASSERT(aPreviousDriver);
  MOZ_DIAGNOSTIC_ASSERT(GraphImpl()->CurrentDriver() == aPreviousDriver);

  LOG(LogLevel::Debug,
      ("%p: Setting previous driver: %p (%s)", GraphImpl(), aPreviousDriver,
       aPreviousDriver->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                                : "SystemClockDriver"));

  SetPreviousDriver(aPreviousDriver);
}

void GraphDriver::SwitchAtNextIteration(GraphDriver* aNextDriver) {
  MOZ_ASSERT(OnGraphThread());
  MOZ_ASSERT(aNextDriver);
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();

  LOG(LogLevel::Debug,
      ("%p: Switching to new driver: %p (%s)", GraphImpl(), aNextDriver,
       aNextDriver->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                            : "SystemClockDriver"));
  if (mNextDriver && mNextDriver != GraphImpl()->CurrentDriver()) {
    LOG(LogLevel::Debug,
        ("%p: Discarding previous next driver: %p (%s)", GraphImpl(),
         mNextDriver.get(),
         mNextDriver->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                              : "SystemClockDriver"));
  }
  SetNextDriver(aNextDriver);
}

GraphTime GraphDriver::StateComputedTime() const {
  return GraphImpl()->mStateComputedTime;
}

void GraphDriver::EnsureNextIteration() { GraphImpl()->EnsureNextIteration(); }

#ifdef DEBUG
bool GraphDriver::OnGraphThread() {
  return GraphImpl()->RunByGraphDriver(this);
}
#endif

bool GraphDriver::Switching() {
  MOZ_ASSERT(OnGraphThread());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  return mNextDriver || mPreviousDriver;
}

void GraphDriver::SwitchToNextDriver() {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  MOZ_ASSERT(NextDriver());

  NextDriver()->SetGraphTime(this, mIterationStart, mIterationEnd);
  GraphImpl()->SetCurrentDriver(NextDriver());
  NextDriver()->Start();
  SetNextDriver(nullptr);
}

GraphDriver* GraphDriver::NextDriver() {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  return mNextDriver;
}

GraphDriver* GraphDriver::PreviousDriver() {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  return mPreviousDriver;
}

void GraphDriver::SetNextDriver(GraphDriver* aNextDriver) {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  MOZ_ASSERT(aNextDriver != this);
  MOZ_ASSERT(aNextDriver != mNextDriver);

  if (mNextDriver && mNextDriver != GraphImpl()->CurrentDriver()) {
    LOG(LogLevel::Debug,
        ("Discarding previous next driver: %p (%s)", mNextDriver.get(),
         mNextDriver->AsAudioCallbackDriver() ? "AudioCallbackDriver"
                                              : "SystemClockDriver"));
  }

  mNextDriver = aNextDriver;
}

void GraphDriver::SetPreviousDriver(GraphDriver* aPreviousDriver) {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  mPreviousDriver = aPreviousDriver;
}

ThreadedDriver::ThreadedDriver(MediaTrackGraphImpl* aGraphImpl)
    : GraphDriver(aGraphImpl), mThreadRunning(false) {}

class MediaTrackGraphShutdownThreadRunnable : public Runnable {
 public:
  explicit MediaTrackGraphShutdownThreadRunnable(
      already_AddRefed<nsIThread> aThread)
      : Runnable("MediaTrackGraphShutdownThreadRunnable"), mThread(aThread) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mThread);

    mThread->Shutdown();
    mThread = nullptr;
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIThread> mThread;
};

ThreadedDriver::~ThreadedDriver() {
  if (mThread) {
    nsCOMPtr<nsIRunnable> event =
        new MediaTrackGraphShutdownThreadRunnable(mThread.forget());
    SystemGroup::Dispatch(TaskCategory::Other, event.forget());
  }
}

class MediaTrackGraphInitThreadRunnable : public Runnable {
 public:
  explicit MediaTrackGraphInitThreadRunnable(ThreadedDriver* aDriver)
      : Runnable("MediaTrackGraphInitThreadRunnable"), mDriver(aDriver) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(!mDriver->ThreadRunning());
    LOG(LogLevel::Debug, ("Starting a new system driver for graph %p",
                          mDriver->mGraphImpl.get()));

    RefPtr<GraphDriver> previousDriver;
    {
      Monitor2AutoLock mon(mDriver->mGraphImpl->GetMonitor());
      previousDriver = mDriver->PreviousDriver();
    }
    if (previousDriver) {
      LOG(LogLevel::Debug,
          ("%p releasing an AudioCallbackDriver(%p), for graph %p",
           mDriver.get(), previousDriver.get(), mDriver->GraphImpl()));
      MOZ_ASSERT(!mDriver->AsAudioCallbackDriver());
      RefPtr<AsyncCubebTask> releaseEvent =
          new AsyncCubebTask(previousDriver->AsAudioCallbackDriver(),
                             AsyncCubebOperation::SHUTDOWN);
      releaseEvent->Dispatch();

      Monitor2AutoLock mon(mDriver->mGraphImpl->GetMonitor());
      mDriver->SetPreviousDriver(nullptr);
    } else {
      Monitor2AutoLock mon(mDriver->mGraphImpl->GetMonitor());
      MOZ_ASSERT(mDriver->mGraphImpl->MessagesQueued(),
                 "Don't start a graph without messages queued.");
      mDriver->mGraphImpl->SwapMessageQueues();
    }

    mDriver->RunThread();
    return NS_OK;
  }

 private:
  RefPtr<ThreadedDriver> mDriver;
};

void ThreadedDriver::Start() {
  MOZ_ASSERT(!ThreadRunning());
  LOG(LogLevel::Debug,
      ("Starting thread for a SystemClockDriver  %p", mGraphImpl.get()));
  Unused << NS_WARN_IF(mThread);
  MOZ_ASSERT(!mThread);  // Ensure we haven't already started it

  nsCOMPtr<nsIRunnable> event = new MediaTrackGraphInitThreadRunnable(this);
  // Note: mThread may be null during event->Run() if we pass to NewNamedThread!
  // See AudioInitTask
  nsresult rv = NS_NewNamedThread("MediaTrackGrph", getter_AddRefs(mThread));
  if (NS_SUCCEEDED(rv)) {
    mThread->EventTarget()->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
  }
}

void ThreadedDriver::Shutdown() {
  NS_ASSERTION(NS_IsMainThread(), "Must be called on main thread");
  // mGraph's thread is not running so it's OK to do whatever here
  LOG(LogLevel::Debug, ("Stopping threads for MediaTrackGraph %p", this));

  if (mThread) {
    LOG(LogLevel::Debug,
        ("%p: Stopping ThreadedDriver's %p thread", GraphImpl(), this));
    mThread->Shutdown();
    mThread = nullptr;
  }
}

SystemClockDriver::SystemClockDriver(MediaTrackGraphImpl* aGraphImpl)
    : ThreadedDriver(aGraphImpl),
      mInitialTimeStamp(TimeStamp::Now()),
      mCurrentTimeStamp(TimeStamp::Now()),
      mLastTimeStamp(TimeStamp::Now()),
      mIsFallback(false) {}

SystemClockDriver::~SystemClockDriver() {}

void SystemClockDriver::MarkAsFallback() { mIsFallback = true; }

bool SystemClockDriver::IsFallback() { return mIsFallback; }

void ThreadedDriver::RunThread() {
  mThreadRunning = true;
  while (true) {
    mIterationStart = IterationEnd();
    mIterationEnd += GetIntervalForIteration();

    GraphTime stateComputedTime = StateComputedTime();
    if (stateComputedTime < mIterationEnd) {
      LOG(LogLevel::Warning, ("%p: Global underrun detected", GraphImpl()));
      mIterationEnd = stateComputedTime;
    }

    if (mIterationStart >= mIterationEnd) {
      NS_ASSERTION(mIterationStart == mIterationEnd,
                   "Time can't go backwards!");
      // This could happen due to low clock resolution, maybe?
      LOG(LogLevel::Debug, ("%p: Time did not advance", GraphImpl()));
    }

    GraphTime nextStateComputedTime = GraphImpl()->RoundUpToEndOfAudioBlock(
        mIterationEnd + GraphImpl()->MillisecondsToMediaTime(AUDIO_TARGET_MS));
    if (nextStateComputedTime < stateComputedTime) {
      // A previous driver may have been processing further ahead of
      // iterationEnd.
      LOG(LogLevel::Warning,
          ("%p: Prevent state from going backwards. interval[%ld; %ld] "
           "state[%ld; "
           "%ld]",
           GraphImpl(), (long)mIterationStart, (long)mIterationEnd,
           (long)stateComputedTime, (long)nextStateComputedTime));
      nextStateComputedTime = stateComputedTime;
    }
    LOG(LogLevel::Verbose,
        ("%p: interval[%ld; %ld] state[%ld; %ld]", GraphImpl(),
         (long)mIterationStart, (long)mIterationEnd, (long)stateComputedTime,
         (long)nextStateComputedTime));

    bool stillProcessing = GraphImpl()->OneIteration(nextStateComputedTime);

    if (!stillProcessing) {
      // Enter shutdown mode. The stable-state handler will detect this
      // and complete shutdown if the graph does not get restarted.
      dom::WorkletThread::DeleteCycleCollectedJSContext();
      GraphImpl()->SignalMainThreadCleanup();
      break;
    }
    Monitor2AutoLock lock(GraphImpl()->GetMonitor());
    if (NextDriver()) {
      LOG(LogLevel::Debug,
          ("%p: Switching to AudioCallbackDriver", GraphImpl()));
      SwitchToNextDriver();
      break;
    }
  }
  mThreadRunning = false;
}

MediaTime SystemClockDriver::GetIntervalForIteration() {
  TimeStamp now = TimeStamp::Now();
  MediaTime interval =
      GraphImpl()->SecondsToMediaTime((now - mCurrentTimeStamp).ToSeconds());
  mCurrentTimeStamp = now;

  MOZ_LOG(
      gMediaTrackGraphLog, LogLevel::Verbose,
      ("%p: Updating current time to %f (real %f, StateComputedTime() %f)",
       GraphImpl(), GraphImpl()->MediaTimeToSeconds(IterationEnd() + interval),
       (now - mInitialTimeStamp).ToSeconds(),
       GraphImpl()->MediaTimeToSeconds(StateComputedTime())));

  return interval;
}

void ThreadedDriver::WaitForNextIteration() {
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();

  TimeDuration timeout = TimeDuration::Forever();

  // This lets us avoid hitting the Atomic twice when we know we won't sleep
  bool another = GraphImpl()->mNeedAnotherIteration;  // atomic
  if (!another) {
    GraphImpl()->mGraphDriverAsleep = true;  // atomic
  }
  // NOTE: mNeedAnotherIteration while also atomic may have changed before
  // we could set mGraphDriverAsleep, so we must re-test it.
  // (EnsureNextIteration sets mNeedAnotherIteration, then tests
  // mGraphDriverAsleep
  if (another || GraphImpl()->mNeedAnotherIteration) {  // atomic
    timeout = WaitInterval();
    if (!another) {
      GraphImpl()->mGraphDriverAsleep = false;  // atomic
      another = true;
    }
  }
  if (!timeout.IsZero()) {
    CVStatus2 status = GraphImpl()->GetMonitor().Wait(timeout);
    LOG(LogLevel::Verbose,
        ("%p: Resuming after %s", GraphImpl(),
         status == CVStatus2::Timeout ? "timeout" : "wake-up"));
  }

  if (!another) {
    GraphImpl()->mGraphDriverAsleep = false;  // atomic
  }
  GraphImpl()->mNeedAnotherIteration = false;  // atomic
}

void ThreadedDriver::WakeUp() {
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  GraphImpl()->mGraphDriverAsleep = false;  // atomic
  GraphImpl()->GetMonitor().Signal();
}

TimeDuration SystemClockDriver::WaitInterval() {
  TimeStamp now = TimeStamp::Now();
  int64_t timeoutMS = MEDIA_GRAPH_TARGET_PERIOD_MS -
                      int64_t((now - mCurrentTimeStamp).ToMilliseconds());
  // Make sure timeoutMS doesn't overflow 32 bits by waking up at
  // least once a minute, if we need to wake up at all
  timeoutMS = std::max<int64_t>(0, std::min<int64_t>(timeoutMS, 60 * 1000));
  LOG(LogLevel::Verbose,
      ("%p: Waiting for next iteration; at %f, timeout=%f", GraphImpl(),
       (now - mInitialTimeStamp).ToSeconds(), timeoutMS / 1000.0));

  return TimeDuration::FromMilliseconds(timeoutMS);
}

OfflineClockDriver::OfflineClockDriver(MediaTrackGraphImpl* aGraphImpl,
                                       GraphTime aSlice)
    : ThreadedDriver(aGraphImpl), mSlice(aSlice) {}

OfflineClockDriver::~OfflineClockDriver() {}

MediaTime OfflineClockDriver::GetIntervalForIteration() {
  return GraphImpl()->MillisecondsToMediaTime(mSlice);
}

TimeDuration OfflineClockDriver::WaitInterval() {
  // We want to go as fast as possible when we are offline
  return 0;
}

AsyncCubebTask::AsyncCubebTask(AudioCallbackDriver* aDriver,
                               AsyncCubebOperation aOperation)
    : Runnable("AsyncCubebTask"),
      mDriver(aDriver),
      mOperation(aOperation),
      mShutdownGrip(aDriver->GraphImpl()) {
  NS_WARNING_ASSERTION(
      mDriver->mAudioStream || aOperation == AsyncCubebOperation::INIT,
      "No audio stream!");
}

AsyncCubebTask::~AsyncCubebTask() {}

NS_IMETHODIMP
AsyncCubebTask::Run() {
  MOZ_ASSERT(mDriver);

  switch (mOperation) {
    case AsyncCubebOperation::INIT: {
      LOG(LogLevel::Debug, ("%p: AsyncCubebOperation::INIT driver=%p",
                            mDriver->GraphImpl(), mDriver.get()));
      if (!mDriver->Init()) {
        LOG(LogLevel::Warning,
            ("AsyncCubebOperation::INIT failed for driver=%p", mDriver.get()));
        return NS_ERROR_FAILURE;
      }
      mDriver->CompleteAudioContextOperations(mOperation);
      break;
    }
    case AsyncCubebOperation::SHUTDOWN: {
      LOG(LogLevel::Debug, ("%p: AsyncCubebOperation::SHUTDOWN driver=%p",
                            mDriver->GraphImpl(), mDriver.get()));
      mDriver->Stop();

      mDriver->CompleteAudioContextOperations(mOperation);

      mDriver = nullptr;
      mShutdownGrip = nullptr;
      break;
    }
    default:
      MOZ_CRASH("Operation not implemented.");
  }

  // The thread will kill itself after a bit
  return NS_OK;
}

TrackAndPromiseForOperation::TrackAndPromiseForOperation(
    MediaTrack* aTrack, void* aPromise, dom::AudioContextOperation aOperation,
    dom::AudioContextOperationFlags aFlags)
    : mTrack(aTrack),
      mPromise(aPromise),
      mOperation(aOperation),
      mFlags(aFlags) {}

AudioCallbackDriver::AudioCallbackDriver(MediaTrackGraphImpl* aGraphImpl,
                                         uint32_t aOutputChannelCount,
                                         uint32_t aInputChannelCount,
                                         AudioInputType aAudioInputType)
    : GraphDriver(aGraphImpl),
      mOutputChannels(aOutputChannelCount),
      mSampleRate(0),
      mInputChannelCount(aInputChannelCount),
      mIterationDurationMS(MEDIA_GRAPH_TARGET_PERIOD_MS),
      mStarted(false),
      mInitShutdownThread(
          SharedThreadPool::Get(NS_LITERAL_CSTRING("CubebOperation"), 1)),
      mAddedMixer(false),
      mAudioThreadId(std::thread::id()),
      mAudioThreadRunning(false),
      mShouldFallbackIfError(false),
      mFromFallback(false) {
  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver ctor", GraphImpl()));

  const uint32_t kIdleThreadTimeoutMs = 2000;
  mInitShutdownThread->SetIdleThreadTimeout(
      PR_MillisecondsToInterval(kIdleThreadTimeoutMs));

#if defined(XP_WIN)
  if (XRE_IsContentProcess()) {
    audio::AudioNotificationReceiver::Register(this);
  }
#endif
  if (aAudioInputType == AudioInputType::Voice) {
    LOG(LogLevel::Debug, ("VOICE."));
    mInputDevicePreference = CUBEB_DEVICE_PREF_VOICE;
    CubebUtils::SetInCommunication(true);
  } else {
    mInputDevicePreference = CUBEB_DEVICE_PREF_ALL;
  }
}

AudioCallbackDriver::~AudioCallbackDriver() {
  MOZ_ASSERT(mPromisesForOperation.IsEmpty());
  MOZ_ASSERT(!mAddedMixer);
#if defined(XP_WIN)
  if (XRE_IsContentProcess()) {
    audio::AudioNotificationReceiver::Unregister(this);
  }
#endif
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    CubebUtils::SetInCommunication(false);
  }
}

bool IsMacbookOrMacbookAir() {
#ifdef XP_MACOSX
  size_t len = 0;
  sysctlbyname("hw.model", NULL, &len, NULL, 0);
  if (len) {
    UniquePtr<char[]> model(new char[len]);
    // This string can be
    // MacBook%d,%d for a normal MacBook
    // MacBookPro%d,%d for a MacBook Pro
    // MacBookAir%d,%d for a Macbook Air
    sysctlbyname("hw.model", model.get(), &len, NULL, 0);
    char* substring = strstr(model.get(), "MacBook");
    if (substring) {
      const size_t offset = strlen("MacBook");
      if (!strncmp(model.get() + offset, "Air", 3) ||
          isdigit(model[offset + 1])) {
        return true;
      }
    }
  }
#endif
  return false;
}

bool AudioCallbackDriver::Init() {
  cubeb* cubebContext = CubebUtils::GetCubebContext();
  if (!cubebContext) {
    NS_WARNING("Could not get cubeb context.");
    LOG(LogLevel::Warning, ("%s: Could not get cubeb context", __func__));
    if (!mFromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(true);
    }
    Monitor2AutoLock lock(GraphImpl()->GetMonitor());
    FallbackToSystemClockDriver();
    return true;
  }

  cubeb_stream_params output;
  cubeb_stream_params input;
  bool firstStream = CubebUtils::GetFirstStream();

  MOZ_ASSERT(!NS_IsMainThread(),
             "This is blocking and should never run on the main thread.");

  mSampleRate = output.rate = mGraphImpl->GraphRate();

  if (AUDIO_OUTPUT_FORMAT == AUDIO_FORMAT_S16) {
    output.format = CUBEB_SAMPLE_S16NE;
  } else {
    output.format = CUBEB_SAMPLE_FLOAT32NE;
  }

  if (!mOutputChannels) {
    LOG(LogLevel::Warning, ("Output number of channels is 0."));
    Monitor2AutoLock lock(GraphImpl()->GetMonitor());
    FallbackToSystemClockDriver();
    return true;
  }

  CubebUtils::AudioDeviceID forcedOutputDeviceId = nullptr;

  char* forcedOutputDeviceName = CubebUtils::GetForcedOutputDevice();
  if (forcedOutputDeviceName) {
    RefPtr<CubebDeviceEnumerator> enumerator = Enumerator::GetInstance();
    RefPtr<AudioDeviceInfo> device = enumerator->DeviceInfoFromName(
        NS_ConvertUTF8toUTF16(forcedOutputDeviceName), EnumeratorSide::OUTPUT);
    if (device && device->DeviceID()) {
      forcedOutputDeviceId = device->DeviceID();
    }
  }

  mBuffer = AudioCallbackBufferWrapper<AudioDataValue>(mOutputChannels);
  mScratchBuffer =
      SpillBuffer<AudioDataValue, WEBAUDIO_BLOCK_SIZE * 2>(mOutputChannels);

  output.channels = mOutputChannels;
  AudioConfig::ChannelLayout::ChannelMap channelMap =
      AudioConfig::ChannelLayout(mOutputChannels).Map();

  output.layout = static_cast<uint32_t>(channelMap);
  output.prefs = CubebUtils::GetDefaultStreamPrefs();
#if !defined(XP_WIN)
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    output.prefs |= static_cast<cubeb_stream_prefs>(CUBEB_STREAM_PREF_VOICE);
  }
#endif

  uint32_t latencyFrames = CubebUtils::GetCubebMTGLatencyInFrames(&output);

  // Macbook and MacBook air don't have enough CPU to run very low latency
  // MediaTrackGraphs, cap the minimal latency to 512 frames int this case.
  if (IsMacbookOrMacbookAir()) {
    latencyFrames = std::max((uint32_t)512, latencyFrames);
  }

  // On OSX, having a latency that is lower than 10ms is very common. It's
  // not very useful when doing voice, because all the WebRTC code deal in 10ms
  // chunks of audio.  Take the first power of two above 10ms at the current
  // rate in this case. It's probably 512, for common rates.
#if defined(XP_MACOSX)
  if (mInputDevicePreference == CUBEB_DEVICE_PREF_VOICE) {
    if (latencyFrames < mSampleRate / 100) {
      latencyFrames = mozilla::RoundUpPow2(mSampleRate / 100);
    }
  }
#endif
  LOG(LogLevel::Debug, ("Effective latency in frames: %d", latencyFrames));

  input = output;
  input.channels = mInputChannelCount;
  input.layout = CUBEB_LAYOUT_UNDEFINED;

  cubeb_stream* stream = nullptr;
  bool inputWanted = mInputChannelCount > 0;
  CubebUtils::AudioDeviceID outputId = GraphImpl()->mOutputDeviceID;
  CubebUtils::AudioDeviceID inputId = GraphImpl()->mInputDeviceID;

  // XXX Only pass input input if we have an input listener.  Always
  // set up output because it's easier, and it will just get silence.
  if (cubeb_stream_init(cubebContext, &stream, "AudioCallbackDriver", inputId,
                        inputWanted ? &input : nullptr,
                        forcedOutputDeviceId ? forcedOutputDeviceId : outputId,
                        &output, latencyFrames, DataCallback_s, StateCallback_s,
                        this) == CUBEB_OK) {
    mAudioStream.own(stream);
    DebugOnly<int> rv =
        cubeb_stream_set_volume(mAudioStream, CubebUtils::GetVolumeScale());
    NS_WARNING_ASSERTION(
        rv == CUBEB_OK,
        "Could not set the audio stream volume in GraphDriver.cpp");
    CubebUtils::ReportCubebBackendUsed();
  } else {
    NS_WARNING(
        "Could not create a cubeb stream for MediaTrackGraph, falling "
        "back to a SystemClockDriver");
    // Only report failures when we're not coming from a driver that was
    // created itself as a fallback driver because of a previous audio driver
    // failure.
    if (!mFromFallback) {
      CubebUtils::ReportCubebStreamInitFailure(firstStream);
    }
    Monitor2AutoLock lock(GraphImpl()->GetMonitor());
    FallbackToSystemClockDriver();
    return true;
  }

#ifdef XP_MACOSX
  PanOutputIfNeeded(inputWanted);
#endif

  cubeb_stream_register_device_changed_callback(
      mAudioStream, AudioCallbackDriver::DeviceChangedCallback_s);

  if (!StartStream()) {
    LOG(LogLevel::Warning,
        ("%p: AudioCallbackDriver couldn't start a cubeb stream.",
         GraphImpl()));
    return false;
  }

  LOG(LogLevel::Debug, ("%p: AudioCallbackDriver started.", GraphImpl()));
  return true;
}

void AudioCallbackDriver::Start() {
  MOZ_ASSERT(!IsStarted());
  MOZ_ASSERT(NS_IsMainThread() || OnCubebOperationThread() ||
             (PreviousDriver() && PreviousDriver()->OnGraphThread()));
  if (mPreviousDriver) {
    if (mPreviousDriver->AsAudioCallbackDriver()) {
      LOG(LogLevel::Debug, ("Releasing audio driver off main thread."));
      RefPtr<AsyncCubebTask> releaseEvent =
          new AsyncCubebTask(mPreviousDriver->AsAudioCallbackDriver(),
                             AsyncCubebOperation::SHUTDOWN);
      releaseEvent->Dispatch();
      mPreviousDriver = nullptr;
    } else {
      LOG(LogLevel::Debug,
          ("Dropping driver reference for SystemClockDriver."));
      MOZ_ASSERT(mPreviousDriver->AsSystemClockDriver());
      mFromFallback = mPreviousDriver->AsSystemClockDriver()->IsFallback();
      mPreviousDriver = nullptr;
    }
  }

  LOG(LogLevel::Debug, ("Starting new audio driver off main thread, "
                        "to ensure it runs after previous shutdown."));
  RefPtr<AsyncCubebTask> initEvent =
      new AsyncCubebTask(AsAudioCallbackDriver(), AsyncCubebOperation::INIT);
  initEvent->Dispatch();
}

bool AudioCallbackDriver::StartStream() {
  MOZ_ASSERT(!IsStarted() && OnCubebOperationThread());
  mShouldFallbackIfError = true;
  if (cubeb_stream_start(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not start cubeb stream for MTG.");
    return false;
  }

  mStarted = true;
  return true;
}

void AudioCallbackDriver::Stop() {
  MOZ_ASSERT(OnCubebOperationThread());
  if (cubeb_stream_stop(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not stop cubeb stream for MTG.");
  }
  mStarted = false;
}

void AudioCallbackDriver::RemoveMixerCallback() {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());

  if (mAddedMixer) {
    GraphImpl()->mMixer.RemoveCallback(this);
    mAddedMixer = false;
  }
}

void AudioCallbackDriver::AddMixerCallback() {
  MOZ_ASSERT(OnGraphThread());

  if (!mAddedMixer) {
    mGraphImpl->mMixer.AddCallback(WrapNotNull(this));
    mAddedMixer = true;
  }
}

void AudioCallbackDriver::WaitForNextIteration() {
  // Do not block.
}

void AudioCallbackDriver::WakeUp() {}

void AudioCallbackDriver::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Debug,
      ("%p: Releasing audio driver off main thread (GraphDriver::Shutdown).",
       GraphImpl()));
  RefPtr<AsyncCubebTask> releaseEvent =
      new AsyncCubebTask(this, AsyncCubebOperation::SHUTDOWN);
  releaseEvent->Dispatch(NS_DISPATCH_SYNC);
}

#if defined(XP_WIN)
void AudioCallbackDriver::ResetDefaultDevice() {
  if (cubeb_stream_reset_default_device(mAudioStream) != CUBEB_OK) {
    NS_WARNING("Could not reset cubeb stream to default output device.");
  }
}
#endif

/* static */
long AudioCallbackDriver::DataCallback_s(cubeb_stream* aStream, void* aUser,
                                         const void* aInputBuffer,
                                         void* aOutputBuffer, long aFrames) {
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  return driver->DataCallback(static_cast<const AudioDataValue*>(aInputBuffer),
                              static_cast<AudioDataValue*>(aOutputBuffer),
                              aFrames);
}

/* static */
void AudioCallbackDriver::StateCallback_s(cubeb_stream* aStream, void* aUser,
                                          cubeb_state aState) {
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  driver->StateCallback(aState);
}

/* static */
void AudioCallbackDriver::DeviceChangedCallback_s(void* aUser) {
  AudioCallbackDriver* driver = reinterpret_cast<AudioCallbackDriver*>(aUser);
  driver->DeviceChangedCallback();
}

AudioCallbackDriver::AutoInCallback::AutoInCallback(
    AudioCallbackDriver* aDriver)
    : mDriver(aDriver) {
  mDriver->mAudioThreadId = std::this_thread::get_id();
}

AudioCallbackDriver::AutoInCallback::~AutoInCallback() {
  mDriver->mAudioThreadId = std::thread::id();
}

long AudioCallbackDriver::DataCallback(const AudioDataValue* aInputBuffer,
                                       AudioDataValue* aOutputBuffer,
                                       long aFrames) {
  TRACE_AUDIO_CALLBACK_BUDGET(aFrames, mSampleRate);
  TRACE_AUDIO_CALLBACK();

#ifdef DEBUG
  AutoInCallback aic(this);
#endif

  // Don't add the callback until we're inited and ready
  if (!mAddedMixer) {
    GraphImpl()->mMixer.AddCallback(WrapNotNull(this));
    mAddedMixer = true;
  }

  GraphTime stateComputedTime = StateComputedTime();
  if (stateComputedTime == 0) {
    Monitor2AutoLock mon(GraphImpl()->GetMonitor());
    // Because this function is called during cubeb_stream_init (to prefill the
    // audio buffers), it can be that we don't have a message here (because this
    // driver is the first one for this graph), and the graph would exit. Simply
    // return here until we have messages.
    if (!GraphImpl()->MessagesQueued()) {
      PodZero(aOutputBuffer, aFrames * mOutputChannels);
      return aFrames;
    }
    GraphImpl()->SwapMessageQueues();
  }

  uint32_t durationMS = aFrames * 1000 / mSampleRate;

  // For now, simply average the duration with the previous
  // duration so there is some damping against sudden changes.
  if (!mIterationDurationMS) {
    mIterationDurationMS = durationMS;
  } else {
    mIterationDurationMS = (mIterationDurationMS * 3) + durationMS;
    mIterationDurationMS /= 4;
  }

  mBuffer.SetBuffer(aOutputBuffer, aFrames);
  // fill part or all with leftover data from last iteration (since we
  // align to Audio blocks)
  mScratchBuffer.Empty(mBuffer);

  // State computed time is decided by the audio callback's buffer length. We
  // compute the iteration start and end from there, trying to keep the amount
  // of buffering in the graph constant.
  GraphTime nextStateComputedTime = GraphImpl()->RoundUpToEndOfAudioBlock(
      stateComputedTime + mBuffer.Available());

  mIterationStart = mIterationEnd;
  // inGraph is the number of audio frames there is between the state time and
  // the current time, i.e. the maximum theoretical length of the interval we
  // could use as [mIterationStart; mIterationEnd].
  GraphTime inGraph = stateComputedTime - mIterationStart;
  // We want the interval [mIterationStart; mIterationEnd] to be before the
  // interval [stateComputedTime; nextStateComputedTime]. We also want
  // the distance between these intervals to be roughly equivalent each time, to
  // ensure there is no clock drift between current time and state time. Since
  // we can't act on the state time because we have to fill the audio buffer, we
  // reclock the current time against the state time, here.
  mIterationEnd = mIterationStart + 0.8 * inGraph;

  LOG(LogLevel::Verbose,
      ("%p: interval[%ld; %ld] state[%ld; %ld] (frames: %ld) (durationMS: %u) "
       "(duration ticks: %ld)",
       GraphImpl(), (long)mIterationStart, (long)mIterationEnd,
       (long)stateComputedTime, (long)nextStateComputedTime, (long)aFrames,
       (uint32_t)durationMS,
       (long)(nextStateComputedTime - stateComputedTime)));

  if (stateComputedTime < mIterationEnd) {
    LOG(LogLevel::Error,
        ("%p: Media graph global underrun detected", GraphImpl()));
    MOZ_ASSERT_UNREACHABLE("We should not underrun in full duplex");
    mIterationEnd = stateComputedTime;
  }

  // Process mic data if any/needed
  if (aInputBuffer && mInputChannelCount > 0) {
    GraphImpl()->NotifyInputData(aInputBuffer, static_cast<size_t>(aFrames),
                                 mSampleRate, mInputChannelCount);
  }

  bool stillProcessing;
  if (mBuffer.Available()) {
    // We totally filled the buffer (and mScratchBuffer isn't empty).
    // We don't need to run an iteration and if we do so we may overflow.
    stillProcessing = GraphImpl()->OneIteration(nextStateComputedTime);
  } else {
    LOG(LogLevel::Verbose,
        ("%p: DataCallback buffer filled entirely from scratch "
         "buffer, skipping iteration.",
         GraphImpl()));
    stillProcessing = true;
  }

  mBuffer.BufferFilled();

  // Callback any observers for the AEC speaker data.  Note that one
  // (maybe) of these will be full-duplex, the others will get their input
  // data off separate cubeb callbacks.  Take care with how stuff is
  // removed/added to this list and TSAN issues, but input and output will
  // use separate callback methods.
  GraphImpl()->NotifyOutputData(aOutputBuffer, static_cast<size_t>(aFrames),
                                mSampleRate, mOutputChannels);

#ifdef XP_MACOSX
  // This only happens when the output is on a macbookpro's external speaker,
  // that are stereo, but let's just be safe.
  if (mNeedsPanning && mOutputChannels == 2) {
    // hard pan to the right
    for (uint32_t i = 0; i < aFrames * 2; i += 2) {
      aOutputBuffer[i + 1] += aOutputBuffer[i];
      aOutputBuffer[i] = 0.0;
    }
  }
#endif

  if (!stillProcessing) {
    // About to hand over control of the graph.  Do not start a new driver if
    // StateCallback() receives an error for this stream while the main thread
    // or another driver has control of the graph.
    mShouldFallbackIfError = false;
    RemoveMixerCallback();
    // Update the flag before handing over the graph and going to drain.
    mAudioThreadRunning = false;
    // Enter shutdown mode. The stable-state handler will detect this
    // and complete shutdown if the graph does not get restarted.
    mGraphImpl->SignalMainThreadCleanup();
    return aFrames - 1;
  }

  bool switching = false;
  {
    Monitor2AutoLock mon(GraphImpl()->GetMonitor());
    switching = !!NextDriver();
  }

  if (switching) {
    mShouldFallbackIfError = false;
    // If the audio stream has not been started by the previous driver or
    // the graph itself, keep it alive.
    Monitor2AutoLock mon(GraphImpl()->GetMonitor());
    if (!IsStarted()) {
      return aFrames;
    }
    LOG(LogLevel::Debug, ("%p: Switching to system driver.", GraphImpl()));
    RemoveMixerCallback();
    mAudioThreadRunning = false;
    SwitchToNextDriver();
    // Returning less than aFrames starts the draining and eventually stops the
    // audio thread. This function will never get called again.
    return aFrames - 1;
  }

  return aFrames;
}

static const char* StateToString(cubeb_state aState) {
  switch (aState) {
    case CUBEB_STATE_STARTED:
      return "STARTED";
    case CUBEB_STATE_STOPPED:
      return "STOPPED";
    case CUBEB_STATE_DRAINED:
      return "DRAINED";
    case CUBEB_STATE_ERROR:
      return "ERROR";
    default:
      MOZ_CRASH("Unexpected state!");
  }
}

void AudioCallbackDriver::StateCallback(cubeb_state aState) {
  MOZ_ASSERT(!OnGraphThread());
  LOG(LogLevel::Debug,
      ("AudioCallbackDriver State: %s", StateToString(aState)));

  // Clear the flag for the not running
  // states: stopped, drained, error.
  mAudioThreadRunning = (aState == CUBEB_STATE_STARTED);

  if (aState == CUBEB_STATE_ERROR && mShouldFallbackIfError) {
    MOZ_ASSERT(!ThreadRunning());
    mShouldFallbackIfError = false;
    Monitor2AutoLock lock(GraphImpl()->GetMonitor());
    RemoveMixerCallback();
    FallbackToSystemClockDriver();
  } else if (aState == CUBEB_STATE_STOPPED) {
    MOZ_ASSERT(!ThreadRunning());
    RemoveMixerCallback();
  }
}

void AudioCallbackDriver::MixerCallback(AudioDataValue* aMixedBuffer,
                                        AudioSampleFormat aFormat,
                                        uint32_t aChannels, uint32_t aFrames,
                                        uint32_t aSampleRate) {
  MOZ_ASSERT(OnGraphThread());
  uint32_t toWrite = mBuffer.Available();

  if (!mBuffer.Available()) {
    NS_WARNING("DataCallback buffer full, expect frame drops.");
  }

  MOZ_ASSERT(mBuffer.Available() <= aFrames);

  mBuffer.WriteFrames(aMixedBuffer, mBuffer.Available());
  MOZ_ASSERT(mBuffer.Available() == 0,
             "Missing frames to fill audio callback's buffer.");

  DebugOnly<uint32_t> written = mScratchBuffer.Fill(
      aMixedBuffer + toWrite * aChannels, aFrames - toWrite);
  NS_WARNING_ASSERTION(written == aFrames - toWrite, "Dropping frames.");
};

void AudioCallbackDriver::PanOutputIfNeeded(bool aMicrophoneActive) {
#ifdef XP_MACOSX
  cubeb_device* out = nullptr;
  int rv;
  char name[128];
  size_t length = sizeof(name);

  rv = sysctlbyname("hw.model", name, &length, NULL, 0);
  if (rv) {
    return;
  }

  if (!strncmp(name, "MacBookPro", 10)) {
    if (cubeb_stream_get_current_device(mAudioStream, &out) == CUBEB_OK) {
      MOZ_ASSERT(out);
      // Check if we are currently outputing sound on external speakers.
      if (out->output_name && !strcmp(out->output_name, "ispk")) {
        // Pan everything to the right speaker.
        LOG(LogLevel::Debug, ("Using the built-in speakers, with%s audio input",
                              aMicrophoneActive ? "" : "out"));
        mNeedsPanning = aMicrophoneActive;
      } else {
        LOG(LogLevel::Debug, ("Using an external output device"));
        mNeedsPanning = false;
      }
      cubeb_stream_device_destroy(mAudioStream, out);
    }
  }
#endif
}

void AudioCallbackDriver::DeviceChangedCallback() {
  MOZ_ASSERT(!OnGraphThread());
  // Tell the audio engine the device has changed, it might want to reset some
  // state.
  Monitor2AutoLock mon(mGraphImpl->GetMonitor());
  GraphImpl()->DeviceChanged();
#ifdef XP_MACOSX
  RefPtr<AudioCallbackDriver> self(this);
  bool hasInput = mInputChannelCount;
  NS_DispatchBackgroundTask(NS_NewRunnableFunction(
      "PanOutputIfNeeded", [self{std::move(self)}, hasInput]() {
        self->PanOutputIfNeeded(hasInput);
      }));
#endif
}

uint32_t AudioCallbackDriver::IterationDuration() {
  MOZ_ASSERT(OnGraphThread());
  // The real fix would be to have an API in cubeb to give us the number. Short
  // of that, we approximate it here. bug 1019507
  return mIterationDurationMS;
}

bool AudioCallbackDriver::IsStarted() { return mStarted; }

void AudioCallbackDriver::EnqueueTrackAndPromiseForOperation(
    MediaTrack* aTrack, void* aPromise, dom::AudioContextOperation aOperation,
    dom::AudioContextOperationFlags aFlags) {
  MOZ_ASSERT(OnGraphThread() || !ThreadRunning());
  Monitor2AutoLock mon(mGraphImpl->GetMonitor());
  MOZ_ASSERT((aFlags | dom::AudioContextOperationFlags::SendStateChange) ||
             !aPromise);
  if (aFlags == dom::AudioContextOperationFlags::SendStateChange) {
    mPromisesForOperation.AppendElement(
        TrackAndPromiseForOperation(aTrack, aPromise, aOperation, aFlags));
  }
}

void AudioCallbackDriver::CompleteAudioContextOperations(
    AsyncCubebOperation aOperation) {
  MOZ_ASSERT(OnCubebOperationThread());
  AutoTArray<TrackAndPromiseForOperation, 1> array;

  // We can't lock for the whole function because AudioContextOperationCompleted
  // will grab the monitor
  {
    Monitor2AutoLock mon(GraphImpl()->GetMonitor());
    array.SwapElements(mPromisesForOperation);
  }

  for (uint32_t i = 0; i < array.Length(); i++) {
    TrackAndPromiseForOperation& s = array[i];
    if ((aOperation == AsyncCubebOperation::INIT &&
         s.mOperation == dom::AudioContextOperation::Resume) ||
        (aOperation == AsyncCubebOperation::SHUTDOWN &&
         s.mOperation != dom::AudioContextOperation::Resume)) {
      MOZ_ASSERT(s.mFlags == dom::AudioContextOperationFlags::SendStateChange);
      GraphImpl()->AudioContextOperationCompleted(s.mTrack, s.mPromise,
                                                  s.mOperation, s.mFlags);
      array.RemoveElementAt(i);
      i--;
    }
  }

  if (!array.IsEmpty()) {
    Monitor2AutoLock mon(GraphImpl()->GetMonitor());
    mPromisesForOperation.AppendElements(array);
  }
}

TimeDuration AudioCallbackDriver::AudioOutputLatency() {
  uint32_t latencyFrames;
  int rv = cubeb_stream_get_latency(mAudioStream, &latencyFrames);
  if (rv || mSampleRate == 0) {
    return TimeDuration::FromSeconds(0.0);
  }

  return TimeDuration::FromSeconds(static_cast<double>(latencyFrames) /
                                   mSampleRate);
}

void AudioCallbackDriver::FallbackToSystemClockDriver() {
  MOZ_ASSERT(!ThreadRunning());
  GraphImpl()->GetMonitor().AssertCurrentThreadOwns();
  SystemClockDriver* nextDriver = new SystemClockDriver(GraphImpl());
  nextDriver->MarkAsFallback();
  SetNextDriver(nextDriver);
  // We're not using SwitchAtNextIteration here, because there
  // won't be a next iteration if we don't restart things manually:
  // the audio stream just signaled that it's in error state.
  SwitchToNextDriver();
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef LOG
