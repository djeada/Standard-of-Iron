# How the Save/Load System Actually Works

Imagine you've been playing a campaign for two hours. You've built up an army of 500 soldiers, captured strategic positions, and you're about to launch your final assault. Then life happens—dinner's ready, or you need to close your laptop. You need to save your progress and come back later with everything exactly as you left it.

This is the story of how Standard of Iron captures your entire game state, stores it in a database, and brings it back to life when you load. We'll walk through the journey from clicking "Save Game" to seeing your army restored on the battlefield.

## What we'll cover

We'll start with the high-level architecture: how the three layers work together to persist game state. Then we'll dig into each component: the serialization layer that converts game objects to JSON, the storage layer that manages the SQLite database, and the service layer that coordinates everything. We'll look at the database schema with concrete examples, understand how campaigns track progress, and cover debugging and common issues.

## The three-layer architecture

The save/load system is built in three layers, each with a specific responsibility:

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           APPLICATION LAYER                                  │
│                                                                              │
│   ┌────────────────┐     ┌────────────────────────────────────────────────┐  │
│   │  GameEngine    │────▶│              SaveLoadService                   │  │
│   │  ::save_game() │     │                                                │  │
│   │  ::load_game() │     │  • Coordinates save/load operations            │  │
│   └────────────────┘     │  • Manages save directory                      │  │
│                          │  • Caches metadata after operations            │  │
│                          │  • Provides error tracking                     │  │
│                          └───────────────────────┬────────────────────────┘  │
│                                                  │                           │
└──────────────────────────────────────────────────┼───────────────────────────┘
                                                   │
┌──────────────────────────────────────────────────┼───────────────────────────┐
│                          SERIALIZATION LAYER     │                           │
│                                                  ▼                           │
│   ┌────────────────────────────────────────────────────────────────────────┐ │
│   │                          Serialization                                 │ │
│   │                                                                        │ │
│   │  World ◀─────▶ QJsonDocument                                           │ │
│   │    │                                                                   │ │
│   │    ├── Entities ◀─────▶ QJsonObject                                    │ │
│   │    │     └── Components (Transform, Unit, Attack, Movement, etc.)      │ │
│   │    │                                                                   │ │
│   │    └── Terrain ◀─────▶ QJsonObject                                     │ │
│   │          ├── Height map data                                           │ │
│   │          ├── Biome settings                                            │ │
│   │          └── Roads, rivers, bridges                                    │ │
│   └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
└──────────────────────────────────────────────────┬───────────────────────────┘
                                                   │
┌──────────────────────────────────────────────────┼───────────────────────────┐
│                           STORAGE LAYER          │                           │
│                                                  ▼                           │
│   ┌────────────────────────────────────────────────────────────────────────┐ │
│   │                          SaveStorage                                   │ │
│   │                                                                        │ │
│   │  SQLite Database (saves.sqlite)                                        │ │
│   │    │                                                                   │ │
│   │    ├── saves              # Game save slots                            │ │
│   │    ├── campaigns          # Campaign definitions                       │ │
│   │    ├── campaign_progress  # Campaign completion status                 │ │
│   │    ├── campaign_missions  # Mission unlock/completion                  │ │
│   │    └── mission_progress   # Individual mission results                 │ │
│   │                                                                        │ │
│   │  Features:                                                             │ │
│   │    • ACID transactions                                                 │ │
│   │    • Schema versioning and migrations                                  │ │
│   │    • BLOB storage for large data                                       │ │
│   └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

The key insight is separation of concerns. The serialization layer knows how to convert game objects but doesn't care where they're stored. The storage layer knows how to persist data but doesn't understand game objects. The service layer coordinates between them and handles the plumbing.

## The save flow

When you click "Save Game," here's what happens:

