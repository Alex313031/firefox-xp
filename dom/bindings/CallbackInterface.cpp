/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CallbackInterface.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace dom {

bool CallbackInterface::GetCallableProperty(
    JSContext* cx, JS::Handle<jsid> aPropId,
    JS::MutableHandle<JS::Value> aCallable) {
  if (!JS_GetPropertyById(cx, CallbackKnownNotGray(), aPropId, aCallable)) {
    return false;
  }
  if (!aCallable.isObject() || !JS::IsCallable(&aCallable.toObject())) {
    JS::RootedString propId(cx, JSID_TO_STRING(aPropId));
    JS::UniqueChars propName = JS_EncodeStringToUTF8(cx, propId);
    nsPrintfCString description("Property '%s'", propName.get());
    ThrowErrorMessage<MSG_NOT_CALLABLE>(cx, description.get());
    return false;
  }

  return true;
}

}  // namespace dom
}  // namespace mozilla
