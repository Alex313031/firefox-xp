/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProxyRelease_h__
#define nsProxyRelease_h__

#include "nsIEventTarget.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "MainThreadUtils.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/Move.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"

#ifdef XPCOM_GLUE_AVOID_NSPR
#  error NS_ProxyRelease implementation depends on NSPR.
#endif

namespace detail {

template <typename T>
class ProxyReleaseEvent : public mozilla::CancelableRunnable {
 public:
  ProxyReleaseEvent(const char* aName, already_AddRefed<T> aDoomed)
      : CancelableRunnable(aName), mDoomed(aDoomed.take()) {}

  NS_IMETHOD Run() override {
    NS_IF_RELEASE(mDoomed);
    return NS_OK;
  }

  nsresult Cancel() override { return Run(); }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  NS_IMETHOD GetName(nsACString& aName) override {
    if (mName) {
      aName.Append(nsPrintfCString("ProxyReleaseEvent for %s", mName));
    } else {
      aName.AssignLiteral("ProxyReleaseEvent");
    }
    return NS_OK;
  }
#endif

 private:
  T* MOZ_OWNING_REF mDoomed;
};

template <typename T>
void ProxyRelease(const char* aName, nsIEventTarget* aTarget,
                  already_AddRefed<T> aDoomed, bool aAlwaysProxy) {
  // Auto-managing release of the pointer.
  RefPtr<T> doomed = aDoomed;
  nsresult rv;

  if (!doomed || !aTarget) {
    return;
  }

  if (!aAlwaysProxy) {
    bool onCurrentThread = false;
    rv = aTarget->IsOnCurrentThread(&onCurrentThread);
    if (NS_SUCCEEDED(rv) && onCurrentThread) {
      return;
    }
  }

  nsCOMPtr<nsIRunnable> ev = new ProxyReleaseEvent<T>(aName, doomed.forget());

  rv = aTarget->Dispatch(ev, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to post proxy release event, leaking!");
    // It is better to leak the aDoomed object than risk crashing as
    // a result of deleting it on the wrong thread.
  }
}

template <bool nsISupportsBased>
struct ProxyReleaseChooser {
  template <typename T>
  static void ProxyRelease(const char* aName, nsIEventTarget* aTarget,
                           already_AddRefed<T> aDoomed, bool aAlwaysProxy) {
    ::detail::ProxyRelease(aName, aTarget, std::move(aDoomed), aAlwaysProxy);
  }
};

template <>
struct ProxyReleaseChooser<true> {
  // We need an intermediate step for handling classes with ambiguous
  // inheritance to nsISupports.
  template <typename T>
  static void ProxyRelease(const char* aName, nsIEventTarget* aTarget,
                           already_AddRefed<T> aDoomed, bool aAlwaysProxy) {
    ProxyReleaseISupports(aName, aTarget, ToSupports(aDoomed.take()),
                          aAlwaysProxy);
  }

  static void ProxyReleaseISupports(const char* aName, nsIEventTarget* aTarget,
                                    nsISupports* aDoomed, bool aAlwaysProxy);
};

}  // namespace detail

/**
 * Ensures that the delete of a smart pointer occurs on the target thread.
 *
 * @param aName
 *        the labelling name of the runnable involved in the releasing
 * @param aTarget
 *        the target thread where the doomed object should be released.
 * @param aDoomed
 *        the doomed object; the object to be released on the target thread.
 * @param aAlwaysProxy
 *        normally, if NS_ProxyRelease is called on the target thread, then the
 *        doomed object will be released directly. However, if this parameter is
 *        true, then an event will always be posted to the target thread for
 *        asynchronous release.
 */
template <class T>
inline NS_HIDDEN_(void)
    NS_ProxyRelease(const char* aName, nsIEventTarget* aTarget,
                    already_AddRefed<T> aDoomed, bool aAlwaysProxy = false) {
  ::detail::ProxyReleaseChooser<
      std::is_base_of<nsISupports, T>::value>::ProxyRelease(aName, aTarget,
                                                            std::move(aDoomed),
                                                            aAlwaysProxy);
}

/**
 * Ensures that the delete of a smart pointer occurs on the main thread.
 *
 * @param aName
 *        the labelling name of the runnable involved in the releasing
 * @param aDoomed
 *        the doomed object; the object to be released on the main thread.
 * @param aAlwaysProxy
 *        normally, if NS_ReleaseOnMainThreadSystemGroup is called on the main
 *        thread, then the doomed object will be released directly. However, if
 *        this parameter is true, then an event will always be posted to the
 *        main thread for asynchronous release.
 */
