/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPContentChild_h_
#define GMPContentChild_h_

#include "mozilla/gmp/PGMPContentChild.h"
#include "GMPSharedMemManager.h"

namespace mozilla {
namespace gmp {

class GMPChild;

class GMPContentChild : public PGMPContentChild, public GMPSharedMem {
 public:
  explicit GMPContentChild(GMPChild* aChild);
  virtual ~GMPContentChild();

  MessageLoop* GMPMessageLoop();

  mozilla::ipc::IPCResult RecvPGMPVideoDecoderConstructor(
      PGMPVideoDecoderChild* aActor, const uint32_t& aDecryptorId) override;
  mozilla::ipc::IPCResult RecvPGMPVideoEncoderConstructor(
      PGMPVideoEncoderChild* aActor) override;
  mozilla::ipc::IPCResult RecvPChromiumCDMConstructor(
      PChromiumCDMChild* aActor) override;

  already_AddRefed<PGMPVideoDecoderChild> AllocPGMPVideoDecoderChild(
      const uint32_t& aDecryptorId);

  already_AddRefed<PGMPVideoEncoderChild> AllocPGMPVideoEncoderChild();

  already_AddRefed<PChromiumCDMChild> AllocPChromiumCDMChild();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;

  // GMPSharedMem
  void CheckThread() override;

  void CloseActive();
  bool IsUsed();

  GMPChild* mGMPChild;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPContentChild_h_
