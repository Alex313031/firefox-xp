/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shell/jsrtfuzzing/jsrtfuzzing.h"

#include "mozilla/ArrayUtils.h"  // mozilla::ArrayLength
#include "mozilla/Assertions.h"  // MOZ_CRASH
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <stdio.h>  // fflush, fprintf, fputs

#include "FuzzerDefs.h"
#include "FuzzingInterface.h"
#include "jsapi.h"  // JS_ClearPendingException, JS_IsExceptionPending, JS_SetProperty

#include "js/CompilationAndEvaluation.h"  // JS::Evaluate
#include "js/CompileOptions.h"            // JS::CompileOptions
#include "js/RootingAPI.h"                // JS::Rooted
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "js/Value.h"                     // JS::Value
#include "shell/jsshell.h"  // js::shell::{reportWarnings,PrintStackTrace,sArg{c,v}}
#include "vm/Interpreter.h"
#include "vm/JSContext.h"  // js::PrintError
#include "vm/TypedArrayObject.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/JSContext-inl.h"

static JSContext* gCx = nullptr;
static std::string gFuzzModuleName;

static void CrashOnPendingException() {
  if (JS_IsExceptionPending(gCx)) {
    JS::Rooted<JS::Value> exn(gCx);
    (void)JS_GetPendingException(gCx, &exn);
    JS::Rooted<JSObject*> stack(gCx, JS::GetPendingExceptionStack(gCx));

    JS_ClearPendingException(gCx);

    js::ErrorReport report(gCx);
    if (!report.init(gCx, exn, js::ErrorReport::WithSideEffects)) {
      fprintf(stderr, "out of memory initializing ErrorReport\n");
      fflush(stderr);
    } else {
      js::PrintError(gCx, stderr, report.toStringResult(), report.report(),
                     js::shell::reportWarnings);
      if (!js::shell::PrintStackTrace(gCx, stack)) {
        fputs("(Unable to print stack trace)\n", stderr);
      }
    }

    MOZ_CRASH("Unhandled exception from JS runtime!");
  }
}

int js::shell::FuzzJSRuntimeStart(JSContext* cx, int* argc, char*** argv) {
  gCx = cx;
  gFuzzModuleName = getenv("FUZZER");

  int ret = FuzzJSRuntimeInit(argc, argv);
  if (ret) {
    fprintf(stderr, "Fuzzing Interface: Error: Initialize callback failed\n");
    return ret;
  }

#ifdef LIBFUZZER
  fuzzer::FuzzerDriver(&shell::sArgc, &shell::sArgv, FuzzJSRuntimeFuzz);
#elif __AFL_COMPILER
  MOZ_CRASH("AFL is unsupported for JS runtime fuzzing integration");
#endif
  return 0;
}

int js::shell::FuzzJSRuntimeInit(int* argc, char*** argv) {
  JS::Rooted<JS::Value> v(gCx);
  JS::CompileOptions opts(gCx);

  // Load the fuzzing module specified in the FUZZER environment variable
  JS::EvaluateUtf8Path(gCx, opts, gFuzzModuleName.c_str(), &v);

  // Any errors while loading the fuzzing module should be fatal
  CrashOnPendingException();

  return 0;
}

int js::shell::FuzzJSRuntimeFuzz(const uint8_t* buf, size_t size) {
  if (!size) {
    return 0;
  }

  JS::Rooted<JSObject*> arr(gCx, JS_NewUint8ClampedArray(gCx, size));
  if (!arr) {
    MOZ_CRASH("OOM");
  }

  do {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    uint8_t* data = JS_GetUint8ClampedArrayData(arr, &isShared, nogc);
    MOZ_RELEASE_ASSERT(!isShared);
    memcpy(data, buf, size);
  } while (false);

  JS::RootedValue arrVal(gCx, JS::ObjectValue(*arr));
  if (!JS_SetProperty(gCx, gCx->global(), "fuzzBuf", arrVal)) {
    MOZ_CRASH("JS_SetProperty failed");
  }

  JS::Rooted<JS::Value> v(gCx);
  JS::CompileOptions opts(gCx);

  static const char data[] = "JSFuzzIterate();";

  JS::SourceText<mozilla::Utf8Unit> srcBuf;
  if (!srcBuf.init(gCx, data, mozilla::ArrayLength(data) - 1,
                   JS::SourceOwnership::Borrowed)) {
    return 0;
  }

  JS::Evaluate(gCx, opts.setFileAndLine(__FILE__, __LINE__), srcBuf, &v);

  // The fuzzing module is required to handle any exceptions
  CrashOnPendingException();

  return 0;
}
