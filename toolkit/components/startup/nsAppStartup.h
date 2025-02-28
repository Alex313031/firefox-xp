/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAppStartup_h__
#define nsAppStartup_h__

#include "nsIAppStartup.h"
#include "nsIWindowCreator.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include "nsIAppShell.h"
#include "mozilla/Attributes.h"

// {7DD4D320-C84B-4624-8D45-7BB9B2356977}
#define NS_TOOLKIT_APPSTARTUP_CID                    \
  {                                                  \
    0x7dd4d320, 0xc84b, 0x4624, {                    \
      0x8d, 0x45, 0x7b, 0xb9, 0xb2, 0x35, 0x69, 0x77 \
    }                                                \
  }

class nsAppStartup final : public nsIAppStartup,
                           public nsIWindowCreator,
                           public nsIObserver,
                           public nsSupportsWeakReference {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIAPPSTARTUP
  NS_DECL_NSIWINDOWCREATOR
  NS_DECL_NSIOBSERVER

  nsAppStartup();
  nsresult Init();

 private:
  ~nsAppStartup() {}

  void CloseAllWindows();

  friend class nsAppExitEvent;

  nsCOMPtr<nsIAppShell> mAppShell;

  int32_t mConsiderQuitStopper;  // if > 0, Quit(eConsiderQuit) fails
  bool mRunning;                 // Have we started the main event loop?
  bool mShuttingDown;            // Quit method reentrancy check
  bool mStartingUp;              // Have we passed final-ui-startup?
  bool mAttemptingQuit;          // Quit(eAttemptQuit) still trying
  bool mRestart;                 // Quit(eRestart)
  bool mInterrupted;  // Was startup interrupted by an interactive prompt?
  bool mIsSafeModeNecessary;        // Whether safe mode is necessary
  bool mStartupCrashTrackingEnded;  // Whether startup crash tracking has
                                    // already ended
};

#endif  // nsAppStartup_h__
