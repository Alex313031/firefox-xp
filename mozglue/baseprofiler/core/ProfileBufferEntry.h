/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfileBufferEntry_h
#define ProfileBufferEntry_h

#include "BaseProfileJSONWriter.h"

#include "gtest/MozGtestFriend.h"
#include "BaseProfilingCategory.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

#include <string>

namespace mozilla {
namespace baseprofiler {

class ProfilerMarker;

// NOTE!  If you add entries, you need to verify if they need to be added to the
// switch statement in DuplicateLastSample!
#define FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(MACRO)                   \
  MACRO(CategoryPair, int)                                          \
  MACRO(CollectionStart, double)                                    \
  MACRO(CollectionEnd, double)                                      \
  MACRO(Label, const char*)                                         \
  MACRO(FrameFlags, uint64_t)                                       \
  MACRO(DynamicStringFragment, char*) /* char[kNumChars], really */ \
  MACRO(JitReturnAddr, void*)                                       \
  MACRO(LineNumber, int)                                            \
  MACRO(ColumnNumber, int)                                          \
  MACRO(NativeLeafAddr, void*)                                      \
  MACRO(Marker, ProfilerMarker*)                                    \
  MACRO(Pause, double)                                              \
  MACRO(Responsiveness, double)                                     \
  MACRO(Resume, double)                                             \
  MACRO(ThreadId, int)                                              \
  MACRO(Time, double)                                               \
  MACRO(ResidentMemory, uint64_t)                                   \
  MACRO(UnsharedMemory, uint64_t)                                   \
  MACRO(CounterId, void*)                                           \
  MACRO(CounterKey, uint64_t)                                       \
  MACRO(Number, uint64_t)                                           \
  MACRO(Count, int64_t)                                             \
  MACRO(ProfilerOverheadTime, double)                               \
  MACRO(ProfilerOverheadDuration, double)

class ProfileBufferEntry {
 public:
  enum class Kind : uint8_t {
    INVALID = 0,
#define KIND(k, t) k,
    FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(KIND)
#undef KIND
        LIMIT
  };

  ProfileBufferEntry();

  // This is equal to sizeof(double), which is the largest non-char variant in
  // |u|.
  static const size_t kNumChars = 8;

 private:
  // aString must be a static string.
  ProfileBufferEntry(Kind aKind, const char* aString);
  ProfileBufferEntry(Kind aKind, char aChars[kNumChars]);
  ProfileBufferEntry(Kind aKind, void* aPtr);
  ProfileBufferEntry(Kind aKind, ProfilerMarker* aMarker);
  ProfileBufferEntry(Kind aKind, double aDouble);
  ProfileBufferEntry(Kind aKind, int64_t aInt64);
  ProfileBufferEntry(Kind aKind, uint64_t aUint64);
  ProfileBufferEntry(Kind aKind, int aInt);

 public:
#define CTOR(k, t)                            \
  static ProfileBufferEntry k(t aVal) {       \
    return ProfileBufferEntry(Kind::k, aVal); \
  }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(CTOR)
#undef CTOR

  Kind GetKind() const { return mKind; }

#define IS_KIND(k, t) \
  bool Is##k() const { return mKind == Kind::k; }
  FOR_EACH_PROFILE_BUFFER_ENTRY_KIND(IS_KIND)
#undef IS_KIND

 private:
  FRIEND_TEST(ThreadProfile, InsertOneEntry);
  FRIEND_TEST(ThreadProfile, InsertOneEntryWithTinyBuffer);
  FRIEND_TEST(ThreadProfile, InsertEntriesNoWrap);
  FRIEND_TEST(ThreadProfile, InsertEntriesWrap);
  FRIEND_TEST(ThreadProfile, MemoryMeasure);
  friend class ProfileBuffer;

  Kind mKind;
  uint8_t mStorage[kNumChars];

  const char* GetString() const;
  void* GetPtr() const;
  ProfilerMarker* GetMarker() const;
  double GetDouble() const;
  int GetInt() const;
  int64_t GetInt64() const;
  uint64_t GetUint64() const;
  void CopyCharsInto(char (&aOutArray)[kNumChars]) const;
};

// Packed layout: 1 byte for the tag + 8 bytes for the value.
static_assert(sizeof(ProfileBufferEntry) == 9, "bad ProfileBufferEntry size");

class UniqueJSONStrings {
 public:
  UniqueJSONStrings();
  explicit UniqueJSONStrings(const UniqueJSONStrings& aOther);

  void SpliceStringTableElements(SpliceableJSONWriter& aWriter) {
    aWriter.TakeAndSplice(mStringTableWriter.WriteFunc());
  }

