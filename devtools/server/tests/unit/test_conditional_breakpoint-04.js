/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

/**
 * Confirm that we ignore breakpoint condition exceptions
 * unless pause-on-exceptions is set to true.
 *
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test() {
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-conditional-breakpoint");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect().then(function() {
    attachTestTabAndResume(gClient, "test-conditional-breakpoint",
                           function(response, targetFront, threadClient) {
                             gThreadClient = threadClient;
                             test_simple_breakpoint();
                           });
  });
  do_test_pending();
}

async function test_simple_breakpoint() {
  await gThreadClient.setBreakpoint(
    { sourceUrl: "conditional_breakpoint-04.js", line: 3 },
    { condition: "throw new Error()" }
  );

  gThreadClient.once("paused", async function(packet) {
    Assert.equal(packet.frame.where.line, 1);
    Assert.equal(packet.why.type, "debuggerStatement");

    gThreadClient.resume();
    const pausedPacket = await waitForEvent(gThreadClient, "paused");
    Assert.equal(pausedPacket.frame.where.line, 4);
    Assert.equal(pausedPacket.why.type, "debuggerStatement");

    // Remove the breakpoint.
    await gThreadClient.removeBreakpoint(
      { sourceUrl: "conditional_breakpoint-04.js", line: 3 }
    );
    await gThreadClient.resume();
    finishClient(gClient);
  });

  /* eslint-disable */
  Cu.evalInSandbox(
    `debugger;
    var a = 1;
    var b = 2;
    debugger;`,
    gDebuggee,
    "1.8",
    "conditional_breakpoint-04.js",
    1
  );
  /* eslint-enable */
}
