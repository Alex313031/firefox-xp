/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_serializationhelpers_h__
#define mozilla_dom_indexeddb_serializationhelpers_h__

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/indexedDB/Key.h"
#include "mozilla/dom/indexedDB/KeyPath.h"
#include "mozilla/dom/IDBCursor.h"
#include "mozilla/dom/IDBTransaction.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::indexedDB::StructuredCloneFile::FileType>
    : public ContiguousEnumSerializer<
          mozilla::dom::indexedDB::StructuredCloneFile::FileType,
          mozilla::dom::indexedDB::StructuredCloneFile::eBlob,
          mozilla::dom::indexedDB::StructuredCloneFile::eEndGuard> {};

template <>
struct ParamTraits<mozilla::dom::indexedDB::Key> {
  typedef mozilla::dom::indexedDB::Key paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mBuffer);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mBuffer);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.mBuffer, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::indexedDB::KeyPath::KeyPathType>
    : public ContiguousEnumSerializer<
          mozilla::dom::indexedDB::KeyPath::KeyPathType,
          mozilla::dom::indexedDB::KeyPath::NONEXISTENT,
          mozilla::dom::indexedDB::KeyPath::ENDGUARD> {};

template <>
struct ParamTraits<mozilla::dom::indexedDB::KeyPath> {
  typedef mozilla::dom::indexedDB::KeyPath paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mStrings);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mType) &&
           ReadParam(aMsg, aIter, &aResult->mStrings);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {
    LogParam(aParam.mStrings, aLog);
  }
};

template <>
struct ParamTraits<mozilla::dom::IDBCursor::Direction>
    : public ContiguousEnumSerializer<
          mozilla::dom::IDBCursor::Direction,
          mozilla::dom::IDBCursor::Direction::Next,
          mozilla::dom::IDBCursor::Direction::EndGuard_> {};

template <>
struct ParamTraits<mozilla::dom::IDBTransaction::Mode>
    : public ContiguousEnumSerializer<
          mozilla::dom::IDBTransaction::Mode,
          mozilla::dom::IDBTransaction::Mode::ReadOnly,
          mozilla::dom::IDBTransaction::Mode::Invalid> {};

}  // namespace IPC

#endif  // mozilla_dom_indexeddb_serializationhelpers_h__
