/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_URL =
  URL_ROOT + "resources/service-workers/dynamic-registration.html";

add_task(async function() {
  await enableApplicationPanel();

  const { panel, tab } = await openNewTabAndApplicationPanel(TAB_URL);
  const doc = panel.panelWin.document;

  const isWorkerListEmpty = !!doc.querySelector(".worker-list-empty");
  ok(isWorkerListEmpty, "No Service Worker displayed");

  info("Register a service worker in the page.");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.registerServiceWorker();
  });

  info("Wait until the service worker appears in the application panel");
  await waitUntil(() => getWorkerContainers(doc).length > 0);

  const workerContainer = getWorkerContainers(doc)[0];

  info("Wait until the unregister button is displayed for the service worker");
  await waitUntil(() => workerContainer.querySelector(".js-unregister-button"));

  const scopeEl = workerContainer.querySelector(".js-sw-scope");
  const expectedScope =
    "example.com/browser/devtools/client/application/test/" +
    "browser/resources/service-workers/";
  ok(
    scopeEl.textContent.startsWith(expectedScope),
    "Service worker has the expected scope"
  );

  const updatedEl = workerContainer.querySelector(".js-sw-updated");
  ok(
    updatedEl.textContent.includes(`${new Date().getFullYear()}`),
    "Service worker has a last updated time"
  );

  info("Unregister the service worker");
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    const registration = await content.wrappedJSObject.sw;
    registration.unregister();
  });

  info("Wait until the service worker is removed from the application panel");
  await waitUntil(() => getWorkerContainers(doc).length === 0);
});
