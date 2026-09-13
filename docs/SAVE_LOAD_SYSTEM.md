# Save and Load System

Standard of Iron stores match saves in a versioned SQLite database. A save is more than a serialized entity list: it combines the authoritative battlefield, non-entity session state, campaign/mission metadata, and a preview image, then stores that snapshot with compression, checksums, transactional database writes, schema versioning, migration, integrity checking, recovery, and autosave retention.

The save system is designed around a clear ownership rule: persist state that changes the meaning of the match, rebuild state that is derived, and keep renderer/UI-only state outside simulation authority.

## Main components

The implementation is split by responsibility:

| Area | Source | Responsibility |
| --- | --- | --- |
| World serialization | `game/save/serialization.*` | entity/component world document |
| Session snapshot | `game/session/session_snapshot.*` | authoritative non-entity match state |
| Snapshot contract | `game/save/snapshot_contract.*` | field classification and version contract |
| Application coordination | `app/persistence/save_load_coordinator.*` | capture/restore orchestration around the live game |
| Async save service | `game/systems/save_load_service.*` | queued jobs, slots, progress, cancellation, verification |
| SQLite storage | `game/systems/save_storage.*` | schema, transactions, migration, recovery, campaign tables |
| Payload format | `game/systems/save_format.*` | compression, checksums, packed world payload |

These layers deliberately separate “what constitutes the match” from “how bytes are stored.”

## Save-state model

`game/save/snapshot_contract.cpp` classifies runtime state into four categories.

### Authoritative serialized state

State that affects the meaning or future evolution of the match is persisted explicitly.

Examples include:

- world entities and authoritative components;
- terrain and authored world props;
- owner, team, colour, and nation assignment;
- player resources and harvested-resource totals;
- match statistics;
- simulation clock state;
- deterministic RNG state;
- explored visibility/fog knowledge; and
- subsystem state contributed through `Game::Session::SessionSnapshot`, including AI and mission/victory state where registered.

If losing a value would change what happens after load, that value belongs in the authoritative contract or must be derivable from other authoritative state.

### Derived state

Derived runtime data is rebuilt after restoration instead of being serialized as a second authority.

Examples include:

- troop-count registries;
- building-collision indexes;
- movement facts;
- engagement assignments;
- one-tick lookup caches; and
- other runtime indexes that can be reconstructed from the restored world/session.

The command queue is also rebuilt empty. Commands that have not executed when the save is captured are not replayed after loading.

That is an important semantic boundary: the save captures the authoritative state at the save point, not a speculative set of queued future mutations.

### Presentation-only state

Renderer and UI presentation are not part of simulation authority.

Transient examples include:

- animation interpolation state;
- short-lived hit effects;
- placement ghosts;
- renderer resource caches; and
- other visual effects that normal runtime systems can regenerate.

Some player-facing presentation state, such as camera state, can be restored separately for continuity without making it authoritative gameplay state.

### Campaign-level metadata

The database also stores information that describes the save slot rather than entity simulation state, including:

- campaign ID;
- mission ID;
- difficulty;
- play time;
- slot title;
- timestamps;
- save kind; and
- screenshot/preview data.

Campaign completion and mission result tables live in the same storage layer but have their own schema and lifecycle.

## Save capture boundary

`SaveLoadCoordinator` is the application-facing capture boundary.

It captures the world/session state on the owning thread, prepares the metadata needed for the slot, and submits a `SaveLoadService::SaveRequest` to the background save service.

The expensive storage work can then happen off the live gameplay path because the worker operates on the captured save document rather than continuing to read the mutable world.

This boundary is what makes asynchronous save writes safe: the worker is writing a snapshot, not sharing authority over live entities.

## Save job lifecycle

`SaveLoadService` queues save work on its worker thread and reports progress through `save_progress`.

The current stages are:

