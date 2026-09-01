# Audio Improvement TODO

The priority is audible gameplay wiring, not isolated mixer code. Every audio task must prove the complete path:

`gameplay event → cue ID → cue binding → loaded resource → accepted playback → audible output`

An audio feature is not complete until a real gameplay scenario triggers it and runtime evidence confirms that it reached the output. Preserve authored source files in full; imports must not trim, normalize, fade, or otherwise alter timing without explicit approval.

## P0 — Wire and prove every gameplay sound

- [x] Maintain one wiring matrix for the entire audio catalogue. `docs/AUDIO_WISHLIST.md` now carries one row per cue: gameplay action, call site, audience rule, pacing bucket, pool size, the tests that fire it through a production path, and -- when `--runtime` is given a mission summary -- how often it was actually heard.
    - Record each cue ID, gameplay action, event/call site, ownership/audience rule, expected frequency, resource pool, and verification scenario.
    - Distinguish “declared,” “called by code,” “resource loaded,” and “heard in gameplay.” The first three come from `docs/AUDIO_WISHLIST.md`; the last now comes from the mission summary described in `docs/AUDIO_RUNTIME_TRACE.md`.
    - Do not count a mixer unit test or catalogue entry as proof that a gamer can hear the sound.

- [x] Add end-to-end runtime tracing for every cue request. Cue IDs reach the audio thread, every request resolves to exactly one outcome, and `docs/AUDIO_RUNTIME_TRACE.md` documents the format.
    - [x] Log the gameplay source, cue ID, chosen resource, and final result: played, cooldown drop, instance-limit drop, priority drop, muted, unloaded, or missing. Audience filtering is now an outcome of its own rather than a silent return.
    - [x] Include timestamps and listener/camera context.
    - [x] Export a mission summary with request and audible-play counts per cue.

- [~] Create deterministic gameplay verification scenarios. `tests/core/audio_gameplay_scenarios_test.cpp` drives real production paths and fails when a cue never reaches accepted playback.
    - [~] Focused scenarios exist for selection, melee by weapon, deaths, structure loss, base alarm, gates, construction start, arrows, volleys, siege, healing, production queueing, wildlife and the UI. Orders, placement confirmation/rejection, objectives and commander mode still need a seam: their cues are played from private `GameEngine` members that no test can reach.
    - [x] Each scenario triggers the real production gameplay path rather than calling the mixer directly.
    - [x] Fail verification when an expected cue never reaches accepted playback.

- [~] Audit all 93 current cues in real gameplay. 53 of 93 are now covered by a gameplay scenario; the remaining 40 are listed with an empty **Verified by** column in `docs/AUDIO_WISHLIST.md`.
    - [~] Confirm that the event actually occurs, audience filtering is correct, the resource is loaded at that moment, and the sound is audible at the intended volume.
    - [x] Fix disconnected or incorrectly routed cues before adding new mixer features. Three were found and fixed by the audit: a destroyed building played the human death cry, a cue whose chosen variant was on cooldown went silent while its other variants were free, and an arrow volley lost its own sound to the individual launches it was published with.
    - [x] Remove dead cues and unused audio-system branches rather than maintaining code with no gameplay caller. No cue is unwired. Five `AudioSystem` entry points had no caller anywhere in the game, the tools or the tests -- `pause_all`, `resume_all`, `unload_all_sounds`, `unload_all_music` and `set_max_channels` -- and are gone, along with the PAUSE and RESUME events, which only ever paused music and would have been a latent bug for whoever first wired a pause menu to them.

- [x] Wire the four dedicated construction/objective replacements.
    - `build.placement_rejected` must use its own rejection recording.
    - `build.construction_started` must use its own work-start recording.
    - `build.construction_complete` must use its own completion recording.
    - `alert.objective_complete` must use its own objective-success recording.
    - Verify each through its real gameplay event after installation.

- [x] Add a developer audio status overlay driven by runtime state. `F9` in game, or `SOI_AUDIO_HUD=1` to start with it up.
    - [x] Show the last requested cue, chosen resource, drop reason, loaded state, active instances, and effective volume.
    - [x] Show cues requested frequently but rarely or never accepted.
    - [x] Make silent wiring failures visible during ordinary playtests.

## P1 — Improve the sounds gamers actually hear

- [~] Use runtime frequency data to tune cue cooldowns, priorities, and pools. The data now exists: `tests/core/audio_battle_load_test.cpp` runs a paced ten-a-side melee and reports roughly 19 impact requests a second, of which about four are heard. What that measurement bought is below; the remaining work is a listening pass on the numbers themselves, which needs ears.
    - [x] Focus first on melee impacts, arrows, deaths, movement, construction, orders, and UI feedback. Two structural losses were found and fixed rather than tuned around: a cue whose chosen variant was on cooldown went silent while its siblings were free, and a volley lost its own sound to the launches it was published with.
    - [x] Prevent frequent events from masking commander actions, alerts, and objective feedback. The battle-load test fails if a death or a lost-unit alert cannot get through the impacts.
    - [ ] Keep the deliberately consolidated horse, arrow, spear, sword, and shield pools small until playtesting proves more variants are needed.

