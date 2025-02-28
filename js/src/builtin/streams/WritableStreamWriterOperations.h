/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream writer abstract operations. */

#ifndef builtin_streams_WritableStreamWriterOperations_h
#define builtin_streams_WritableStreamWriterOperations_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/RootingAPI.h"  // JS::{,Mutable}Handle
#include "js/Value.h"       // JS::Value

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;

namespace js {

class PromiseObject;
class WritableStreamDefaultWriter;

extern JSObject* WritableStreamDefaultWriterAbort(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    JS::Handle<JS::Value> reason);

extern JSObject* WritableStreamDefaultWriterClose(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter);

extern MOZ_MUST_USE bool WritableStreamDefaultWriterEnsureClosedPromiseRejected(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    JS::Handle<JS::Value> error);

extern MOZ_MUST_USE bool WritableStreamDefaultWriterEnsureReadyPromiseRejected(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    JS::Handle<JS::Value> error);

extern MOZ_MUST_USE bool WritableStreamDefaultWriterGetDesiredSize(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    JS::MutableHandle<JS::Value> size);

extern MOZ_MUST_USE bool WritableStreamDefaultWriterRelease(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter);

extern PromiseObject* WritableStreamDefaultWriterWrite(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    JS::Handle<JS::Value> chunk);

}  // namespace js

#endif  // builtin_streams_WritableStreamWriterOperations_h
