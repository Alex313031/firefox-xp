/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderManagerParent_h
#define include_dom_media_ipc_RemoteDecoderManagerParent_h
#include "mozilla/PRemoteDecoderManagerParent.h"

namespace mozilla {

class RemoteDecoderManagerThreadHolder;

class RemoteDecoderManagerParent final : public PRemoteDecoderManagerParent {
  friend class PRemoteDecoderManagerParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerParent)

  static bool CreateForContent(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

  // Can be called from any thread
  SurfaceDescriptorGPUVideo StoreImage(layers::Image* aImage,
                                       layers::TextureClient* aTexture);

  static bool StartupThreads();
  static void ShutdownThreads();

  static void ShutdownVideoBridge();

  bool OnManagerThread();

 protected:
  PRemoteDecoderParent* AllocPRemoteDecoderParent(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      const layers::TextureFactoryIdentifier& aIdentifier, bool* aSuccess,
      nsCString* aErrorDescription);
  bool DeallocPRemoteDecoderParent(PRemoteDecoderParent* actor);

  mozilla::ipc::IPCResult RecvReadback(const SurfaceDescriptorGPUVideo& aSD,
                                       SurfaceDescriptor* aResult);
  mozilla::ipc::IPCResult RecvDeallocateSurfaceDescriptorGPUVideo(
      const SurfaceDescriptorGPUVideo& aSD);

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;

  void ActorDealloc() override;

 private:
  explicit RemoteDecoderManagerParent(
      RemoteDecoderManagerThreadHolder* aThreadHolder);
  ~RemoteDecoderManagerParent();

  void Open(Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

  std::map<uint64_t, RefPtr<layers::Image>> mImageMap;
  std::map<uint64_t, RefPtr<layers::TextureClient>> mTextureMap;

  RefPtr<RemoteDecoderManagerThreadHolder> mThreadHolder;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderManagerParent_h
