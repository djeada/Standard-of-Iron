# Simulation performance: the review, checked against the code and the profiler

The incoming review proposed nine changes to the ECS and simulation, ranked by
expected value, and asked for a headless benchmark "immediately, because without
those numbers 'optimized' is mostly guesswork". That last point is the one that
mattered. The benchmark already existed; running it reordered almost everything
else on the list.

Everything below is measured on `build/bin/sim_benchmark`, two armies deployed as
battle lines, fixed tick, RelWithDebInfo-equivalent Release flags.

---

## What the profiler said

At 5,000 units the per-system table is not close:

```
CombatSystem          1 647 300 us/tick      ← 99.4% of the tick
MovementSystem            9 147 us
LocalAvoidanceSystem      5 323 us
EngagementSlotSystem        801 us
CommanderSystem             448 us
AISystem                    160 us
...everything else together under 1 ms
```

`perf` inside that:

```
62.2%  FormationCombat::resolve_contact_context
 3.7%  FormationCombat::resolve_layout
 2.5%  spawn_typeToTroopType
 2.0%  malloc
 1.9%  UnitLayoutLibrary::find
 1.7%  resolve_definition
```

So the whole list below has to be read against one fact: **the simulation is
formation contact geometry, and everything else is rounding error.** Items 1, 4,
6, 8, 9 and 10 of the review are optimisations of code that together accounts
for under 1% of a tick.

---

## The review, item by item

| #   | Proposal                                                               | Status                                                                                                                                                                                                                                                                                              |
| --- | ---------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Zero-allocation multi-component ECS iteration (`world.each<A,B,C>`)    | **Already built.** `World::view<...>` / `World::each<...>` exist, allocate nothing, pick the smallest dense storage as the source and resolve the rest through sparse `try_get`. `get_entities_with<T>()` no longer exists. `MovementSystem` drives its loop from `world->each<MovementComponent>`. |
| 2   | Remove the global recursive mutex from the tick                        | **Already mitigated.** `World::update()` takes the registry lock once; `ScopedRegistryLock` has a reentrancy fast path, so every nested view or emplace inside the tick is one relaxed atomic load and a compare, not a lock.                                                                       |
| 3   | Shared/group pathfinding (priority 2)                                  | **Not attempted, and not indicated.** `MovementSystem` is 9 ms of a 1 670 ms tick. Worth revisiting only after combat stops dominating.                                                                                                                                                             |
| 4   | Cache navigation state, stop re-querying walkability per unit per tick | **Not attempted, same reason.** The redundant walkability checks are real; they are 0.5% of the tick.                                                                                                                                                                                               |
| 5   | Cache formation geometry instead of recomputing it per tick            | **Already built** for the turn radius — `formation_turn_radius` reads a signature-validated layout cache. The review was right about the _shape_ of the problem and pointed at the wrong function; see below.                                                                                       |
| 6   | Different update frequencies per system                                | **Partly, ad hoc.** `AISystem` and `HomeSystem` carry their own intervals; there is no framework-level cadence on `System`. A shared mechanism is a reasonable future change, but the systems it would slow down cost under 1 ms combined.                                                          |
| 7   | A spatial query layer used everywhere                                  | **Already built.** `Engine::Core::WorldSpatialIndex`, uniform grid, `query_radius` / `for_each_in_radius`. The profiler counts it: 985 spatial queries in the last tick.                                                                                                                            |
| 8   | Animation/render LOD                                                   | Covered separately in `docs/RENDERING_ARCHITECTURE.md`; the previous round made LOD selection screen-size relative. High and Ultra deliberately evaluate no LOD at all.                                                                                                                             |
| 9   | SIMD/SoA, fast trig, micro-optimisation                                | Correctly ranked last, and still last.                                                                                                                                                                                                                                                              |
| —   | "Add a headless performance benchmark immediately"                     | **Already built:** `tools/sim_benchmark`, 1k/5k/10k units, per-system table, per-call-site attribution of materialising scans, peak RSS and a world digest.                                                                                                                                         |

---

## What was actually changed

Two defects in `resolve_contact_context`, both invisible from a file listing and
both obvious in a profile:

**It scanned every slot pair twice.** One nested loop found the nearest pair of
bodies (the surface gap); a second nested loop over exactly the same pairs found
the contact distance along the axis. Now one fused pass answers both.

