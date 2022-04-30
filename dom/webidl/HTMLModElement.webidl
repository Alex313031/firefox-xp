/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#attributes-common-to-ins-and-del-elements
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

// http://www.whatwg.org/specs/web-apps/current-work/#attributes-common-to-ins-and-del-elements
[HTMLConstructor]
interface HTMLModElement : HTMLElement {
  [CEReactions, SetterThrows, Pure]
           attribute DOMString cite;
  [CEReactions, SetterThrows, Pure]
           attribute DOMString dateTime;
};