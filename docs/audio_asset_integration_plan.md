# Audio Asset Integration Plan

Status: complete for the current runtime scope. Phases 1 to 4 are now implemented for the shipped audio pipeline, with screen/mission/lazy policies, tag-driven manifest routing, front-end music states, and ambience mix control all wired.

## Progress Snapshot (as of latest implementation)

Implemented:

- Audio manifest exists at `assets/audio/audio_manifest.json` and is loaded by `AudioResourceLoader`.
- OGG runtime loading replaced hardcoded WAV file paths.
- Legacy runtime IDs are supported through manifest aliases.
- Audio now consumes `UnitSpawnedEvent`, `UnitDiedEvent`, `BuildingAttackedEvent`, and `BarrackCapturedEvent`.
- `AudioSystem::play_music(..., crossfade)` now passes crossfade to `MusicPlayer` and uses multi-channel fade behavior.
- `Sound::stop()` now stops active miniaudio SFX slots through backend stop-by-track.
- Combat-hit IDs referenced by `AudioEventHandler` are loaded from manifest.
- Combat-hit routing now includes dedicated cavalry and elephant families.
- Manifest playback metadata now supports `volume`, `priority`, `cooldown_ms`, and `max_instances`.
- `AudioSystem` now enforces cooldown and per-ID instance caps generically and only allows stricter-priority requests to evict active sounds.
- Ambience now has its own runtime category instead of sharing the SFX category path.
- `AudioResourceLoader` now caches the manifest, resolves tracks by tags, and supports targeted single-resource loads for screen/lazy flows.
- Standard faction/unit voice mappings and ambient-state music mappings are now configured from manifest tags rather than hardcoded sound-id tables.
- Front-end audio now has explicit menu and campaign screen contexts, with screen-policy music tracks crossfaded independently from battle-state music.
- Result stingers now use lazy-load preparation so victory/defeat tracks can stay out of startup memory.
- Settings now expose a dedicated ambience volume slider alongside master/music/SFX/voice.
- Loader path handling now works for both filesystem and qrc-backed manifests.
- Voice playback still keeps the hidden 2x gain on top of the visible voice slider.

No blocking items remain from this plan.

Optional future follow-ups:

- Add a true streaming path if long-track memory pressure becomes a real runtime problem.
- Expand manifest coverage further for alternate staged tracks that are not yet chosen by gameplay/runtime rules.

## Asset Staging Done

The new audio drop from `new_audio_assets/` has been moved into the runtime asset tree with shorter, stable names.

Current `assets/audio` layout:

| Area | Count | Path |
| --- | ---: | --- |
| Ambience loops / beds | 18 | `assets/audio/ambience/` |
| Base / calm music | 5 | `assets/audio/music/base/` |
| Campaign / strategy music | 2 | `assets/audio/music/campaign/` |
| Combat music | 6 | `assets/audio/music/combat/` |
| Menu music | 2 | `assets/audio/music/menu/` |
| Result stingers | 5 | `assets/audio/music/stingers/` |
| Alert stingers | 5 | `assets/audio/sfx/alerts/` |
| Combat SFX | 27 | `assets/audio/sfx/combat/` |
| Carthage voices | 16 | `assets/audio/voices/carthage/` |
| Roman voices | 12 | `assets/audio/voices/roman/` |
| Commander voices | 3 | `assets/audio/voices/commanders/` |

New OGG total: 101 files, about 1,677 seconds of audio.

All old WAV assets have been removed from `assets/audio`. The asset tree is now OGG-only.

## Current Audio System Assessment

The game uses miniaudio through `MiniaudioBackend`.

Strengths:

- OGG/Vorbis is already supported.
- Files are predecoded at load time, so playback avoids decode work during the audio callback.
- Music supports fades at backend level.
- SFX has a fixed slot pool, currently 32 simultaneous sound-effect slots.
- Music has 4 backend channels available.
- Audio requests are queued through `AudioSystem`, so gameplay code does not play directly on the audio callback.

Current risks / limitations:

- Predecoding every long music and ambience asset can still become memory-heavy if the active mission/screen catalog grows significantly.
- `stream` remains a reserved manifest policy rather than an active backend mode because the current asset set still fits the predecode approach comfortably.

