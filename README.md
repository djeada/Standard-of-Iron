<p align="center">
  <img src="assets/visuals/standard_of_iron.png" alt="Standard of Iron banner" width="230">
</p>

<h1 align="center">Standard of Iron</h1>

<p align="center">
  <strong>A large-scale strategy game of formations, command, and survival during the Second Punic War.</strong>
</p>

<p align="center">
  <a href="https://github.com/djeada/Standard-of-Iron/releases"><img src="https://img.shields.io/badge/version-v0.1.0-b45336" alt="Version 0.1.0"></a>
  <a href="https://github.com/djeada/Standard-of-Iron/actions/workflows/pr.yml"><img src="https://github.com/djeada/Standard-of-Iron/actions/workflows/pr.yml/badge.svg" alt="Build status"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/code-MIT-8c6a3e" alt="MIT license"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-5c6b73" alt="C++20">
  <img src="https://img.shields.io/badge/Qt-6.4%2B-3f7d57" alt="Qt 6.4 or newer">
</p>

<p align="center">
  <a href="#download">Download</a> ·
  <a href="#gameplay">Gameplay</a> ·
  <a href="#building-from-source">Build</a> ·
  <a href="#architecture">Architecture</a> ·
  <a href="CONTRIBUTING.md">Contribute</a>
</p>

Standard of Iron is an open-source, single-player real-time strategy game in which Rome and Carthage fight across an altered Second Punic War. The player commands formations from the strategic view, can take direct control of a battlefield commander, runs a settlement economy, assaults fortified positions, and encounters the supernatural Iron Sepulcher during the campaign.

The game is written in C++20 with Qt 6 and a custom tiered OpenGL renderer. Its simulation kernel is separate from presentation, so the same gameplay systems are used by the live game, headless simulation, deterministic replay verification, balance tooling, and developer scenarios.

> [!NOTE]
> The project is pre-1.0. Data and save formats are versioned and the repository includes migration/recovery paths where implemented, but 0.x releases should not be treated as a permanent external file-format contract.

![Standard of Iron main menu](docs/screenshots/main-menu.webp)

## At a glance

|             | Current scope                                                                                                   |
| ----------- | --------------------------------------------------------------------------------------------------------------- |
| Campaign    | **The Barcid Road**, eight missions from the Rhône crossing to Zama                                             |
| Tutorial    | **Field Training**, covering orders, economy, building, armies, and defence                                     |
| Factions    | Rome and Carthage are playable; the Iron Sepulcher is a campaign/world threat                                   |
| Command     | Top-down RTS control and direct commander combat in the same match                                              |
| Forces      | Infantry, archers, cavalry, healers, builders, commanders, siege engines, elephants, civilians, and wildlife    |
| Formations  | Nation doctrine, troop roles, authored layouts, shield formations, cavalry wedges, and army-level grouping      |
| Languages   | Translation catalogues for English, German, Spanish, Brazilian Portuguese, Arabic, Turkish, Polish, and Russian |
| Platforms   | Linux, macOS, and Windows                                                                                       |
| Multiplayer | The shipped application is single-player; no network multiplayer mode is wired into the current game            |
| Replay      | Command recording, replay, deterministic verification, and headless replay are implemented                      |

## Gameplay

### Fight at army scale

- Select individual troops or groups and issue move, attack, patrol, guard, hold, and context-sensitive orders.
- Deploy formations based on nation doctrine, troop role, terrain, and battlefield intent.
- Combine infantry, ranged troops, cavalry, elephants, siege engines, healers, builders, and commander abilities.
- Assault walls, gates, towers, and capturable structures while projectiles, fire, morale, and melee contact change the field.

### Lead from the front

The player can switch between the strategic camera and direct commander control during a battle. Commander control includes authored melee actions, blocking/dodging, lock-on, abilities, and ranged combat when the selected commander loadout supports it. Army orders continue to run while direct control is active.

### Run the settlement economy

