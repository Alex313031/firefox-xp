/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/ServiceWorker/#serviceworkercontainer
 *
 */

[Func="ServiceWorkerContainer::IsEnabled",
 Exposed=Window]
interface ServiceWorkerContainer : EventTarget {
  // FIXME(nsm):
  // https://github.com/slightlyoff/ServiceWorker/issues/198
  // and discussion at https://etherpad.mozilla.org/serviceworker07apr
  [Unforgeable] readonly attribute ServiceWorker? controller;

  [Throws]
  readonly attribute Promise<ServiceWorkerRegistration> ready;

  [NewObject]
  Promise<ServiceWorkerRegistration> register(USVString scriptURL,
                                              optional RegistrationOptions options = {});

  [NewObject]
  Promise<any> getRegistration(optional USVString documentURL = "");

  [NewObject]
  Promise<sequence<ServiceWorkerRegistration>> getRegistrations();

  void startMessages();

  attribute EventHandler oncontrollerchange;
  attribute EventHandler onerror;
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

// Testing only.
partial interface ServiceWorkerContainer {
  [Throws,Pref="dom.serviceWorkers.testing.enabled"]
  DOMString getScopeForUrl(DOMString url);
};

dictionary RegistrationOptions {
  USVString scope;
  ServiceWorkerUpdateViaCache updateViaCache = "imports";
};
