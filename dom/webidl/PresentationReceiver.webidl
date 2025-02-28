/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/presentation-api/#interface-presentationreceiver
 */

[Pref="dom.presentation.receiver.enabled",
 Exposed=Window]
interface PresentationReceiver {
  /*
   * Get a list which contains all connected presentation connections
   * in a receiving browsing context.
   */
  [Throws]
  readonly attribute Promise<PresentationConnectionList> connectionList;
};
