/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbfactory_h__
#define mozilla_dom_idbfactory_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StorageTypeBinding.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsIEventTarget;
class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace ipc {

class PBackgroundChild;
class PrincipalInfo;

}  // namespace ipc

namespace dom {

struct IDBOpenDBOptions;
class IDBOpenDBRequest;
template <typename>
class Optional;
class BrowserChild;
enum class CallerType : uint32_t;

namespace indexedDB {
class BackgroundFactoryChild;
class FactoryRequestParams;
class LoggingInfo;
}  // namespace indexedDB

class IDBFactory final : public nsISupports, public nsWrapperCache {
  typedef mozilla::dom::StorageType StorageType;
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;
  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;

  class BackgroundCreateCallback;
  struct PendingRequestInfo;

  UniquePtr<PrincipalInfo> mPrincipalInfo;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // This will only be set if the factory belongs to a window in a child
  // process.
  RefPtr<BrowserChild> mBrowserChild;

  indexedDB::BackgroundFactoryChild* mBackgroundActor;

  // It is either set to a DocGroup-specific EventTarget if created by
  // CreateForWindow() or set to GetCurrentThreadEventTarget() otherwise.
  nsCOMPtr<nsIEventTarget> mEventTarget;

  uint64_t mInnerWindowID;
  uint32_t mActiveTransactionCount;
  uint32_t mActiveDatabaseCount;

  bool mBackgroundActorFailed;
  bool mPrivateBrowsingMode;

 public:
  static nsresult CreateForWindow(nsPIDOMWindowInner* aWindow,
                                  IDBFactory** aFactory);

  static nsresult CreateForMainThreadJS(nsIGlobalObject* aGlobal,
                                        IDBFactory** aFactory);

  static nsresult CreateForWorker(nsIGlobalObject* aGlobal,
                                  const PrincipalInfo& aPrincipalInfo,
                                  uint64_t aInnerWindowID,
                                  IDBFactory** aFactory);

  static bool AllowedForWindow(nsPIDOMWindowInner* aWindow);

  static bool AllowedForPrincipal(nsIPrincipal* aPrincipal,
                                  bool* aIsSystemPrincipal = nullptr);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(IDBFactory); }

  nsIEventTarget* EventTarget() const {
    AssertIsOnOwningThread();
    MOZ_RELEASE_ASSERT(mEventTarget);
    return mEventTarget;
  }

  void ClearBackgroundActor() {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  // Increase/Decrease the number of active transactions for the decision
  // making of preemption and throttling.
  // Note: If the state of its actor is not committed or aborted, it could block
  // IDB operations in other window.
  void UpdateActiveTransactionCount(int32_t aDelta);

  // Increase/Decrease the number of active databases and IDBOpenRequests for
  // the decision making of preemption and throttling.
  // Note: A non-closed database or a pending IDBOpenRequest could block
  // IDB operations in other window.
  void UpdateActiveDatabaseCount(int32_t aDelta);

  void IncrementParentLoggingRequestSerialNumber();

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  BrowserChild* GetBrowserChild() const { return mBrowserChild; }

  PrincipalInfo* GetPrincipalInfo() const {
    AssertIsOnOwningThread();

    return mPrincipalInfo.get();
  }

  uint64_t InnerWindowID() const {
    AssertIsOnOwningThread();

    return mInnerWindowID;
  }

  bool IsChrome() const;

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> Open(JSContext* aCx,
                                             const nsAString& aName,
                                             uint64_t aVersion,
                                             CallerType aCallerType,
                                             ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> Open(JSContext* aCx,
                                             const nsAString& aName,
                                             const IDBOpenDBOptions& aOptions,
                                             CallerType aCallerType,
                                             ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> DeleteDatabase(
      JSContext* aCx, const nsAString& aName, const IDBOpenDBOptions& aOptions,
      CallerType aCallerType, ErrorResult& aRv);

  int16_t Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
              JS::Handle<JS::Value> aSecond, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> OpenForPrincipal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      uint64_t aVersion, SystemCallerGuarantee, ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> OpenForPrincipal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      const IDBOpenDBOptions& aOptions, SystemCallerGuarantee,
      ErrorResult& aRv);

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> DeleteForPrincipal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      const IDBOpenDBOptions& aOptions, SystemCallerGuarantee,
      ErrorResult& aRv);

  void DisconnectFromGlobal(nsIGlobalObject* aOldGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

 private:
  IDBFactory();
  ~IDBFactory();

  static nsresult CreateForMainThreadJSInternal(
      nsIGlobalObject* aGlobal, UniquePtr<PrincipalInfo> aPrincipalInfo,
      IDBFactory** aFactory);

  static nsresult CreateInternal(nsIGlobalObject* aGlobal,
                                 UniquePtr<PrincipalInfo> aPrincipalInfo,
                                 uint64_t aInnerWindowID,
                                 IDBFactory** aFactory);

  static nsresult AllowedForWindowInternal(nsPIDOMWindowInner* aWindow,
                                           nsCOMPtr<nsIPrincipal>* aPrincipal);

  MOZ_MUST_USE RefPtr<IDBOpenDBRequest> OpenInternal(
      JSContext* aCx, nsIPrincipal* aPrincipal, const nsAString& aName,
      const Optional<uint64_t>& aVersion,
      const Optional<StorageType>& aStorageType, bool aDeleting,
      CallerType aCallerType, ErrorResult& aRv);

  nsresult InitiateRequest(IDBOpenDBRequest* aRequest,
                           const indexedDB::FactoryRequestParams& aParams);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbfactory_h__
