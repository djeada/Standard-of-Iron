# Audio runtime tracing

`docs/AUDIO_WISHLIST.md` is the static half of the wiring matrix: it proves a cue is
declared, bound to an asset that exists, and fired from somewhere in the source. None of
that proves a player heard it. A cue can be perfectly wired and still be silent in a real
mission because its resource was not loaded at that moment, because a cooldown swallowed
every request, or because a louder category evicted it.

The runtime trace is the other half. It answers one question per request: what did the
game ask for, what did the mixer do with it, and when.

## Turn it on

| Variable                         | Effect                                         |
| -------------------------------- | ---------------------------------------------- |
| `SOI_AUDIO_TRACE=1`              | Logs one line per cue request to stdout.       |
| `SOI_AUDIO_TRACE_SUMMARY=<path>` | Writes a per-mission JSON summary to `<path>`. |

Counting is always on and costs one hashed increment per request; only the logging and
the export are gated. Neither variable changes what the mixer does.

```sh
SOI_AUDIO_TRACE=1 SOI_AUDIO_TRACE_SUMMARY=artifacts/audio/rhone.json \
  build/bin/standard_of_iron \
  --campaign-mission second_punic_war/crossing_the_rhone \
  --skip-briefing --benchmark-seconds 20
```

## The trace line

```
audio cue [30.838s rts -318.9,69.6,-196.4] alert.commander_message -> sfx.alert.commander_message: accepted (app/core/game_engine.cpp:1996)
audio cue [39.809s rts -318.9,69.6,-196.4] build.gate_open -> -: audience_filtered (game/systems/gate_system.cpp:201)
```

The bracket is the timestamp in seconds since the trace last reset, the listener mode
(`rts`, `commander`, or `none` before a camera exists) and the listener position. Then
the cue ID, the resource the pool actually chose, the outcome, and the code that fired
it. A `-` means no resource was reached, which is itself the finding: the drop happened
before selection.

Outcomes are `accepted` or one of `unbound`, `cue_cooldown`, `no_loaded_resource`,
`audio_system_stopped`, `instance_limit`, `resource_cooldown`, `global_priority`,
`category_priority`, `muted`, `resource_not_loaded`, `audience_filtered`. The first
three come from `CueRegistry`, `audience_filtered` from `AudioEventHandler`, the rest
from `AudioSystem`; each request produces exactly one of them, so requests always equal
accepted plus drops.

`audience_filtered` deserves its own note. A cue published for another player is dropped
before it ever reaches the registry, which used to be indistinguishable from a cue that
was never fired. It is usually correct -- you are not meant to hear an AI opening its own
gate -- but it is also where a mis-set owner id hides, so it is now counted rather than
discarded.

## The call site

The source is captured automatically with `std::source_location`, so no call site has to
pass anything. `play_cue` records where it was called; an `AudioCueEvent` records where
it was _published_, not where the handler dispatched it, so a cue fired from a system
names that system. Paths are trimmed to repository-relative. Cues coming from QML are
tagged `qml`, since the C++ location would only ever name the proxy.

## The mission summary

One file per mission. The engine exports it when a mission is torn down and again at
shutdown, and clears the counters afterwards, so a second mission in the same process
writes `<name>.2.json` instead of overwriting the first.

```json
{
    "label": ":/assets/maps/map_crossing_rhone.json",
    "duration_seconds": 27.43,
    "listener_mode": "rts",
    "totals": { "cues_requested": 3, "requests": 6, "accepted": 5 },
    "cues": [
        {
            "cue": "alert.commander_message",
            "requests": 2,
            "accepted": 1,
            "drops": { "cue_cooldown": 1 },
            "sources": [
                "app/core/game_engine.cpp:1996",
                "app/core/game_engine.cpp:1999"
            ],
            "last_resource": "sfx.alert.commander_message",
            "last_request_seconds": 6.738
        }
    ],
    "never_accepted": ["build.gate_open"]
}
```

`never_accepted` is the list that matters for an audit: cues the mission asked for and
the player never heard. `sources` is capped at the first eight distinct call sites per
cue, which is enough to tell one firing path from another without letting a hot cue grow
an unbounded list. An empty list is not a clean bill of health on its own -- a cue
that is never requested does not appear in the file at all, which is what the static
report is for. Read the two together: the wishlist says a cue could play, the summary
says whether it did.

## The spatial mix

World-space cues are placed where they happen and heard relative to the camera.
A cue opts in with `"spatial": true` in the catalogue; everything else -- UI,
orders, alerts, objective results -- plays flat, because those are the game
talking to the player rather than the world making a noise.

`Game::Audio::spatialize` is the whole law: full volume within 14 m of the
listener, falling off to silence at 90 m, panned by how far the source sits
along the camera's right vector. A silenced cue is reported as a `muted` drop,
so "why did I not hear that" has the same answer format as every other drop.
`GameEngine::sync_render_camera` publishes the listener each frame; before a
camera exists the listener is invalid and every cue plays flat at full volume.

Distant fighting is not simply dropped. `AudioEventHandler` counts the impacts
that distance has silenced, and six of them inside three seconds become one
`combat.distant_battle` bed on a nine-second cooldown. A couple of traded blows
across the map stay silent; an actual battle out of earshot reads as one. The
thresholds come from measurement rather than taste: a paced ten-a-side melee
asks for about 19 impacts a second and lands about four of them, which is in
`tests/core/audio_battle_load_test.cpp` and re-measured on every run.

## Cooldowns outlive the scene that set them

Two throttles decide whether a request is heard: a per-cue one in
`CueRegistry` (`m_last_played`) and a per-resource one in `AudioSystem`
(`resource_last_played_at`). Both are wall-clock and neither is scoped to a
mission, so they carry across a restart, a scenario change, or the boundary
between two tests. `CueRegistry::reset_cooldowns()` and
`AudioSystem::reset_playback_throttles()` clear them without disturbing the
bindings or the loaded resources.

This matters most where two cues share one recording -- `combat.hit.siege` and
`combat.hit.structure` are both bound to `sfx.combat.stone_impact_01` today, so
a masonry hit puts the catapult hit on that resource's cooldown and the trace
reports `resource_cooldown` against a cue that is wired correctly. The
scenario tests in `tests/core/audio_gameplay_scenarios_test.cpp` clear both
throttles in `SetUp` for exactly this reason; the missing masonry recording is
listed in [AUDIO_REGENERATION.md](AUDIO_REGENERATION.md).

## Where it lives

`game/audio/cue_trace.*` owns the counters, the log line and the export.
`CueRegistry::play` records the drops it makes itself and hands the cue ID to
`AudioSystem::play_sound`, which carries it through the event queue so the audio thread
can attribute its own decision back to the gameplay cue. `GameEngine::sync_render_camera`
publishes both listeners each frame -- the one the trace reports and the one the
mixer pans against.

`CueTrace::set_logging_enabled` exists so a test can capture the log line itself; it is
not a runtime switch and nothing in the game calls it.
