/* Test script cloning.
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <string.h>  // strlen

#include "jsapi.h"  // sundry symbols not moved to more-specific headers yet
#include "jsfriendapi.h"
#include "jspubtd.h"  // JS::RootedObjectVector

#include "js/CompilationAndEvaluation.h"  // JS::CompileFunction
#include "js/CompileOptions.h"            // JS::CompileOptions
#include "js/RootingAPI.h"                // JS::Rooted
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "js/TypeDecls.h"                 // JSFunction, JSObject
#include "jsapi-tests/tests.h"

BEGIN_TEST(test_cloneScript) {
  JS::RootedObject A(cx, createGlobal());
  JS::RootedObject B(cx, createGlobal());

  CHECK(A);
  CHECK(B);

  static const char source[] =
      "var i = 0;\n"
      "var sum = 0;\n"
      "while (i < 10) {\n"
      "    sum += i;\n"
      "    ++i;\n"
      "}\n"
      "(sum);\n";

  JS::RootedObject obj(cx);

  // compile for A
  {
    JSAutoRealm a(cx, A);

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, source, mozilla::ArrayLength(source) - 1,
                      JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, 1);

    JS::RootedFunction fun(cx);
    JS::RootedObjectVector emptyScopeChain(cx);
    fun = JS::CompileFunction(cx, emptyScopeChain, options, "f", 0, nullptr,
                              srcBuf);
    CHECK(fun);
    CHECK(obj = JS_GetFunctionObject(fun));
  }

  // clone into B
  {
    JSAutoRealm b(cx, B);
    CHECK(JS::CloneFunctionObject(cx, obj));
  }

  return true;
}
END_TEST(test_cloneScript)

struct Principals final : public JSPrincipals {
 public:
  Principals() { refcount = 0; }

  bool write(JSContext* cx, JSStructuredCloneWriter* writer) override {
    MOZ_ASSERT(false, "not imlemented");
    return false;
  }

  bool isSystemOrAddonPrincipal() override { return true; }
};

static void DestroyPrincipals(JSPrincipals* principals) {
  auto p = static_cast<Principals*>(principals);
  delete p;
}

BEGIN_TEST(test_cloneScriptWithPrincipals) {
  JS_InitDestroyPrincipalsCallback(cx, DestroyPrincipals);

  JS::AutoHoldPrincipals principalsA(cx, new Principals());
  JS::AutoHoldPrincipals principalsB(cx, new Principals());

  JS::RootedObject A(cx, createGlobal(principalsA.get()));
  JS::RootedObject B(cx, createGlobal(principalsB.get()));

  CHECK(A);
  CHECK(B);

  const char* argnames[] = {"arg"};
  static const char source[] = "return function() { return arg; }";

  JS::RootedObject obj(cx);

  // Compile in A
  {
    JSAutoRealm a(cx, A);

    JS::SourceText<mozilla::Utf8Unit> srcBuf;
    CHECK(srcBuf.init(cx, source, mozilla::ArrayLength(source) - 1,
                      JS::SourceOwnership::Borrowed));

    JS::CompileOptions options(cx);
    options.setFileAndLine(__FILE__, 1);

    JS::RootedFunction fun(cx);
    JS::RootedObjectVector emptyScopeChain(cx);
    fun = JS::CompileFunction(cx, emptyScopeChain, options, "f",
                              mozilla::ArrayLength(argnames), argnames, srcBuf);
    CHECK(fun);

    JSScript* script;
    CHECK(script = JS_GetFunctionScript(cx, fun));

    CHECK(JS_GetScriptPrincipals(script) == principalsA.get());
    CHECK(obj = JS_GetFunctionObject(fun));
  }

  // Clone into B
  {
    JSAutoRealm b(cx, B);
    JS::RootedObject cloned(cx);
    CHECK(cloned = JS::CloneFunctionObject(cx, obj));

    JS::RootedFunction fun(cx);
    JS::RootedValue clonedValue(cx, JS::ObjectValue(*cloned));
    CHECK(fun = JS_ValueToFunction(cx, clonedValue));

    JSScript* script;
    CHECK(script = JS_GetFunctionScript(cx, fun));

    CHECK(JS_GetScriptPrincipals(script) == principalsB.get());

    JS::RootedValue v(cx);
    JS::RootedValue arg(cx, JS::Int32Value(1));
    CHECK(JS_CallFunctionValue(cx, B, clonedValue, JS::HandleValueArray(arg),
                               &v));
    CHECK(v.isObject());

    JSObject* funobj = &v.toObject();
    CHECK(JS_ObjectIsFunction(funobj));
    CHECK(fun = JS_ValueToFunction(cx, v));
    CHECK(script = JS_GetFunctionScript(cx, fun));
    CHECK(JS_GetScriptPrincipals(script) == principalsB.get());
  }

  return true;
}
END_TEST(test_cloneScriptWithPrincipals)