## Current Incoming Audio Signals

Core event bus event types found in `game/core/event_manager.h`: 10.

Audio currently subscribes to 9 of them:

- `UnitSelectedEvent`
- `AmbientStateChangedEvent`
- `AudioTriggerEvent`
- `MusicTriggerEvent`
- `CombatHitEvent`
- `UnitSpawnedEvent`
- `UnitDiedEvent`
- `BuildingAttackedEvent`
- `BarrackCapturedEvent`

Existing core event types not consumed by audio:

- `UnitMovedEvent`

The generic trigger events are useful escape hatches:

- `AudioTriggerEvent` can already request any loaded SFX ID.
- `MusicTriggerEvent` can already request any loaded music ID.

They are not enough by themselves because most gameplay systems do not publish these trigger events today.

## Signals Needed To Use The New Audio Well

Minimum useful coverage for the new asset set:

| Needed signal | Source idea | Asset groups used |
| --- | --- | --- |
| Unit selected by faction and type | extend selection audio mapping | `voices/roman`, `voices/carthage`, `voices/commanders` |
| Unit spawned / trained | consume `UnitSpawnedEvent`, ignoring initial map load | voices, alerts |
| Unit death / casualty burst | consume `UnitDiedEvent` plus combat throttling | `troops_death_scream`, aftermath, victory cheer |
| Combat hit by attack family | existing `CombatHitEvent` | gladius, arrows, javelin, shield, elephant, cavalry |
| Ranged volley fired | new or existing combat/projectile signal | archer volley and arrow flybys |
| Cavalry charge / elephant charge | unit command or combat proximity signal | horse, Numidian, elephant SFX |
| Building attacked | consume `BuildingAttackedEvent` | warning alert, siege camp, impacts |
| Barracks captured | consume `BarrackCapturedEvent` | alert / result stingers |
| Low resources | existing Qt/controller signal or new core event | `low_resources_click` |
| Population limit | existing `troop_limit_reached` Qt signal or new core event | `population_limit_horn` |
| Enemy spotted / enemy reinforcements | mission/objective event or new core event | alert stingers |
| Reinforcements arrived | mission/objective event or spawn batch event | alert stinger |
| Mission victory/defeat by faction | ambient victory/defeat plus mission result | music stingers |
| Map biome / scenario ambience | mission/map metadata | `ambience/` |

Suggested target: keep the core audio bus small and explicit. Add data-driven asset definitions first, then wire 6 to 8 high-value gameplay signals instead of creating a unique event for every file.

## Proposed Data Model

Create an audio manifest, for example `assets/audio/audio_manifest.json`.

Suggested fields per entry:

- `id`: stable runtime ID, for example `sfx.combat.gladius_shield_impacts_close`
- `path`: relative path under `assets/audio`
- `category`: `music`, `ambience`, `sfx`, or `voice`
- `bus`: `music`, `ambience`, `sfx`, `voice`, `ui`
- `loop`: default loop behavior
- `priority`: eviction priority
- `volume`: default gain
- `cooldown_ms`: per-ID throttle for spammy sounds
- `max_instances`: per-ID simultaneous limit
- `tags`: faction, unit type, biome, intensity, attack family
- `load_policy`: `startup`, `mission`, `lazy`, or `stream`

Example IDs:

- `music.menu.rome_vs_carthage`
- `music.base.management_camp`
- `music.combat.large_scale_rome_carthage`
- `music.stinger.victory_roman`
- `ambience.roman_army_camp_01`
- `sfx.alert.enemy_spotted`
- `sfx.combat.gladius_shield_impacts_close`
- `voice.roman.swordsman`
- `voice.carthage.elephant`

## Proposed Runtime Plan

Phase 1: manifest loader.

- Load audio definitions from the manifest.
- Replace the current hardcoded WAV loading with OGG manifest loading.
- Keep current runtime IDs as aliases during migration only as IDs, not as WAV filenames: `music_peaceful`, `music_tense`, `music_combat`, `music_victory`, `music_defeat`, `archer_voice`, `swordsman_voice`, `spearman_voice`.
- Load only short SFX and currently needed voices at startup.
- Load music and ambience by mission or screen, not all at startup.

