/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Exposed=Window]
interface PluginCrashedEvent : Event
{
  constructor(DOMString type,
              optional PluginCrashedEventInit eventInitDict = {});

  readonly attribute unsigned long pluginID;
  readonly attribute DOMString pluginDumpID;
  readonly attribute DOMString pluginName;
  readonly attribute DOMString? browserDumpID;
  readonly attribute DOMString? pluginFilename;
  readonly attribute boolean submittedCrashReport;
  readonly attribute boolean gmpPlugin;
};

dictionary PluginCrashedEventInit : EventInit
{
  unsigned long pluginID = 0;
  DOMString pluginDumpID = "";
  DOMString pluginName = "";
  DOMString? browserDumpID = null;
  DOMString? pluginFilename = null;
  boolean submittedCrashReport = false;
  boolean gmpPlugin = false;
};
