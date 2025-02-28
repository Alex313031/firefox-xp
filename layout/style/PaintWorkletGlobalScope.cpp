/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintWorkletGlobalScope.h"

#include "mozilla/dom/WorkletPrincipals.h"
#include "mozilla/dom/PaintWorkletGlobalScopeBinding.h"
#include "mozilla/dom/FunctionBinding.h"
#include "PaintWorkletImpl.h"

namespace mozilla {
namespace dom {

PaintWorkletGlobalScope::PaintWorkletGlobalScope(PaintWorkletImpl* aImpl)
    : mImpl(aImpl) {}

bool PaintWorkletGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  JS::RealmOptions options;
  JS::AutoHoldPrincipals principals(aCx, new WorkletPrincipals(mImpl));
  return PaintWorkletGlobalScope_Binding::Wrap(
      aCx, this, this, options, principals.get(), true, aReflector);
}

void PaintWorkletGlobalScope::RegisterPaint(const nsAString& aType,
                                            VoidFunction& aProcessorCtor) {
  // Nothing to do here, yet.
}

WorkletImpl* PaintWorkletGlobalScope::Impl() const { return mImpl; }

}  // namespace dom
}  // namespace mozilla