- Builders gather timber, stone, iron, and food and haul gathered loads back to a barracks stockpile before they are credited.
- Farms grow grain; sheep provide another food source.
- Homes recruit civilians, civilians deliver manpower, and military buildings spend reserve/resources to recruit troops.
- Builders construct and repair structures, walls, and gates; marketplaces provide trading.
- Quick-save, manual saves, autosaves, previews, campaign progress, compression, checksums, schema migration, and database recovery are part of the current save system.

See [docs/ECONOMY_GUIDANCE.md](docs/ECONOMY_GUIDANCE.md), [docs/FOOD_AND_FARMS.md](docs/FOOD_AND_FARMS.md), and [docs/SAVE_LOAD_SYSTEM.md](docs/SAVE_LOAD_SYSTEM.md).

### March the Barcid Road

The campaign contains eight missions: Crossing the Rhône, Crossing the Alps, Battle of Ticino, Battle of Trebia, Battle of Lake Trasimene, Battle of Cannae, The Campanian Vigil, and Battle of Zama.

Missions use data-driven objectives, waves, commander rules, map regions, rewards, and defeat conditions. Their design intent is documented in [docs/CAMPAIGN_MISSIONS.md](docs/CAMPAIGN_MISSIONS.md), while the authoring schema is documented in [docs/MISSION_FRAMEWORK.md](docs/MISSION_FRAMEWORK.md).

![The Barcid Road campaign war table](docs/screenshots/campaign-war-table.webp)

### Accessibility and input

- Gameplay commands are rebindable with primary/alternate chords and context-aware conflict handling.
- Interface scale, reduced motion, camera-motion/effect controls, edge-scroll controls, colour-vision palettes, and patterned team rings are exposed through Settings.
- Arabic uses right-to-left layout support.
- The repository ships translation catalogues for eight languages listed above.

See [docs/ACCESSIBILITY.md](docs/ACCESSIBILITY.md) and [docs/CAMERA_CONTROLS.md](docs/CAMERA_CONTROLS.md).

## Replays and headless simulation

Replay recording is part of the current command pipeline.

Record a match:

```sh
standard_of_iron --record-replay match.soireplay
```

Replay and verify it:

```sh
standard_of_iron --replay match.soireplay --replay-verify
```

A replay stores the accepted command stream and periodic world digests. During verified playback, local input and AI command generation are excluded; the process exits non-zero at the first detected simulation divergence.

`soi_headless` uses the same simulation without a window and supports record/replay/verify workflows. `battlefield_gameplay_verifier --determinism-runs N` runs deterministic scenario checks repeatedly and reports divergence details.

## Download

