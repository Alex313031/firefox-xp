/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BytecodeAnalysis_h
#define jit_BytecodeAnalysis_h

#include "jit/JitAllocPolicy.h"
#include "js/Vector.h"
#include "vm/JSScript.h"

namespace js {
namespace jit {

// Basic information about bytecodes in the script.  Used to help baseline
// compilation.
struct BytecodeInfo {
  static const uint16_t MAX_STACK_DEPTH = 0xffffU;
  uint16_t stackDepth;
  bool initialized : 1;
  bool jumpTarget : 1;

  // If true, this is a JSOp::LoopHead op inside a catch or finally block.
  bool loopHeadInCatchOrFinally : 1;

  // True if the script has a resume offset for this bytecode op.
  bool hasResumeOffset : 1;

  void init(unsigned depth) {
    MOZ_ASSERT(depth <= MAX_STACK_DEPTH);
    MOZ_ASSERT_IF(initialized, stackDepth == depth);
    initialized = true;
    stackDepth = depth;
  }
};

class BytecodeAnalysis {
  JSScript* script_;
  Vector<BytecodeInfo, 0, JitAllocPolicy> infos_;

  bool hasTryFinally_;

 public:
  explicit BytecodeAnalysis(TempAllocator& alloc, JSScript* script);

  MOZ_MUST_USE bool init(TempAllocator& alloc);

  BytecodeInfo& info(jsbytecode* pc) {
    uint32_t pcOffset = script_->pcToOffset(pc);
    MOZ_ASSERT(infos_[pcOffset].initialized);
    return infos_[pcOffset];
  }

  BytecodeInfo* maybeInfo(jsbytecode* pc) {
    uint32_t pcOffset = script_->pcToOffset(pc);
    if (infos_[pcOffset].initialized) {
      return &infos_[pcOffset];
    }
    return nullptr;
  }

  bool hasTryFinally() const { return hasTryFinally_; }
};

// Bytecode analysis pass necessary for IonBuilder. The result is cached in
// JitScript.
struct IonBytecodeInfo;
IonBytecodeInfo AnalyzeBytecodeForIon(JSContext* cx, JSScript* script);

}  // namespace jit
}  // namespace js

#endif /* jit_BytecodeAnalysis_h */
