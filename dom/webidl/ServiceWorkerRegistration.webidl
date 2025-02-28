/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 * https://w3c.github.io/push-api/
 * https://notifications.spec.whatwg.org/
 */

[Pref="dom.serviceWorkers.enabled",
 Exposed=(Window,Worker)]
interface ServiceWorkerRegistration : EventTarget {
  [Unforgeable] readonly attribute ServiceWorker? installing;
  [Unforgeable] readonly attribute ServiceWorker? waiting;
  [Unforgeable] readonly attribute ServiceWorker? active;

  readonly attribute USVString scope;
  [Throws]
  readonly attribute ServiceWorkerUpdateViaCache updateViaCache;

  [Throws, NewObject]
  Promise<void> update();

  [Throws, NewObject]
  Promise<boolean> unregister();

  // event
  attribute EventHandler onupdatefound;
};

enum ServiceWorkerUpdateViaCache {
  "imports",
  "all",
  "none"
};

// https://w3c.github.io/push-api/
partial interface ServiceWorkerRegistration {
  [Throws, Exposed=(Window,Worker), Pref="dom.push.enabled"]
  readonly attribute PushManager pushManager;
};

// https://notifications.spec.whatwg.org/
partial interface ServiceWorkerRegistration {
  [Throws, Pref="dom.webnotifications.serviceworker.enabled"]
  Promise<void> showNotification(DOMString title, optional NotificationOptions options = {});
  [Throws, Pref="dom.webnotifications.serviceworker.enabled"]
  Promise<sequence<Notification>> getNotifications(optional GetNotificationOptions filter = {});
};
