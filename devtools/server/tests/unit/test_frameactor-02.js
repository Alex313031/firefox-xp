/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Verify that two pauses in a row will keep the same frame actor.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-stack", function(
      response,
      targetFront,
      threadClient
    ) {
      gThreadClient = threadClient;
      test_pause_frame();
    });
  });
  do_test_pending();
}

function test_pause_frame() {
  gThreadClientgThreadClient.once("paused", function(packet1) {
    gThreadClient.gThreadClient.once("paused", function(packet2) {
      Assert.equal(packet1.frame.actor, packet2.frame.actor);
      gThreadClient.resume().then(function() {
        finishClient(gClient);
      });
    });
    gThreadClient.resume();
  });

  gDebuggee.eval(
    "(" +
      function() {
        function stopMe() {
          debugger;
          debugger;
        }
        stopMe();
      } +
      ")()"
  );
}
