/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/cssom/
 * https://wicg.github.io/construct-stylesheets/
 */

enum CSSStyleSheetParsingMode {
  "author",
  "user",
  "agent"
};

dictionary CSSStyleSheetInit {
  (MediaList or DOMString) media = "";
  DOMString title = "";
  boolean alternate = false;
  boolean disabled = false;
};

[Exposed=Window]
interface CSSStyleSheet : StyleSheet {
  [Throws, Pref="layout.css.constructable-stylesheets.enabled"]
  constructor(optional CSSStyleSheetInit options = {});
  [Pure, BinaryName="DOMOwnerRule"]
  readonly attribute CSSRule? ownerRule;
  [Throws, NeedsSubjectPrincipal]
  readonly attribute CSSRuleList cssRules;
  [ChromeOnly, BinaryName="parsingModeDOM"]
  readonly attribute CSSStyleSheetParsingMode parsingMode;
  [Throws, NeedsSubjectPrincipal]
  unsigned long insertRule(DOMString rule, optional unsigned long index = 0);
  [Throws, NeedsSubjectPrincipal]
  void deleteRule(unsigned long index);
  [Throws, Pref="layout.css.constructable-stylesheets.enabled"]
  Promise<CSSStyleSheet> replace(UTF8String text);
  [Throws, Pref="layout.css.constructable-stylesheets.enabled"]
  void replaceSync(UTF8String text);

  // Non-standard WebKit things.
  [Throws, NeedsSubjectPrincipal, BinaryName="cssRules"]
  readonly attribute CSSRuleList rules;
  [Throws, NeedsSubjectPrincipal, BinaryName="deleteRule"]
  void removeRule(optional unsigned long index = 0);
  [Throws, NeedsSubjectPrincipal]
  long addRule(optional DOMString selector = "undefined", optional DOMString style = "undefined", optional unsigned long index);
};
