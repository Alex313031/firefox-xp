/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary WebrtcGlobalStatisticsReport {
  sequence<RTCStatsReportInternal> reports;
};

callback WebrtcGlobalStatisticsCallback = void (WebrtcGlobalStatisticsReport reports);
callback WebrtcGlobalLoggingCallback = void (sequence<DOMString> logMessages);

[ChromeOnly, Exposed=Window]
namespace WebrtcGlobalInformation {

  [Throws]
  void getAllStats(WebrtcGlobalStatisticsCallback callback,
                   optional DOMString pcIdFilter);

  void clearAllStats();

  [Throws]
  void getLogging(DOMString pattern, WebrtcGlobalLoggingCallback callback);

  void clearLogging();

  // NSPR WebRTC Trace debug level (0 - 65535)
  //
  // Notes:
  // - Setting a non-zero debug level turns on gathering of log for file output.
  // - Subsequently setting a zero debug level writes that log to disk.

  attribute long debugLevel;

  // WebRTC AEC debugging enable
  attribute boolean aecDebug;

  readonly attribute DOMString aecDebugLogDir;
};
