/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef AudioWorkletNode_h_
#define AudioWorkletNode_h_

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class AudioParamMap;
struct AudioWorkletNodeOptions;
class MessagePort;

class AudioWorkletNode : public AudioNode {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioWorkletNode, AudioNode)

  IMPL_EVENT_HANDLER(processorerror)

  static already_AddRefed<AudioWorkletNode> Constructor(
      const GlobalObject& aGlobal, AudioContext& aAudioContext,
      const nsAString& aName, const AudioWorkletNodeOptions& aOptions,
      ErrorResult& aRv);

  AudioParamMap* GetParameters(ErrorResult& aRv) const;

  MessagePort* Port() const { return mPort; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // AudioNode methods
  uint16_t NumberOfInputs() const override { return mInputCount; }
  uint16_t NumberOfOutputs() const override { return mOutputCount; }
  const char* NodeType() const override { return "AudioWorkletNode"; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

 private:
  AudioWorkletNode(AudioContext* aAudioContext, const nsAString& aName,
                   const AudioWorkletNodeOptions& aOptions);
  ~AudioWorkletNode() = default;

  nsString mNodeName;
  RefPtr<MessagePort> mPort;
  uint16_t mInputCount;
  uint16_t mOutputCount;
};

}  // namespace dom
}  // namespace mozilla

#endif  // AudioWorkletNode_h_
