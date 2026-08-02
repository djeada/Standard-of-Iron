# Standard of Iron

**A historical real-time strategy engine set during the Punic Wars**

Standard of Iron is a complete real-time strategy engine built for large-scale tactical battles between Rome and Carthage. It includes the full engine stack: rendering, audio, AI, gameplay systems, campaign persistence, and tools for creating and testing maps. The project supports persistent campaigns with save/load functionality, multiple playable factions with distinct unit rosters, and data-driven troop customization.

The engine is written in C++20, uses OpenGL 3.3 for rendering, and relies on Qt 6 for the interface layer.

Primary focus is army management and tactical strategy.

## Engine Systems

### Game Engine

The game logic layer follows an _Entity-Component-System_ architecture, in a simulation kernel that links no rendering code. This separates data storage from processing logic and lets the same match run with or without a screen.

- When units receive movement orders, the _pathfinding_ module computes grid-based routes through walkable cells. Formation spacing is applied only when initial targets are assigned; each unit then follows its own path.
- Damage resolution is handled by a dedicated _combat system_. It calculates hit detection, applies damage, and triggers death handling. Without these steps, units could become immortal or fail silently during combat.
- A centralized _AI director_ evaluates threats, issues build orders, and coordinates attacks. If disabled, opponents become passive and stop producing troops or responding to incursions.
- Buildings with production capability maintain a _spawn queue_ that respects population caps and production timers, ensuring that rapid clicking cannot bypass recruitment limits.
- Units assigned to patrol follow a _waypoint loop_ and automatically engage enemies within aggro range. Without the aggro check, patrols would only serve as visual movement patterns.
- Territory changes hands through a _capture system_ that requires sustained troop presence. A 3× advantage must be held for five seconds so that brief skirmishes do not immediately flip ownership.
- The _serialization layer_ writes the complete world state to disk, enabling mid-campaign saves. Without it, progress would reset every time the game restarts.

### Render Engine

A custom OpenGL 3.3 pipeline handles all visual output. Rendering is organized into separate passes so each stage can be profiled and optimized independently.

