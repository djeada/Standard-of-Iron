# Audio Improvement TODO

The priority is audible gameplay wiring, not isolated mixer code. Every audio task must prove the complete path:

`gameplay event → cue ID → cue binding → loaded resource → accepted playback → audible output`

An audio feature is not complete until a real gameplay scenario triggers it and runtime evidence confirms that it reached the output. Preserve authored source files in full; imports must not trim, normalize, fade, or otherwise alter timing without explicit approval.

## P0 — Wire and prove every gameplay sound

- [ ] Maintain one wiring matrix for the entire audio catalogue. The first static pass is recorded in `docs/AUDIO_WISHLIST.md`: all 93 catalogue cues currently have production call sites and playable resources.
    - Record each cue ID, gameplay action, event/call site, ownership/audience rule, expected frequency, resource pool, and verification scenario.
    - Distinguish “declared,” “called by code,” “resource loaded,” and “heard in gameplay.”
    - Do not count a mixer unit test or catalogue entry as proof that a gamer can hear the sound.

- [ ] Add end-to-end runtime tracing for every cue request. First pass now carries cue IDs into the audio thread and reports accepted playback or concrete drop reasons under `SOI_AUDIO_TRACE`.
    - Log the gameplay source, cue ID, chosen resource, and final result: played, cooldown drop, instance-limit drop, priority drop, muted, unloaded, or missing.
    - Include timestamps and listener/camera context.
    - Export a mission summary with request and audible-play counts per cue.

- [ ] Create deterministic gameplay verification scenarios.
    - Provide focused scenarios for UI/orders, placement, construction start/complete/rejection, objective results, melee, arrows, cavalry, deaths, siege, commander actions, wildlife, and state changes.
    - Each scenario must trigger the real production gameplay path rather than calling the mixer directly.
    - Fail verification when an expected cue never reaches accepted playback.

- [ ] Audit all 93 current cues in real gameplay.
    - Confirm that the event actually occurs, audience filtering is correct, the resource is loaded at that moment, and the sound is audible at the intended volume.
    - Fix disconnected or incorrectly routed cues before adding new mixer features.
    - Remove dead cues and unused audio-system branches rather than maintaining code with no gameplay caller.

- [x] Wire the four dedicated construction/objective replacements.
    - `build.placement_rejected` must use its own rejection recording.
    - `build.construction_started` must use its own work-start recording.
    - `build.construction_complete` must use its own completion recording.
    - `alert.objective_complete` must use its own objective-success recording.
    - Verify each through its real gameplay event after installation.

- [ ] Add a developer audio status overlay driven by runtime state.
    - Show the last requested cue, chosen resource, drop reason, loaded state, active instances, and effective volume.
    - Show cues requested frequently but rarely or never accepted.
    - Make silent wiring failures visible during ordinary playtests.

## P1 — Improve the sounds gamers actually hear

- [ ] Use runtime frequency data to tune cue cooldowns, priorities, and pools.
    - Focus first on melee impacts, arrows, deaths, movement, construction, orders, and UI feedback.
    - Prevent frequent events from masking commander actions, alerts, and objective feedback.
    - Keep the deliberately consolidated horse, arrow, spear, sword, and shield pools small until playtesting proves more variants are needed.

- [ ] Route impacts by weapon and struck material where gameplay exposes both.
    - Distinguish flesh, shield, armour, wood, stone, and earth.
    - Layer weapon and material responses only after the base impact cue is proven wired.
    - Verify every new route with a production combat scenario.

- [ ] Give construction a complete audible lifecycle.
    - Trigger the dedicated start and completion one-shots through the existing construction events.
    - Add a quiet spatial work loop only if it is connected to progress, builder count, cancellation, destruction, and completion.
    - Never reuse placement or unit-ready sounds as permanent construction substitutes.

- [ ] Improve variant selection only for cues that repeat audibly.
    - Expand immediate-repeat avoidance to a short shuffle history for pools large enough to benefit.
    - Add optional weights for rare variants.
    - Validate the selection logic through real repeated gameplay triggers, not direct mixer calls.

- [ ] Add positional playback for proven world-space cues.
    - Pass world positions from the actual combat, movement, construction, wildlife, and siege events.
    - Pan and attenuate relative to the active camera or commander listener.
    - Keep UI, orders, alerts, and objective results non-spatial.

- [ ] Aggregate dense battle events only after measuring actual trigger volume.
    - Preserve detailed nearby and player-focused impacts.
    - Represent distant dense combat with controlled beds and sparse transients.
    - Prove that the aggregation path is invoked during a maximum-size battle.

## P2 — Keep asset work reliable

- [ ] Automate the approval-first SFX import workflow.
    - Scan `new sfx/`, detect exact duplicates, and report codec, channels, sample rate, and duration.
    - Convert to 48 kHz stereo Vorbis OGG with no filters or timing changes.
    - Produce a proposed source-to-destination map without touching live assets.
    - After explicit approval, update files, cue pools, manifest entries, QRC entries, promo references, and generated documentation.

- [ ] Add static asset validation.
    - Confirm every cue resource exists, every manifest path exists, and every QRC audio path is valid.
    - Reject duplicate resource IDs, duplicate aliases, and broken direct promo references.
    - Report leading silence and boundary discontinuities for human review, but never alter files automatically.

- [ ] Record provenance for every imported or generated recording.
    - Store source/generator, creation date, prompt or recording notes, and license status.
    - Update third-party attribution whenever a tracked audio file is replaced.
    - Do not ship an asset whose usage rights are unknown.

- [ ] Run listening passes through real gameplay builds.
    - Test headphones, laptop speakers, and a typical stereo desktop setup.
    - Cover small skirmish, maximum melee, arrow volley, cavalry charge, siege, construction, repeated orders, and UI use.
    - Log missing triggers, masking, repetition, delayed feedback, harshness, and sounds that suggest the wrong action.
