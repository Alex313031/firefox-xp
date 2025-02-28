/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULSelectControlAccessible.h"

#include "nsAccessibilityService.h"
#include "DocAccessible.h"

#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULMultSelectCntrlEl.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/KeyboardEventBinding.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible
////////////////////////////////////////////////////////////////////////////////

XULSelectControlAccessible::XULSelectControlAccessible(nsIContent* aContent,
                                                       DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eSelect;
  mSelectControl = aContent->AsElement();
}

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible: Accessible

void XULSelectControlAccessible::Shutdown() {
  mSelectControl = nullptr;
  AccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible: SelectAccessible

void XULSelectControlAccessible::SelectedItems(nsTArray<Accessible*>* aItems) {
  // For XUL multi-select control
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> xulMultiSelect =
      mSelectControl->AsXULMultiSelectControl();
  if (xulMultiSelect) {
    int32_t length = 0;
    xulMultiSelect->GetSelectedCount(&length);
    for (int32_t index = 0; index < length; index++) {
      RefPtr<dom::Element> element;
      xulMultiSelect->MultiGetSelectedItem(index, getter_AddRefs(element));
      Accessible* item = mDoc->GetAccessible(element);
      if (item) aItems->AppendElement(item);
    }
  } else {  // Single select?
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    RefPtr<dom::Element> element;
    selectControl->GetSelectedItem(getter_AddRefs(element));
    if (element) {
      Accessible* item = mDoc->GetAccessible(element);
      if (item) aItems->AppendElement(item);
    }
  }
}

Accessible* XULSelectControlAccessible::GetSelectedItem(uint32_t aIndex) {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();

  RefPtr<dom::Element> element;
  if (multiSelectControl) {
    multiSelectControl->MultiGetSelectedItem(aIndex, getter_AddRefs(element));
  } else if (aIndex == 0) {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    if (selectControl) {
      selectControl->GetSelectedItem(getter_AddRefs(element));
    }
  }

  return element && mDoc ? mDoc->GetAccessible(element) : nullptr;
}

uint32_t XULSelectControlAccessible::SelectedItemCount() {
  // For XUL multi-select control
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();
  if (multiSelectControl) {
    int32_t count = 0;
    multiSelectControl->GetSelectedCount(&count);
    return count;
  }

  // For XUL single-select control/menulist
  nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
      mSelectControl->AsXULSelectControl();
  if (selectControl) {
    int32_t index = -1;
    selectControl->GetSelectedIndex(&index);
    return (index >= 0) ? 1 : 0;
  }

  return 0;
}

bool XULSelectControlAccessible::AddItemToSelection(uint32_t aIndex) {
  Accessible* item = GetChildAt(aIndex);
  if (!item || !item->GetContent()) return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
      item->GetContent()->AsElement()->AsXULSelectControlItem();
  if (!itemElm) return false;

  bool isItemSelected = false;
  itemElm->GetSelected(&isItemSelected);
  if (isItemSelected) return true;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();

  if (multiSelectControl) {
    multiSelectControl->AddItemToSelection(itemElm);
  } else {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    if (selectControl) {
      selectControl->SetSelectedItem(item->Elm());
    }
  }

  return true;
}

bool XULSelectControlAccessible::RemoveItemFromSelection(uint32_t aIndex) {
  Accessible* item = GetChildAt(aIndex);
  if (!item || !item->GetContent()) return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
      item->GetContent()->AsElement()->AsXULSelectControlItem();
  if (!itemElm) return false;

  bool isItemSelected = false;
  itemElm->GetSelected(&isItemSelected);
  if (!isItemSelected) return true;

  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();

  if (multiSelectControl) {
    multiSelectControl->RemoveItemFromSelection(itemElm);
  } else {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    if (selectControl) {
      selectControl->SetSelectedItem(nullptr);
    }
  }

  return true;
}

bool XULSelectControlAccessible::IsItemSelected(uint32_t aIndex) {
  Accessible* item = GetChildAt(aIndex);
  if (!item || !item->GetContent()) return false;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> itemElm =
      item->GetContent()->AsElement()->AsXULSelectControlItem();
  if (!itemElm) return false;

  bool isItemSelected = false;
  itemElm->GetSelected(&isItemSelected);
  return isItemSelected;
}

bool XULSelectControlAccessible::UnselectAll() {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();
  if (multiSelectControl) {
    multiSelectControl->ClearSelection();
  } else {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    if (selectControl) {
      selectControl->SetSelectedIndex(-1);
    }
  }

  return true;
}

bool XULSelectControlAccessible::SelectAll() {
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();
  if (multiSelectControl) {
    multiSelectControl->SelectAll();
    return true;
  }

  // otherwise, don't support this method
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// XULSelectControlAccessible: Widgets

Accessible* XULSelectControlAccessible::CurrentItem() const {
  if (!mSelectControl) return nullptr;

  RefPtr<dom::Element> currentItemElm;
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      mSelectControl->AsXULMultiSelectControl();
  if (multiSelectControl) {
    multiSelectControl->GetCurrentItem(getter_AddRefs(currentItemElm));
  } else {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    if (selectControl) {
      selectControl->GetSelectedItem(getter_AddRefs(currentItemElm));
    }
  }

  if (currentItemElm) {
    DocAccessible* document = Document();
    if (document) return document->GetAccessible(currentItemElm);
  }

  return nullptr;
}

void XULSelectControlAccessible::SetCurrentItem(const Accessible* aItem) {
  if (!mSelectControl) return;

  nsCOMPtr<dom::Element> itemElm = aItem->Elm();
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelectControl =
      itemElm->AsXULMultiSelectControl();
  if (multiSelectControl) {
    multiSelectControl->SetCurrentItem(itemElm);
  } else {
    nsCOMPtr<nsIDOMXULSelectControlElement> selectControl =
        mSelectControl->AsXULSelectControl();
    if (selectControl) {
      selectControl->SetSelectedItem(itemElm);
    }
  }
}
