/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_GPUParent_h__
#define _include_gfx_ipc_GPUParent_h__

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/PGPUParent.h"

namespace mozilla {

class TimeStamp;
class ChildProfilerController;

namespace gfx {

class VsyncBridgeParent;

class GPUParent final : public PGPUParent {
 public:
  GPUParent();
  ~GPUParent();

  static GPUParent* GetSingleton();

  // Gets the name of the GPU process, in the format expected by about:memory.
  // There must be a GPU process active, and the caller must be either in that
  // process or the parent process.
  static void GetGPUProcessName(nsACString& aStr);

  bool Init(base::ProcessId aParentPid, const char* aParentBuildID,
            MessageLoop* aIOLoop, IPC::Channel* aChannel);
  void NotifyDeviceReset();

  already_AddRefed<PAPZInputBridgeParent> AllocPAPZInputBridgeParent(
      const LayersId& aLayersId);

  mozilla::ipc::IPCResult RecvInit(nsTArray<GfxVarUpdate>&& vars,
                                   const DevicePrefs& devicePrefs,
                                   nsTArray<LayerTreeIdMapping>&& mappings);
  mozilla::ipc::IPCResult RecvInitCompositorManager(
      Endpoint<PCompositorManagerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvInitVsyncBridge(
      Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint);
  mozilla::ipc::IPCResult RecvInitImageBridge(
      Endpoint<PImageBridgeParent>&& aEndpoint);
#ifdef MOZ_VR
  mozilla::ipc::IPCResult RecvInitVRManager(
      Endpoint<PVRManagerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvInitVR(Endpoint<PVRGPUChild>&& aVRGPUChild);
  mozilla::ipc::IPCResult RecvNewContentVRManager(
      Endpoint<PVRManagerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvShutdownVR();
#endif
  mozilla::ipc::IPCResult RecvInitUiCompositorController(
      const LayersId& aRootLayerTreeId,
      Endpoint<PUiCompositorControllerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvInitProfiler(
      Endpoint<PProfilerChild>&& aEndpoint);
  mozilla::ipc::IPCResult RecvUpdateVar(const GfxVarUpdate& pref);
  mozilla::ipc::IPCResult RecvPreferenceUpdate(const Pref& pref);
  mozilla::ipc::IPCResult RecvNewContentCompositorManager(
      Endpoint<PCompositorManagerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvNewContentImageBridge(
      Endpoint<PImageBridgeParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvNewContentRemoteDecoderManager(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);
  mozilla::ipc::IPCResult RecvGetDeviceStatus(GPUDeviceData* aOutStatus);
  mozilla::ipc::IPCResult RecvSimulateDeviceReset(GPUDeviceData* aOutStatus);
  mozilla::ipc::IPCResult RecvAddLayerTreeIdMapping(
      const LayerTreeIdMapping& aMapping);
  mozilla::ipc::IPCResult RecvRemoveLayerTreeIdMapping(
      const LayerTreeIdMapping& aMapping);
  mozilla::ipc::IPCResult RecvNotifyGpuObservers(const nsCString& aTopic);
  mozilla::ipc::IPCResult RecvRequestMemoryReport(
      const uint32_t& generation, const bool& anonymize,
      const bool& minimizeMemoryUsage,
      const Maybe<ipc::FileDescriptor>& DMDFile);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  const TimeStamp mLaunchTime;
  RefPtr<VsyncBridgeParent> mVsyncBridge;
#ifdef MOZ_GECKO_PROFILER
  RefPtr<ChildProfilerController> mProfilerController;
#endif
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_GPUParent_h__
