/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface please see
 * http://dev.w3.org/2006/webapi/clipops/#x5-clipboard-event-interfaces
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=Window]
interface ClipboardEvent : Event
{
  [Throws]
  constructor(DOMString type, optional ClipboardEventInit eventInitDict = {});

  readonly attribute DataTransfer? clipboardData;
};

dictionary ClipboardEventInit : EventInit
{
  DOMString data = "";
  DOMString dataType = "";
};