```
┌───────────────┐     ┌────────────────────┐     ┌──────────────────┐
│  User clicks  │────▶│  GameEngine::      │────▶│ SaveLoadService::│
│  "Save Game"  │     │  save_game()       │     │ save_game_to_slot│
└───────────────┘     └────────────────────┘     └────────┬─────────┘
                                                          │
                                                          ▼
                      ┌────────────────────────────────────────────────────┐
                      │  1. Serialize world to JSON                        │
                      │     Serialization::serialize_world(&world)         │
                      │                                                    │
                      │  2. Build metadata                                 │
                      │     { slotName, title, timestamp, map_name, ... }  │
                      │                                                    │
                      │  3. Convert JSON to compact bytes                  │
                      │     world_doc.toJson(QJsonDocument::Compact)       │
                      │                                                    │
                      │  4. Persist to SQLite                              │
                      │     SaveStorage::save_slot(...)                    │
                      │                                                    │
                      │  5. Update cached metadata                         │
                      │     m_last_metadata = combined_metadata            │
                      └────────────────────────────────────────────────────┘
```

The serialization happens entirely in memory. We walk through every entity in the world, serialize each component to JSON, then combine it all into a QJsonDocument. This gets converted to compact JSON bytes and handed to the storage layer.

From [save_load_service.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/save_load_service.cpp):

```cpp
auto SaveLoadService::save_game_to_slot(Engine::Core::World &world,
                                        const QString &slot_name,
                                        const QString &title,
                                        const QString &map_name,
                                        const QJsonObject &metadata,
                                        const QByteArray &screenshot) -> bool {
  // Serialize entire world to JSON
  QJsonDocument const world_doc =
      Engine::Core::Serialization::serialize_world(&world);
  const QByteArray world_bytes = world_doc.toJson(QJsonDocument::Compact);

  // Build combined metadata
  QJsonObject combined_metadata = metadata;
  combined_metadata["slotName"] = slot_name;
  combined_metadata["title"] = title;
  combined_metadata["timestamp"] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

  // Persist to database
  if (!m_storage->save_slot(slot_name, title, combined_metadata,
                            world_bytes, screenshot, &storage_error)) {
    m_last_error = storage_error;
    return false;
  }

  return true;
}
```

## The load flow

Loading reverses the process:

```
┌───────────────┐     ┌────────────────────┐     ┌──────────────────┐
│  User clicks  │────▶│  GameEngine::      │────▶│ SaveLoadService::│
│  saved slot   │     │  load_game()       │     │ load_game_from_  │
└───────────────┘     └────────────────────┘     │ slot             │
                                                 └────────┬─────────┘
                                                          │
                                                          ▼
                      ┌────────────────────────────────────────────────────┐
                      │  1. Load from SQLite                               │
                      │     SaveStorage::load_slot(slot_name, ...)         │
                      │                                                    │
                      │  2. Parse JSON                                     │
                      │     QJsonDocument::fromJson(world_bytes)           │
                      │                                                    │
                      │  3. Clear existing world                           │
                      │     world.clear()                                  │
                      │                                                    │
                      │  4. Deserialize world from JSON                    │
                      │     Serialization::deserialize_world(&world, doc)  │
                      │                                                    │
                      │  5. Cache metadata for restoration                 │
                      │     m_last_metadata = metadata                     │
                      └────────────────────────────────────────────────────┘
```

The crucial step is clearing the world before deserializing. This ensures no stale entities remain. The deserialization then recreates every entity with its components exactly as they were when saved.

### Exploration comes back with the world

One thing the world's entities cannot tell you is where the player has already been. Fog that has been cleared is player knowledge, so `GameStateSerializer` writes it into the save metadata as `visibility`: the grid size plus a run-length-encoded, base64 explored mask (`game/map/explored_mask_codec.h`). Exploration is large and uniform, so the runs squash a 650x650 map into a few hundred bytes.

On load the mask is folded back in _after_ the world and the visibility grid exist, by `GameStateSerializer::restore_visibility_from_metadata()`. The restore is additive: it can only turn `Unseen` tiles into `Explored`, never take away what the restored units can already see. A mask whose dimensions do not match the restored map is ignored with a warning, and the match simply starts with fog recomputed from unit sight rather than refusing to load.

## Entity serialization

Each entity is serialized as a JSON object containing all its components. Here's what a serialized soldier looks like:

```json
{
    "id": 42,
    "transform": {
        "pos_x": 150.5,
        "pos_y": 0.0,
        "pos_z": 200.3,
        "rot_x": 0.0,
        "rot_y": 1.57,
        "rot_z": 0.0,
        "scale_x": 1.0,
        "scale_y": 1.0,
        "scale_z": 1.0,
        "has_desired_yaw": false,
        "desired_yaw": 0.0
    },
    "unit": {
        "health": 85,
        "max_health": 100,
        "speed": 3.5,
        "vision_range": 12.0,
        "unit_type": "spearman",
        "owner_id": 1,
        "nation_id": "roman_republic"
    },
    "movement": {
        "has_target": true,
        "target_x": 180.0,
        "target_y": 210.0,
        "goal_x": 180.0,
        "goal_y": 210.0,
        "vx": 2.5,
        "vz": 1.8,
        "path_pending": false,
        "path": [
            { "x": 160.0, "y": 205.0 },
            { "x": 170.0, "y": 208.0 },
            { "x": 180.0, "y": 210.0 }
        ]
    },
    "attack": {
        "range": 2.0,
        "damage": 15,
        "cooldown": 1.2,
        "time_since_last": 0.8,
        "melee_range": 1.5,
        "melee_damage": 15,
        "preferred_mode": "auto",
        "current_mode": "melee",
        "can_melee": true,
        "can_ranged": false
    },
    "stamina": {
        "stamina": 75.0,
        "max_stamina": 100.0,
        "regen_rate": 10.0,
        "depletion_rate": 20.0,
        "is_running": false,
        "run_requested": false
    }
}
```

The serialization code in [serialization.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/core/serialization.cpp) handles each component type:

```cpp
auto Serialization::serialize_entity(const Entity *entity) -> QJsonObject {
  QJsonObject entity_obj;
  entity_obj["id"] = static_cast<qint64>(entity->get_id());

  if (const auto *transform = entity->get_component<TransformComponent>()) {
    QJsonObject transform_obj;
    transform_obj["pos_x"] = transform->position.x;
    transform_obj["pos_y"] = transform->position.y;
    transform_obj["pos_z"] = transform->position.z;
    // ... rotation and scale
    entity_obj["transform"] = transform_obj;
  }

  if (const auto *unit = entity->get_component<UnitComponent>()) {
    QJsonObject unit_obj;
    unit_obj["health"] = unit->health;
    unit_obj["max_health"] = unit->max_health;
    unit_obj["speed"] = unit->speed;
    // ... other fields
    entity_obj["unit"] = unit_obj;
  }

  // ... 20+ more component types

  return entity_obj;
}
```

### Supported components

The serialization system supports all game components:

| Component                  | Key Fields                       | Purpose                               |
| -------------------------- | -------------------------------- | ------------------------------------- |
| TransformComponent         | position, rotation, scale        | Entity location and orientation       |
| RenderableComponent        | mesh_path, texture_path, visible | Visual representation                 |
| UnitComponent              | health, speed, nation_id         | Unit stats and ownership              |
| MovementComponent          | target, path, velocity           | Movement state                        |
| AttackComponent            | damage, range, cooldown          | Combat capabilities                   |
| PatrolComponent            | waypoints, current_waypoint      | Patrol behavior                       |
| ProductionComponent        | queue, build_time, rally_point   | Building production                   |
| CaptureComponent           | progress, capturing_player       | Capture state                         |
| StaminaComponent           | stamina, regen_rate, is_running  | Stamina system                        |
| HealerComponent            | healing_range, healing_amount    | Healer units                          |
| ElephantComponent          | charge_state, trample_damage     | War elephants                         |
| CombatStateComponent       | animation_state, hit_pause       | Combat animation states and hit pause |
| FormationModeComponent     | active, formation_center         | Unit formations                       |
| BuilderProductionComponent | construction_site, progress      | Builder units                         |
| HomeComponent              | population_contribution          | Population buildings                  |
| TerrainContextComponent    | is_on_bridge, is_at_hill         | Terrain awareness                     |

## The SQLite database

All persistent data lives in a SQLite database at `~/.local/share/StandardOfIron/saves/saves.sqlite` (on Linux) or the equivalent AppData location on other platforms.

### Database schema

There are two version numbers, and keeping them apart is the point.