template <class T>
inline NS_HIDDEN_(void)
    NS_ReleaseOnMainThreadSystemGroup(const char* aName,
                                      already_AddRefed<T> aDoomed,
                                      bool aAlwaysProxy = false) {
  // NS_ProxyRelease treats a null event target as "the current thread".  So a
  // handle on the main thread is only necessary when we're not already on the
  // main thread or the release must happen asynchronously.
  nsCOMPtr<nsIEventTarget> systemGroupEventTarget;
  if (!NS_IsMainThread() || aAlwaysProxy) {
    systemGroupEventTarget =
        mozilla::SystemGroup::EventTargetFor(mozilla::TaskCategory::Other);

    if (!systemGroupEventTarget) {
      MOZ_ASSERT_UNREACHABLE("Could not get main thread; leaking an object!");
      mozilla::Unused << aDoomed.take();
      return;
    }
  }

  NS_ProxyRelease(aName, systemGroupEventTarget, std::move(aDoomed),
                  aAlwaysProxy);
}

template <class T>
inline NS_HIDDEN_(void)
    NS_ReleaseOnMainThreadSystemGroup(already_AddRefed<T> aDoomed,
                                      bool aAlwaysProxy = false) {
  NS_ReleaseOnMainThreadSystemGroup("NS_ReleaseOnMainThreadSystemGroup",
                                    std::move(aDoomed), aAlwaysProxy);
}

/**
 * Class to safely handle main-thread-only pointers off the main thread.
 *
 * Classes like XPCWrappedJS are main-thread-only, which means that it is
 * forbidden to call methods on instances of these classes off the main thread.
 * For various reasons (see bug 771074), this restriction recently began to
 * apply to AddRef/Release as well.
 *
 * This presents a problem for consumers that wish to hold a callback alive
 * on non-main-thread code. A common example of this is the proxy callback
 * pattern, where non-main-thread code holds a strong-reference to the callback
 * object, and dispatches new Runnables (also with a strong reference) to the
 * main thread in order to execute the callback. This involves several AddRef
 * and Release calls on the other thread, which is (now) verboten.
 *
 * The basic idea of this class is to introduce a layer of indirection.
 * nsMainThreadPtrHolder is a threadsafe reference-counted class that internally
 * maintains one strong reference to the main-thread-only object. It must be
 * instantiated on the main thread (so that the AddRef of the underlying object
 * happens on the main thread), but consumers may subsequently pass references
 * to the holder anywhere they please. These references are meant to be opaque
 * when accessed off-main-thread (assertions enforce this).
 *
 * The semantics of RefPtr<nsMainThreadPtrHolder<T> > would be cumbersome, so
 * we also introduce nsMainThreadPtrHandle<T>, which is conceptually identical
 * to the above (though it includes various convenience methods). The basic
 * pattern is as follows.
 *
 * // On the main thread:
 * nsCOMPtr<nsIFooCallback> callback = ...;
 * nsMainThreadPtrHandle<nsIFooCallback> callbackHandle =
 *   new nsMainThreadPtrHolder<nsIFooCallback>(callback);
 * // Pass callbackHandle to structs/classes that might be accessed on other
 * // threads.
 *
 * All structs and classes that might be accessed on other threads should store
 * an nsMainThreadPtrHandle<T> rather than an nsCOMPtr<T>.
 */
template <class T>
class MOZ_IS_SMARTPTR_TO_REFCOUNTED nsMainThreadPtrHolder final {
 public:
  // We can only acquire a pointer on the main thread. We to fail fast for
  // threading bugs, so by default we assert if our pointer is used or acquired
  // off-main-thread. But some consumers need to use the same pointer for
  // multiple classes, some of which are main-thread-only and some of which
  // aren't. So we allow them to explicitly disable this strict checking.
  nsMainThreadPtrHolder(const char* aName, T* aPtr, bool aStrict = true,
                        nsIEventTarget* aMainThreadEventTarget = nullptr)
      : mRawPtr(nullptr),
        mStrict(aStrict),
        mMainThreadEventTarget(aMainThreadEventTarget)
#ifndef RELEASE_OR_BETA
        ,
        mName(aName)
#endif
  {
    // We can only AddRef our pointer on the main thread, which means that the
    // holder must be constructed on the main thread.
    MOZ_ASSERT(!mStrict || NS_IsMainThread());
    NS_IF_ADDREF(mRawPtr = aPtr);
  }
  nsMainThreadPtrHolder(const char* aName, already_AddRefed<T> aPtr,
                        bool aStrict = true,
                        nsIEventTarget* aMainThreadEventTarget = nullptr)
      : mRawPtr(aPtr.take()),
        mStrict(aStrict),
        mMainThreadEventTarget(aMainThreadEventTarget)
#ifndef RELEASE_OR_BETA
        ,
        mName(aName)
#endif
  {
    // Since we don't need to AddRef the pointer, this constructor is safe to
    // call on any thread.
  }