1. **Queued** — the request is waiting for the save worker.
2. **Serializing world** — the captured world document is converted to compact JSON bytes.
3. **Compressing** — `Save::pack()` creates the stored payload and integrity metadata.
4. **Writing** — `SaveStorage` writes the slot transactionally.
5. **Done** — the slot commit has completed and `save_finished` is emitted.

These stages describe the current worker pipeline rather than a UI-only progress animation.

### Queue management

The service also exposes:

- cancellation by save job ID;
- `pending_save_count()`; and
- `wait_for_pending_saves()` for shutdown or other synchronization points.

A caller can therefore distinguish “save requested” from “save durably completed” and can prevent application shutdown from abandoning queued work.

### Screenshot attachment

A screenshot/preview can be attached as a separate queued job after the slot row exists. The preview is metadata for browsing saves; it is not part of the authoritative world payload.

## Packed payload format

`game/systems/save_format.cpp` owns packing and unpacking of the world payload.

The stored save row records enough information to validate both the compressed representation and the reconstructed world bytes:

- compression mode;
- uncompressed world size;
- SHA-256 checksum of the uncompressed world;
- SHA-256 checksum of the stored blob; and
- the packed world payload.

The current format supports zlib compression.

### Why there are two checksums

The stored-blob checksum catches corruption of the representation that was written to SQLite. The uncompressed-world checksum verifies the bytes obtained after unpacking.

That distinction makes corruption detectable on both sides of decompression instead of assuming that a successful decompression implies a valid save document.

`Save::unpack()` verifies the stored data before JSON deserialization proceeds.

`SaveLoadService::verify_save_slot()` exposes slot verification through the storage layer without requiring a full live-game replacement.

## SQLite database

The save database is named:

```text
saves.sqlite
```

and lives under the application's save directory.

`SaveStorage` configures SQLite with:

- WAL mode;
- `synchronous=FULL`; and
- foreign-key enforcement.

The database is used for both match saves and campaign/mission persistence.

## Storage tables

The current schema includes:

- `saves` — match snapshots, slot metadata, packed payload, and preview;
- `campaign_progress` — campaign-level completion/progression state;
- `campaign_missions` — per-mission unlock/completion state; and
- `mission_results` — recorded mission outcomes.

`SaveStorage::schema_shape_is_current()` checks the expected table/column shape rather than trusting only the numeric schema version.

## Versioning: database vs snapshot

Two different version numbers describe two different contracts.

### Database schema version

SQLite `PRAGMA user_version` describes the database schema.

`game/systems/save_format.h` defines `k_database_schema_version`, currently:

```text
3
```

This version controls whether the SQLite tables/columns need migration.

### Snapshot version

Each save row also carries a `snapshot_version` describing the world/session snapshot format stored in that row.

A database can therefore have the current table schema while still containing snapshot rows written under an older snapshot contract.

Keeping these versions separate avoids conflating “the table layout changed” with “the serialized gameplay document changed.”

## Schema migration

When `SaveStorage` opens an older supported database schema, it performs a guarded migration instead of simply discarding the file.

The migration flow is:

1. copy the database to `<path>.v<version>-backup`;
2. run `migrate_schema()` inside a transaction; and
3. update `PRAGMA user_version` to the current schema.

The backup is created before migration so the pre-migration file remains available if the upgrade cannot be completed.

A schema mismatch is therefore not automatically destructive. Current storage code has a real migration path for supported older versions.

## Database initialization and recovery

Opening a save database is also a validation step.

`SaveStorage::initialize()` checks that the file can be opened, runs an integrity check, and validates the schema/version shape.

A database can be rejected when it is:

- unreadable;
- structurally corrupt;
- on an unsupported/future schema version; or
- unable to complete a required migration.

In those cases the original file is moved aside rather than silently deleted.

The quarantine name uses a timestamped form containing:

```text
.unreadable-...
```

and a fresh database is created for continued operation.

`SaveLoadService::quarantined_database_path()` exposes the moved-aside path so the application can report where the original data was preserved.

