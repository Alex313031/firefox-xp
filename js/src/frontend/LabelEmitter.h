/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_LabelEmitter_h
#define frontend_LabelEmitter_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE, MOZ_STACK_CLASS
#include "mozilla/Maybe.h"       // Maybe

#include "jstypes.h"                             // JS_PUBLIC_API
#include "frontend/BytecodeControlStructures.h"  // LabelControl
#include "frontend/BytecodeOffset.h"             // BytecodeOffset
#include "frontend/JumpList.h"                   // JumpList
#include "js/TypeDecls.h"                        // JSAtom

class JS_PUBLIC_API JSAtom;

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting labeled statement.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `label: expr;`
//     LabelEmitter le(this);
//     le.emitLabel(name_of_label);
//     emit(expr);
//     le.emitEnd();
//
class MOZ_STACK_CLASS LabelEmitter {
  BytecodeEmitter* bce_;

  mozilla::Maybe<LabelControl> controlInfo_;

#ifdef DEBUG
  // The state of this emitter.
  //
  // +-------+ emitLabel +-------+ emitEnd +-----+
  // | Start |---------->| Label |-------->| End |
  // +-------+           +-------+         +-----+
  enum class State {
    // The initial state.
    Start,

    // After calling emitLabel.
    Label,

    // After calling emitEnd.
    End
  };
  State state_ = State::Start;
#endif

 public:
  explicit LabelEmitter(BytecodeEmitter* bce) : bce_(bce) {}

  void emitLabel(HandleAtom name);
  MOZ_MUST_USE bool emitEnd();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_LabelEmitter_h */
