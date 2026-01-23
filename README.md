# Standard of Iron

**A historical real-time strategy engine set during the Punic Wars**

Standard of Iron delivers a complete game engine stack—rendering, audio, AI, and gameplay—purpose-built for large-scale tactical battles between Rome and Carthage. The project supports persistent campaigns with full save/load functionality, multiple playable factions with distinct unit rosters, and deep troop customization through data-driven configuration. Written in C++20, it uses OpenGL 3.3 for rendering and Qt 6 for the interface layer.

-> Main focus: army managment and tactical strategy

---

## Engine Systems

### Game Engine

The game logic layer follows an *Entity-Component-System* architecture, separating data storage from processing logic so that new gameplay features can be added without modifying existing systems.

- When units receive movement orders, the *pathfinding* module computes grid-based routes that avoid obstacles and respect formation spacing; without this, units would overlap or clip through structures.
- Damage resolution flows through a dedicated *combat system* that calculates hit detection, applies damage values, and triggers death handling—skipping any step would leave units immortal or cause silent failures.
- A centralized *AI director* evaluates threats, issues build orders, and coordinates attacks; disabling it results in passive opponents that never produce troops or respond to incursions.
- Buildings with production capability maintain a *spawn queue* that respects population caps and timers, ensuring that rapid-clicking the recruit button does not bypass limits.
- Units assigned to patrol follow a *waypoint loop* and automatically engage enemies within aggro range; omitting the aggro check would make patrols purely decorative.
- Territory changes hands through a *capture system* requiring sustained troop presence—a 3× advantage held for five seconds—so that momentary skirmishes do not flip ownership.
- The *serialization layer* writes complete world state to disk, enabling mid-campaign saves; without it, progress would reset on every launch.

### Render Engine

A custom OpenGL 3.3 pipeline handles all visual output, organized into discrete passes that can be profiled and optimized independently.