This recovery behavior is different from claiming that the old database was successfully migrated. Quarantine means the current runtime refused to trust it as the active save database and preserved it for inspection/recovery.

## Load flow

Loading is deliberately staged so a bad save does not immediately destroy the currently running match.

`SaveLoadService::load_game_from_slot()` performs the storage/document validation path:

1. read the requested slot from SQLite;
2. validate and unpack the stored payload;
3. parse the world JSON;
4. reject a document without an entity array;
5. deserialize into a temporary `SessionContext`;
6. validate that the staged world contains units; and only then
7. clear the live world and deserialize the verified document into it.

The temporary session is a safety boundary. A malformed or semantically unusable document can fail before the live battlefield is replaced.

### Failure semantics before live replacement

If staging fails, the current battle remains intact.

That includes failures such as:

- checksum/decompression failure;
- invalid JSON;
- missing expected entity data; or
- staged-world validation failure.

### Failure semantics after live replacement begins

Once the live world is cleared, a failure during final deserialization is reported and the world remains cleared rather than pretending that the old battle is still present.

This distinction is important: the staging pass protects against known-invalid saves, but the final restore is still an authoritative replacement boundary.

## Session restoration after world load

After the entity world is restored, `SaveLoadCoordinator` restores the remaining session snapshot against the loaded map and rebuilds the derived runtime services required by the new world.

The loaded match therefore consists of both:

- entity/component state; and
- authoritative session-level state.

Restoring one without the other would leave deterministic RNG, AI/victory state, visibility knowledge, or other registered session facts inconsistent with the world.

## First simulation tick after load

The renderer consumes published render snapshots rather than the mutable world.

After loading, the restored entities exist in simulation, but the renderer still needs a new published snapshot representing that world. The runtime resumes in an unpaused state so the first simulation tick can publish fresh presentation data and rebuild transient systems.

This is a real part of load correctness: a successfully deserialized world that never publishes a new render snapshot would still present stale or empty visual state.

## Render-snapshot invalidation

World clearing advances the world's content epoch.

Saved entity IDs can reuse the same index/generation values as entities that existed before the load. Without an additional world-content boundary, renderer-side slot caches could mistake a restored entity for the old entity that occupied the same handle.

Advancing the content epoch invalidates those stale render-snapshot associations.

`tests/core/save_load_render_snapshot_test.cpp` and `tests/core/save_runtime_restore_test.cpp` exercise the post-load publication/restoration path.

## Visibility and explored fog

Explored fog is player knowledge, not merely a function of the units that are currently alive.

`GameStateSerializer` stores the exploration mask in save metadata using run-length encoding plus base64.

`GameStateSerializer::restore_visibility_from_metadata()` applies the mask after the world and visibility grid exist.

The restoration is additive:

- saved exploration can change `Unseen` cells into `Explored`;
- currently visible cells remain visible according to the restored units; and
- loading does not erase legitimate current vision merely because the stored mask says only “explored.”

If the saved mask dimensions do not match the restored map, the mask is ignored with a warning rather than making the entire save unloadable.

## Deterministic state

The save snapshot preserves deterministic simulation state required for the match to continue consistently, including the simulation clock and deterministic RNG state.

The command queue is not persisted, which means determinism after load begins from the committed state represented by the save rather than from commands that were waiting to execute at the instant capture occurred.

This is also why replay and save contracts overlap conceptually but are not the same artifact: a replay stores an accepted command stream and digests, while a save stores a complete restorable state snapshot.

## Autosaves

Autosaves use the same packed world format and storage pipeline as manual saves.

The differences are slot policy and retention rather than serialization semantics.

After a successful autosave, the service prunes old autosave slots according to the supported retention range. Manual and quick-save slots are not transformed into a different world format simply because they belong to another save kind.

## Manual saves, quick-saves, and autosaves

All save kinds share:

- world/session capture;
- payload packing;
- integrity metadata;
- SQLite storage; and
- load verification.

They differ primarily in naming/selection behavior, UI flow, and retention rules.

