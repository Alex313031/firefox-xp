/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC-internal classes for acquiring and releasing the GC lock.
 */

#ifndef gc_GCLock_h
#define gc_GCLock_h

#include "vm/Runtime.h"

namespace js {

class AutoUnlockGC;

/*
 * RAII class that takes the GC lock while it is live.
 *
 * Usually functions will pass const references of this class.  However
 * non-const references can be used to either temporarily release the lock by
 * use of AutoUnlockGC or to start background allocation when the lock is
 * released.
 */
class MOZ_RAII AutoLockGC {
 public:
  explicit AutoLockGC(gc::GCRuntime* gc MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : gc(gc) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    lock();
  }
  explicit AutoLockGC(JSRuntime* rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoLockGC(&rt->gc) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoLockGC() { lockGuard_.reset(); }

 protected:
  void lock() {
    MOZ_ASSERT(lockGuard_.isNothing());
    lockGuard_.emplace(gc->lock);
  }

  void unlock() {
    MOZ_ASSERT(lockGuard_.isSome());
    lockGuard_.reset();
  }

  js::LockGuard<js::Mutex>& guard() { return lockGuard_.ref(); }

  gc::GCRuntime* const gc;

 private:
  mozilla::Maybe<js::LockGuard<js::Mutex>> lockGuard_;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  AutoLockGC(const AutoLockGC&) = delete;
  AutoLockGC& operator=(const AutoLockGC&) = delete;

  friend class AutoUnlockGC;  // For lock/unlock.
};

/*
 * Same as AutoLockGC except it can optionally start a background chunk
 * allocation task when the lock is released.
 */
class MOZ_RAII AutoLockGCBgAlloc : public AutoLockGC {
 public:
  explicit AutoLockGCBgAlloc(gc::GCRuntime* gc) : AutoLockGC(gc) {}
  explicit AutoLockGCBgAlloc(JSRuntime* rt) : AutoLockGCBgAlloc(&rt->gc) {}

  ~AutoLockGCBgAlloc() {
    unlock();

    /*
     * We have to do this after releasing the lock because it may acquire
     * the helper lock which could cause lock inversion if we still held
     * the GC lock.
     */
    if (startBgAlloc) {
      gc->startBackgroundAllocTaskIfIdle();
    }
  }

  /*
   * This can be used to start a background allocation task (if one isn't
   * already running) that allocates chunks and makes them available in the
   * free chunks list.  This happens after the lock is released in order to
   * avoid lock inversion.
   */
  void tryToStartBackgroundAllocation() { startBgAlloc = true; }

 private:
  // true if we should start a background chunk allocation task after the
  // lock is released.
  bool startBgAlloc = false;
};

class MOZ_RAII AutoUnlockGC {
 public:
  explicit AutoUnlockGC(AutoLockGC& lock MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : lock(lock) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    lock.unlock();
  }

  ~AutoUnlockGC() { lock.lock(); }

 private:
  AutoLockGC& lock;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  AutoUnlockGC(const AutoUnlockGC&) = delete;
  AutoUnlockGC& operator=(const AutoUnlockGC&) = delete;
};

}  // namespace js

#endif /* gc_GCLock_h */
