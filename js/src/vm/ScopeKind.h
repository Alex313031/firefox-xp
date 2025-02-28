/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ScopeKind_h
#define vm_ScopeKind_h

#include <stdint.h>

namespace js {

enum class ScopeKind : uint8_t {
  // FunctionScope
  Function,

  // VarScope
  FunctionBodyVar,

  // LexicalScope
  Lexical,
  SimpleCatch,
  Catch,
  NamedLambda,
  StrictNamedLambda,
  FunctionLexical,

  // WithScope
  With,

  // EvalScope
  Eval,
  StrictEval,

  // GlobalScope
  Global,
  NonSyntactic,

  // ModuleScope
  Module,

  // WasmInstanceScope
  WasmInstance,

  // WasmFunctionScope
  WasmFunction
};

}  // namespace js

#endif  // vm_ScopeKind_hs
