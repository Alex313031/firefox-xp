/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_DBAction_h
#define mozilla_dom_cache_DBAction_h

#include "mozilla/dom/cache/Action.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"

class mozIStorageConnection;
class nsIFile;

namespace mozilla {
namespace dom {
namespace cache {

nsresult OpenDBConnection(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                          mozIStorageConnection** aConnOut);

class DBAction : public Action {
 protected:
  // The mode specifies whether the database should already exist or if its
  // ok to create a new database.
  enum Mode { Existing, Create };

  explicit DBAction(Mode aMode);

  // Action objects are deleted through their base pointer
  virtual ~DBAction();

  // Just as the resolver must be ref'd until resolve, you may also
  // ref the DB connection.  The connection can only be referenced from the
  // target thread and must be released upon resolve.
  virtual void RunWithDBOnTarget(Resolver* aResolver,
                                 const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                                 mozIStorageConnection* aConn) = 0;

 private:
  void RunOnTarget(Resolver* aResolver, const QuotaInfo& aQuotaInfo,
                   Data* aOptionalData) override;

  nsresult OpenConnection(const QuotaInfo& aQuotaInfo, nsIFile* aQuotaDir,
                          mozIStorageConnection** aConnOut);

  const Mode mMode;
};

class SyncDBAction : public DBAction {
 protected:
  explicit SyncDBAction(Mode aMode);

  // Action objects are deleted through their base pointer
  virtual ~SyncDBAction();

  virtual nsresult RunSyncWithDBOnTarget(const QuotaInfo& aQuotaInfo,
                                         nsIFile* aDBDir,
                                         mozIStorageConnection* aConn) = 0;

 private:
  virtual void RunWithDBOnTarget(Resolver* aResolver,
                                 const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                                 mozIStorageConnection* aConn) override;
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_DBAction_h