  void WriteProperty(JSONWriter& aWriter, const char* aName, const char* aStr) {
    aWriter.IntProperty(aName, GetOrAddIndex(aStr));
  }

  void WriteElement(JSONWriter& aWriter, const char* aStr) {
    aWriter.IntElement(GetOrAddIndex(aStr));
  }

  uint32_t GetOrAddIndex(const char* aStr);

 private:
  SpliceableChunkedJSONWriter mStringTableWriter;
  HashMap<HashNumber, uint32_t> mStringHashToIndexMap;
};

class UniqueStacks {
 public:
  struct FrameKey {
    explicit FrameKey(const char* aLocation)
        : mData(NormalFrameData{std::string(aLocation), false, Nothing(),
                                Nothing()}) {}

    FrameKey(std::string&& aLocation, bool aRelevantForJS,
             const Maybe<unsigned>& aLine, const Maybe<unsigned>& aColumn,
             const Maybe<ProfilingCategoryPair>& aCategoryPair)
        : mData(NormalFrameData{aLocation, aRelevantForJS, aLine, aColumn,
                                aCategoryPair}) {}

    FrameKey(const FrameKey& aToCopy) = default;

    uint32_t Hash() const;
    bool operator==(const FrameKey& aOther) const {
      return mData == aOther.mData;
    }

    struct NormalFrameData {
      bool operator==(const NormalFrameData& aOther) const;

      std::string mLocation;
      bool mRelevantForJS;
      Maybe<unsigned> mLine;
      Maybe<unsigned> mColumn;
      Maybe<ProfilingCategoryPair> mCategoryPair;
    };
    Variant<NormalFrameData> mData;
  };

  struct FrameKeyHasher {
    using Lookup = FrameKey;

    static HashNumber hash(const FrameKey& aLookup) {
      HashNumber hash = 0;
      if (aLookup.mData.is<FrameKey::NormalFrameData>()) {
        const FrameKey::NormalFrameData& data =
            aLookup.mData.as<FrameKey::NormalFrameData>();
        if (!data.mLocation.empty()) {
          hash = AddToHash(hash, HashString(data.mLocation.c_str()));
        }
        hash = AddToHash(hash, data.mRelevantForJS);
        if (data.mLine.isSome()) {
          hash = AddToHash(hash, *data.mLine);
        }
        if (data.mColumn.isSome()) {
          hash = AddToHash(hash, *data.mColumn);
        }
        if (data.mCategoryPair.isSome()) {
          hash = AddToHash(hash, static_cast<uint32_t>(*data.mCategoryPair));
        }
      }
      return hash;
    }

    static bool match(const FrameKey& aKey, const FrameKey& aLookup) {
      return aKey == aLookup;
    }

    static void rekey(FrameKey& aKey, const FrameKey& aNewKey) {
      aKey = aNewKey;
    }
  };

  struct StackKey {
    Maybe<uint32_t> mPrefixStackIndex;
    uint32_t mFrameIndex;

    explicit StackKey(uint32_t aFrame)
        : mFrameIndex(aFrame), mHash(HashGeneric(aFrame)) {}

    StackKey(const StackKey& aPrefix, uint32_t aPrefixStackIndex,
             uint32_t aFrame)
        : mPrefixStackIndex(Some(aPrefixStackIndex)),
          mFrameIndex(aFrame),
          mHash(AddToHash(aPrefix.mHash, aFrame)) {}

    HashNumber Hash() const { return mHash; }

    bool operator==(const StackKey& aOther) const {
      return mPrefixStackIndex == aOther.mPrefixStackIndex &&
             mFrameIndex == aOther.mFrameIndex;
    }

   private:
    HashNumber mHash;
  };

  struct StackKeyHasher {
    using Lookup = StackKey;

    static HashNumber hash(const StackKey& aLookup) { return aLookup.Hash(); }

    static bool match(const StackKey& aKey, const StackKey& aLookup) {
      return aKey == aLookup;
    }

    static void rekey(StackKey& aKey, const StackKey& aNewKey) {
      aKey = aNewKey;
    }
  };

  UniqueStacks();

  // Return a StackKey for aFrame as the stack's root frame (no prefix).
  MOZ_MUST_USE StackKey BeginStack(const FrameKey& aFrame);

  // Return a new StackKey that is obtained by appending aFrame to aStack.
  MOZ_MUST_USE StackKey AppendFrame(const StackKey& aStack,
                                    const FrameKey& aFrame);

  MOZ_MUST_USE uint32_t GetOrAddFrameIndex(const FrameKey& aFrame);
  MOZ_MUST_USE uint32_t GetOrAddStackIndex(const StackKey& aStack);

