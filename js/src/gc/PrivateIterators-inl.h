/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC-internal iterators for various data structures.
 */

#ifndef gc_PrivateIterators_inl_h
#define gc_PrivateIterators_inl_h

#include "gc/PublicIterators.h"

#include "gc/GC-inl.h"

namespace js {
namespace gc {

class ArenaCellIterUnderGC : public ArenaCellIter {
 public:
  explicit ArenaCellIterUnderGC(Arena* arena) : ArenaCellIter(arena) {
    MOZ_ASSERT(CurrentThreadIsPerformingGC());
  }
};

class ArenaCellIterUnderFinalize : public ArenaCellIter {
 public:
  explicit ArenaCellIterUnderFinalize(Arena* arena) : ArenaCellIter(arena) {
    MOZ_ASSERT(CurrentThreadIsGCFinalizing());
  }
};

class GrayObjectIter : public ZoneAllCellIter<js::gc::TenuredCell> {
 public:
  explicit GrayObjectIter(JS::Zone* zone, AllocKind kind)
      : ZoneAllCellIter<js::gc::TenuredCell>() {
    initForTenuredIteration(zone, kind);
  }

  JSObject* get() const {
    return ZoneAllCellIter<js::gc::TenuredCell>::get<JSObject>();
  }
  operator JSObject*() const { return get(); }
  JSObject* operator->() const { return get(); }
};

class GCZonesIter {
  ZonesIter zone;

 public:
  explicit GCZonesIter(GCRuntime* gc, ZoneSelector selector = WithAtoms)
      : zone(gc, selector) {
    MOZ_ASSERT(JS::RuntimeHeapIsBusy());
    MOZ_ASSERT_IF(gc->atomsZone->isCollectingFromAnyThread(),
                  !gc->rt->hasHelperThreadZones());

    if (!done() && !zone->isCollectingFromAnyThread()) {
      next();
    }
  }
  explicit GCZonesIter(JSRuntime* rt, ZoneSelector selector = WithAtoms)
      : GCZonesIter(&rt->gc, selector) {}

  bool done() const { return zone.done(); }

  void next() {
    MOZ_ASSERT(!done());
    do {
      zone.next();
    } while (!zone.done() && !zone->isCollectingFromAnyThread());
  }

  JS::Zone* get() const {
    MOZ_ASSERT(!done());
    return zone;
  }

  operator JS::Zone*() const { return get(); }
  JS::Zone* operator->() const { return get(); }
};

using GCCompartmentsIter =
    CompartmentsOrRealmsIterT<GCZonesIter, CompartmentsInZoneIter>;
using GCRealmsIter = CompartmentsOrRealmsIterT<GCZonesIter, RealmsInZoneIter>;

/* Iterates over all zones in the current sweep group. */
class SweepGroupZonesIter {
  JS::Zone* current;
  ZoneSelector selector;

 public:
  explicit SweepGroupZonesIter(GCRuntime* gc, ZoneSelector selector = WithAtoms)
      : selector(selector) {
    MOZ_ASSERT(CurrentThreadIsPerformingGC());
    current = gc->getCurrentSweepGroup();
    maybeSkipAtomsZone();
  }
  explicit SweepGroupZonesIter(JSRuntime* rt, ZoneSelector selector = WithAtoms)
      : SweepGroupZonesIter(&rt->gc, selector) {}

  void maybeSkipAtomsZone() {
    if (selector == SkipAtoms && current && current->isAtomsZone()) {
      current = current->nextNodeInGroup();
      MOZ_ASSERT_IF(current, !current->isAtomsZone());
    }
  }

  bool done() const { return !current; }

  void next() {
    MOZ_ASSERT(!done());
    current = current->nextNodeInGroup();
    maybeSkipAtomsZone();
  }

  JS::Zone* get() const {
    MOZ_ASSERT(!done());
    return current;
  }

  operator JS::Zone*() const { return get(); }
  JS::Zone* operator->() const { return get(); }
};

using SweepGroupCompartmentsIter =
    CompartmentsOrRealmsIterT<SweepGroupZonesIter, CompartmentsInZoneIter>;
using SweepGroupRealmsIter =
    CompartmentsOrRealmsIterT<SweepGroupZonesIter, RealmsInZoneIter>;

// Iterate the free cells in an arena. See also ArenaCellIter which iterates
// the allocated cells.
class ArenaFreeCellIter {
  Arena* arena;
  size_t thingSize;
  FreeSpan span;
  uint_fast16_t thing;

 public:
  explicit ArenaFreeCellIter(Arena* arena)
      : arena(arena),
        thingSize(arena->getThingSize()),
        span(*arena->getFirstFreeSpan()),
        thing(span.first) {
    MOZ_ASSERT(arena);
    MOZ_ASSERT(thing < ArenaSize);
  }

  bool done() const {
    MOZ_ASSERT(thing < ArenaSize);
    return !thing;
  }

  TenuredCell* getCell() const {
    MOZ_ASSERT(!done());
    return reinterpret_cast<TenuredCell*>(uintptr_t(arena) + thing);
  }

  void next() {
    MOZ_ASSERT(!done());
    MOZ_ASSERT(thing >= span.first && thing <= span.last);

    if (thing == span.last) {
      span = *span.nextSpan(arena);
      thing = span.first;
    } else {
      thing += thingSize;
    }

    MOZ_ASSERT(thing < ArenaSize);
  }
};

}  // namespace gc
}  // namespace js

#endif  // gc_PrivateIterators_inl_h
