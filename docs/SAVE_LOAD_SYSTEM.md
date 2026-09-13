# Save and Load System

Standard of Iron stores match saves in a versioned SQLite database. A save combines the serialized battlefield with non-entity session state, campaign metadata, and a preview image. The storage layer adds compression, checksums, schema migration, database recovery, autosave retention, and background save writes.

The authoritative implementation is split across:

- `game/save/serialization.*` — entity and world serialization;
- `game/session/session_snapshot.*` — authoritative non-entity match state;
- `game/save/snapshot_contract.*` — classification of serialized, rebuilt, and presentation-only state;
- `app/persistence/save_load_coordinator.*` — application-level capture and restoration;
- `game/systems/save_load_service.*` — queued save jobs and slot operations;
- `game/systems/save_storage.*` — SQLite schema, migration, validation, and recovery; and
- `game/systems/save_format.*` — compression and payload integrity.

## What a save contains

The snapshot contract divides state into four classes.

### Authoritative serialized state

State that changes the meaning of the match is written explicitly. Examples include:

- world entities and authoritative components;
- terrain and authored props;
- owner, team, colour, and nation assignments;
- resource balances and harvested-resource totals;
- match statistics;
- simulation clock state;
- deterministic RNG state;
- explored visibility; and
- subsystem state contributed through `Game::Session::SessionSnapshot`, including AI and victory state.

### Derived state

State that can be reconstructed from authoritative data is rebuilt instead of treated as a second source of truth. Examples include troop-count registries, building-collision indexes, movement facts, engagement assignments, and other one-tick caches.

The command queue is also rebuilt empty. Orders that have not executed when the save is taken are not replayed after loading.

### Presentation-only state

Transient renderer state is not authoritative. Animation interpolation, short-lived hit effects, placement ghosts, and similar presentation data are recreated by normal runtime systems.

Some player-facing presentation state, such as camera state, is restored separately as a convenience without making it part of simulation authority.

### Campaign-level metadata

The save row also carries campaign and slot information such as campaign ID, mission ID, difficulty, play time, title, timestamps, save kind, and screenshot.

`game/save/snapshot_contract.cpp` is the maintained field-by-field inventory of these classifications.

## Save flow

`SaveLoadCoordinator` captures the world and session metadata on the owning thread, then submits a `SaveLoadService::SaveRequest`.

`SaveLoadService` queues the write on its worker thread. A save job reports these stages through `save_progress`:

1. **Queued** — the request is waiting for the worker.
2. **Serializing world** — the captured world document is converted to compact JSON bytes.
3. **Compressing** — `Save::pack()` builds the stored payload.
4. **Writing** — `SaveStorage` writes the slot transactionally.
5. **Done** — the slot is committed and `save_finished` is emitted.

The service supports cancellation by job ID, exposes `pending_save_count()`, and provides `wait_for_pending_saves()` for shutdown and other synchronization points.

Screenshots can be attached as a separate queued job after the save row exists.

## Compression and integrity

Save payloads are packed by `game/systems/save_format.cpp`.

The stored row records:

- compression mode;
- uncompressed world size;
- SHA-256 checksum of the uncompressed world;
- SHA-256 checksum of the stored blob; and
- the compressed world payload.

The format supports zlib compression. `Save::unpack()` verifies the stored representation and reconstructs the raw world bytes before JSON deserialization.

`SaveLoadService::verify_save_slot()` exposes slot verification through the storage layer.

## Load flow

Loading is staged so an invalid save does not destroy the match already in memory.

`SaveLoadService::load_game_from_slot()`:

1. reads the slot from SQLite;
2. verifies and unpacks the stored world payload;
3. parses the world JSON;
4. rejects a document that does not contain an entity array;
5. restores the world into a temporary `SessionContext` first;
6. verifies that the staged world contains units; and only then
7. clears the live world and deserializes the verified document into it.

If staging fails, the running battle is left untouched. Once the live world has been cleared, a failure during the final restore is reported and the world remains cleared rather than pretending the previous battle still exists.

`SaveLoadCoordinator` then restores the remaining session snapshot against the loaded map and rebuilds derived runtime services.

## The first tick after loading

Rendering consumes published render snapshots rather than the mutable simulation world. A loaded match therefore has to resume simulation so a tick can publish the newly restored state.

The load path restores the battlefield into an unpaused runtime state. The first simulation tick publishes the new render snapshot and rebuilds transient systems from the restored authoritative data.

World clearing also advances the world's content epoch, invalidating render-snapshot slot caches. Saved entity IDs can reuse the same slots and generations as entities from the previous world, so this epoch change prevents stale render data from being reused for a newly restored entity.

`tests/core/save_load_render_snapshot_test.cpp` and `tests/core/save_runtime_restore_test.cpp` cover this behavior.

## Visibility restoration

Explored fog is player knowledge and cannot be reconstructed only from current unit sight.

`GameStateSerializer` stores exploration in save metadata as a run-length-encoded, base64 mask. `GameStateSerializer::restore_visibility_from_metadata()` applies it after the world and visibility grid exist.

Restoration is additive: saved exploration can turn `Unseen` cells into `Explored`, but it does not remove cells that restored units can currently see. A mask whose dimensions do not match the restored map is ignored with a warning instead of making the entire save unloadable.

## Database schema and migration

The save database is `saves.sqlite` under the application save directory. `SaveStorage` enables SQLite WAL mode, `synchronous=FULL`, and foreign-key enforcement.

Two version numbers describe different contracts:

- `PRAGMA user_version` is the SQLite database-schema version. `game/systems/save_format.h` defines `k_database_schema_version`, currently `3`.
- each save row has a `snapshot_version`, which describes the world/session snapshot written into that row.

When an older supported database schema is opened, `SaveStorage`:

1. copies the database to `<path>.v<version>-backup`;
2. runs `migrate_schema()` inside a transaction; and
3. updates `PRAGMA user_version` to the current schema.

The repository therefore has an in-place migration path for older supported save databases. A schema mismatch is not automatically a destructive reset.

## Database recovery

`SaveStorage::initialize()` runs an integrity check and validates the schema shape.

A database that cannot be opened, has a failed integrity check, has an invalid or future schema version, or cannot be upgraded is moved aside rather than silently deleted. The replacement path uses a timestamped `.unreadable-...` name and creates a fresh database.

`SaveLoadService::quarantined_database_path()` exposes the path of a database moved aside during initialization so the application can report where the original data was preserved.

## Autosaves

Autosaves use the same storage format as manual saves. The service clamps autosave retention to the supported range and prunes the oldest autosave slots after a successful write.

Manual, quick-save, and autosave behavior differ by slot naming and retention policy, not by world serialization format.

## Storage tables

The current schema includes:

- `saves` — saved matches and their metadata, payload, and preview;
- `campaign_progress` — campaign completion state;
- `campaign_missions` — per-mission unlock/completion state; and
- `mission_results` — recorded mission outcomes.

`SaveStorage::schema_shape_is_current()` validates the required columns for these tables.

## Tests

The save contract is covered by several layers of tests, including:

- `tests/db/save_storage_test.cpp` — schema, migration, recovery, slot storage, and database behavior;
- `tests/save/snapshot_contract_test.cpp` — snapshot versioning and field classification;
- `tests/core/serialization_test.cpp` — world/component serialization;
- `tests/core/save_load_render_snapshot_test.cpp` — render publication after load; and
- `tests/core/save_runtime_restore_test.cpp` — runtime restoration behavior.

Compression, background save jobs, progress signals, schema migration, and database quarantine are all present in the current repository.
