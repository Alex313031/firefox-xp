/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAString.h"

namespace mozilla {
namespace FilePreferences {

void InitPrefs();
void InitDirectoriesWhitelist();
bool IsBlockedUNCPath(const nsAString& aFilePath);

#ifdef XP_WIN
bool IsAllowedPath(const nsAString& aFilePath);
#else
bool IsAllowedPath(const nsACString& aFilePath);
#endif

namespace testing {

void SetBlockUNCPaths(bool aBlock);
void AddDirectoryToWhitelist(nsAString const& aPath);
bool NormalizePath(nsAString const& aPath, nsAString& aNormalized);

}  // namespace testing

}  // namespace FilePreferences
}  // namespace mozilla
