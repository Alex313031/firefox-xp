/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGStyleElement_h
#define mozilla_dom_SVGStyleElement_h

#include "mozilla/Attributes.h"
#include "SVGElement.h"
#include "nsStyleLinkElement.h"
#include "nsStubMutationObserver.h"

nsresult NS_NewSVGStyleElement(
    nsIContent** aResult, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

namespace mozilla {
namespace dom {

typedef SVGElement SVGStyleElementBase;

class SVGStyleElement final : public SVGStyleElementBase,
                              public nsStyleLinkElement,
                              public nsStubMutationObserver {
 protected:
  friend nsresult(::NS_NewSVGStyleElement(
      nsIContent** aResult,
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo));
  explicit SVGStyleElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  ~SVGStyleElement() = default;

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGStyleElement, SVGStyleElementBase)

  // nsIContent
  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aMaybeScriptedPrincipal,
                                bool aNotify) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  // WebIDL
  void GetXmlspace(nsAString& aXmlspace);
  void SetXmlspace(const nsAString& aXmlspace, ErrorResult& rv);
  void GetMedia(nsAString& aMedia);
  void SetMedia(const nsAString& aMedia, ErrorResult& rv);
  void GetType(nsAString& aType);
  void SetType(const nsAString& aType, ErrorResult& rv);
  void GetTitle(nsAString& aTitle);
  void SetTitle(const nsAString& aTitle, ErrorResult& rv);

 protected:
  // Dummy init method to make the NS_IMPL_NS_NEW_SVG_ELEMENT and
  // NS_IMPL_ELEMENT_CLONE_WITH_INIT usable with this class. This should be
  // completely optimized away.
  inline nsresult Init() { return NS_OK; }

  // nsStyleLinkElement overrides
  Maybe<SheetInfo> GetStyleSheetInfo() final;

  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGStyleElement_h
