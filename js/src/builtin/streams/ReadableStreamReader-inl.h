/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_streams_ReadableStreamReader_inl_h
#define builtin_streams_ReadableStreamReader_inl_h

#include "builtin/streams/ReadableStreamReader.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsfriendapi.h"  // JS_IsDeadWrapper

#include "builtin/streams/ReadableStream.h"  // js::ReadableStream
#include "js/Proxy.h"                        // js::IsProxy
#include "js/RootingAPI.h"                   // JS::Handle
#include "vm/NativeObject.h"                 // js::NativeObject::getFixedSlot

#include "vm/Compartment-inl.h"  // js::UnwrapInternalSlot

namespace js {

/**
 * Returns the stream associated with the given reader.
 */
inline MOZ_MUST_USE ReadableStream* UnwrapStreamFromReader(
    JSContext* cx, JS::Handle<ReadableStreamReader*> reader) {
  MOZ_ASSERT(reader->hasStream());
  return UnwrapInternalSlot<ReadableStream>(cx, reader,
                                            ReadableStreamReader::Slot_Stream);
}

/**
 * Returns the reader associated with the given stream.
 *
 * Must only be called on ReadableStreams that already have a reader
 * associated with them.
 *
 * If the reader is a wrapper, it will be unwrapped, so the result might not be
 * an object from the currently active compartment.
 */
inline MOZ_MUST_USE ReadableStreamReader* UnwrapReaderFromStream(
    JSContext* cx, JS::Handle<ReadableStream*> stream) {
  return UnwrapInternalSlot<ReadableStreamReader>(cx, stream,
                                                  ReadableStream::Slot_Reader);
}

inline MOZ_MUST_USE ReadableStreamReader* UnwrapReaderFromStreamNoThrow(
    ReadableStream* stream) {
  JSObject* readerObj =
      &stream->getFixedSlot(ReadableStream::Slot_Reader).toObject();
  if (IsProxy(readerObj)) {
    if (JS_IsDeadWrapper(readerObj)) {
      return nullptr;
    }

    readerObj = readerObj->maybeUnwrapAs<ReadableStreamReader>();
    if (!readerObj) {
      return nullptr;
    }
  }

  return &readerObj->as<ReadableStreamReader>();
}

}  // namespace js

#endif  // builtin_streams_ReadableStreamReader_inl_h
