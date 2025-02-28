/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_IsGivenTypeObject_inl_h
#define vm_IsGivenTypeObject_inl_h

#include "js/Class.h"       // js::ESClass
#include "js/RootingAPI.h"  // JS::Handle

#include "vm/JSContext-inl.h"  // JSContext::check

namespace js {

inline bool IsGivenTypeObject(JSContext* cx, JS::Handle<JSObject*> obj,
                              const ESClass& typeClass, bool* isType) {
  cx->check(obj);

  ESClass cls;
  if (!GetBuiltinClass(cx, obj, &cls)) {
    return false;
  }

  *isType = cls == typeClass;
  return true;
}

}  // namespace js

#endif  // vm_IsGivenTypeObject_inl_h