- **`PRAGMA user_version`** is the _database schema_ version (`k_database_schema_version`,
  currently 3). It describes the tables. It changes only when a table changes.
- **`saves.snapshot_version`** is the _world snapshot_ version (`k_snapshot_version`,
  from `game/save/snapshot_contract.h`), stored on each save row. It describes what
  that one row's `world_state` blob contains.

They used to be the same number, and that cost players their campaigns: bumping the
snapshot format made every existing database "unsupported", and the unsupported path
dropped every table — saves, campaign progress and mission results alike. Now a
snapshot bump makes exactly the affected _rows_ unloadable, and touches nothing else.

The tables:

```sql
CREATE TABLE saves (
    slot_name TEXT PRIMARY KEY NOT NULL,
    title TEXT NOT NULL,
    map_name TEXT NOT NULL,
    map_path TEXT NOT NULL,
    mode TEXT NOT NULL,               -- skirmish | mission | campaign
    campaign_id TEXT NOT NULL,
    mission_id TEXT NOT NULL,
    difficulty TEXT NOT NULL,
    kind TEXT NOT NULL,               -- manual | quicksave | autosave
    play_time_seconds REAL NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    format_version INTEGER NOT NULL,  -- the on-disk record format
    snapshot_version INTEGER NOT NULL,-- what wrote world_state
    compression TEXT NOT NULL,        -- none | zlib
    world_raw_size INTEGER NOT NULL,
    world_raw_checksum TEXT NOT NULL, -- SHA-256 of the uncompressed world
    world_blob_checksum TEXT NOT NULL,-- SHA-256 of what is stored
    metadata BLOB NOT NULL,           -- JSON: camera, level, runtime, mission title
    world_state BLOB NOT NULL,        -- compressed world JSON
    screenshot BLOB                   -- PNG preview, attached after the write
);
CREATE INDEX idx_saves_updated_at ON saves (updated_at DESC);
CREATE INDEX idx_saves_kind ON saves (kind, updated_at);

CREATE TABLE campaign_progress (
    campaign_id TEXT PRIMARY KEY NOT NULL,
    completed INTEGER NOT NULL DEFAULT 0,
    unlocked INTEGER NOT NULL DEFAULT 0,
    completed_at TEXT
);

CREATE TABLE campaign_missions (
    campaign_id TEXT NOT NULL,
    mission_id TEXT NOT NULL,
    order_index INTEGER NOT NULL,
    unlocked INTEGER NOT NULL DEFAULT 0,
    completed INTEGER NOT NULL DEFAULT 0,
    completed_at TEXT,
    PRIMARY KEY (campaign_id, mission_id)
);

CREATE TABLE mission_results (
    mission_id TEXT NOT NULL,
    mode TEXT NOT NULL,
    campaign_id TEXT NOT NULL,
    completed INTEGER NOT NULL DEFAULT 0,
    completion_time REAL,
    difficulty TEXT,
    result TEXT,
    completed_at TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    PRIMARY KEY (mission_id, mode, campaign_id)
);
```

## Schema versioning, migration and recovery

`SaveStorage::initialize` opens the file, then decides what it is looking at. Every
branch either upgrades the file in place or moves it aside; none of them delete it.

```
open()                     ── fails ──▶ move the file aside, start a fresh one
   │
PRAGMA quick_check         ── not ok ─▶ move the file aside, start a fresh one
   │
user_version == 0 && empty ───────────▶ create the current schema
user_version >  current    ───────────▶ move aside (a newer build wrote it)
user_version <  current    ───────────▶ back up, then migrate step by step
   │
table shape check          ── wrong ──▶ move the file aside, start a fresh one
```

"Move the file aside" renames it to `saves.sqlite.unreadable-<timestamp>` and reports
the path through `SaveLoadService::quarantined_database_path()`, which both the save
and load panels show. The player is told where their file went; they are never told
it is gone. A migration first copies the database to `saves.sqlite.v<N>-backup`.

The table shape check matters as much as the version: a file can claim the current
`user_version` and still not be ours. Every column the code reads is checked before
anything is written.

### Migration history

