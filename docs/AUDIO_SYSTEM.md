# Audio System

Everything the player hears goes through one path: a **cue** is requested by gameplay or QML, the cue registry picks a **resource** from the manifest, `AudioSystem` decides whether it may play, and the miniaudio backend mixes it. Music and ambience beds are chosen by **tags** in the manifest rather than by ID in code, so the asset set can change without touching C++ or QML.

Where each file came from and under what licence is in [AUDIO_LICENSES.md](AUDIO_LICENSES.md).

## Where things live

| Path                                                           | Role                                                                               |
| -------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| `assets/audio/audio_manifest.json`                             | Every shipped file: ID, path, category, load policy, tags, playback limits         |
| `assets/audio/audio_cues.json`                                 | Every gameplay cue: which resources answer it, volume, priority, cooldown, spatial |
| `game/audio/audio_cues.*`                                      | `CueRegistry`, `play_cue`, `play_cue_at`                                           |
| `game/audio/audio_system.*`                                    | `AudioSystem`: event queue, admission (cooldowns, priority, channel caps), volumes |
| `game/audio/audio_event_handler.*`                             | Turns engine events (selection, spawn, death, ambient state, hits) into cues       |
| `game/audio/miniaudio_backend.*`                               | Decode worker, track table, device callback mixer                                  |
| `game/audio/audio_mastering.*`                                 | Decode-time loudness, tilt, resonance and limiter chain (`soi_audio_mastering`)    |
| `game/audio/gameplay_mix.h`                                    | Mix buses and their ducking                                                        |
| `game/audio/bus_limiter.h`                                     | Output limiter and listening presets                                               |
| `game/audio/spatial.h`                                         | Distance attenuation and pan                                                       |
| `game/audio/cue_trace.*`                                       | Per-request outcome counting, trace log, mission summary export                    |
| `game/audio/audio_settings.h`                                  | Persisted volume sliders and listening preset                                      |
| `app/audio/audio_resource_loader.*`                            | Reads both JSON files, loads by policy, answers tag queries                        |
| `app/audio/audio_coordinator.*`                                | Frontend music, mission ambience, voice and ambient-state mappings                 |
| `app/audio/weather_audio.*`                                    | Rain and snow layer over the mission bed                                           |
| `app/audio/audio_system_proxy.*`                               | QML entry point (`audioSystem.play_cue`, volume sliders)                           |
| `third_party/miniaudio.h`, `stb_vorbis`                        | Device output and Vorbis decoding                                                  |
| `tools/audio_synth`, `tools/audio_field`, `tools/audio_import` | The three asset pipelines, see [Asset pipelines](#asset-pipelines)                 |

## The path of one cue

```text
gameplay code / system            QML (ui/qml/design/UiSound.qml)
  play_cue("combat.hit.sword")      audioSystem.play_cue("ui.click")
  or publish AudioCueEvent                │ source tagged "qml"
        │                                 │
        ▼                                 ▼
CueRegistry::play                                   caller thread, mutex
  unbound? ─────────────► drop: unbound
  cue cooldown? ────────► drop: cue_cooldown
  choose a variant ─────► drop: no_loaded_resource (none loaded)
        │
        ▼
AudioSystem::play_sound  ──► AudioEvent queue
        │
        ▼
AudioSystem audio thread                            process_event
  resource loaded? ─────► drop: resource_not_loaded
  max_instances ────────► drop: instance_limit
  resource cooldown ────► drop: resource_cooldown
  spatialize + sliders ─► drop: muted (volume ≈ 0, incl. out of earshot)
  32-channel priority ──► drop: global_priority
  category cap ─────────► drop: category_priority
  pick mix bus
        │
        ▼
MiniaudioBackend::play_sound ──► lock-free command ring (256)
        │
        ▼
device callback on_audio                            no locks
  music channels + sound-effect slots
  × per-bus gain (GameplayMix) → BusLimiter → device
```

`AudioEventHandler` also drops a request as `audience_filtered` before it reaches the registry when the event belongs to another player (you do not hear an AI opening its own gate). Every request ends in exactly one outcome, so requests always equal accepted plus drops.

The source location is captured with `std::source_location`, so no call site passes it. An `AudioCueEvent` records where it was _published_, not where the handler dispatched it.

## The two catalogues

### `audio_manifest.json` — the files

```json
{
    "id": "voice.roman.archer",
    "aliases": ["archer_voice", "roman.archer"],
    "path": "voices/roman/archer.ogg",
    "category": "voice",
    "load_policy": "mission",
    "tags": { "faction": "roman", "unit": "archer" }
}
```

| Field                                                        | Meaning                                                                                                               |
| ------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------- |
| `id`, `aliases`                                              | Resource ID; aliases resolve to the same resource                                                                     |
| `path`                                                       | Relative to `assets/audio/`                                                                                           |
| `category`                                                   | `sfx`, `voice`, `ambience` (or `ambient`), `music` — picks the volume slider, channel cap and mastering profile       |
| `load_policy`                                                | See [Loading](#loading)                                                                                               |
| `tags`                                                       | Free-form string pairs used by tag queries (`screen_context`, `ambient_state`, `faction`, `unit`, `biome`, `source`…) |
| `priority`, `cooldown_ms`, `max_instances`, `volume`, `loop` | Per-resource playback limits, applied on the audio thread                                                             |
| `provenance`                                                 | Origin and licence; required on every track and rendered into [AUDIO_LICENSES.md](AUDIO_LICENSES.md)                  |

### `audio_cues.json` — the cues

```json
{
    "id": "ui.click",
    "description": "Menu button, list row or combo box entry is activated.",
    "category": "sfx",
    "importance": "optional",
    "volume": 0.85,
    "priority": 5,
    "cooldown_ms": 40,
    "resources": ["ui.click.primary"],
    "wanted": "Firm wood-on-leather confirm tap with a small low-mid body."
}
```

| Field                  | Meaning                                                                                                      |
| ---------------------- | ------------------------------------------------------------------------------------------------------------ |
| `resources`, `weights` | The pool. Unknown resource IDs are skipped with a warning; mismatched `weights` fall back to equal weights   |
| `spatial`              | `true` places the cue at the world point passed to `play_cue_at`; everything else plays flat                 |
| `importance`           | `required`, `optional` or `ambient`. `make audio-check` fails when a `required` cue is silent or never fired |
| `wanted`               | The sound's spec, in words — what a recipe or a replacement has to match                                     |

Cue IDs used from C++ are constants in `game/audio/cue_ids.h`. A cue bound to nothing warns once (`audio cue requested but bound to nothing`); a cue whose resources exist but are not loaded warns `check its load_policy`.

### Variant choice

With more than one resource, `CueRegistry` avoids the last `pool / 2` picks, prefers a variant whose own resource cooldown has elapsed, then draws by weight. A two-take cue therefore never repeats back to back, and a four-take cue never repeats within two plays.

## Loading

| Policy    | Loaded                                                                                                         | Unloaded         |
| --------- | -------------------------------------------------------------------------------------------------------------- | ---------------- |
| `startup` | At engine composition, before the menu                                                                         | Never            |
| `screen`  | When the audio event handler starts, and whenever frontend music is applied                                    | Never            |
| `mission` | When a mission or skirmish starts, and when a save is loaded                                                   | Mission teardown |
| `lazy`    | Only on demand, through `AudioResourceLoader::ensure_audio_resource_loaded` (most music, some voices and beds) | Mission teardown |

Loading only queues a decode; the decode worker does the work (see [Decode-time mastering](#decode-time-mastering)). A one-shot asked for before its decode lands is skipped, and a looping bed is held and starts the moment it lands. `AudioSystem::is_resource_ready` does not mean decoded: it only says the resource is loaded, off cooldown and under its instance limit.

Resident decoded PCM is tracked against a budget (`SOI_AUDIO_PCM_BUDGET_MB` overrides it; the default is 320 MB); overruns are counted and logged, not refused.

Tracks are held as 32-bit float stereo at the device rate, so a minute of music costs about 22 MB at 48 kHz and twice that at 96 kHz. The two four-minute `peaceful` beds (`music.base.echoes_ancient_outpost`, `music.base.ancient_peak_fires`) are 88 MB each, and the `startup` and `mission` sets add roughly 20 MB more, which is why a mission that rotates onto its second long bed sits near 200 MB. The budget is a diagnostic: raise it (or shorten the long beds) rather than expecting the backend to evict anything.

## Tag-driven selection

`AudioCoordinator` and `AudioEventHandler` pick music, beds and voices by querying manifest tags. No track ID for music or ambience appears in code except the final fallback bed.

**Frontend music.** Tracks tagged `screen_context=<context>` rotate round-robin per context. The context `battle` (or empty) stops music.

**Mission music.** For each ambient state (`peaceful`, `tense`, `combat`, `victory`, `defeat`) the handler holds the tracks tagged `ambient_state=<state>`; for combat, victory and defeat the player's own `faction` is queried first. A state change rotates to the next track, plays one of the SFX tagged `state=<state>` (priority 7, two-second group cooldown) and, for victory and defeat, fires `state.victory` / `state.defeat`.

**Mission ambience.** One looping bed, chosen by the first query below that matches anything:

1. Words in the map path, map name, mission ID, title, summary and terrain: `harbor`/`port` → `biome=harbor`; `city`/`market` → `city`; `village`/`hamlet` → `village`; `siege` → `biome=camp, context=siege`; `road`/`crossroads` (Roman players only) → `biome=plains, context=road, faction=roman`; `desert`/`zama` → `biome=plains, faction=carthage`.
2. The biome: the mission's `terrain_type` if set, otherwise the map — rivers or lakes → `river`, `AlpineMix` → `mountain`, `GrassDry` → `plains`, `SoilRocky` → `battlefield`, `SoilFertile` → `camp`, anything else → `forest`. `camp` and `plains` try the player's faction first.
3. `biome=battlefield`.

Among the matches the bed is picked by a hash of map path, faction and mission ID, so the same mission always gets the same bed. If nothing matches, `ambient.battlefield_dry_wind_distant_march_01` plays.

**Weather.** Weather is a layer, not a bed per sky. `WeatherAudio` plays `ambient.weather_rain` or `ambient.weather_snow` on top of the mission bed at `intensity × 0.85`, and stops below 0.02. Those two beds carry only the weather, never ground sound.

**Unit voices.** Selecting or spawning a unit plays the voice resolved in this order: the `faction.unit` mapping built from tags `faction` + `unit`, the unit-type mapping, the manifest ID `voice.<faction>.<unit>`, then the legacy alias `<unit>_voice`. Commanders are matched by `unit_def`. Selection voices play at least 120 ms apart, or 300 ms when the same line would repeat; spawn voices share a 1.5 s group cooldown.

**Cues chosen by who is acting.** Some actions pick their cue from the units doing them, because a resource pool is drawn at random and a mixed pool plays the wrong animal. A run order counts the selection's foot, cavalry and elephants (`CommandController::selection_mounts`) and plays `combat.charge_elephant` if any elephant is selected, `combat.charge_cavalry` if any cavalry is, and `combat.charge` otherwise (`CommandController::charge_cue`). An accepted move order plays `order.move_mounted` — horses moving off at a walk — only when every selected unit is cavalry (`CommandController::move_order_cue`). A Roman medicus binds wounds with `combat.heal_bind`; every other healer plays `combat.heal`.

**Commander chatter.** The banks in `assets/data/commanders/voices/*.json` are on-screen lines. Each line may name a `voice_cue`, but no bank sets one today, so commanders speak in text only; the three files in `voices/commanders/` are their selection barks.

## Spatial audio

A cue with `"spatial": true` fired through `play_cue_at` is heard relative to the camera. The engine publishes the listener every frame from the render camera (`app/core/game_engine.cpp`).

| Distance to listener | Volume                         |
| -------------------- | ------------------------------ |
| ≤ 14 m               | full                           |
| 14–90 m              | `(1 − reach)²`, quadratic fade |
| ≥ 90 m               | silent, reported as `muted`    |

Pan is the source's offset along the camera's right vector divided by 26 m, clamped, scaled to ±0.85. Before a camera exists the listener is invalid and every cue plays flat at full volume.

Distant fighting is not simply lost: `AudioEventHandler` counts impacts that distance silenced, and six inside three seconds become one `combat.distant_battle` cue. `tests/core/audio_battle_load_test.cpp` measures this against a paced melee.

## The mix

### Categories and channels

| Category | Slider (first run) | Concurrent cap               |
| -------- | ------------------ | ---------------------------- |
| master   | 0.70               | 32 channels                  |
| sfx      | 1.00               | 16                           |
| voice    | 1.00               | 2                            |
| ambience | 0.30               | 4                            |
| music    | 0.45               | 4 music channels, crossfaded |

A new sound above the 32-channel or category cap evicts the lowest-priority, oldest sound only if it outranks it; otherwise it is dropped. Sliders and the listening preset persist through `QSettings` (INI, organisation `djeada`, application `StandardOfIron`, keys `audio/*`). Invalid saved values are clamped with a warning.

### Mix buses

Every playing sound sits on a bus chosen from its cue ID, then its resource ID (a leading `sound_` or `sfx.` is ignored). Voice-category sounds always use the Voice bus.

| Bus         | ID prefixes                                         | Base gain |
| ----------- | --------------------------------------------------- | --------: |
| Music       | music channels                                      |      0.50 |
| Ambience    | ambience category fallback                          |      0.50 |
| Combat      | `combat.`, `move.`, `movement.`; sfx fallback       |      0.40 |
| Voice       | `voice.`; voice category                            |      0.89 |
| Interface   | `ui.`, `order.`, `command.`                         |      0.63 |
| Economy     | `build.`, `economy.`                                |      0.32 |
| Weather     | `weather.`, `ambience.weather.`, `ambient.weather_` |      0.32 |
| Environment | `wildlife.`, `environment.`                         |      0.25 |
| Alert       | `alert.`, `state.`                                  |      0.79 |

`GameplayMix` retargets the gains every mix block (256 frames) and glides toward them (20 ms attack, 350 ms release):

- **Density.** Combat, Economy and Environment scale by `1 / √(max(1, active / 3))` across those three buses, so a large melee does not sum to a wall.
- **Voice ducking.** While any voice plays, Music, Ambience, Weather, Combat, Economy and Environment drop to 0.85×.
- **Critical ducking.** A Voice or Alert sound at priority ≥ 7 drops the same buses to 0.63×.
- **Stacking.** Voice and Alert divide by `√count`, so simultaneous callouts stay intelligible.

### Listening presets and the limiter

The bus sum goes through `BusLimiter`: 3 ms look-ahead, 120 ms release, 6 dB soft knee, hard ceiling 0.79. The preset in the settings menu sets how much it compresses below the ceiling:

| Preset             | Threshold | Ratio | Also                             |
| ------------------ | --------- | ----- | -------------------------------- |
| Headphones         | ceiling   | 1:1   | limiter only                     |
| Speakers (default) | 0.35      | 2:1   |                                  |
| Night              | 0.18      | 4:1   | Combat/Economy/Environment × 0.7 |

## Decode-time mastering

Each track is processed once, on the decode worker, when it is loaded:

1. Decode to 32-bit float stereo.
2. Resample to the device rate (48 kHz by default) with a polyphase filter (90 dB stopband).
3. Analyse: integrated loudness, presence and air bands, up to four resonances.
4. Apply the material's profile: loudness gain, spectral tilt, resonance notches, limiter to the ceiling.
5. Music and ambience only: seal the loop with a 0.12 s crossfade (at most 10% of the length), so the wrap does not click.
6. Store a mono file once if both channels are identical, then convert to 16-bit.

The device callback only reads finished tracks through an atomic table; decoding never blocks the caller or the mixer.

### Loudness is matched within a category, not across them

The material comes from the category; SFX resources whose ID starts with `ui.` (or contains `.ui.` or `/ui/`) count as Interface.

| Material  | Loudness                                                                          | Ceiling   |
| --------- | --------------------------------------------------------------------------------- | --------- |
| Music     | toward −15 LUFS, at most ±6 dB                                                    | −1 dBTP   |
| Ambience  | toward −16.5 LUFS, at most ±6 dB                                                  | −1.5 dBTP |
| Voice     | toward −15.5 LUFS                                                                 | −1 dBTP   |
| Interface | not normalised; fixed make-up gain                                                | −1 dBTP   |
| Effect    | attenuate only: files louder than −18 LUFS come down, quieter ones are left alone | −1 dBTP   |

What this means when authoring a file:

- **The level baked into an effect is the design decision.** The chain never lifts a quiet effect, so a replacement has to land at the RMS of the file it replaces or the mix changes. `import_cues.py` and `tools/audio_field/battle.py` both match RMS for this reason.
- **Music has limited authority.** The lift is capped at 6 dB and by the headroom under the ceiling, so a track baked much below about −19 LUFS stays quiet for good. `import_music.py` bakes to −14.1 LUFS at −1 dBTP.
- **Beds need headroom.** A bed delivered near full scale makes the limiter pull several dB, audible as the bed ducking under itself. The recorded beds are levelled to −19.3 LUFS with peaks held well under full scale.
- **Nothing may decode above 0 dBFS.** Vorbis overshoots peaks. The build tools measure the decoded file and re-encode at lower gain until it lands under the ceiling.

`audio_master_preview` (`make audio-preview`) links the same `soi_audio_mastering` library, so what it renders to `artifacts/audio_preview` is what the game plays. `scripts/promo-edit.py` uses it to score reels with mastered audio.

## Packaging

- `assets.qrc` embeds every `sfx/` and `voices/` file plus both JSON catalogues.
- Music and ambience are **not** embedded. `install(DIRECTORY assets/)` installs them beside the binary, and a player's package always carries them.
- The loader looks for `assets/audio/` at `<app>/assets/audio`, `<app>/../Resources/assets/audio` (macOS bundle) and `<app>/../../assets/audio`, then falls back to `:/assets/audio/`. A file on disk wins over the embedded copy.
- The eight synthesised ambience beds are not committed (`.gitignore`). The `synthesize_ambience_assets` target renders them into both `assets/audio/ambience` and the staged build copy. Without Python or an ffmpeg that writes Vorbis the build warns and those beds are missing at runtime; the game runs without them.

## Asset pipelines

| Material                               | Recipe or record                                  | Rebuild                                                 | Committed |
| -------------------------------------- | ------------------------------------------------- | ------------------------------------------------------- | --------- |
| Synthesised cues (`source: synth`)     | `tools/audio_synth/cues.py`                       | `make audio-assets` or `synthesize_cues.py <cue>`       | yes       |
| Synthesised beds                       | `tools/audio_synth/ambience.py`                   | `make audio-ambience`, and every build                  | no        |
| Recorded beds                          | `tools/audio_field/sources.py`                    | `make audio-field-ambience` (network)                   | yes       |
| Recorded one-shots (hits, footsteps)   | `tools/audio_field/oneshots.py`                   | `python3 tools/audio_field/build_oneshots.py` (network) | yes       |
| Composed battle cues (`source: field`) | `tools/audio_field/battle.py`                     | `make audio-battle` (network)                           | yes       |
| ElevenLabs cues (`source: generated`)  | `PLAN` in `tools/audio_import/import_cues.py`     | `import_cues.py --source DIR`                           | yes       |
| ElevenLabs music                       | `BATCHES` in `tools/audio_import/import_music.py` | `import_music.py --source DIR --batch NAME`             | yes       |
| Voices                                 | none                                              | none                                                    | yes       |
| New drop-ins                           | files in `new sfx/`                               | `make audio-import`, then `AUDIO_IMPORT_ARGS=--apply`   | —         |

`tools/audio_synth/README.md` and `tools/audio_field/README.md` cover recipes, takes and window selection in detail.

### Rebuild traps

- **A recipe can outlive its file.** When a cue is replaced by an import, its recipe still names the path. `synthesize_cues.py`, `register_cues.py` and `build_battle.py` therefore read the manifest's `source` tag and skip any file another pipeline owns (`--force` overrides the two renderers). Recipes for replaced files were deleted in September 2026; delete a recipe when its file is replaced rather than relying on the guard.
- **ffmpeg picks a random Ogg stream serial**, so re-encoding an unchanged recipe still changes a few header bytes. Regenerate the cue you are working on, not the set.

After any rebuild, `git status assets/audio` shows what was actually touched.

## Debugging

| Variable                         | Effect                                                                                                                       |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `SOI_AUDIO_TRACE=1`              | One log line per cue request                                                                                                 |
| `SOI_AUDIO_TRACE_SUMMARY=<path>` | JSON summary per mission, written at mission teardown and at shutdown; later exports in the same process get an index suffix |
| `SOI_AUDIO_HUD=1`                | In-game overlay: volumes, active channels, the last cue request and its outcome; refreshed every 250 ms                      |
| `SOI_AUDIO_OFFLINE=1`            | Opens no audio device; the mixer is driven by offline rendering (reels, preview)                                             |
| `SOI_AUDIO_PCM_BUDGET_MB=<n>`    | Resident decoded-PCM budget (default 320)                                                                                    |

A trace line reads:

```text
audio cue [30.838s rts -318.9,69.6,-196.4] alert.commander_message -> sfx.alert.commander_message: accepted (app/core/game_engine.cpp:1996)
```

Time since the trace last reset, listener mode (`rts`, `commander`, or `none`), listener position, cue, the resource the pool chose (`-` if the drop happened before selection), the outcome, and the call site. The summary's `never_accepted` list is the cues a mission asked for that the player never heard. A cue never requested does not appear at all, which is what the static report covers.

Throttles are wall-clock and not scoped to a mission: cue cooldowns in `CueRegistry` and resource cooldowns in `AudioSystem` carry across restarts, scenarios and tests. `CueRegistry::reset_cooldowns()` and `AudioSystem::reset_playback_throttles()` clear them without touching bindings or loaded resources. Sounds outlive a test in the same way: the tails of earlier tests can fill a category, and the next low-priority cue is dropped as `category_priority`. `AudioSystem::stop_all_sounds()` silences them; the scenario tests call it in `SetUp`. Two cues sharing one resource share its cooldown too, and the loser is reported as `resource_cooldown`.

### Checks and tests

| Command                                     | What it proves                                                                                                                                                                                                    |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `make audio-check`                          | Cues, manifest, files, QRC and direct references agree; required cues are bound and fired; every track has provenance, every shipped `.ogg` is in the manifest, and `docs/AUDIO_LICENSES.md` matches the manifest |
| `make audio-scan`                           | Every shipped clip decoded and scanned for leading silence and boundary clicks (report only)                                                                                                                      |
| `make audio-report`                         | The wiring matrix — declared, fired, loaded, verified — in `artifacts/audio/AUDIO_WISHLIST.md`                                                                                                                    |
| `python3 scripts/audio_provenance.py --doc` | Re-renders `docs/AUDIO_LICENSES.md` from the manifest after a provenance change                                                                                                                                   |
| `make audio-preview`                        | Mastered renders of the library in `artifacts/audio_preview`                                                                                                                                                      |

Tests: `audio_system_test`, `audio_cues_test`, `audio_backend_test`, `audio_command_ring_test`, `audio_mastering_test`, `audio_gameplay_scenarios_test` (production gameplay paths; clears both throttles in `SetUp`), `command_controller_test` (a run order never trumpets without elephants), `audio_battle_load_test`, `ambience_assets_test` (every bed at the mixer rate, 2–6 kHz at least 3 dB under 100–800 Hz), `weather_audio_test`, and the `commander_voice_*` tests.