  void SpliceFrameTableElements(SpliceableJSONWriter& aWriter);
  void SpliceStackTableElements(SpliceableJSONWriter& aWriter);

 private:
  void StreamNonJITFrame(const FrameKey& aFrame);
  void StreamStack(const StackKey& aStack);

 public:
  UniquePtr<UniqueJSONStrings> mUniqueStrings;

 private:
  SpliceableChunkedJSONWriter mFrameTableWriter;
  HashMap<FrameKey, uint32_t, FrameKeyHasher> mFrameToIndexMap;

  SpliceableChunkedJSONWriter mStackTableWriter;
  HashMap<StackKey, uint32_t, StackKeyHasher> mStackToIndexMap;
};

//
// Thread profile JSON Format
// --------------------------
//
// The profile contains much duplicate information. The output JSON of the
// profile attempts to deduplicate strings, frames, and stack prefixes, to cut
// down on size and to increase JSON streaming speed. Deduplicated values are
// streamed as indices into their respective tables.
//
// Further, arrays of objects with the same set of properties (e.g., samples,
// frames) are output as arrays according to a schema instead of an object
// with property names. A property that is not present is represented in the
// array as null or undefined.
//
// The format of the thread profile JSON is shown by the following example
// with 1 sample and 1 marker:
//
// {
//   "name": "Foo",
//   "tid": 42,
//   "samples":
//   {
//     "schema":
//     {
//       "stack": 0,          /* index into stackTable */
//       "time": 1,           /* number */
//       "responsiveness": 2, /* number */
//     },
//     "data":
//     [
//       [ 1, 0.0, 0.0 ]      /* { stack: 1, time: 0.0, responsiveness: 0.0 } */
//     ]
//   },
//
//   "markers":
//   {
//     "schema":
//     {
//       "name": 0,           /* index into stringTable */
//       "time": 1,           /* number */
//       "data": 2            /* arbitrary JSON */
//     },
//     "data":
//     [
//       [ 3, 0.1 ]           /* { name: 'example marker', time: 0.1 } */
//     ]
//   },
//
//   "stackTable":
//   {
//     "schema":
//     {
//       "prefix": 0,         /* index into stackTable */
//       "frame": 1           /* index into frameTable */
//     },
//     "data":
//     [
//       [ null, 0 ],         /* (root) */
//       [ 0,    1 ]          /* (root) > foo.js */
//     ]
//   },
//
//   "frameTable":
//   {
//     "schema":
//     {
//       "location": 0,       /* index into stringTable */
//       "implementation": 1, /* index into stringTable */
//       "optimizations": 2,  /* arbitrary JSON */
//       "line": 3,           /* number */
//       "column": 4,         /* number */
//       "category": 5        /* number */
//     },
//     "data":
//     [
//       [ 0 ],               /* { location: '(root)' } */
//       [ 1, 2 ]             /* { location: 'foo.js',
//                                 implementation: 'baseline' } */
//     ]
//   },
//
//   "stringTable":
//   [
//     "(root)",
//     "foo.js",
//     "baseline",
//     "example marker"
//   ]
// }
//
// Process:
// {
//   "name": "Bar",
//   "pid": 24,
//   "threads":
//   [
//     <0-N threads from above>
//   ],
//   "counters": /* includes the memory counter */
//   [
//     {
//       "name": "qwerty",
//       "category": "uiop",
//       "description": "this is qwerty uiop",
//       "sample_groups:
//       [
//         {
//           "id": 42, /* number (thread id, or object identifier (tab), etc) */
//           "samples:
//           {
//             "schema":
//             {
//               "time": 1,   /* number */
//               "number": 2, /* number (of times the counter was touched) */
//               "count": 3   /* number (total for the counter) */
//             },
//             "data":
//             [
//               [ 0.1, 1824,
//                 454622 ]   /* { time: 0.1, number: 1824, count: 454622 } */
//             ]
//           },
//         },
//         /* more sample-group objects with different id's */
//       ]
//     },
//     /* more counters */
//   ],
//   "memory":
//   {
//     "initial_heap": 12345678,
//     "samples:
//     {
//       "schema":
//       {
//         "time": 1,            /* number */
//         "rss": 2,             /* number */
//         "uss": 3              /* number */
//       },
//       "data":
//       [
//         /* { time: 0.1, rss: 12345678, uss: 87654321} */
//         [ 0.1, 12345678, 87654321 ]
//       ]
//     },
//   },
// }
//

}  // namespace baseprofiler
}  // namespace mozilla

#endif /* ndef ProfileBufferEntry_h */
