/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dbg_Source_h
#define dbg_Source_h

#include "jsapi.h"

#include "NamespaceImports.h"   // for Value, HandleObject, CallArgs
#include "debugger/Debugger.h"  // for DebuggerSourceReferent
#include "gc/Rooting.h"         // for HandleNativeObject
#include "vm/NativeObject.h"    // for NativeObject

namespace js {
class GlobalObject;
}

namespace js {

class DebuggerSource : public NativeObject {
 public:
  static const JSClass class_;

  enum {
    OWNER_SLOT,
    TEXT_SLOT,
    RESERVED_SLOTS,
  };

  static NativeObject* initClass(JSContext* cx, Handle<GlobalObject*> global,
                                 HandleObject debugCtor);
  static DebuggerSource* create(JSContext* cx, HandleObject proto,
                                Handle<DebuggerSourceReferent> referent,
                                HandleNativeObject debugger);

  void trace(JSTracer* trc);

  using ReferentVariant = DebuggerSourceReferent;

  NativeObject* getReferentRawObject() const;
  DebuggerSourceReferent getReferent() const;

  static DebuggerSource* check(JSContext* cx, HandleValue v);
  static bool construct(JSContext* cx, unsigned argc, Value* vp);

  struct CallData;

 private:
  static const JSClassOps classOps_;

  static const JSPropertySpec properties_[];
  static const JSFunctionSpec methods_[];
};

} /* namespace js */

#endif /* dbg_Source_h */