| user_version | Changes                                                                |
| ------------ | ---------------------------------------------------------------------- |
| 0 → 3        | Fresh database at the current schema                                   |
| 1, 2 → 3     | `ALTER TABLE saves ADD COLUMN snapshot_version` — additive, no deletes |

Each step is additive and runs inside one transaction, so an interrupted upgrade
leaves the old database exactly as it was.

## What the save database refuses to lose

These are enforced in code and covered by `SaveStorageSafetyTest` and
`SaveLoadServiceTest` in `tests/db/`:

- **A snapshot format bump never costs campaign progress.** It makes the affected
  save rows unloadable; `list_slots` reports `loadable: false` for them and the load
  panel greys them out with the reason.
- **A save from another build is refused, not deleted.** `read_slot` compares the
  row's `snapshot_version` and fails that one read.
- **A downgraded build cannot destroy a newer database.** A higher `user_version` is
  quarantined, not rebuilt.
- **A corrupt file leaves a working save system behind.** `quick_check` runs on every
  open; a file that fails it, or that SQLite will not open at all, is quarantined and
  replaced with an empty database.
- **A write is proved before it is reported as done.** `write_slot` commits, then
  re-reads the row and compares the stored blob byte for byte against what was handed
  in. `synchronous=FULL` and WAL cover durability past that point; both pragmas are
  read back rather than assumed.
- **The game never overwrites a save the player made.** `quicksave` and `autosave_N`
  are reserved names: a manual save cannot take one, and the autosave rotation steps
  past any slot holding a different kind.
- **A failed load leaves the match alone.** Checksums, decompression, JSON parsing and
  a structural check all run before `world.clear()`.
- **Verify means "this will load".** It decompresses, re-hashes and parses the world,
  not just the stored checksum.

## Transaction handling

All database writes use transactions to ensure data integrity. The TransactionGuard class provides RAII-style transaction management:

```cpp
class TransactionGuard {
public:
  explicit TransactionGuard(QSqlDatabase &database) : m_database(database) {}

  auto begin(QString *out_error) -> bool {
    if (!m_database.transaction()) {
      // Handle error
      return false;
    }
    m_active = true;
    return true;
  }

  auto commit(QString *out_error) -> bool {
    if (!m_active) return true;
    if (!m_database.commit()) {
      rollback();
      return false;
    }
    m_active = false;
    return true;
  }

  ~TransactionGuard() {
    if (m_active) rollback();  // Auto-rollback if not committed
  }
};
```

This ensures that if anything goes wrong during a save, the database rolls back to a consistent state.

## The service layer API

SaveLoadService provides the high-level API that game code interacts with:

```cpp
class SaveLoadService {
public:
  // Save/load operations
  auto save_game_to_slot(World &world, const QString &slot_name,
                         const QString &title, const QString &map_name,
                         const QJsonObject &metadata = {},
                         const QByteArray &screenshot = {}) -> bool;

  auto load_game_from_slot(World &world, const QString &slot_name) -> bool;

  auto get_save_slots() const -> QVariantList;
  auto delete_save_slot(const QString &slot_name) -> bool;

  // Error handling
  auto get_last_error() const -> QString;
  void clear_error();

  // Metadata from last operation
  auto get_last_metadata() const -> QJsonObject;
  auto get_last_title() const -> QString;
  auto get_last_screenshot() const -> QByteArray;

  // Campaign management
  auto list_campaigns(QString *out_error = nullptr) -> QVariantList;
  auto get_campaign_progress(const QString &campaign_id) const -> QVariantMap;
  auto mark_campaign_completed(const QString &campaign_id) -> bool;

  // Mission tracking
  auto save_mission_result(const QString &mission_id, const QString &mode,
                           const QString &campaign_id, bool completed,
                           const QString &result, const QString &difficulty,
                           float completion_time) -> bool;
  auto unlock_next_campaign_mission(const QString &campaign_id,
                                    const QString &completed_mission_id) -> bool;

  // Singleton access
  static SaveLoadService *instance();
};
```

### Usage example