Keeping one payload path prevents a “quick save format” from drifting away from a “manual save format.”

## Campaign persistence

Campaign progression is stored in the database alongside match saves but is not embedded into every entity snapshot.

The storage layer maintains campaign progression, mission completion/unlock state, and mission result records in dedicated tables.

That keeps campaign-level facts separate from battlefield ECS state while still giving the application one persistence service for the player's local progress.

## Concurrency and shutdown

Because save writes are queued, application shutdown and destructive transitions need to account for outstanding work.

`pending_save_count()` exposes whether jobs remain. `wait_for_pending_saves()` provides the explicit synchronization point used when the process must not exit while a requested save is still being written.

Cancellation is job-based rather than a global “throw away all persistence” switch.

The worker owns storage work; it does not become another thread mutating the live world.

## What is intentionally not serialized

The snapshot contract is as important for what it excludes as for what it stores.

Examples of intentionally rebuilt or omitted state include:

- transient spatial/collision indexes;
- one-frame movement facts;
- renderer caches;
- temporary combat presentation;
- pending UI interactions; and
- queued-but-unexecuted commands.

Serializing these would make restoration harder to reason about because the loaded game would need to decide whether the persisted cache or the authoritative components were correct.

## Practical failure diagnosis

A save/load problem can usually be localized to one layer.

### Slot cannot be read

Inspect database initialization, schema shape, migration, quarantine state, and the slot row.

### Slot reads but verification fails

Inspect packed-payload checksum, compression mode, uncompressed size, and unpacking.

### Payload parses but staging fails

Inspect the serialized world shape, entity list, component decoding, and staged-world validation.

### Staging succeeds but live restore is wrong

Inspect final deserialization, session snapshot restoration, map-dependent services, and derived-state rebuild.

### World restores but visuals are stale

Inspect content epoch/render-snapshot publication and the first post-load simulation tick.

### Fog knowledge is wrong

Inspect visibility metadata dimensions and `restore_visibility_from_metadata()`.

This layered diagnosis mirrors the actual architecture and avoids treating every load failure as a generic database problem.

## Tests

The save contract is exercised at several levels:

- `tests/db/save_storage_test.cpp` — schema, migration, quarantine/recovery, slot storage, and database behavior;
- `tests/save/snapshot_contract_test.cpp` — snapshot classification/versioning;
- `tests/core/serialization_test.cpp` — entity/component world serialization;
- `tests/core/save_load_render_snapshot_test.cpp` — render publication after load; and
- `tests/core/save_runtime_restore_test.cpp` — runtime/session restoration behavior.

Background save jobs, progress stages, compression, integrity checks, database migration, and quarantine are current implemented behavior and should be documented as such.

## Architectural invariants

The current save/load design depends on these invariants:

- only authoritative gameplay state is persisted as authority;
- derived caches are rebuilt;
- queued-but-unexecuted commands do not survive load;
- save workers operate on captured data, not the mutable live world;
- packed payloads are integrity-checked;
- a candidate world is staged before live replacement;
- database migration is backed up and transactional;
- unreadable/corrupt/future storage is quarantined rather than silently destroyed; and
- render/presentation state is republished after restoration rather than trusted from the previous world.

## Source map

| Concern | Source |
| --- | --- |
| Entity/world serialization | `game/save/serialization.*` |
| Authoritative non-entity snapshot | `game/session/session_snapshot.*` |
| Snapshot classification | `game/save/snapshot_contract.*` |
| Save job queue | `game/systems/save_load_service.*` |
| SQLite storage/migration | `game/systems/save_storage.*` |
| Compression/checksums | `game/systems/save_format.*` |
| Application restore orchestration | `app/persistence/save_load_coordinator.*` |
| Storage tests | `tests/db/save_storage_test.cpp` |
| Restore/render tests | `tests/core/save_*` |

The storage, migration, compression, asynchronous write, integrity, and recovery paths described here are present in the current repository. They are not proposed enhancements.