 private:
  // We can be released on any thread.
  ~nsMainThreadPtrHolder() {
    if (NS_IsMainThread()) {
      NS_IF_RELEASE(mRawPtr);
    } else if (mRawPtr) {
      if (!mMainThreadEventTarget) {
        mMainThreadEventTarget = do_GetMainThread();
      }
      MOZ_ASSERT(mMainThreadEventTarget);
      NS_ProxyRelease(
#ifdef RELEASE_OR_BETA
          nullptr,
#else
          mName,
#endif
          mMainThreadEventTarget, dont_AddRef(mRawPtr));
    }
  }

 public:
  T* get() {
    // Nobody should be touching the raw pointer off-main-thread.
    if (mStrict && MOZ_UNLIKELY(!NS_IsMainThread())) {
      NS_ERROR("Can't dereference nsMainThreadPtrHolder off main thread");
      MOZ_CRASH();
    }
    return mRawPtr;
  }

  bool operator==(const nsMainThreadPtrHolder<T>& aOther) const {
    return mRawPtr == aOther.mRawPtr;
  }
  bool operator!() const { return !mRawPtr; }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsMainThreadPtrHolder<T>)

 private:
  // Our wrapped pointer.
  T* mRawPtr;

  // Whether to strictly enforce thread invariants in this class.
  bool mStrict;

  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

#ifndef RELEASE_OR_BETA
  const char* mName = nullptr;
#endif

  // Copy constructor and operator= not implemented. Once constructed, the
  // holder is immutable.
  T& operator=(nsMainThreadPtrHolder& aOther);
  nsMainThreadPtrHolder(const nsMainThreadPtrHolder& aOther);
};

template <class T>
class MOZ_IS_SMARTPTR_TO_REFCOUNTED nsMainThreadPtrHandle {
  RefPtr<nsMainThreadPtrHolder<T>> mPtr;

 public:
  nsMainThreadPtrHandle() : mPtr(nullptr) {}
  MOZ_IMPLICIT nsMainThreadPtrHandle(decltype(nullptr)) : mPtr(nullptr) {}
  explicit nsMainThreadPtrHandle(nsMainThreadPtrHolder<T>* aHolder)
      : mPtr(aHolder) {}
  explicit nsMainThreadPtrHandle(
      already_AddRefed<nsMainThreadPtrHolder<T>> aHolder)
      : mPtr(aHolder) {}
  nsMainThreadPtrHandle(const nsMainThreadPtrHandle& aOther)
      : mPtr(aOther.mPtr) {}
  nsMainThreadPtrHandle& operator=(const nsMainThreadPtrHandle& aOther) {
    mPtr = aOther.mPtr;
    return *this;
  }
  nsMainThreadPtrHandle& operator=(nsMainThreadPtrHolder<T>* aHolder) {
    mPtr = aHolder;
    return *this;
  }

  // These all call through to nsMainThreadPtrHolder, and thus implicitly
  // assert that we're on the main thread. Off-main-thread consumers must treat
  // these handles as opaque.
  T* get() const {
    if (mPtr) {
      return mPtr.get()->get();
    }
    return nullptr;
  }

  operator T*() const { return get(); }
  T* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN { return get(); }

  // These are safe to call on other threads with appropriate external locking.
  bool operator==(const nsMainThreadPtrHandle<T>& aOther) const {
    if (!mPtr || !aOther.mPtr) {
      return mPtr == aOther.mPtr;
    }
    return *mPtr == *aOther.mPtr;
  }
  bool operator!=(const nsMainThreadPtrHandle<T>& aOther) const {
    return !operator==(aOther);
  }
  bool operator==(decltype(nullptr)) const { return mPtr == nullptr; }
  bool operator!=(decltype(nullptr)) const { return mPtr != nullptr; }
  bool operator!() const { return !mPtr || !*mPtr; }
};

namespace mozilla {

template <typename T>
using PtrHolder = nsMainThreadPtrHolder<T>;

template <typename T>
using PtrHandle = nsMainThreadPtrHandle<T>;

}  // namespace mozilla

class nsCycleCollectionTraversalCallback;
template <typename T>
void CycleCollectionNoteChild(nsCycleCollectionTraversalCallback& aCallback,
                              T* aChild, const char* aName, uint32_t aFlags);

template <typename T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsMainThreadPtrHandle<T>& aField, const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

template <typename T>
inline void ImplCycleCollectionUnlink(nsMainThreadPtrHandle<T>& aField) {
  aField = nullptr;
}

#endif
