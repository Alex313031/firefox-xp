/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG2/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=Window]
interface SVGElement : Element {
           attribute DOMString id;

  [Constant]
  readonly attribute SVGAnimatedString className;

  readonly attribute SVGSVGElement? ownerSVGElement;
  readonly attribute SVGElement? viewportElement;
};

SVGElement includes GlobalEventHandlers;
SVGElement includes HTMLOrForeignElement;
SVGElement includes DocumentAndElementEventHandlers;
SVGElement includes ElementCSSInlineStyle;
SVGElement includes TouchEventHandlers;
SVGElement includes OnErrorEventHandlerForNodes;
