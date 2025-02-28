/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txMozillaTextOutput.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentTransformer.h"
#include "nsCharsetSource.h"
#include "txURIUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "mozilla/Encoding.h"
#include "nsTextNode.h"
#include "nsNameSpaceManager.h"
#include "mozilla/dom/DocumentFragment.h"

using namespace mozilla;
using namespace mozilla::dom;

txMozillaTextOutput::txMozillaTextOutput(nsITransformObserver* aObserver) {
  MOZ_COUNT_CTOR(txMozillaTextOutput);
  mObserver = do_GetWeakReference(aObserver);
}

txMozillaTextOutput::txMozillaTextOutput(DocumentFragment* aDest) {
  MOZ_COUNT_CTOR(txMozillaTextOutput);
  mTextParent = aDest;
  mDocument = mTextParent->OwnerDoc();
}

txMozillaTextOutput::~txMozillaTextOutput() {
  MOZ_COUNT_DTOR(txMozillaTextOutput);
}

nsresult txMozillaTextOutput::attribute(nsAtom* aPrefix, nsAtom* aLocalName,
                                        nsAtom* aLowercaseLocalName,
                                        int32_t aNsID, const nsString& aValue) {
  return NS_OK;
}

nsresult txMozillaTextOutput::attribute(nsAtom* aPrefix, const nsAString& aName,
                                        const int32_t aNsID,
                                        const nsString& aValue) {
  return NS_OK;
}

nsresult txMozillaTextOutput::characters(const nsAString& aData, bool aDOE) {
  mText.Append(aData);

  return NS_OK;
}

nsresult txMozillaTextOutput::comment(const nsString& aData) { return NS_OK; }

nsresult txMozillaTextOutput::endDocument(nsresult aResult) {
  NS_ENSURE_TRUE(mDocument && mTextParent, NS_ERROR_FAILURE);

  RefPtr<nsTextNode> text = new nsTextNode(mDocument->NodeInfoManager());

  text->SetText(mText, false);
  nsresult rv = mTextParent->AppendChildTo(text, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // This should really be handled by Document::EndLoad
  if (mObserver) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == Document::READYSTATE_LOADING,
               "Bad readyState");
  } else {
    MOZ_ASSERT(
        mDocument->GetReadyStateEnum() == Document::READYSTATE_INTERACTIVE,
        "Bad readyState");
  }
  mDocument->SetReadyStateInternal(Document::READYSTATE_INTERACTIVE);

  if (NS_SUCCEEDED(aResult)) {
    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
      observer->OnTransformDone(aResult, mDocument);
    }
  }

  return NS_OK;
}

nsresult txMozillaTextOutput::endElement() { return NS_OK; }

nsresult txMozillaTextOutput::processingInstruction(const nsString& aTarget,
                                                    const nsString& aData) {
  return NS_OK;
}

nsresult txMozillaTextOutput::startDocument() { return NS_OK; }

nsresult txMozillaTextOutput::createResultDocument(Document* aSourceDocument,
                                                   bool aLoadedAsData) {
  /*
   * Create an XHTML document to hold the text.
   *
   * <html>
   *   <head />
   *   <body>
   *     <pre id="transformiixResult"> * The text comes here * </pre>
   *   <body>
   * </html>
   *
   * Except if we are transforming into a non-displayed document we create
   * the following DOM
   *
   * <transformiix:result> * The text comes here * </transformiix:result>
   */

  // Create the document
  nsresult rv = NS_NewXMLDocument(getter_AddRefs(mDocument), aLoadedAsData);
  NS_ENSURE_SUCCESS(rv, rv);
  // This should really be handled by Document::BeginLoad
  MOZ_ASSERT(
      mDocument->GetReadyStateEnum() == Document::READYSTATE_UNINITIALIZED,
      "Bad readyState");
  mDocument->SetReadyStateInternal(Document::READYSTATE_LOADING);
  bool hasHadScriptObject = false;
  nsIScriptGlobalObject* sgo =
      aSourceDocument->GetScriptHandlingObject(hasHadScriptObject);
  NS_ENSURE_STATE(sgo || !hasHadScriptObject);

  NS_ASSERTION(mDocument, "Need document");

  // Reset and set up document
  URIUtils::ResetWithSource(mDocument, aSourceDocument);
  // Only do this after resetting the document to ensure we have the
  // correct principal.
  mDocument->SetScriptHandlingObject(sgo);

  // Set the charset
  if (!mOutputFormat.mEncoding.IsEmpty()) {
    const Encoding* encoding = Encoding::ForLabel(mOutputFormat.mEncoding);
    if (encoding) {
      mDocument->SetDocumentCharacterSetSource(kCharsetFromOtherComponent);
      mDocument->SetDocumentCharacterSet(WrapNotNull(encoding));
    }
  }

  // Notify the contentsink that the document is created
  nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
  if (observer) {
    rv = observer->OnDocumentCreated(mDocument);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create the content

  // When transforming into a non-displayed document (i.e. when there is no
  // observer) we only create a transformiix:result root element.
  if (!observer) {
    int32_t namespaceID;
    rv = nsContentUtils::NameSpaceManager()->RegisterNameSpace(
        NS_LITERAL_STRING(kTXNameSpaceURI), namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    mTextParent =
        mDocument->CreateElem(nsDependentAtomString(nsGkAtoms::result),
                              nsGkAtoms::transformiix, namespaceID);

    rv = mDocument->AppendChildTo(mTextParent, true);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    RefPtr<Element> html, head, body;
    rv = createXHTMLElement(nsGkAtoms::html, getter_AddRefs(html));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = createXHTMLElement(nsGkAtoms::head, getter_AddRefs(head));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = html->AppendChildTo(head, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = createXHTMLElement(nsGkAtoms::body, getter_AddRefs(body));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = html->AppendChildTo(body, false);
    NS_ENSURE_SUCCESS(rv, rv);

    {
      RefPtr<Element> textParent;
      rv = createXHTMLElement(nsGkAtoms::pre, getter_AddRefs(textParent));
      NS_ENSURE_SUCCESS(rv, rv);
      mTextParent = std::move(textParent);
    }

    rv = mTextParent->AsElement()->SetAttr(
        kNameSpaceID_None, nsGkAtoms::id,
        NS_LITERAL_STRING("transformiixResult"), false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = body->AppendChildTo(mTextParent, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDocument->AppendChildTo(html, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult txMozillaTextOutput::startElement(nsAtom* aPrefix, nsAtom* aLocalName,
                                           nsAtom* aLowercaseLocalName,
                                           int32_t aNsID) {
  return NS_OK;
}

nsresult txMozillaTextOutput::startElement(nsAtom* aPrefix,
                                           const nsAString& aName,
                                           const int32_t aNsID) {
  return NS_OK;
}

void txMozillaTextOutput::getOutputDocument(Document** aDocument) {
  NS_IF_ADDREF(*aDocument = mDocument);
}

nsresult txMozillaTextOutput::createXHTMLElement(nsAtom* aName,
                                                 Element** aResult) {
  nsCOMPtr<Element> element = mDocument->CreateHTMLElement(aName);
  element.forget(aResult);
  return NS_OK;
}
