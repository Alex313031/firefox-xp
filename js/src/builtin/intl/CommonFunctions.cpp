/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Operations used to implement multiple Intl.* classes. */

#include "builtin/intl/CommonFunctions.h"

#include "mozilla/Assertions.h"
#include "mozilla/TextUtils.h"

#include <algorithm>

#include "jsfriendapi.h"  // for GetErrorMessage, JSMSG_INTERNAL_INTL_ERROR

#include "gc/GCEnum.h"
#include "gc/Zone.h"
#include "gc/ZoneAllocator.h"
#include "js/Value.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/SelfHosting.h"
#include "vm/Stack.h"

#include "vm/JSObject-inl.h"

bool js::intl::InitializeObject(JSContext* cx, JS::Handle<JSObject*> obj,
                                JS::Handle<PropertyName*> initializer,
                                JS::Handle<JS::Value> locales,
                                JS::Handle<JS::Value> options) {
  FixedInvokeArgs<3> args(cx);

  args[0].setObject(*obj);
  args[1].set(locales);
  args[2].set(options);

  RootedValue ignored(cx);
  if (!CallSelfHostedFunction(cx, initializer, JS::NullHandleValue, args,
                              &ignored)) {
    return false;
  }

  MOZ_ASSERT(ignored.isUndefined(),
             "Unexpected return value from non-legacy Intl object initializer");
  return true;
}

bool js::intl::LegacyInitializeObject(JSContext* cx, JS::Handle<JSObject*> obj,
                                      JS::Handle<PropertyName*> initializer,
                                      JS::Handle<JS::Value> thisValue,
                                      JS::Handle<JS::Value> locales,
                                      JS::Handle<JS::Value> options,
                                      DateTimeFormatOptions dtfOptions,
                                      JS::MutableHandle<JS::Value> result) {
  FixedInvokeArgs<5> args(cx);

  args[0].setObject(*obj);
  args[1].set(thisValue);
  args[2].set(locales);
  args[3].set(options);
  args[4].setBoolean(dtfOptions == DateTimeFormatOptions::EnableMozExtensions);

  if (!CallSelfHostedFunction(cx, initializer, NullHandleValue, args, result)) {
    return false;
  }

  MOZ_ASSERT(result.isObject(),
             "Legacy Intl object initializer must return an object");
  return true;
}

JSObject* js::intl::GetInternalsObject(JSContext* cx,
                                       JS::Handle<JSObject*> obj) {
  FixedInvokeArgs<1> args(cx);

  args[0].setObject(*obj);

  RootedValue v(cx);
  if (!js::CallSelfHostedFunction(cx, cx->names().getInternals, NullHandleValue,
                                  args, &v)) {
    return nullptr;
  }

  return &v.toObject();
}

void js::intl::ReportInternalError(JSContext* cx) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_INTERNAL_INTL_ERROR);
}

const js::intl::OldStyleLanguageTagMapping
    js::intl::oldStyleLanguageTagMappings[] = {
        {"pa-PK", "pa-Arab-PK"}, {"zh-CN", "zh-Hans-CN"},
        {"zh-HK", "zh-Hant-HK"}, {"zh-SG", "zh-Hans-SG"},
        {"zh-TW", "zh-Hant-TW"},
};

js::UniqueChars js::intl::EncodeLocale(JSContext* cx, JSString* locale) {
  MOZ_ASSERT(locale->length() > 0);

  js::UniqueChars chars = EncodeAscii(cx, locale);

#ifdef DEBUG
  // Ensure the returned value contains only valid BCP 47 characters.
  // (Lambdas can't be placed inside MOZ_ASSERT, so move the checks in an
  // #ifdef block.)
  if (chars) {
    auto alnumOrDash = [](char c) {
      return mozilla::IsAsciiAlphanumeric(c) || c == '-';
    };
    MOZ_ASSERT(mozilla::IsAsciiAlpha(chars[0]));
    MOZ_ASSERT(
        std::all_of(chars.get(), chars.get() + locale->length(), alnumOrDash));
  }
#endif

  return chars;
}

void js::intl::AddICUCellMemory(JSObject* obj, size_t nbytes) {
  // Account the (estimated) number of bytes allocated by an ICU object against
  // the JSObject's zone.
  AddCellMemory(obj, nbytes, MemoryUse::ICUObject);

  // Manually trigger malloc zone GCs in case there's memory pressure and
  // collecting any unreachable Intl objects could free ICU allocated memory.
  //
  // (ICU allocations use the system memory allocator, so we can't rely on
  // ZoneAllocPolicy to call |maybeMallocTriggerZoneGC|.)
  obj->zone()->maybeMallocTriggerZoneGC();
}

void js::intl::RemoveICUCellMemory(JSFreeOp* fop, JSObject* obj,
                                   size_t nbytes) {
  fop->removeCellMemory(obj, nbytes, MemoryUse::ICUObject);
}