![untitled-ezgif com-optimize](https://github.com/user-attachments/assets/2405d711-1708-48ff-aaae-bb56f18881f0)

- The _scene renderer_ coordinates multi-pass drawing with depth sorting and shader batching, reducing GPU state changes that could otherwise cause frame drops.
- Each entity type has a dedicated _renderer class_ for units, buildings, and effects. These classes encapsulate mesh binding and material setup, making new visuals easier to add.
- Humanoid units use _skeletal animation_ combined with procedural cloth physics that reacts to wind and leg movement, giving marching units a more natural appearance.
- Terrain is drawn by a _ground renderer_ with support for normal maps and an optional tactical grid overlay for range and spacing estimation.
- Visual feedback such as arrows, health bars, and selection rings is produced by a _VFX system_ that batches transparent geometry for correct blending.
- Material effects, including fabric sheen and wrap lighting, are defined in GLSL _shaders_ loaded at startup. If shader files are missing, the renderer falls back to flat colors.
- The _camera controller_ supports rotation, elevation, edge scrolling, and follow mode. It automatically disables scrolling over UI regions to prevent accidental camera movement.

### Audio Engine

Spatial audio playback uses the _miniaudio_ library, chosen for its lightweight footprint and cross-platform support.

- Sounds attached to units use _positional audio_ with distance attenuation and stereo panning, so battles on the left side of the map sound left-biased in headphones.
- An _event bus_ triggers playback for combat hits, footsteps, and UI clicks. Missing event bindings result in silent actions.
- Audio files are loaded through a _resource cache_ to avoid redundant disk reads when the same sound plays repeatedly.
- Separate volume sliders for master, SFX, and music let players mute background tracks without losing important combat feedback.

### Campaign Layer

The Punic Wars setting pits the _Roman Republic_ against the _Carthaginian Empire_, with each faction using unique unit stats and visual styling.

- Campaign progress persists across sessions through the _save/load system_, which stores territory control, unit counts, and player-defined rally points.
- Each map defines its own _victory conditions_ in JSON, such as elimination, survival timers, or structure defense. This allows designers to vary objectives without engine changes.
- Troop stats and formation preferences are stored in _nation configuration files_, allowing balance changes through JSON instead of recompilation.

## Gameplay

### Strategic Systems

- Choosing a _faction_ such as Rome or Carthage determines starting units, available reinforcements, and visual theming. Neutral starts are not currently supported.
- Production buildings maintain a _recruitment queue_ that displays progress and respects population caps, preventing endless unit spam.
- Capturing an enemy or neutral barracks requires holding a 3× _troop advantage_ within eight world units for five seconds. If control is lost, capture progress decays at double speed.
- The _AI opponent_ follows scripted build orders, evaluates threat levels, and commits to attacks when strength thresholds are met. It does not cheat with extra resources.
- Before battle, the _skirmish setup_ screen lets players choose teams, colors, and nations. If skipped, the game defaults to Rome versus Carthage with preset colors.

### Tactical Commands

- Left-clicking a destination issues a _move order_ that automatically spreads selected units into formation. Right-clicking the ground performs the same action when units are selected.
- Pressing A enters _attack mode_, changing the cursor and treating the next click as a force-attack command, regardless of target alliance.
- Pressing P twice sets a two-point _patrol route_. Units move between the waypoints and engage enemies that enter their aggro range.
- Pressing S sends a _stop command_ that immediately halts movement, cancels attacks, and clears patrol loops.
- Pressing H toggles _hold position_, preventing units from chasing fleeing enemies beyond their current location.
- Buildings selected during production display a _rally point_ flag. Right-clicking relocates it so newly produced units spawn closer to the front line.

### Selection Interface

- Clicking a single unit or building performs a _direct selection_ and clears any previous group. Shift-clicking adds the target to the current selection.
- Dragging a rectangle performs _area selection_, selecting all friendly units inside the bounds. This is useful for gathering scattered reinforcements.
- Pressing X triggers _select all_, gathering every controllable unit on the map into one group for mass orders.
- The selection system runs a _filter pass_ each frame, automatically removing dead units so commands never target corpses.
- Right-clicking with units selected issues a _context command_: move if the target is ground, attack if it is an enemy, or interact if it is an allied structure.

### Visual Presentation

- Each faction's archers use distinct _3D models_ with team-colored tunics and equipment, making ownership clear at a glance.
- Tunics use real-time _cloth simulation_ driven by wind vectors and leg collision, adding movement realism that static meshes cannot provide.
- Materials apply _wrap diffuse lighting_ and view-dependent sheen so fabric surfaces catch light naturally as the camera moves.
- Every combat unit and structure displays a _health bar_ that updates in real time, giving players immediate feedback on durability.
- Arrows travel along arcing _projectile paths_ calculated from launch angle and gravity, visually linking shooters to their targets.
- An optional _tactical grid_ overlays the terrain for players who want precise range and spacing information.
- The camera supports full _free movement_: pan with arrow keys, rotate with Q/E, change elevation with R/F, and edge-scroll by moving the cursor to the screen borders. Edge scrolling is disabled while hovering over UI panels.

### Victory and Defeat

- In _elimination mode_, the match ends when all enemy key structures, such as barracks or headquarters, are destroyed.
- In _survival mode_, the player must hold territory for a specified duration defined in the map JSON, such as 600 seconds.
- Defeat occurs when all player key structures fall or, in some modes, when no player units remain alive.
- All win and loss conditions are _data-driven_, meaning new objective types can be added through JSON rather than code changes.

## Requirements

**Build-time dependencies:**

- A _C++20_ compiler is required, such as GCC 10+, Clang 11+, or MSVC 19.29+. Older standards lack features used throughout the codebase, including concepts and ranges.
- The build system requires _CMake 3.21_ or later. Earlier versions may fail to parse modern target properties.
- Qt 6.4 or newer must be installed with the Core, Quick, OpenGL, and Multimedia modules. Missing modules can cause link errors for UI or audio components.
- The GPU driver must expose an _OpenGL 3.3 Core Profile_. Integrated graphics from before 2012 may not support the required features.

**Runtime requirements:**

- A GPU with _OpenGL 3.3_ drivers is expected for normal play. A CPU rasteriser backend also ships and can be selected with `--force-software` (or `--quality none`); it draws a reduced-fidelity image and exists for diagnostics and for machines without a 3.3 driver, not as a supported way to play.
- At least 4 GB of RAM is required, though 8 GB is recommended for battles with several hundred units.
- Supported operating systems include _Linux_ distributions such as Ubuntu 20.04+, Arch, and Manjaro, as well as _Windows 10_ or later.

## Build Instructions

### Linux (Ubuntu/Debian, Arch/Manjaro)

```bash
git clone https://github.com/djeada/Standard-of-Iron.git
cd Standard-of-Iron

# Install system dependencies (uses apt or pacman as appropriate)
make install

# Compile the engine and game binary
make build

# Launch the game
make run

# Launch the arena playground tool
make arena
```

### Map Pipeline Assets

- `make run` invokes the campaign map pipeline when required outputs are missing, including base textures, meshes, rivers, coastlines, provinces, and the Hannibal path.
- `make arena` uses the same map-pipeline/bootstrap flow, then builds only `arena_app` before launching the standalone Qt/OpenGL playground.
- Arena includes declarative rendered-gameplay scenarios for interactive inspection, local batch PASS/FAIL runs, and machine-readable traces. See [`tools/arena/README.md`](tools/arena/README.md) for commands and artifacts.
- In the arena tool, controls mirror the main game more closely: left click or drag selects, right click issues move or attack orders, arrow keys pan, Q/E yaw, R/F orbit pitch, the mouse wheel zooms, and F1 or ? toggles the help overlay.
- The arena unit panel supports batch spawning, opposing or mirrored quick-setup spawns, per-unit member-count overrides for single-soldier previews, riderless mounted previews, forced full-detail creature previews, and a live selection summary showing side, health, members, center, and composition.
- The pipeline downloads Natural Earth data and installs Python dependencies, so it requires network access the first time it runs.
- Generated outputs live in `assets/campaign_map/` and are ignored by Git. Tracked defaults include `campaign_state.json` and `hannibal_path.json`.
- Generated creature animation outputs live in `assets/creatures/`. `make build` regenerates them with `bpat_baker`, and `make run` loads them through its build dependency.
- To force a rebuild, run:

```bash
make run-map-pipeline map_pipeline_rebuild=1
```

### Running Tests

```bash
# Execute the full test suite (five binaries, split by link surface)
make test

# Build the test binaries only (useful for IDE integration)
cd build && make soi_test_binaries

# Filter to specific test suites
./build/bin/simulation_tests --gtest_filter=SerializationTest.*
./build/bin/persistence_tests --gtest_filter=SaveStorageTest.*
```

See [tests/README.md](https://github.com/djeada/Standard-of-Iron/blob/main/tests/README.md) for additional testing documentation.

## Architecture

### Directory Layout

```
app/
  controllers/   Input handling, VFX triggers
  core/          Composition root, level orchestration, state transitions
  viewmodels/    Focused QML-facing objects split out of GameEngine

game/
  core/          ECS primitives, component definitions, world state
  session/       Per-match context: clock, RNG, and the registries a match owns
  command/       Typed command model, validation, queue, dispatch
  save/          The versioned snapshot contract
  systems/       Per-tick logic (movement, combat, AI, production)
  view/          Gameplay services that need a camera (selection controller)
  map/           Level loading, victory condition parsing
  units/         Unit type factories and stat definitions
  util/          Shared helpers the kernel and the app both use
  visuals/       Team colors, visual configuration

render/
  gl/            OpenGL wrappers (shaders, buffers, VAOs)
  entity/        Renderer classes for units, buildings, effects
  geom/          Procedural geometry (arrows, flags, rings)
  ground/        Terrain mesh and grid overlay
  humanoid/      Skeletal animation and cloth physics

assets/
  audio/         WAV and OGG sound files
  data/          JSON configs for troops, nations, rules
  maps/          Level definitions with spawn and victory data
  meshes/        Binary mesh files
  shaders/       GLSL source files (vertex, fragment)
  textures/      PNG textures for units, buildings, and terrain

ui/qml/          Qt Quick components (HUD, menus, dialogs)
tests/           GTest-based unit and integration tests
scripts/         Build helpers, validation scripts, deployment tools
```

### ECS Implementation

The engine uses an _Entity-Component-System_ pattern to decouple data from logic.

- Each game object is a 64-bit _generational entity handle_: a 32-bit slot index plus a 32-bit generation, bumped whenever the slot is recycled. A handle to a dead entity therefore fails to resolve instead of silently addressing whatever took its place. Entities live in a contiguous slot array, so resolving a handle is a bounds check and a generation compare.
- A _component_ is a data struct such as Transform, Unit, Movement or Production. Each type has its own chunked pool, so instances of one type are contiguous in memory rather than one heap allocation each. Components still derive from a small polymorphic base with a virtual destructor; the base carries no behaviour, and access goes through a dense per-type id rather than RTTI.
- Membership per component type is a _sparse set_: a contiguous array of the handles that own the component, plus an index from slot to position. Iterating everything with a component is a straight walk, and add/remove are O(1).
- A _system_ iterates over entities with the required component set and performs one logical step per fixed tick. System ordering is explicit to avoid race conditions.

Full detail, including the layer boundaries and where they are enforced, is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Core component types:**

- `TransformComponent` stores world-space position, rotation, and scale. Without it, an entity cannot be rendered or participate in spatial logic.
- `UnitComponent` stores health, faction, speed, damage, and unit type. It is the primary marker for combatants.
- `MovementComponent` tracks only target/goal position, current velocity, and assigned waypoints. Clearing it stops the unit.
- `AttackTargetComponent` references the target entity, attack range, and cooldown timer. Removing it cancels an ongoing attack.
- `PatrolComponent` contains the waypoint list, current index, and aggro radius. Patrol behavior is disabled if the waypoint list is empty.
- `ProductionComponent` manages the build queue, spawn timer, and rally point position. Buildings without this component cannot train units.
- `BuildingComponent` stores building type, capture progress, and ownership. It distinguishes structures from mobile units.

**System execution order per fixed tick** (abridged; `game/systems/runtime_system_registry.cpp` is the full list):

1. _CommandSystem_ drains the match's command queue, so every order — player, AI or replay — is applied before any other system observes the world.
2. _ArrowSystem_ updates projectile positions and triggers hit detection.
3. _MovementSystem_ advances units along their assigned path, recovers units that are inside invalid cells, and stops only for explicit movement overrides or arrival.
4. _PatrolSystem_ cycles waypoints and scans for enemies within aggro range.
5. _CombatSystem_ executes attacks, applies damage, and removes dead entities.
6. _AISystem_ evaluates strategic state and submits commands for AI-owned units, which the next tick's _CommandSystem_ applies.
7. _ProductionSystem_ decrements spawn timers and instantiates new units at rally points.
8. _SelectionSystem_ synchronizes UI state with entity selection sets.

### Render Pipeline

Rendering is organized into sequential passes for clearer profiling and targeted optimization.

1. The _scene setup_ pass computes camera matrices and configures the viewport.
2. The _terrain pass_ draws the ground mesh with normal mapping and an optional tactical grid.
3. The _entity pass_ batches units and buildings by shader type, applying skeletal animation for humanoids and flag animation for structures.
4. The _VFX pass_ renders transparent geometry such as arrows, health bars, and selection rings with correct depth blending.
5. The _UI overlay_ pass draws patrol flags, rally markers, and capture progress bars on top of the scene.
6. In debug builds, a _visualization pass_ shows pathfinding grids and collision bounds.

**Optimizations applied:**

- Entities sharing the same shader are batched into a single draw call, reducing GPU state switches.
- _Frustum culling_ skips entities outside the camera view, avoiding unnecessary vertex processing.
- Complex scenes use a _depth pre-pass_ to minimize overdraw during the main shading pass.
- Particle effects use _instanced rendering_ to draw many quads with a single draw call.

## Controls

Every command below is rebindable, and the defaults are what the tables show.
Settings → Controls lists the full catalog and refuses a chord another command
already holds without telling you what it would break first. Accessibility
options — interface scale, colour-vision-safe team palettes, selection-ring
patterns, edge-scroll and camera-motion controls — are described in
[docs/ACCESSIBILITY.md](docs/ACCESSIBILITY.md).

### Camera

| Input                | Action                                              |
| -------------------- | --------------------------------------------------- |
| Arrow Keys / WASD    | Pan the camera across the battlefield               |
| Q / E                | Rotate the view left or right                       |
| R / F                | Raise or lower camera elevation                     |
| Mouse to screen edge | Edge scrolling, disabled when the cursor is over UI |

### Selection

| Input              | Action                                           |
| ------------------ | ------------------------------------------------ |
| Left-click         | Select a single unit or building                 |
| Click and drag     | Draw a selection rectangle around multiple units |
| Shift + click      | Add to the current selection                     |
| X                  | Select all controllable units on the map         |
| Click empty ground | Deselect everything                              |

### Unit Commands

| Input              | Action                                                |
| ------------------ | ----------------------------------------------------- |
| Right-click ground | Move selected units to the target location            |
| Right-click enemy  | Attack the target                                     |
| A, then click      | Enter attack mode and force-attack the clicked target |
| M                  | Return to the normal move cursor                      |
| S                  | Stop all current actions                              |
| P, click twice     | Set a two-point patrol route                          |
| H                  | Hold position so units will not chase                 |

### Building Commands

| Input                              | Action                             |
| ---------------------------------- | ---------------------------------- |
| Select barracks → Recruit button   | Add a unit to the production queue |
| Right-click with barracks selected | Set the rally point for new units  |

### Game Controls

| Input        | Action                                                       |
| ------------ | ------------------------------------------------------------ |
| Space        | Pause or resume the game                                     |
| ESC          | Cancel the current command mode or open the menu             |
| Enter        | Switch between commanding the army and leading the commander |
| F5 / F9      | Quick save and quick load                                    |
| Speed slider | Adjust simulation speed from 0.5× to 2×                      |

## Data Configuration

### Map Files

Each map is a JSON document that defines terrain dimensions, spawn points, and victory conditions.

```json
{
    "name": "Siege of Carthage",
    "terrain": {
        "width": 100,
        "height": 100
    },
    "victory": {
        "type": "elimination",
        "key_structures": ["barracks", "HQ"],
        "defeat_conditions": ["no_commander", "only_commander_remaining"]
    },
    "spawns": [
        {
            "type": "barracks",
            "x": 30,
            "z": 50,
            "player_id": 1,
            "nation": "rome",
            "maxPopulation": 100
        },
        {
            "type": "barracks",
            "x": 70,
            "z": 50,
            "player_id": 2,
            "nation": "carthage",
            "maxPopulation": 100
        }
    ]
}
```

- Setting the _victory type_ to `elimination` ends the match when all key structures are destroyed. Setting it to `survive_time` with a `duration` value requires holding out for that many seconds.
- Listing structures in _key_structures_ marks them as required for victory. Unlisted buildings can be lost without triggering defeat.
- The default defeat model is commander-centric: `no_commander` loses when the commander dies, and `only_commander_remaining` loses when the commander is the only thing left after troops and barracks are gone.
- `no_key_structures` and `no_units` remain available as explicit opt-in defeat rules.
- See [docs/VICTORY_SYSTEM.md](docs/VICTORY_SYSTEM.md) for the full runtime rule model and extension workflow.

### Neutral Barracks

Omitting the `player_id` field creates a _neutral barracks_ that starts inactive and can be captured by any player.

```json
{
    "type": "barracks",
    "x": 50,
    "z": 50,
    "maxPopulation": 150
}
```

- A neutral structure does not produce troops until captured, encouraging map control and reducing early-game turtling.
- Capture requires maintaining a 3× troop advantage within eight world units for five seconds. Partial progress decays at double speed when that advantage is lost.

### Nation Configuration

Each faction is defined in a JSON file under `assets/data/nations/`.

```json
{
    "id": "rome",
    "display_name": "Roman Republic",
    "troop_variants": {
        "archer": {
            "stat_modifiers": {
                "health": 110,
                "damage": 12
            },
            "formation": "testudo",
            "renderer": "roman_archer"
        }
    }
}
```

- Baseline troop stats live in `assets/data/troops/base.json`. The _stat_modifiers_ in nation files override those defaults at runtime.
- The _renderer_ field selects which visual class to use. If the specified renderer is missing, the engine falls back to the default model.
- Changing a nation's _formation_ preference changes how its units spread out when given group orders.

## Development

### Contributing

See [CONTRIBUTING.md](https://github.com/djeada/Standard-of-Iron/blob/main/CONTRIBUTING.md) for full guidelines covering environment setup, formatting standards, and pull request workflow.

**Quick start:**

```bash
git clone https://github.com/<your-fork>/Standard-of-Iron.git
make install          # Install system dependencies
make format-bootstrap # Install the pinned formatting/linting toolchain
make hooks-install    # Install the pre-commit git hooks
make format           # Apply code formatting before committing
make quality          # Formatting + linting gates, no compiler needed
make test             # Verify all tests pass
```

### Extending the Engine

**Adding a new unit type:**

1. Create a unit definition in `game/units/<unit_type>.cpp` that registers components and default stats.
2. Implement a renderer in `render/entity/<unit_type>_renderer.cpp` to handle mesh binding and materials.
3. Register the factory in `UnitFactoryRegistry` so production systems can instantiate it.
4. Add troop entries to the relevant nation JSON files under `assets/data/nations/`.

**Adding a custom command:**

```cpp
// game_engine.h
Q_INVOKABLE void onCustomCommand(qreal sx, qreal sy);

// game_engine.cpp
void GameEngine::onCustomCommand(qreal sx, qreal sy) {
  QVector3D world_pos;
  if (!screen_to_ground(QPointF(sx, sy), world_pos))
    return;

  for (auto id : m_selection_system->get_selected_units()) {
    // Issue command to entity
  }
}
```

**Adding UI elements:**

Edit or add QML files in `ui/qml/`. HUD components communicate with the engine through Qt signals, so new panels only need to bind to exposed properties.

### Code Signing for Maintainers

- For macOS notarization, follow [docs/MACOS_SIGNING.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MACOS_SIGNING.md).
- Windows builds use the `WINDOWS_CERTIFICATE` secret configured in GitHub Actions.
- Linux AppImage distribution does not require code signing.

## Roadmap

### Nation System Migration

The engine is moving from a single hardcoded nation toward a scalable multi-faction architecture. This allows Rome, Carthage, and future civilizations to share troop classes while differing in stats, formations, and visuals.

**Phase 1 — Data Foundations**

- A _TroopClass_ catalog will store baseline stats such as health, speed, damage, and renderer ID for each unit type. Without it, each faction would duplicate the same data.
- Unit constructors in `game/units/` will read from the catalog instead of hardcoded literals, reducing copy-paste errors.
- The _NationTroopVariant_ structure will capture per-faction overrides, including stat deltas, formation preference, and renderer ID, inside the Nation object.
- Initial data will be persisted to `assets/data/troops/base.json` and per-nation JSON files so runtime behavior matches the current build.

**Phase 2 — Runtime Loading**

- A JSON loader in `game/systems/nation_loader.*` will construct Nation objects from disk and register them through _NationRegistry_.
- The _TroopProfileService_ will merge catalog defaults with nation overrides and expose `get_profile(nationId, TroopType)` for systems to query.
- The production pipeline, including SpawnParams, ProductionSystem, and AI spawners, will pass the owning nation ID so units receive the correct profile at creation.
- Formation spacing and individual counts will read from profiles, falling back to catalog defaults when overrides are absent.

**Phase 3 — Multi-Faction Deployment**

- Roman and Carthaginian JSON definitions will ship with differentiated stats, formations, and renderer IDs.
- Shared melee infantry will be renamed to _Swordsman_ so multiple factions can reference the same base asset while tuning stats independently.
- Gameplay systems such as AI build orders, UI panels, and tutorials will resolve troop data through `NationRegistry::get_nation_for_player`.
- Faction-specific renderers, such as `roman_archer_renderer.cpp`, will register by renderer ID and fall back to baseline assets if missing.
- Balance hooks for passive modifiers and tech prerequisites will live inside NationTroopVariant, avoiding engine rewrites for future expansions.

**Validation**

- Unit tests will confirm data loading, profile application, and correct production counts per nation.
- Renderer switching will be verified across factions before legacy hardcoded data is removed.

### Current Status

**Completed:**

- The _ECS framework_ drives all game logic, separating data storage from system processing.
- A custom _OpenGL render pipeline_ handles multi-pass drawing with batching and culling.
- The _AI director_ issues build orders, evaluates threats, and controls units without cheating.
- A _combat system_ processes attacks, applies damage, and removes dead entities each frame.
- _Victory conditions_ are data-driven and support elimination, survival, and custom objectives.
- _Patrol routes_ display visual waypoint flags and engage enemies within aggro range.
- _Rally points_ direct newly spawned units to player-specified locations.
- Full _save/load_ serialization preserves campaign state across sessions.
- _Spatial audio_ provides positional sound for combat and movement.
- Multiple _map files_ are playable with distinct layouts and objectives.
- A _resource economy_ with gathering, spending, and marketplace trade.
- _Campaign progression_ with mission unlocking and per-slot save metadata.
- A _simulation kernel_ (`game_sim`) that builds and runs with no renderer linked, exercised by `bin/simulation_tests`, which links that kernel and nothing else.
- A _session context_ that owns all per-match state, so two matches can run in one process and tests get isolation.
- A _canonical command pipeline_: player, AI and scripted orders share one validation pass, one queue and one execution point per tick.
- A _fixed-tick simulation clock_ and a seeded deterministic RNG, both persisted in saves.
- A _versioned snapshot contract_ that classifies every component as serialised, rebuilt, presentational or campaign-level, enforced by tests.

**In progress:**

- Migrating gameplay call sites from the ambient `instance()` accessors to explicit `SessionContext&` parameters.
- Moving `GameEngine`'s QML surface onto focused view models; the save-slot browser and placement have moved, campaign and camera control are next.

**Planned:**

- _Multiplayer networking_ for LAN and online matchmaking.
- _Replay recording and playback_ on top of the command stream.
- _Unit veterancy_ and territory control across a campaign.
- _Mod support_ with exposed data formats and tooling.

## License

This project is released under the MIT License. See [LICENSE](https://github.com/djeada/Standard-of-Iron/blob/main/LICENSE) for the full text.

### Third-Party Licenses

- The engine uses the _Qt framework_ ([https://www.qt.io](https://www.qt.io)), licensed under the GNU Lesser General Public License v3 (LGPL v3). Qt is dynamically linked, allowing library replacement. Source code is available at [https://www.qt.io/download-open-source](https://www.qt.io/download-open-source).
- Audio playback uses _miniaudio_ (public domain / MIT-0). See [THIRD_PARTY_LICENSES.md](https://github.com/djeada/Standard-of-Iron/blob/main/THIRD_PARTY_LICENSES.md) for details.

## Acknowledgments

Standard of Iron is built with C++20, Qt 6, and OpenGL 3.3. Development has benefited from open-source documentation, community feedback, and contributions from the volunteers listed in [CONTRIBUTORS.md](https://github.com/djeada/Standard-of-Iron/blob/main/CONTRIBUTORS.md).
