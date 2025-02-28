/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBCursorType.h"

namespace mozilla {
namespace dom {
CommonCursorDataBase::CommonCursorDataBase(Key aKey) : mKey{std::move(aKey)} {}

IndexCursorDataBase::IndexCursorDataBase(Key aKey, Key aLocaleAwareKey,
                                         Key aObjectStoreKey)
    : CommonCursorDataBase{std::move(aKey)},
      mLocaleAwareKey{std::move(aLocaleAwareKey)},
      mObjectStoreKey{std::move(aObjectStoreKey)} {}

ValueCursorDataBase::ValueCursorDataBase(StructuredCloneReadInfo&& aCloneInfo)
    : mCloneInfo{std::move(aCloneInfo)} {}

CursorData<IDBCursorType::ObjectStore>::CursorData(
    Key aKey, StructuredCloneReadInfo&& aCloneInfo)
    : ObjectStoreCursorDataBase{std::move(aKey)},
      ValueCursorDataBase{std::move(aCloneInfo)} {}

CursorData<IDBCursorType::Index>::CursorData(
    Key aKey, Key aLocaleAwareKey, Key aObjectStoreKey,
    StructuredCloneReadInfo&& aCloneInfo)
    : IndexCursorDataBase{std::move(aKey), std::move(aLocaleAwareKey),
                          std::move(aObjectStoreKey)},
      ValueCursorDataBase{std::move(aCloneInfo)} {}

}  // namespace dom
}  // namespace mozilla
