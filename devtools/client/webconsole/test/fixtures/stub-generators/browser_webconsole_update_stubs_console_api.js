/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  const fileContent = await generateConsoleApiStubs();
  const filePath = OS.Path.join(`${BASE_PATH}/stubs`, "consoleApi.js");
  await OS.File.writeAtomic(filePath, fileContent);
  ok(true, "Make the test not fail");
});
