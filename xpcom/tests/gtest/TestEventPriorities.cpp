/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsXPCOM.h"
#include "nsThreadUtils.h"
#include "gtest/gtest.h"

#include <functional>

using namespace mozilla;

class TestEvent final : public Runnable, nsIRunnablePriority {
 public:
  explicit TestEvent(int* aCounter, std::function<void()>&& aCheck,
                     uint32_t aPriority = nsIRunnablePriority::PRIORITY_NORMAL)
      : Runnable("TestEvent"),
        mCounter(aCounter),
        mCheck(std::move(aCheck)),
        mPriority(aPriority) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetPriority(uint32_t* aPriority) override {
    *aPriority = mPriority;
    return NS_OK;
  }

  NS_IMETHODIMP Run() override {
    (*mCounter)++;
    mCheck();
    return NS_OK;
  }

 private:
  ~TestEvent() {}

  int* mCounter;
  std::function<void()> mCheck;
  uint32_t mPriority;
};

NS_IMPL_ISUPPORTS_INHERITED(TestEvent, Runnable, nsIRunnablePriority)

TEST(EventPriorities, IdleAfterNormal)
{
  int normalRan = 0, idleRan = 0;

  RefPtr<TestEvent> evNormal =
      new TestEvent(&normalRan, [&] { ASSERT_EQ(idleRan, 0); });
  RefPtr<TestEvent> evIdle =
      new TestEvent(&idleRan, [&] { ASSERT_EQ(normalRan, 3); });

  NS_DispatchToCurrentThreadQueue(do_AddRef(evIdle), EventQueuePriority::Idle);
  NS_DispatchToCurrentThreadQueue(do_AddRef(evIdle), EventQueuePriority::Idle);
  NS_DispatchToCurrentThreadQueue(do_AddRef(evIdle), EventQueuePriority::Idle);
  NS_DispatchToMainThread(evNormal);
  NS_DispatchToMainThread(evNormal);
  NS_DispatchToMainThread(evNormal);

  MOZ_ALWAYS_TRUE(
      SpinEventLoopUntil([&]() { return normalRan == 3 && idleRan == 3; }));
}

TEST(EventPriorities, InterleaveHighNormal)
{
  int normalRan = 0, highRan = 0;

  RefPtr<TestEvent> evNormal = new TestEvent(
      &normalRan, [&] { ASSERT_TRUE(abs(normalRan - highRan) <= 1); });
  RefPtr<TestEvent> evHigh = new TestEvent(
      &highRan, [&] { ASSERT_TRUE(abs(normalRan - highRan) <= 1); },
      nsIRunnablePriority::PRIORITY_HIGH);

  NS_DispatchToMainThread(evNormal);
  NS_DispatchToMainThread(evNormal);
  NS_DispatchToMainThread(evNormal);
  NS_DispatchToMainThread(evHigh);
  NS_DispatchToMainThread(evHigh);
  NS_DispatchToMainThread(evHigh);

  MOZ_ALWAYS_TRUE(
      SpinEventLoopUntil([&]() { return normalRan == 3 && highRan == 3; }));
}