**It returned two `FormationLayout` objects by value** — three vectors each — and
`contact_geometry()`, which is what six of the eight call sites use, threw both
away. That is six vector copies per query, and there are about 179 000 queries
per tick at 2 000 units. `resolve_layout_entry` now hands out a pointer to the
cached layout. `resolve_layout` is unchanged for callers that want a copy.

Measured, `--no-systems`, same machine, interleaved, re-baselined against main
at `0e5e1a48` (the numbers below are the post-merge measurement; the pre-merge
run gave the same result to within a percent):

| Scenario               | Baseline mean |            Now | Baseline p95 |            Now |
| ---------------------- | ------------: | -------------: | -----------: | -------------: |
| 2 000 units, 240 ticks |      290.5 ms |   **240.7 ms** |     335.2 ms |   **276.9 ms** |
| 5 000 units, 60 ticks  |    1 698.3 ms | **1 397.0 ms** |   1 723.4 ms | **1 421.0 ms** |

About 17% off the entire simulation, in the one system that had 99% of it.

---

## Two changes that were tried, measured and reverted

**Memoising the contact geometry per (attacker, target) pair.** The same pair is
queried around 59 times over a run, which reads as a textbook memoisation. The
instrumented hit rate says otherwise: of 10 765 133 lookups, 176 915 hit, 180 954
found nothing, and **10 407 264 found the entry and rejected it as stale**. Each
pair is asked about roughly once per tick, and by the next tick the units have
moved. Removed. The lesson generalises: a high repeat count across a run says
nothing about whether a cache can hit.

**Hashing `health > 0` instead of the exact health into `layout_signature`.** The
layout reads health exactly once, as `health > 0` for a rigid body, and never
reads `max_health`, so hashing the values looked like pure over-invalidation —
every point of damage rebuilding three vectors of slots during a melee. It
measured as noise and it broke ten navigation tests. The layout cache is keyed by
`const Entity*` and validated only by that signature, so two `World` instances in
one process can place an entity at the same address with the same id; the exact
health value was accidentally the thing keeping them apart. Reverted. Narrowing
that signature requires fixing the cache key first.

---

## The measurement trap this work fell into

`sim_benchmark` prints a world digest, and it is tempting to treat it as a
"did my refactor change behaviour" oracle. It is not one. Release builds use
`-ffast-math`, so re-associating or re-inlining a hot float expression moves the
last bit of a distance, and a battle diverges from that within a few hundred
ticks.

- Identical digests across two runs of **one** binary are the determinism
  contract. `scripts/check-replay-determinism.sh` enforces the real one by
  record-and-replay.
- Different digests across two **builds** are evidence of nothing on their own.

The rewrite above was proved equivalent by a compile-gated pass that ran the old
and new loops side by side on live data: `surface_gap` was bit-identical in all
37 154 788 comparisons, and `contact_center_distance` differed by one ULP in
0.42% of them — the signature of fast-math code motion, not of an algorithm
change.

Written up in `docs/ARCHITECTURE.md` under _Measuring the simulation_.

---

## Where the remaining time goes

At 2 000 units the combat system still issues about **179 000 exact formation
contact queries per tick — roughly 90 per unit** — each scanning ~550 slot pairs.
That is the real structural cost, and it is a query-volume problem, not a
per-query one. The next move is a broad phase: reject candidate pairs on bounding
circles before asking for exact slot geometry, so the all-pairs scan only runs
for formations that can actually touch. That is the review's item 7 applied where
it pays, rather than where the grid already exists.

Only after that do items 1, 3, 4 and 6 become measurable.

---

## Unrelated bug found and fixed along the way

The selected-unit HUD flickered between empty and full health/stamina during
fights. The values were never wrong — a headless probe
(`tests/headless/selected_unit_readout_test.cpp`) watches exactly what the HUD
reads, every tick, through a real battle, and never sees a dip that recovers.

`IronProgressBar` had its `Behavior` on the fill's **width**. Width changes both
when the value moves and when the bar is laid out, and a delegate created inside
a zero-width parent gets its first real width as a _change_, which is the case a
`Behavior` animates. Since `grouped_by_type()` returns a new array on every
refresh, and a new array on a `Repeater` model rebuilds every delegate, both bars
were recreated and re-swept continuously while a fight churned the selection.

The bar now eases a `real animatedPosition` bound to `visualPosition` and lets
width follow it: layout is immediate, a genuine value change still eases.
`tst_component_library.qml` pins both halves. Written up in
`docs/UI_DESIGN_SYSTEM.md`.
