/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/css-pseudo-4/#csspseudoelement
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Func="Document::IsWebAnimationsGetAnimationsEnabled",
 Exposed=Window]
interface CSSPseudoElement {
  readonly attribute DOMString type;
  readonly attribute Element element;
};

// https://drafts.csswg.org/web-animations/#extensions-to-the-pseudoelement-interface
CSSPseudoElement includes Animatable;