```cpp
// Save current game
auto *service = SaveLoadService::instance();
if (!service->save_game_to_slot(world, "quicksave", "Quick Save",
                                 current_map_name)) {
  qWarning() << "Save failed:" << service->get_last_error();
}

// Load saved game
if (!service->load_game_from_slot(world, "quicksave")) {
  qWarning() << "Load failed:" << service->get_last_error();
}

// Get list of saves for UI
QVariantList saves = service->get_save_slots();
for (const QVariant &save : saves) {
  QVariantMap slot = save.toMap();
  qInfo() << slot["title"].toString()
          << "saved at" << slot["timestamp"].toString();
}
```

## File locations

The save system stores data in platform-appropriate locations:

| Platform | Location                                                          |
| -------- | ----------------------------------------------------------------- |
| Linux    | `~/.local/share/StandardOfIron/saves/saves.sqlite`                |
| macOS    | `~/Library/Application Support/StandardOfIron/saves/saves.sqlite` |
| Windows  | `%APPDATA%/StandardOfIron/saves/saves.sqlite`                     |

The directory is created automatically on first save.

## Debugging

### Common issues

**"Save storage unavailable"**

- Database file might be locked by another process
- Check file permissions on saves directory
- Delete corrupted database file to reset

**"Corrupted save data"**

- JSON parsing failed—save file may be truncated
- Check disk space during save
- Look for `QJsonParseError` in logs

**"Save slot not found"**

- Slot name doesn't exist in database
- Case-sensitive matching—check exact name

**"Failed to begin transaction"**

- SQLite is locked
- Check for other processes accessing the database

### Logging

Enable debug logging to trace save/load operations:

```bash
QT_LOGGING_RULES="*.debug=true" ./standard_of_iron
```

Key log messages to look for:

```
Saving game to slot: quicksave
Loading game from slot: quicksave
SaveLoadService: failed to persist slot <error>
SaveLoadService: failed to load slot <error>
```

### Database inspection

You can inspect the database directly with the `sqlite3` command:

```bash
sqlite3 ~/.local/share/StandardOfIron/saves/saves.sqlite

-- List all saves
SELECT slot_name, title, timestamp FROM saves ORDER BY timestamp DESC;

-- Check schema version
PRAGMA user_version;

-- View campaign progress
SELECT * FROM campaign_missions WHERE campaign_id = 'second_punic_war';
```

## Performance characteristics

| Operation        | Typical Time | Notes                             |
| ---------------- | ------------ | --------------------------------- |
| Save (small map) | 50-100ms     | ~1000 entities                    |
| Save (large map) | 200-500ms    | ~5000 entities                    |
| Load (small map) | 100-200ms    | Includes entity creation          |
| Load (large map) | 500-1000ms   | Entity creation is the bottleneck |
| List slots       | <10ms        | Metadata only, no world_state     |

The main bottleneck is JSON parsing/generation. For very large maps, consider:

- Reducing entity count through LOD systems
- Compressing world_state with zlib before storage
- Async save/load with progress UI

## Finding your way around

| What you want to do             | Where to look                                                                                                                        |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Modify save/load flow           | [save_load_service.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/save_load_service.cpp)                     |
| Add new component serialization | [serialization.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/core/serialization.cpp)                                |
| Change database schema          | [save_storage.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/save_storage.cpp) - add migration               |
| Modify metadata fields          | [save_load_service.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/save_load_service.cpp) - combined_metadata |
| Add new campaign table          | [save_storage.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/save_storage.cpp) - migrate_to_N                |
| Test save/load                  | [tests/db/save_storage_test.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/tests/db/save_storage_test.cpp)                |
| Test serialization              | [tests/core/serialization_test.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/tests/core/serialization_test.cpp)          |

## Future improvements

Planned enhancements to the save system:

- [ ] Compressed world_state using zlib/gzip
- [ ] Async save/load with progress callbacks
- [ ] Save file validation and checksums
- [ ] Cloud save synchronization
- [ ] Save file export/import for sharing
- [ ] Replay system integration
- [ ] Differential saves for faster autosave

## API Reference

See also:

- `game/systems/save_storage.h` - Database layer API
- `game/systems/save_load_service.h` - Service layer API
- `game/core/serialization.h` - Serialization API
- `game/core/component.h` - Component definitions