- [~] Route impacts by weapon and struck material where gameplay exposes both.
    - [~] Distinguish flesh, shield, armour, wood, stone, and earth. Gameplay exposes two of these honestly: the attacker's weapon, which already routed, and whether the target is a structure, which now routes to `combat.hit.structure`. Shield, armour and earth have no signal in RTS combat that a cue could read, so they are not faked.
    - [x] Layer weapon and material responses only after the base impact cue is proven wired.
    - [x] Verify every new route with a production combat scenario: a sword on masonry plays stone and not the shield clash, and a catapult keeps its own impact.

- [~] Give construction a complete audible lifecycle.
    - [x] Trigger the dedicated start and completion one-shots through the existing construction events. Both are now proven by a scenario that drives `ProductionSystem`.
    - [ ] Add a quiet spatial work loop only if it is connected to progress, builder count, cancellation, destruction, and completion. Deliberately not added: there is no authored work-loop recording, and this same item forbids standing one in from another cue.
    - [x] Never reuse placement or unit-ready sounds as permanent construction substitutes.

- [x] Improve variant selection only for cues that repeat audibly.
    - [x] Expand immediate-repeat avoidance to a short shuffle history for pools large enough to benefit. The window is half the pool, so a four-clip cue cannot repeat inside two plays.
    - [x] Add optional weights for rare variants: an optional `weights` array beside `resources` in the catalogue.
    - [x] Validate the selection logic through real repeated gameplay triggers, not direct mixer calls. Selection now also skips variants the mixer would refuse, which is what stopped bursts of blows from going quiet.

- [x] Add positional playback for proven world-space cues. See the spatial mix in `docs/AUDIO_RUNTIME_TRACE.md`.
    - [x] Pass world positions from the actual combat, movement, construction, wildlife, and siege events. 24 cues are marked `"spatial": true`; a cue with no position still plays flat.
    - [x] Pan and attenuate relative to the active camera or commander listener, published each frame from the camera.
    - [x] Keep UI, orders, alerts, and objective results non-spatial.

- [x] Aggregate dense battle events only after measuring actual trigger volume. The volume was measured first: about 19 impacts a second in a ten-a-side melee.
    - [x] Preserve detailed nearby and player-focused impacts: aggregation only ever consumes impacts distance has already silenced.
    - [x] Represent distant dense combat with controlled beds and sparse transients: six silenced impacts inside three seconds raise one `combat.distant_battle`, itself on a nine-second cooldown.
    - [x] Prove that the aggregation path is invoked during a maximum-size battle: a twenty-man fight with the camera 400 m away produces 109 silent impacts and the bed.

## P2 — Keep asset work reliable

- [x] Automate the approval-first SFX import workflow: `make audio-import`, or `scripts/audio_import.py`.
    - [x] Scan `new sfx/`, detect exact duplicates, and report codec, channels, sample rate, and duration. It also recognises a clip already shipped under another name, by content hash.
    - [x] Convert to 48 kHz stereo Vorbis OGG with no filters or timing changes.
    - [x] Produce a proposed source-to-destination map without touching live assets.
    - [~] After explicit approval, update files, manifest entries and QRC entries. Cue pools stay manual on purpose: which pool a new clip belongs in is an authoring decision, and the importer says so.

- [x] Add static asset validation: `scripts/audio_validate.py`, run by `make audio-check`.
    - [x] Confirm every cue resource exists, every manifest path exists, and every QRC audio path is valid. Effects and voice must be in the QRC; music and ambience ship loose and are exempt.
    - [~] Reject duplicate resource IDs and duplicate aliases; report broken direct promo references. The trailer scripts name seven music files that have never existed and fall back to a silent cut, so they are reported rather than failed: choosing replacement tracks is an authoring decision.
    - [x] Report leading silence and boundary discontinuities for human review, but never alter files automatically: `make audio-scan`, currently 71 findings including a hit sound with 485 ms of lead-in.

- [~] Record provenance for every imported or generated recording: `scripts/audio_provenance.py`.
    - [~] Store source/generator, creation date, prompt or recording notes, and license status. 103 of 223 tracks now carry a `provenance` block, backfilled only where the repository itself proves the answer. The other 120 need a person: guessing a licence is worse than recording that it is unknown.
    - [x] Update third-party attribution whenever a tracked audio file is replaced: `THIRD_PARTY_LICENSES.md` now points at the machine-readable record.
    - [x] Do not ship an asset whose usage rights are unknown: a new track with no provenance and no baseline entry fails `make audio-check`.

- [ ] Run listening passes through real gameplay builds. **This one needs ears and cannot be automated.** What the tooling can hand a listener is ready: `SOI_AUDIO_HUD=1` or `F9` shows what is firing and what is being dropped while you play, `SOI_AUDIO_TRACE_SUMMARY` writes the session up afterwards, and `make audio-scan` already lists the clips whose timing is worth checking first.
    - Test headphones, laptop speakers, and a typical stereo desktop setup.
    - Cover small skirmish, maximum melee, arrow volley, cavalry charge, siege, construction, repeated orders, and UI use.
    - Log missing triggers, masking, repetition, delayed feedback, harshness, and sounds that suggest the wrong action.
