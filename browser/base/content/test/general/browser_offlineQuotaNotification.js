/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test offline quota warnings - must be run as a mochitest-browser test or
// else the test runner gets in the way of notifications due to bug 857897.

const URL =
  "http://mochi.test:8888/browser/browser/base/content/test/general/offlineQuotaNotification.html";

registerCleanupFunction(function() {
  // Clean up after ourself
  let uri = Services.io.newURI(URL);
  let principal = Services.scriptSecurityManager.createCodebasePrincipal(
    uri,
    {}
  );
  Services.perms.removeFromPrincipal(principal, "offline-app");
  Services.prefs.clearUserPref("offline-apps.quota.warn");
  let { OfflineAppCacheHelper } = ChromeUtils.import(
    "resource://gre/modules/offlineAppCache.jsm"
  );
  OfflineAppCacheHelper.clear();
});

// Same as the other one, but for in-content preferences
function checkInContentPreferences(win) {
  let doc = win.document;
  let sel = doc.getElementById("categories").selectedItems[0].id;
  is(
    gBrowser.currentURI.spec,
    "about:preferences#privacy",
    "about:preferences loaded"
  );
  is(sel, "category-privacy", "Privacy pane was selected");
  // all good, we are done.
  win.close();
  finish();
}

async function test() {
  waitForExplicitFinish();

  let notificationShown = promiseNotification();

  // Open a new tab.
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, URL);
  registerCleanupFunction(() => gBrowser.removeCurrentTab());

  // Wait for the tab to load.
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  info("Loaded page, adding onCached handler");
  // Need a promise to keep track of when we've added our handler.
  let mm = gBrowser.selectedBrowser.messageManager;
  let onCachedAttached = BrowserTestUtils.waitForMessage(
    mm,
    "Test:OnCachedAttached"
  );
  let gotCached = ContentTask.spawn(
    gBrowser.selectedBrowser,
    null,
    async function() {
      return new Promise(resolve => {
        content.window.applicationCache.oncached = function() {
          setTimeout(resolve, 0);
        };
        sendAsyncMessage("Test:OnCachedAttached");
      });
    }
  );
  gotCached.then(async function() {
    // We got cached - now we should have provoked the quota warning.
    await notificationShown;
    let notification = PopupNotifications.getNotification("offline-app-usage");
    ok(notification, "have offline-app-usage notification");
    // select the default action - this should cause the preferences
    // tab to open - which we track via an "Initialized" event.
    PopupNotifications.panel.firstElementChild.button.click();
    let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
    newTabBrowser.addEventListener(
      "Initialized",
      function() {
        executeSoon(function() {
          checkInContentPreferences(newTabBrowser.contentWindow);
        });
      },
      { capture: true, once: true }
    );
  });
  onCachedAttached.then(function() {
    Services.prefs.setIntPref("offline-apps.quota.warn", 1);
  });
}

function promiseNotification() {
  return new Promise(resolve => {
    PopupNotifications.panel.addEventListener(
      "popupshown",
      function() {
        resolve();
      },
      { once: true }
    );
  });
}