![untitled-ezgif com-optimize](https://github.com/user-attachments/assets/2405d711-1708-48ff-aaae-bb56f18881f0)

- The *scene renderer* orchestrates multi-pass drawing with depth sorting and shader batching, reducing GPU state changes that would otherwise cause frame drops.
- Each entity type has a dedicated *renderer class* (archers, barracks, horses) that encapsulates mesh binding and material setup, making it straightforward to add new unit visuals.
- Humanoid units feature *skeletal animation* combined with procedural cloth physics that responds to wind and leg movement; static meshes would look lifeless during marches.
- Terrain draws through a *ground renderer* supporting normal maps and an optional tactical grid overlay useful for range estimation.
- Visual feedback—arrows in flight, health bars, selection rings—comes from a *VFX system* that batches transparent geometry for correct blending.
- All material effects (fabric sheen, wrap lighting) are defined in GLSL *shaders* loaded at startup; missing shader files cause a fallback to flat colors.
- The *camera controller* supports rotation, elevation, edge scrolling, and follow mode, with automatic disabling over UI regions to prevent accidental panning.

### Audio Engine

Spatial sound playback relies on the *miniaudio* library, chosen for its lightweight footprint and cross-platform support.

- Sounds attached to units use *positional audio* with distance attenuation and stereo panning, so combat on the left side of the map sounds left-biased in headphones.
- An *event bus* triggers playback for combat hits, footsteps, and UI clicks; missing event bindings result in silent actions.
- Audio files load through a *resource cache* that prevents redundant disk reads when the same sound plays repeatedly.
- Separate volume sliders for master, SFX, and music allow players to mute background tracks without losing combat feedback.

### Campaign Layer

The full Punic Wars setting pits the *Roman Republic* against the *Carthaginian Empire*, each with unique unit stats and visual styling.

- Campaign progress persists across sessions via the *save/load system*, storing territory control, unit counts, and player-set rally points.
- Each map defines its own *victory conditions* in JSON—elimination, survival timer, or structure defense—so designers can vary objectives without engine changes.
- Troop stats and formation preferences live in *nation config files*, enabling balance tweaks by editing JSON rather than recompiling.

---

## Gameplay

### Strategic Systems

- Choosing a *faction* (Rome or Carthage) determines starting units, available reinforcements, and visual theming; neutral starts are not currently supported.
- Buildings with production capability maintain a *recruitment queue* that displays progress and respects population caps, preventing endless unit spam.
- Capturing an enemy or neutral barracks requires holding a 3× *troop advantage* within eight world units for five seconds; failing to maintain presence resets progress at double speed.
- The *AI opponent* follows scripted build orders, evaluates threat levels, and commits to attacks when strength thresholds are met; it does not cheat with extra resources.
- Before battle, the *skirmish setup* screen lets players pick teams, colors, and nations; skipping it defaults to Rome versus Carthage with preset colors.

### Tactical Commands

- Left-clicking a destination issues a *move order* that spreads units into formation automatically; right-clicking on the ground does the same when units are selected.
- Pressing A enters *attack mode*, changing the cursor and treating the next click as a force-attack regardless of target alliance.
- Pressing P twice sets a two-point *patrol route* where units walk between waypoints and engage enemies that enter aggro range.
- The S key sends a *stop command* that halts movement, cancels attacks, and clears patrol loops immediately.
- The H key toggles *hold position*, preventing units from chasing fleeing enemies beyond their current location.
- Buildings selected during production display a *rally point* flag; right-clicking relocates it so new units spawn closer to the front line.

### Selection Interface

- Clicking a single unit or building performs a *direct selection*, clearing any previous group; shift-click adds to the existing selection instead.
- Dragging a rectangle performs *area selection*, capturing all friendly units within the bounds—useful for grabbing scattered reinforcements.
- Pressing X triggers *select all*, gathering every controllable unit on the map into one group for mass orders.
- The selection system runs a *filter pass* each frame, automatically removing dead units so commands never target corpses.
- Right-clicking with units selected issues a *context command*: move if the target is ground, attack if it's an enemy, or interact if it's an allied structure.

### Visual Presentation

- Each faction's archers use distinct *3D models* with team-colored tunics and equipment, making ownership obvious at a glance.
- Tunics feature real-time *cloth simulation* driven by wind vectors and leg collision, adding movement realism absent from static meshes.
- Materials apply *wrap diffuse lighting* and view-dependent sheen so fabric surfaces catch light naturally during camera rotation.
- Every combat unit and structure displays a *health bar* that updates in real time; hiding health would force players to guess remaining durability.
- Arrows travel along arcing *projectile paths* computed from launch angle and gravity, visually connecting shooters to targets.
- An optional *tactical grid* overlays the terrain for players who want precise range and spacing information.
- The camera supports full *free movement*: pan with arrow keys, rotate with Q/E, elevate with R/F, and edge-scroll by pushing the cursor to screen borders (disabled when hovering UI panels).

### Victory and Defeat

- In *elimination mode*, the match ends when all enemy key structures (barracks, headquarters) are destroyed.
- In *survival mode*, the player must hold territory for a specified duration defined in the map JSON (e.g., 600 seconds).
- Defeat triggers when all player key structures fall or, in some modes, when no player units remain alive.
- All conditions are *data-driven*, meaning new objective types require JSON edits rather than code changes.

---

## Requirements

**Build-time dependencies:**

- A *C++20* compiler is required (GCC 10+, Clang 11+, or MSVC 19.29+); older standards lack features like concepts and ranges used throughout the codebase.
- The build system expects *CMake 3.21* or later; earlier versions fail to parse modern target properties.
- Qt 6.4 or newer must be installed with the Core, Quick, OpenGL, and Multimedia modules; missing modules cause link errors for UI and audio.
- The GPU driver must expose an *OpenGL 3.3 Core Profile*; integrated graphics from before 2012 may lack support.

**Runtime requirements:**

- A GPU with *OpenGL 3.3* drivers is mandatory; software rendering is not supported.
- At least 4 GB of RAM is needed, though 8 GB is recommended when running battles with several hundred units.
- Supported operating systems include *Linux* (Ubuntu 20.04+, Arch, Manjaro) and *Windows 10* or later.

---

## Build Instructions

### Linux (Ubuntu/Debian, Arch/Manjaro)

```bash
git clone https://github.com/djeada/Standard-of-Iron.git
cd Standard-of-Iron

# Install system dependencies (runs apt or pacman as appropriate)
make install

# Compile the engine and game binary
make build

# Launch the game
make run
```

### Map Pipeline Assets

- `make run` invokes the campaign map pipeline when any required outputs are missing (base textures, mesh, rivers/coastlines, provinces, and Hannibal path).
- The pipeline downloads Natural Earth data and installs Python dependencies, so it needs network access the first time it runs.
- Generated outputs live in `assets/campaign_map/` and are gitignored; tracked defaults include `campaign_state.json` and `hannibal_path.json`.
- To force a rebuild: `make run-map-pipeline map_pipeline_rebuild=1`

### Running Tests

```bash
# Execute the full test suite
make test

# Build test binary only (useful for IDE integration)
cd build && make standard_of_iron_tests

# Filter to specific test suites
./build/bin/standard_of_iron_tests --gtest_filter=SerializationTest.*
./build/bin/standard_of_iron_tests --gtest_filter=SaveStorageTest.*
```

See [tests/README.md](https://github.com/djeada/Standard-of-Iron/blob/main/tests/README.md) for additional testing documentation.

---

## Architecture

### Directory Layout

```
app/
  controllers/   Input handling, VFX triggers
  core/          Engine loop, level orchestration, state transitions

game/
  core/          ECS primitives, component definitions, world state
  systems/       Per-frame logic (movement, combat, AI, production)
  map/           Level loading, victory condition parsing
  units/         Unit type factories and stat definitions
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
  shaders/       GLSL source (vertex, fragment)
  textures/      PNG textures for units, buildings, terrain

ui/qml/          Qt Quick components (HUD, menus, dialogs)
tests/           GTest-based unit and integration tests
scripts/         Build helpers, validation scripts, deployment tools
```

### ECS Implementation

The engine uses an *Entity-Component-System* pattern to decouple data from logic.

- Each game object is represented by a 64-bit *entity ID* with no behavior of its own; attaching components determines what systems will process it.
- A *component* is a plain data struct (Transform, Unit, Movement, Health) stored contiguously for cache efficiency; polymorphism is avoided.
- A *system* iterates over entities that have a required set of components and performs one logical step per frame; ordering is explicit to avoid race conditions.

**Core component types:**

- `TransformComponent` stores world-space position, rotation, and scale; without it, an entity cannot be rendered or participate in physics.
- `UnitComponent` holds health, faction, speed, damage, and unit type; this is the primary marker for combatants.
- `MovementComponent` tracks the target position, current velocity, and pathfinding state; clearing it stops the unit.
- `AttackTargetComponent` references the target entity, attack range, and cooldown timer; removing it cancels an ongoing attack.
- `PatrolComponent` contains the waypoint list, current index, and aggro radius; patrol behavior is disabled if waypoints are empty.
- `ProductionComponent` manages the build queue, spawn timer, and rally point position; buildings without this component cannot train units.
- `BuildingComponent` holds building type, capture progress, and ownership; it distinguishes structures from mobile units.

**System execution order (per frame):**

1. *ArrowSystem* updates projectile positions and triggers hit detection.
2. *MovementSystem* advances units toward their targets and recalculates paths when blocked.
3. *PatrolSystem* cycles waypoints and scans for enemies within aggro range.
4. *CombatSystem* executes attacks, applies damage, and removes dead entities.
5. *AISystem* evaluates strategic state and issues commands to AI-owned units.
6. *ProductionSystem* decrements spawn timers and instantiates new units at rally points.
7. *SelectionSystem* synchronizes UI state with entity selection sets.

### Render Pipeline

Rendering is organized into sequential passes that allow clear profiling and targeted optimization.

1. The *scene setup* pass computes camera matrices and configures the viewport.
2. The *terrain pass* draws the ground mesh with normal mapping and an optional tactical grid.
3. The *entity pass* batches units and buildings by shader type, applying skeletal animation for humanoids and flag animation for structures.
4. The *VFX pass* renders transparent geometry (arrows, health bars, selection rings) with correct depth blending.
5. The *UI overlay* pass draws patrol flags, rally markers, and capture progress bars on top of the scene.
6. In debug builds, a *visualization pass* shows pathfinding grids and collision bounds.

**Optimizations applied:**

- Entities sharing the same shader are batched into a single draw call, reducing GPU state switches.
- *Frustum culling* skips entities outside the camera view, avoiding wasted vertex processing.
- Complex scenes use a *depth pre-pass* to minimize overdraw during the main shading pass.
- Particle effects leverage *instanced rendering* to draw many quads with a single draw call.

---

## Controls

### Camera

| Input | Action |
|-------|--------|
| Arrow Keys / WASD | Pan the camera across the battlefield |
| Q / E | Rotate view left or right |
| R / F | Raise or lower camera elevation |
| Mouse to screen edge | Edge scrolling (disabled when cursor is over UI) |

### Selection

| Input | Action |
|-------|--------|
| Left-click | Select a single unit or building |
| Click and drag | Draw a selection rectangle around multiple units |
| Shift + click | Add to the current selection |
| X | Select all controllable units on the map |
| Click empty ground | Deselect everything |

### Unit Commands

| Input | Action |
|-------|--------|
| Right-click ground | Move selected units to location |
| Right-click enemy | Attack the target |
| A, then click | Enter attack mode and force-attack the clicked target |
| M | Return to normal move cursor |
| S | Stop all current actions |
| P, click twice | Set a two-point patrol route |
| H | Hold position (units will not chase) |

### Building Commands

| Input | Action |
|-------|--------|
| Select barracks → Recruit button | Add a unit to the production queue |
| Right-click (with barracks selected) | Set the rally point for new units |

### Game Controls

| Input | Action |
|-------|--------|
| Space | Pause or resume the game |
| ESC | Cancel current command mode or open the menu |
| Speed slider | Adjust simulation speed (0.5× to 2×) |

---

## Data Configuration

### Map Files

Each map is a JSON document that defines terrain dimensions, spawn points, and victory conditions.

```json
{
  "name": "Siege of Carthage",
  "terrain": { "width": 100, "height": 100 },
  "victory": {
    "type": "elimination",
    "key_structures": ["barracks", "HQ"],
    "defeat_conditions": ["no_key_structures"]
  },
  "spawns": [
    { "type": "barracks", "x": 30, "z": 50, "player_id": 1, "nation": "rome", "maxPopulation": 100 },
    { "type": "barracks", "x": 70, "z": 50, "player_id": 2, "nation": "carthage", "maxPopulation": 100 }
  ]
}
```

- Setting the *victory type* to `elimination` ends the match when all key structures are destroyed; setting it to `survive_time` with a `duration` value requires holding out for that many seconds.
- Listing structures in *key_structures* marks them as required for victory; unlisted buildings can be lost without triggering defeat.
- Adding `no_key_structures` to *defeat_conditions* causes a loss when all key structures fall; adding `no_units` triggers defeat when the last unit dies.

### Neutral Barracks

Omitting the `player_id` field creates a *neutral barracks* that starts inactive and can be captured by any player.

```json
{ "type": "barracks", "x": 50, "z": 50, "maxPopulation": 150 }
```

- A neutral structure does not produce troops until captured; this encourages map control and prevents early-game turtling.
- Capture requires maintaining a 3× troop advantage within eight world units for five seconds; partial progress decays at double speed if advantage is lost.

### Nation Configuration

Each faction is defined in a JSON file under `assets/data/nations/`.

```json
{
  "id": "rome",
  "display_name": "Roman Republic",
  "troop_variants": {
    "archer": {
      "stat_modifiers": { "health": 110, "damage": 12 },
      "formation": "testudo",
      "renderer": "roman_archer"
    }
  }
}
```

- Baseline troop stats live in `assets/data/troops/base.json`; the *stat_modifiers* in nation files override those defaults at runtime.
- The *renderer* field selects which visual class to use; if the specified renderer is missing, the engine falls back to the default model.
- Changing a nation's *formation* preference alters how units spread when given group orders.

---

## Development

### Contributing

See [CONTRIBUTING.md](https://github.com/djeada/Standard-of-Iron/blob/main/CONTRIBUTING.md) for full guidelines covering environment setup, formatting standards, and pull request workflow.

**Quick start:**

```bash
git clone https://github.com/<your-fork>/Standard-of-Iron.git
make install   # Install system dependencies
make format    # Apply code formatting before committing
make test      # Verify all tests pass
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
    if (!screen_to_ground(QPointF(sx, sy), world_pos)) return;
    for (auto id : m_selection_system->get_selected_units()) {
        // Issue command to entity
    }
}
```

**Adding UI elements:**

Edit or add QML files in `ui/qml/`. HUD components communicate with the engine via Qt signals, so new panels only need to bind to exposed properties.

### Code Signing (Maintainers)

- For macOS notarization, follow [docs/MACOS_SIGNING.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MACOS_SIGNING.md).
- Windows builds use the `WINDOWS_CERTIFICATE` secret configured in GitHub Actions.
- Linux AppImage distribution does not require code signing.

---

## Roadmap

### Nation System Migration

The engine is transitioning from a single hardcoded nation to a scalable multi-faction architecture. This allows Rome, Carthage, and future civilizations to share troop classes while diverging on stats, formations, and visuals.

**Phase 1 — Data Foundations**

- A *TroopClass* catalog will store baseline stats (health, speed, damage, renderer) for each unit type; without it, every faction would duplicate the same data.
- Unit constructors in `game/units/` will read from the catalog instead of hardcoded literals, reducing the surface area for copy-paste errors.
- The *NationTroopVariant* structure will capture per-faction overrides (stat deltas, formation preference, renderer ID) inside the Nation object.
- Initial data will be persisted to `assets/data/troops/base.json` and per-nation JSONs so runtime behavior matches the current build.

**Phase 2 — Runtime Loading**

- A JSON loader in `game/systems/nation_loader.*` will construct Nation objects from disk and register them through *NationRegistry*.
- The *TroopProfileService* will merge catalog defaults with nation overrides and expose `get_profile(nationId, TroopType)` for systems to query.
- The production pipeline (SpawnParams, ProductionSystem, AI spawners) will thread the owning nation ID so units receive the correct profile at creation.
- Formation spacing and individual counts will read from profiles, falling back to catalog defaults when overrides are absent.

**Phase 3 — Multi-Faction Deployment**

- Roman and Carthaginian JSON definitions will ship with differentiated stats, formations, and renderer IDs.
- Shared melee infantry will rename to *Swordsman* so multiple factions can reference the same base asset while tuning stats independently.
- Gameplay systems (AI build orders, UI panels, tutorials) will resolve troop data through `NationRegistry::get_nation_for_player`.
- Faction-specific renderers (e.g., `roman_archer_renderer.cpp`) will register by renderer ID, with fallback to baseline assets if missing.
- Balance hooks for passive modifiers and tech prerequisites will live inside NationTroopVariant, avoiding engine rewrites for future expansions.

**Validation**

- Unit tests will confirm data loading, profile application, and correct production counts per nation.
- Renderer switching will be verified across factions before legacy hardcoded data is removed.

---

### Current Status

**Completed:**

- The *ECS framework* drives all game logic, separating data storage from system processing.
- A custom *OpenGL render pipeline* handles multi-pass drawing with batching and culling.
- The *AI director* issues build orders, evaluates threats, and controls units without cheating.
- A *combat system* processes attacks, applies damage, and removes dead entities each frame.
- *Victory conditions* are data-driven, supporting elimination, survival, and custom objectives.
- *Patrol routes* display visual waypoint flags and engage enemies within aggro range.
- *Rally points* direct newly spawned units to player-specified locations.
- Full *save/load* serialization preserves campaign state across sessions.
- *Spatial audio* provides positional sound for combat and movement.
- Multiple *map files* are playable with distinct layouts and objectives.

**Planned:**

- *Multiplayer networking* for LAN and online matchmaking.
- A *resource economy* with gathering and spending.
- *Campaign progression* tracking territory control and unit veterancy.
- *Mod support* with exposed data formats and tooling.

---

## License

This project is released under the MIT License. See [LICENSE](https://github.com/djeada/Standard-of-Iron/blob/main/LICENSE) for full text.

### Third-Party Licenses

- The engine uses the *Qt framework* (https://www.qt.io), licensed under the GNU Lesser General Public License v3 (LGPL v3). Qt is dynamically linked, allowing library replacement. Source code is available at https://www.qt.io/download-open-source.
- Audio playback uses *miniaudio* (public domain / MIT-0). See [THIRD_PARTY_LICENSES.md](https://github.com/djeada/Standard-of-Iron/blob/main/THIRD_PARTY_LICENSES.md) for details.

---

## Acknowledgments

Standard of Iron is built with C++20, Qt 6, and OpenGL 3.3. Development has benefited from open-source documentation, community feedback, and contributions from volunteers listed in [CONTRIBUTORS.md](https://github.com/djeada/Standard-of-Iron/blob/main/CONTRIBUTORS.md).