Phase 2: event mapping.

- Replace hardcoded `load_unit_voice_mapping()` calls with manifest/tag driven mappings.
- Map selection voices by faction plus `SpawnType`.
- Map combat hit sounds by attack family:
  - sword/melee: `gladius_shield_impacts_close`, `roman_shield_wall_impact`
  - spear/javelin: `javelin_throw_whoosh`, `spearmen_formation_advance`
  - arrow: `archer_volley_many`, `arrows_fast_flybys_lr`, `arrows_impact_shields_dirt`
  - siege: `battlefield_crowd_chaos`, later add dedicated catapult/ballista impacts if needed
  - elephant/cavalry: `elephant_charge_carthage`, `roman_cavalry_charge`, `numidian_cavalry_chase`
- Consume `UnitSpawnedEvent`, `UnitDiedEvent`, `BuildingAttackedEvent`, and `BarrackCapturedEvent` for high-value stingers and alerts.

Phase 3: music and ambience states.

- Split music into at least these layers/states:
  - menu
  - campaign map / command tent
  - peaceful base
  - tense
  - combat low
  - combat high
  - victory / defeat stinger
- Add ambience as its own looping bus, separate from music.
- Pick ambience from mission/map metadata: mountains, river crossing, Roman camp, Carthage camp, city, harbor, plains, rainy battlefield.

Phase 4: transitions and mix polish.

- Music crossfade target:
  - peaceful to tense: 1.5 to 2.5 seconds
  - tense to combat: 0.8 to 1.5 seconds
  - combat to peaceful: 3 to 5 seconds
  - victory/defeat stinger: duck music, play stinger, then stop or move to result loop
- SFX polish:
  - randomize volume within a narrow range for repeated combat SFX.
  - add small pitch variation only if backend support is added.
  - add per-family cooldowns for combat sounds.
  - cap loud battlefield beds to avoid clipping when 20+ units hit at once.
- Bus balance starting point:
  - music: 0.65
  - ambience: 0.35
  - SFX: 0.75
  - voice: 0.9
  - alerts: 0.85

## CPU / Memory Plan

Current predecode behavior is good for low CPU during playback but can be expensive in memory for long tracks.

Recommended load policy:

- Startup: UI alerts, selection voices for the active faction, current menu music.
- Mission load: mission ambience, current faction voices, core combat SFX.
- Lazy load: rare result stingers and rare faction-specific SFX.
- Stream or delayed-predecode: long music/ambience if memory becomes a problem.

SFX concurrency targets:

- Keep backend SFX slots at 32 initially.
- Add per-ID `max_instances`, usually 1 to 3.
- Add per-family limits:
  - arrows: 4
  - melee impacts: 6
  - death screams: 2
  - cavalry/elephant: 2
  - alerts: 1
- Keep priorities:
  - result stingers / mission critical alerts: 100
  - unit voice confirmations: 60
  - local combat hits: 40
  - distant battlefield beds: 20
  - ambience: separate bus, not SFX eviction

Current manifest defaults already start enforcing low-risk caps for combat hits, alerts, and ambience loops through `cooldown_ms`, `max_instances`, and per-entry priority/volume values.

## Delivered Scope

- [x] Keep the staged directory layout and OGG-only runtime asset tree.
- [x] Load runtime audio from `assets/audio/audio_manifest.json` with legacy aliases preserved.
- [x] Wire audio to `UnitSpawnedEvent`, `UnitDiedEvent`, `BuildingAttackedEvent`, and `BarrackCapturedEvent`.
- [x] Select ambience from mission/map metadata and keep it on its own mix path.
- [x] Apply the documented transition/mix starting point with menu/campaign/battle state separation.

## Resolved Design Choices

- Unit selection voices are faction-specific when tagged assets exist, with safe fallback paths still allowed for missing entries.
- Ambience selection uses mission terrain first, then map biome fallback.
- Result stingers are faction-specific for victory where assets exist and generic for defeat.
- Combat intensity remains driven by the existing ambient-state manager thresholds rather than a broader global battle-score system.
