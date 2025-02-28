/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_fileinfo_h__
#define mozilla_dom_indexeddb_fileinfo_h__

#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

class FileManager;

class FileInfo {
  friend class FileManager;

  ThreadSafeAutoRefCnt mRefCnt;
  ThreadSafeAutoRefCnt mDBRefCnt;
  ThreadSafeAutoRefCnt mSliceRefCnt;

  RefPtr<FileManager> mFileManager;

 public:
  class CustomCleanupCallback;

  static FileInfo* Create(FileManager* aFileManager, int64_t aId);

  explicit FileInfo(FileManager* aFileManager);

  void AddRef() { UpdateReferences(mRefCnt, 1); }

  void Release(CustomCleanupCallback* aCustomCleanupCallback = nullptr) {
    UpdateReferences(mRefCnt, -1, aCustomCleanupCallback);
  }

  void UpdateDBRefs(int32_t aDelta) { UpdateReferences(mDBRefCnt, aDelta); }

  void UpdateSliceRefs(int32_t aDelta) {
    UpdateReferences(mSliceRefCnt, aDelta);
  }

  void GetReferences(int32_t* aRefCnt, int32_t* aDBRefCnt,
                     int32_t* aSliceRefCnt);

  FileManager* Manager() const { return mFileManager; }

  virtual int64_t Id() const = 0;

  static nsCOMPtr<nsIFile> GetFileForFileInfo(FileInfo* aFileInfo);

 protected:
  virtual ~FileInfo() = default;

 private:
  void UpdateReferences(
      ThreadSafeAutoRefCnt& aRefCount, int32_t aDelta,
      CustomCleanupCallback* aCustomCleanupCallback = nullptr);

  bool LockedClearDBRefs();

  void Cleanup();
};

class NS_NO_VTABLE FileInfo::CustomCleanupCallback {
 public:
  virtual nsresult Cleanup(FileManager* aFileManager, int64_t aId) = 0;

 protected:
  CustomCleanupCallback() = default;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_fileinfo_h__
