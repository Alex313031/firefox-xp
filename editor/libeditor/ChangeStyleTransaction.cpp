/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ChangeStyleTransaction.h"

#include "mozilla/dom/Element.h"  // for Element
#include "nsAString.h"            // for nsAString::Append, etc.
#include "nsCRT.h"                // for nsCRT::IsAsciiSpace
#include "nsDebug.h"              // for NS_ENSURE_SUCCESS, etc.
#include "nsError.h"              // for NS_ERROR_NULL_POINTER, etc.
#include "nsGkAtoms.h"            // for nsGkAtoms, etc.
#include "nsICSSDeclaration.h"    // for nsICSSDeclaration.
#include "nsLiteralString.h"      // for NS_LITERAL_STRING, etc.
#include "nsReadableUtils.h"      // for ToNewUnicode
#include "nsString.h"             // for nsAutoString, nsString, etc.
#include "nsStyledElement.h"      // for nsStyledElement.
#include "nsUnicharUtils.h"       // for nsCaseInsensitiveStringComparator

namespace mozilla {

using namespace dom;

// static
already_AddRefed<ChangeStyleTransaction> ChangeStyleTransaction::Create(
    Element& aElement, nsAtom& aProperty, const nsAString& aValue) {
  RefPtr<ChangeStyleTransaction> transaction =
      new ChangeStyleTransaction(aElement, aProperty, aValue, false);
  return transaction.forget();
}

// static
already_AddRefed<ChangeStyleTransaction> ChangeStyleTransaction::CreateToRemove(
    Element& aElement, nsAtom& aProperty, const nsAString& aValue) {
  RefPtr<ChangeStyleTransaction> transaction =
      new ChangeStyleTransaction(aElement, aProperty, aValue, true);
  return transaction.forget();
}

ChangeStyleTransaction::ChangeStyleTransaction(Element& aElement,
                                               nsAtom& aProperty,
                                               const nsAString& aValue,
                                               bool aRemove)
    : EditTransactionBase(),
      mElement(&aElement),
      mProperty(&aProperty),
      mValue(aValue),
      mRemoveProperty(aRemove),
      mUndoValue(),
      mRedoValue(),
      mUndoAttributeWasSet(false),
      mRedoAttributeWasSet(false) {}

#define kNullCh (char16_t('\0'))

NS_IMPL_CYCLE_COLLECTION_INHERITED(ChangeStyleTransaction, EditTransactionBase,
                                   mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeStyleTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMPL_ADDREF_INHERITED(ChangeStyleTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(ChangeStyleTransaction, EditTransactionBase)

ChangeStyleTransaction::~ChangeStyleTransaction() {}

// Answers true if aValue is in the string list of white-space separated values
// aValueList.
bool ChangeStyleTransaction::ValueIncludes(const nsAString& aValueList,
                                           const nsAString& aValue) {
  nsAutoString valueList(aValueList);
  bool result = false;

  // put an extra null at the end
  valueList.Append(kNullCh);

  char16_t* value = ToNewUnicode(aValue);
  char16_t* start = valueList.BeginWriting();
  char16_t* end = start;

  while (kNullCh != *start) {
    while (kNullCh != *start && nsCRT::IsAsciiSpace(*start)) {
      // skip leading space
      start++;
    }
    end = start;

    while (kNullCh != *end && !nsCRT::IsAsciiSpace(*end)) {
      // look for space or end
      end++;
    }
    // end string here
    *end = kNullCh;

    if (start < end) {
      if (nsDependentString(value).Equals(
              nsDependentString(start), nsCaseInsensitiveStringComparator())) {
        result = true;
        break;
      }
    }
    start = ++end;
  }
  free(value);
  return result;
}

// Removes the value aRemoveValue from the string list of white-space separated
// values aValueList
void ChangeStyleTransaction::RemoveValueFromListOfValues(
    nsAString& aValues, const nsAString& aRemoveValue) {
  nsAutoString classStr(aValues);
  nsAutoString outString;
  // put an extra null at the end
  classStr.Append(kNullCh);

  char16_t* start = classStr.BeginWriting();
  char16_t* end = start;

  while (kNullCh != *start) {
    while (kNullCh != *start && nsCRT::IsAsciiSpace(*start)) {
      // skip leading space
      start++;
    }
    end = start;

    while (kNullCh != *end && !nsCRT::IsAsciiSpace(*end)) {
      // look for space or end
      end++;
    }
    // end string here
    *end = kNullCh;

    if (start < end && !aRemoveValue.Equals(start)) {
      outString.Append(start);
      outString.Append(char16_t(' '));
    }

    start = ++end;
  }
  aValues.Assign(outString);
}

NS_IMETHODIMP
ChangeStyleTransaction::DoTransaction() {
  nsCOMPtr<nsStyledElement> inlineStyles = do_QueryInterface(mElement);
  NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsICSSDeclaration> cssDecl = inlineStyles->Style();

  // FIXME(bug 1606994): Using atoms forces a string copy here which is not
  // great.
  nsAutoCString propertyNameString;
  mProperty->ToUTF8String(propertyNameString);

  mUndoAttributeWasSet = mElement->HasAttr(kNameSpaceID_None, nsGkAtoms::style);

  nsAutoString values;
  nsresult rv = cssDecl->GetPropertyValue(propertyNameString, values);
  NS_ENSURE_SUCCESS(rv, rv);
  mUndoValue.Assign(values);

  // Does this property accept more than one value? (bug 62682)
  bool multiple = AcceptsMoreThanOneValue(*mProperty);

  if (mRemoveProperty) {
    nsAutoString returnString;
    if (multiple) {
      // Let's remove only the value we have to remove and not the others
      RemoveValueFromListOfValues(values, NS_LITERAL_STRING("none"));
      RemoveValueFromListOfValues(values, mValue);
      if (values.IsEmpty()) {
        ErrorResult res;
        cssDecl->RemoveProperty(propertyNameString, returnString, res);
        if (NS_WARN_IF(res.Failed())) {
          return res.StealNSResult();
        }
      } else {
        ErrorResult res;
        nsAutoString priority;
        cssDecl->GetPropertyPriority(propertyNameString, priority);
        cssDecl->SetProperty(propertyNameString, NS_ConvertUTF16toUTF8(values),
                             priority, res);
        if (NS_WARN_IF(res.Failed())) {
          return res.StealNSResult();
        }
      }
    } else {
      ErrorResult res;
      cssDecl->RemoveProperty(propertyNameString, returnString, res);
      if (NS_WARN_IF(res.Failed())) {
        return res.StealNSResult();
      }
    }
  } else {
    nsAutoString priority;
    cssDecl->GetPropertyPriority(propertyNameString, priority);
    if (multiple) {
      // Let's add the value we have to add to the others
      AddValueToMultivalueProperty(values, mValue);
    } else {
      values.Assign(mValue);
    }
    ErrorResult res;
    cssDecl->SetProperty(propertyNameString, NS_ConvertUTF16toUTF8(values),
                         priority, res);
    if (NS_WARN_IF(res.Failed())) {
      return res.StealNSResult();
    }
  }

  // Let's be sure we don't keep an empty style attribute
  uint32_t length = cssDecl->Length();
  if (!length) {
    rv = mElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, true);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    mRedoAttributeWasSet = true;
  }

  return cssDecl->GetPropertyValue(propertyNameString, mRedoValue);
}

nsresult ChangeStyleTransaction::SetStyle(bool aAttributeWasSet,
                                          nsAString& aValue) {
  if (aAttributeWasSet) {
    // The style attribute was not empty, let's recreate the declaration
    nsAutoCString propertyNameString;
    mProperty->ToUTF8String(propertyNameString);

    nsCOMPtr<nsStyledElement> inlineStyles = do_QueryInterface(mElement);
    NS_ENSURE_TRUE(inlineStyles, NS_ERROR_NULL_POINTER);
    nsCOMPtr<nsICSSDeclaration> cssDecl = inlineStyles->Style();

    ErrorResult rv;
    if (aValue.IsEmpty()) {
      // An empty value means we have to remove the property
      nsAutoString returnString;
      cssDecl->RemoveProperty(propertyNameString, returnString, rv);
      if (NS_WARN_IF(rv.Failed())) {
        return rv.StealNSResult();
      }
    }
    // Let's recreate the declaration as it was
    nsAutoString priority;
    cssDecl->GetPropertyPriority(propertyNameString, priority);
    cssDecl->SetProperty(propertyNameString, NS_ConvertUTF16toUTF8(aValue),
                         priority, rv);
    return rv.StealNSResult();
  }
  return mElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::style, true);
}

NS_IMETHODIMP
ChangeStyleTransaction::UndoTransaction() {
  return SetStyle(mUndoAttributeWasSet, mUndoValue);
}

NS_IMETHODIMP
ChangeStyleTransaction::RedoTransaction() {
  return SetStyle(mRedoAttributeWasSet, mRedoValue);
}

// True if the CSS property accepts more than one value
bool ChangeStyleTransaction::AcceptsMoreThanOneValue(nsAtom& aCSSProperty) {
  return &aCSSProperty == nsGkAtoms::text_decoration;
}

// Adds the value aNewValue to the list of white-space separated values aValues
void ChangeStyleTransaction::AddValueToMultivalueProperty(
    nsAString& aValues, const nsAString& aNewValue) {
  if (aValues.IsEmpty() || aValues.LowerCaseEqualsLiteral("none")) {
    aValues.Assign(aNewValue);
  } else if (!ValueIncludes(aValues, aNewValue)) {
    // We already have another value but not this one; add it
    aValues.Append(char16_t(' '));
    aValues.Append(aNewValue);
  }
}

}  // namespace mozilla