Tagged release packages are published on the [GitHub Releases page](https://github.com/djeada/Standard-of-Iron/releases). Release packaging writes SHA-256 checksum files and runs packaged-game self-tests before publication.

### Linux

The Linux workflow produces an AppImage for x86-64.

### macOS

The macOS workflow reads the architecture set from the installed Qt framework, configures `CMAKE_OSX_ARCHITECTURES` to match it, and verifies the resulting executable with `lipo`. The package tag is `universal`, `arm64`, or `x86_64` according to the actual Qt/binary slices rather than being assumed in advance.

The workflow always re-seals the deployed bundle with an ad-hoc signature. Developer ID signing and notarization are optional credential-driven stages. The exact current package order and its DMG-signing constraint are documented in [docs/MACOS_SIGNING.md](docs/MACOS_SIGNING.md).

### Windows

The Windows workflow produces an x64 ZIP. When `WINDOWS_CERTIFICATE` and `WINDOWS_CERTIFICATE_PASSWORD` are available, it signs `standard_of_iron.exe` with Authenticode, SHA-256, and an RFC 3161 timestamp, then verifies the signature with SignTool. See [docs/WINDOWS_CODE_SIGNING.md](docs/WINDOWS_CODE_SIGNING.md).

## Requirements

### Runtime

- A 64-bit Linux, macOS, or Windows system.
- OpenGL **3.3 Core** is the portable rendering floor.
- Higher OpenGL feature tiers enable faster rendering paths when the context supports them.
- macOS uses renderer paths compatible with Apple's OpenGL ceiling.
- The Windows package includes a Mesa llvmpipe fallback.
- The separate CPU rasterizer can be selected with `--force-software`; it is a diagnostic/reduced-fidelity fallback rather than the normal renderer.

### Source build

- CMake 3.21 or newer
- A C++20 compiler
- Qt 6.4 or newer with Core, Widgets, Quick/QML, Quick Controls 2, SQL, and OpenGL; Multimedia is used when available
- OpenGL development files, Python 3, and FFmpeg with Vorbis support
- Network access on the first campaign-map pipeline run when source datasets/dependencies are not already cached

## Building from source

The repository Makefile wraps the supported developer workflow:

```sh
git clone https://github.com/djeada/Standard-of-Iron.git
cd Standard-of-Iron

make install
make run
```

Useful targets:

| Command                 | Purpose                                                    |
| ----------------------- | ---------------------------------------------------------- |
| `make build-app`        | Build the game and runtime assets                          |
| `make run`              | Build and launch the game                                  |
| `make editor`           | Build and launch the map editor                            |
| `make arena`            | Build and launch the gameplay/render scenario harness      |
| `make test`             | Build and run the complete test suite                      |
| `make quality`          | Run formatting, linting, and quality-marker checks         |
| `make validate-content` | Validate campaign/mission/content data                     |
| `make validate`         | Run the complete local quality, build, test, and data gate |
| `make bake-bpat`        | Bake the built-in creature animation/body assets           |

The first map-pipeline run may download/generate campaign-map source data. Force regeneration with:

```sh
make run-map-pipeline map_pipeline_rebuild=1
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for platform setup, formatting, tests, and pull-request requirements.

## Default controls

All gameplay bindings can be changed under **Settings → Controls**.

| Context   | Default input          | Action                                |
| --------- | ---------------------- | ------------------------------------- |
| Camera    | Arrow keys or WASD     | Pan; Shift increases step/speed       |
| Camera    | Q / E                  | Rotate                                |
| Camera    | Ctrl+Up / Ctrl+Down    | Tilt                                  |
| Camera    | Wheel or PgUp / PgDown | Zoom                                  |
| Camera    | Home                   | Reset/focus the authored camp framing |
| Camera    | Right-drag             | Drag-pan                              |
| Selection | Left-click / drag      | Select a unit or rectangle            |
| Selection | Shift + left-click     | Add to selection                      |
| Orders    | Right-click            | Context move, attack, or interact     |
| Orders    | C / M                  | Attack mode / move mode               |
| Orders    | Z / H / G              | Stop / hold / guard                   |
| Orders    | P, then two clicks     | Patrol route                          |
| Game      | Space                  | Pause/resume                          |
| Game      | Enter                  | Enter/leave direct commander control  |
| Game      | F5 / F9                | Quick-save / quick-load               |
| Game      | Escape                 | Cancel current mode or open the menu  |

## Architecture

Standard of Iron separates authoritative simulation, presentation, and application composition:

```text
animation / scene
        │
   engine_core
        │
   soi_world … domain libraries
        │
     game_sim
       ├── soi_ai / soi_missions / soi_campaign / soi_persistence / soi_runtime
       ├── game_view
       └── render_gl
                │
             app_core
                │
        standard_of_iron
```

Kernel/domain libraries link only toward lower layers. The headless `game_sim` target does not require the renderer. Player input, AI, and scripted systems submit typed commands through the same command pipeline.

Per-match authority lives in `SessionContext`, including world state, terrain, economy, simulation clock, deterministic RNG, ownership, and the command/replay stream.

The renderer has a 3.3 Core baseline and enables higher-tier paths according to the active OpenGL context. A CPU renderer exists independently of those shader tiers.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/RENDERING_ARCHITECTURE.md](docs/RENDERING_ARCHITECTURE.md).

### Repository layout

```text
app/          application composition, controllers, and QML-facing view models
animation/    animation clips, BPAT format, and runtime sampling
assets/       maps, missions, factions, formations, shaders, audio, and visuals
game/         ECS, simulation, commands, AI, economy, combat, save/load
render/       OpenGL pipeline, entity rendering, terrain, VFX, CPU fallback
scene/        camera and scene primitives
ui/           Qt/QML interface and accessibility design system
tools/        map editor, arena, balance/replay/performance and asset tools
tests/        simulation, persistence, renderer, application, tools, and QML tests
scripts/      validation, portability, release, content, and asset automation
```

## Developer tooling

- **Map editor** — authors terrain, missions, walls, gates, wildlife, weather, and scenario data.
- **Arena** — runs gameplay/rendering scenarios interactively or in deterministic batch mode.
- **Balance simulator** — executes seeded production-simulation matchup fixtures.
- **Content validator** — validates campaign, mission, map, faction, and asset contracts.
- **Replay verifier** — records and checks accepted command streams and deterministic world digests.
- **Performance tooling** — simulation budgets, profiling counters, frame/startup reports, and repeatable performance suites.
- **Asset pipelines** — campaign map generation, creature baking, fonts, audio processing, and promotional rendering.

Start with [tests/README.md](tests/README.md), [tools/arena/README.md](tools/arena/README.md), and [docs/UI_DESIGN_SYSTEM.md](docs/UI_DESIGN_SYSTEM.md).

## Current product scope

The current application provides campaign, tutorial, and skirmish play for a local player and AI opponents. It does not expose a network multiplayer mode.

Replay recording is implemented and is not a project limitation. Save storage also includes schema migration, compressed snapshots, asynchronous save jobs, integrity checks, and database quarantine/recovery; those capabilities should not be described as future work.

Subsystem-specific constraints belong in the documentation for the subsystem that enforces them. Numeric or capability limitations should be backed by source, tests, or checked machine-readable budgets rather than copied forward from an old roadmap.

## Documentation

| Area          | Reference                                                                                                                                                                                                |
| ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Architecture  | [Architecture](docs/ARCHITECTURE.md), [rendering](docs/RENDERING_ARCHITECTURE.md)                                                                                                                        |
| Gameplay      | [Combat](docs/COMBAT_SYSTEM.md), [formations](docs/FORMATION_ARCHITECTURE.md), [AI](docs/AI_ARCHITECTURE.md), [economy](docs/ECONOMY_GUIDANCE.md)                                                        |
| Campaign/data | [Mission roster](docs/CAMPAIGN_MISSIONS.md), [mission framework](docs/MISSION_FRAMEWORK.md), [hill shapes](docs/HILL_SHAPES.md)                                                                          |
| Persistence   | [Save/load system](docs/SAVE_LOAD_SYSTEM.md)                                                                                                                                                             |
| Presentation  | [UI design system](docs/UI_DESIGN_SYSTEM.md), [typography](docs/TYPOGRAPHY.md), [accessibility](docs/ACCESSIBILITY.md), [audio](docs/AUDIO_SYSTEM.md), [audio mastering](docs/AUDIO_MASTERING.md), [audio licences](docs/AUDIO_LICENSES.md) |
| Performance   | [Instrumentation](docs/PERFORMANCE_INSTRUMENTATION.md), [mission startup](docs/MISSION_STARTUP.md), [massed battles](docs/MASSED_BATTLE_PERFORMANCE.md), [pathfinding](docs/PATHFINDING_ARCHITECTURE.md) |
| Development   | [Contributing](CONTRIBUTING.md), [tests](tests/README.md), [arena](tools/arena/README.md)                                                                                                                |

## Contributing

Issues, focused bug reports, documentation improvements, content work, and code contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request; it documents the formatting toolchain, test expectations, portability checks, and review workflow.

## License and asset terms

The source code is released under the [MIT License](LICENSE). Qt is dynamically linked under LGPL v3, and vendored libraries retain their own licences.

Asset provenance and redistribution terms are recorded in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) and the per-asset provenance data referenced there.
