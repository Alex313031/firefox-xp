/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Useful extensions to UniquePtr. */

#ifndef mozilla_UniquePtrExtensions_h
#define mozilla_UniquePtrExtensions_h

#include "mozilla/fallible.h"
#include "mozilla/UniquePtr.h"

#ifdef XP_WIN
#  include <cstdint>
#endif

namespace mozilla {

/**
 * MakeUniqueFallible works exactly like MakeUnique, except that the memory
 * allocation performed is done fallibly, i.e. it can return nullptr.
 */
template <typename T, typename... Args>
typename detail::UniqueSelector<T>::SingleObject MakeUniqueFallible(
    Args&&... aArgs) {
  return UniquePtr<T>(new (fallible) T(std::forward<Args>(aArgs)...));
}

template <typename T>
typename detail::UniqueSelector<T>::UnknownBound MakeUniqueFallible(
    decltype(sizeof(int)) aN) {
  typedef typename RemoveExtent<T>::Type ArrayType;
  return UniquePtr<T>(new (fallible) ArrayType[aN]());
}

template <typename T, typename... Args>
typename detail::UniqueSelector<T>::KnownBound MakeUniqueFallible(
    Args&&... aArgs) = delete;

namespace detail {

template <typename T>
struct FreePolicy {
  void operator()(const void* ptr) { free(const_cast<void*>(ptr)); }
};

#if defined(XP_WIN)
// Can't include <windows.h> to get the actual definition of HANDLE
// because of namespace pollution.
typedef void* FileHandleType;
#elif defined(XP_UNIX)
typedef int FileHandleType;
#else
#  error "Unsupported OS?"
#endif

struct FileHandleHelper {
  MOZ_IMPLICIT FileHandleHelper(FileHandleType aHandle) : mHandle(aHandle) {}

  MOZ_IMPLICIT constexpr FileHandleHelper(std::nullptr_t)
      : mHandle(kInvalidHandle) {}

  bool operator!=(std::nullptr_t) const {
#ifdef XP_WIN
    // Windows uses both nullptr and INVALID_HANDLE_VALUE (-1 cast to
    // HANDLE) in different situations, but nullptr is more reliably
    // null while -1 is also valid input to some calls that take
    // handles.  So class considers both to be null (since neither
    // should be closed) but default-constructs as nullptr.
    if (mHandle == (void*)-1) {
      return false;
    }
#endif
    return mHandle != kInvalidHandle;
  }

  operator FileHandleType() const { return mHandle; }

#ifdef XP_WIN
  // NSPR uses an integer type for PROsfd, so this conversion is
  // provided for working with it without needing reinterpret casts
  // everywhere.
  operator std::intptr_t() const {
    return reinterpret_cast<std::intptr_t>(mHandle);
  }
#endif

  // When there's only one user-defined conversion operator, the
  // compiler will use that to derive equality, but that doesn't work
  // when the conversion is ambiguoug (the XP_WIN case above).
  bool operator==(const FileHandleHelper& aOther) const {
    return mHandle == aOther.mHandle;
  }

 private:
  FileHandleType mHandle;

#ifdef XP_WIN
  // See above for why this is nullptr.  (Also, INVALID_HANDLE_VALUE
  // can't be expressed as a constexpr.)
  static constexpr FileHandleType kInvalidHandle = nullptr;
#else
  static constexpr FileHandleType kInvalidHandle = -1;
#endif
};

struct FileHandleDeleter {
  typedef FileHandleHelper pointer;
  MFBT_API void operator()(FileHandleHelper aHelper);
};

}  // namespace detail

template <typename T>
using UniqueFreePtr = UniquePtr<T, detail::FreePolicy<T>>;

// A RAII class for the OS construct used for open files and similar
// objects: a file descriptor on Unix or a handle on Windows.
using UniqueFileHandle =
    UniquePtr<detail::FileHandleType, detail::FileHandleDeleter>;

}  // namespace mozilla

#endif  // mozilla_UniquePtrExtensions_h
