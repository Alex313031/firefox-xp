/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheWorkerRef_h
#define mozilla_dom_cache_CacheWorkerRef_h

#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class IPCWorkerRef;
class StrongWorkerRef;
class WorkerPrivate;

namespace cache {

class ActorChild;

class CacheWorkerRef final {
 public:
  enum Behavior {
    eStrongWorkerRef,
    eIPCWorkerRef,
  };

  static already_AddRefed<CacheWorkerRef> Create(WorkerPrivate* aWorkerPrivate,
                                                 Behavior aBehavior);

  static already_AddRefed<CacheWorkerRef> PreferBehavior(
      CacheWorkerRef* aCurrentRef, Behavior aBehavior);

  void AddActor(ActorChild* aActor);
  void RemoveActor(ActorChild* aActor);

  bool Notified() const;

 private:
  explicit CacheWorkerRef(Behavior aBehavior);
  ~CacheWorkerRef();

  void Notify();

  nsTArray<ActorChild*> mActorList;

  Behavior mBehavior;
  bool mNotified;

  RefPtr<StrongWorkerRef> mStrongWorkerRef;
  RefPtr<IPCWorkerRef> mIPCWorkerRef;

 public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::cache::CacheWorkerRef)
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_CacheWorkerRef_h
