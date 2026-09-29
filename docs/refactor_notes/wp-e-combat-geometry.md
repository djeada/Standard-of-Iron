# WP-E: formation combat geometry split

`game/systems/formation_combat_geometry.cpp` (1,850 lines) is gone. The public
header `formation_combat_geometry.h` is unchanged; its functions now live in
seven translation units that share one internal header.

| File                              | Owns                                                                                                                                                                            |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `formation_geometry_internal.h`   | cache key/hash, epoch, incremental eviction, every cache entry type, `GeometryCaches` (the single per-thread cache set), `caches()` accessor, declarations shared between units |
| `formation_geometry_cache.cpp`    | cache epoch, `GeometryCaches::clear`, `invalidate_layout_cache`, `formation_cache_generation`, contact stats                                                                    |
| `formation_layout_definition.cpp` | `resolve_definition`, living slot queries, seed, reach, `has_formation_slots`, `max_contact_extent`                                                                             |
| `formation_layout_cache.cpp`      | layout signature, layout build (rigid body vs formation), cache lookup and store, turn radius, extents, navigation clearance                                                    |
| `formation_work_site.cpp`         | builder work-site perimeter placement                                                                                                                                           |
| `formation_spatial_anchors.cpp`   | soldier anchors from traversal/presentation facts, spatialised layouts and their cache, `face_about_in_place`                                                                   |
| `formation_contact.cpp`           | contact-slot cache, slot contact scan, `resolve_contact`, `contact_is_active`                                                                                                   |
| `formation_engagement.cpp`        | engagement pairs and damage-pair selection                                                                                                                                      |

## Why one cache header

Before, five `thread_local` maps, a generation counter and the stats struct
were separate file-scope variables, and `invalidate_layout_cache` had to be
kept in step with each by hand. They are now members of `GeometryCaches`; the
only way to reach them is `Detail::caches()`, and `GeometryCaches::clear()` is
the one place that empties them. A new cache added to the struct is cleared for
free. The process-wide epoch (`std::atomic`) still invalidates other threads'
caches lazily: entries carry the epoch they were built in.

`slot_presence` is the old `g_slots_cache` (renamed because `slots` is a Qt
keyword macro).

## Long functions

- `build_layout_into_cache` (170 lines) is now `build_rigid_body_layout`,
  `build_formation_layout` (orchestration), `SlotPlacer`, `living_slot_mask`,
  `casualty_slot_mask`, `place_live_slots`, `append_casualty_slots`.
- `resolve_contact` (150) is split into `resolve_single_body_contact`,
  `resolve_spatial_layouts`, `resolve_engagement_distance` and the cache
  stamping that stays in the orchestrator.
- `accumulate_slot_contact` (110) is split into `sort_target_entries`,
  `sort_attacker_entries` and `scan_attacker_slot` around a `ContactFrame`.
- `soldier_spatial_anchors_into` (110) uses `RootFrame`,
  `index_presentation_by_slot`, `index_traversal_by_slot` and
  `apply_presentation_fact`.
- The two `resolve_definition` overloads share `apply_unit_overrides`.

Order of cache probes, eviction, generation bumps and float expressions were
kept as they were; the scratch `thread_local` vectors used inside hot loops
(anchors, contact entries) stay function-local because they are buffers, not
caches, and hold no state across calls.
