# WP-D: save serialization and save storage (#1559 PR 5)

Scope: `game/save/serialization.cpp` (2,508 lines, `deserialize_entity` 993 lines,
`serialize_entity` ~750 lines) and `game/systems/persistence/save_storage.cpp` (1,625 lines,
`complete_campaign_mission` 143 lines). No behaviour, format or ordering changed.

## Serialization

`Serialization` keeps its public API. `serialize_entity` and `deserialize_entity`
are now dispatchers over component families; each family owns the paired
read/write of its components in one file, next to that family's enum
conversions and defaults:

| File                                            | Components                                                                                                                                                            |
| ----------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `component_core_codec.cpp`                      | transform, renderable, unit, movement, player order intent, patrol, AI marker, formation mode, army formation membership, unit layout state, stamina, terrain context |
| `component_combat_codec.cpp`                    | attack (combat-mode strings), attack target, hold mode, guard mode, combat state, hit feedback                                                                        |
| `component_ability_codec.cpp`                   | healer (affinity strings), special attack (projectile-kind strings), fire patch, catapult loading                                                                     |
| `component_status_codec.cpp`                    | undead, cursed, burning, elephant, elephant panic, stomp impacts, wildlife                                                                                            |
| `component_commander_codec.cpp`                 | commander, RPG health, morale, commander guard                                                                                                                        |
| `component_economy_codec.cpp`                   | production, builder production, home, farm, civilian delivery, resource carry, settlement resident                                                                    |
| `component_structures_codec.cpp`                | building, capture, assault wave, structure fire, wall segment, wall construction site, dismantle site, gate                                                           |
| `terrain_codec.cpp` / `terrain_biome_codec.cpp` | terrain grid, hills, rivers, lakes, bridges, roads, world props; biome palette, scatter, surface, climate                                                             |
| `serialization.cpp`                             | world envelope (capture stamp, entities, next id, owner registry, formations, terrain) and file IO                                                                    |

The per-component bodies were moved verbatim (mechanically, from the original
line ranges) into `write_<component>` / `read_<component>` functions. Only the
wrapping changed. Schema keys and defaults remain literal at the point of use
inside the one family that owns them; there is no shared key table, because the
key strings are the on-disk format and each appears in exactly one pair of
functions.

Ordering notes:

- Components live in a registry keyed by type, so the order families are
  written or read does not change the result. The one read-side dependency
  (`ElephantStompImpactComponent` reads after `ElephantPanicComponent` exists)
  is kept inside `component_status_codec.cpp` in the original order.
- `QJsonObject` sorts its keys, so write order never reached the bytes.
- Biome fields are read and written in groups; every field is independent, so
  regrouping is behaviour-neutral. The count is pinned: 51 written keys, 37 read
  statements (one per field, colours read three channels each).

`snapshot_contract_test` used to grep `serialization.cpp` for
`get_component<X>` / `add_component<X>`. It now scans `serialization.cpp` plus
every `component_*_codec.cpp`, so the authoritative/derived classification is
still enforced against the code that actually touches each component.

`MovementComponent` declares `friend class Serialization` for its private
fields. Its codec (`write_movement` / `read_movement`) therefore stays a pair of
private static members of `Serialization`, defined in
`component_core_codec.cpp`. No new friend was added; when the movement package
gives the component public accessors those two functions can join the anonymous
namespace with the rest of the core family.

### Format proof

`tests/save/serialization_golden_test.cpp` with the goldens in
`tests/save/golden/` was recorded from the pre-refactor code:

- `entity_all_components.json`: an entity carrying every serialized component,
  serialised at defaults; then every numeric leaf replaced by a distinct value
  and every bool flipped, deserialised into a fresh entity and serialised again.
  The three documents plus the world-envelope key set are byte-compared.
- `save_database_schema.txt`: `sqlite_master` and `user_version` of a fresh
  database.

In addition, the whitespace-normalised statement multiset of the old
`serialization.cpp` was compared with the new files: every component and biome
statement is present exactly once; the only differences are wrapper signatures,
the lambdas that became functions, four dead locals in the old
`deserialize_world`, and the local `read_color` lambda that became
`read_biome_color`.

Regenerate deliberately with `SOI_UPDATE_GOLDEN=1` (run from the repo root).
A red golden means the save format changed and needs a version decision.

## Save storage

`SaveStorage` remains the facade (one class, one header, same public methods).
Its member functions are now defined in files by responsibility:

| File                         | Responsibility                                                                          |
| ---------------------------- | --------------------------------------------------------------------------------------- |
| `save_storage.cpp`           | construction, connection open/close, PRAGMAs, `initialize`                              |
| `save_schema.cpp`            | required table shape, CREATE statements, integrity check, migration, backup, quarantine |
| `save_slots.cpp`             | slot CRUD: write (upsert + read-back), read, verify, list, screenshot, delete           |
| `save_campaign_progress.cpp` | campaign/mission progress and `complete_campaign_mission`                               |
| `save_campaign_catalog.cpp`  | loading campaign definitions and building the QML-facing campaign entry                 |
| `save_sql.h`                 | the transaction boundary and error formatting shared by all of the above                |

`SaveSql::TransactionGuard` is the explicit transaction boundary: every
mutating method begins one, helper steps return `bool` and leave rollback to
the method that owns the transaction, and only that method commits. All SQL text
and every `QCoreApplication::translate("SaveStorage", ...)` string is unchanged,
so the translation context and the schema are the same.

Splits inside methods (same statements, same order):

- `write_slot` -> `check_slot_writable`, `upsert_slot` (prepare, bind, exec).
- `read_slot` -> `select_slot_row`, `check_row_versions`, `decode_record`.
- `ensure_schema` -> `read_schema_version`; its fresh-database branch was a
  copy of `create_fresh_schema` and now calls it.
- `quarantine_and_recreate` -> `move_database_aside`.
- `complete_campaign_mission` -> `look_up_mission`, `mark_mission_completed`,
  `unlock_next_mission`, `campaign_has_no_open_mission`,
  `record_campaign_completion`.
- `ensure_campaign_missions_in_db` -> `register_campaign_missions`,
  `prune_removed_missions`.

## Save/load service lifetime audit (no code change)

Audited `game/systems/persistence/save_load_service.{h,cpp}` for callbacks, observers and
queued work crossing a world reset or restore:

- The worker thread never sees a `World`. A queued `Job` holds a value copy of
  the serialised `QJsonDocument`, so `world.clear()` or a restore cannot dangle
  a pending write.
- Load deserialises into a staging `SessionContext` first and only then clears
  and fills the live world; the worker's `SaveStorage` and the main-thread
  `m_storage` use separate connections, so the two do not share a handle.
- `shutdown()` sets `m_stopping` under the mutex before reading `joinable()`,
  and `begin_save` checks it under the same mutex while starting the worker, so
  a save racing shutdown either starts the worker before shutdown looks or is
  refused. The worker drains its queue before exiting.
- Signals are emitted from the worker thread and delivered queued to receivers
  on their own threads; Qt drops them for destroyed receivers.
- Observation only: `cancel_save` for a job id that has already finished leaves
  that id in `m_cancelled` (ids are never reused, so it is inert). Not changed.

No lifetime defect was demonstrated, so nothing was modified.
