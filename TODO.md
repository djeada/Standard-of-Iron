# Fight Animation Fluidity Refactor

Users are reporting short, flickering, jumpy fight animations and mechanical synchronized movement across all soldiers in a unit. The current code confirms the likely causes:

- Combat animation is driven by very short gameplay phases in `Engine::Core::CombatStateComponent` (`0.10s` impact, `0.16s` advance, `0.18s` strike, etc.) and `process_combat_state()` hard-switches phase state when each timer expires.
- `sample_anim_state()` can set `is_attacking` from either `CombatStateComponent` or melee lock fallback. That means soldiers can enter/leave attack intent from multiple conditions without a single visual ownership contract.
- `combat_phase_to_attack_phase()` maps current combat phase directly into a clip phase. When gameplay state exits early or flips between chase, melee lock, attack, and idle, the rendered clip can jump to a different phase.
- `apply_transient_attack_recovery()` exists, but the exit runway is only `0.18s` and it only stores a small amount of attack context. This is not enough to hide state churn, retargeting, LOD switches, or moving/attacking transitions.
- Per-soldier offsets and `visual_attack_variant()` exist, but all soldiers still inherit the same unit-level combat phase. The offsets are small, deterministic, and phase-locked, so units can still read as one synchronized mechanical puppet.

## Phase 1 - Diagnose And Stabilize The Current Pipeline

- [x] Add debug visualization/logging for each unit/soldier: `combat_phase`, `combat_phase_progress`, `attack_phase`, `attack_variant`, `is_attacking`, `is_in_melee_lock`, locomotion state, and chosen `AnimationStateId`.
- [x] Add a render debug overlay or arena toggle to freeze one selected unit and scrub attack phase from `0.0` to `1.0`.
- [x] Add assertions/counters for attack state churn: count transitions between attack/idle/walk/run per entity per second and flag units exceeding a threshold.
- [x] Add profiling scopes around combat state update, animation input sampling, humanoid preparation, BPAT playback, template cache lookup, and per-soldier layout generation.
- [x] Record current CPU cost for dense melee scenes before changing behavior: average frame time, p95 frame time, visible soldier count, cache miss count, and animation preparation time.
- [x] Extend arena test scenarios for sword, spear, bow, mounted, melee lock, chase-to-attack, attack-to-chase, target death, retargeting, hold/guard exit, and LOD switching.
- [x] Document exact current attack flow across `attack_processor.cpp`, `combat_state_processor.cpp`, `animation_inputs.cpp`, `pose_intent.cpp`, and `humanoid/prepare.cpp`.
- [x] Identify duplicated combat-animation decisions across `sample_anim_state()`, `resolve_animation_intent()`, `combat_phase_to_attack_phase()`, `resolve_humanoid_animation_selection()`, and renderer-specific pose layers.
- [x] Mark code smells with owner notes: attack booleans from multiple systems, magic timing constants, mixed gameplay/render state, direct component mutation from render preparation, and hidden fallback loops.
- [x] Temporarily increase `k_transient_attack_recovery_seconds` behind a tuning constant and test whether longer exits immediately reduce visible flicker.

Acceptance criteria:

- We can reproduce the flicker in a controlled arena scene.
- We can identify whether each visible pop came from gameplay phase churn, clip phase discontinuity, variant change, LOD/template switch, movement state switch, or target/mode retargeting.
- We have baseline CPU numbers before the refactor, so performance wins or regressions are measurable.
- No visual refactor starts without baseline recordings/screenshots.

## Phase 2 - Introduce A Dedicated Visual Combat Animation State Machine

- [x] Add `CombatVisualAnimationComponent` or extend `HumanoidAnimationStateComponent` with a stable visual attack transaction: `None`, `Enter`, `Anticipation`, `Strike`, `FollowThrough`, `Recover`, `ExitBlend`.
- [x] Decouple visual attack lifetime from damage timing. Gameplay can still apply damage on cooldown, but visuals must complete minimum readable windows before exiting.
- [x] Replace direct attack/idle flipping in render selection with a visual state resolver that owns:
  - attack start time,
  - locked attack family,
  - locked variant,
  - source target id,
  - phase,
  - minimum hold time,
  - exit blend progress,
  - interruption reason.
- [x] Move combat animation timing constants out of scattered code and into one typed configuration object with names for gameplay timing, visual timing, blend timing, and debug overrides.
- [x] Split pure decision logic from ECS mutation. State resolution should be testable with simple structs before it writes components.
- [x] Replace ambiguous visual-state booleans like `is_attacking`, `is_melee`, and `is_in_melee_lock` with a richer resolved intent object, while keeping compatibility fields only at system boundaries.
- [x] Stop render preparation from recomputing the same unit-level combat facts for every soldier. Compute once per unit, then pass compact immutable inputs into per-soldier preparation.
- [x] Avoid adding/removing visual components every frame. Prefer stable components with explicit state fields to reduce ECS churn and allocator pressure.
- [x] Add explicit exit policies:
  - normal complete: finish follow-through and recover,
  - target died: finish current recovery unless unit is dying/staggered,
  - chase resumes: blend attack upper body out while lower body resumes locomotion,
  - retarget: keep current attack until recovery, then enter next attack,
  - hit reaction/death: allowed hard interrupt.
- [x] Make attack phase monotonic inside a visual attack transaction. Never jump backwards unless a new transaction starts.
- [x] Add tests for visual state continuity: attack phase cannot reset to `0` during recovery, variant cannot change mid-attack, and exit blend lasts at least the configured duration.

Acceptance criteria:

- A unit leaving combat no longer snaps from strike/recover directly to idle/walk.
- Target death and retargeting do not restart the same soldier at frame zero unless a new attack transaction is intentionally created.
- The renderer has one authoritative combat visual state per soldier, not multiple ad hoc attack booleans.
- Combat visual state transition logic has focused unit tests and can be read without following five separate render/gameplay files.
- CPU cost does not increase from state-machine bookkeeping in the baseline dense melee scene.

## Phase 3 - Per-Soldier Variety, Desynchronization, And Combat Roles

- [x] Promote per-soldier combat state from derived offsets to persistent lane state. Each soldier gets a stable combat lane: lead striker, support striker, shield bracer, step-in, step-out, idle-ready, ranged reload, etc.
- [x] Assign lanes from deterministic seeds, row/column position, health, weapon family, and local enemy pressure.
- [x] Store per-soldier lane, variant, and phase state in a compact structure owned by the existing layout/animation cache, not in separate heap allocations per soldier.
- [x] Make per-soldier state invalidation explicit: formation changed, soldier count changed, unit type changed, visual seed changed, or combat animation schema changed.
- [x] Give each soldier independent timing windows within a unit-level attack event:
  - front rank attacks first,
  - second rank thrusts/supports,
  - rear rank braces, reloads, or shifts weight,
  - wounded/missing soldiers reduce visible participation.
- [x] Increase attack phase offset range for active combat, but keep it structured by rank so it looks tactical instead of random noise.
- [x] Expand `visual_attack_variant()` into a richer selector: slash left/right, overhead, shield bash, spear thrust high/low, bow draw/release/reload, mounted pass, blocked strike.
- [x] Add micro-variation layers: torso lean, shoulder delay, wrist angle, shield reaction, small foot adjustment, head tracking, breathing, stagger on nearby impact.
- [x] Ensure variety is stable across frames and LOD changes by deriving it from persistent soldier seed plus current visual attack transaction id.
- [x] Replace ad hoc random/thread-local RNG in visual selection paths with deterministic hash helpers where possible, improving reproducibility and avoiding hidden synchronization costs.
- [x] Precompute row/column/rank data in the layout cache instead of recalculating it in every per-soldier render path.
- [x] Keep lane selection branch-light in hot loops: resolve high-level lane once, then use small tables for phase offsets, variant weights, and role probabilities.

Acceptance criteria:

- A 15-soldier infantry unit never has every soldier in the same attack phase.
- At least three visible behaviors are present in melee: striking, bracing/blocking, and repositioning.
- Ranged units show staggered draw/release/reload instead of one simultaneous identical volley animation.
- Per-soldier variety does not add avoidable allocations or repeated formation math in the hot render path.

## Phase 4 - Blend System, Assets, And Procedural Pose Layers

- [x] Add clip blend support for humanoid BPAT playback: previous clip, next clip, blend weight, and configurable blend duration.
- [x] Add upper/lower body layering so soldiers can keep walking while upper body attacks, aims, blocks, or exits.
- [x] Design BPAT blend data for cache locality: contiguous bone palettes, stable clip ids, reusable blend descriptors, and no per-frame vector growth.
- [ ] Add a fast path for zero-weight and one-weight blends so unchanged single-clip playback pays no blend overhead.
- [x] Centralize clip playback normalization and frame lookup. Remove duplicate phase wrapping/frame index code currently present in multiple preparation/pipeline paths.
- [ ] Add additive procedural layers after clip sampling:
  - attack anticipation lean,
  - follow-through recoil,
  - shield absorb,
  - weapon trail pose,
  - small step recovery,
  - hit reaction overlay.
- [ ] Replace hard combat phase boundaries with eased curves. Use cubic/smoothstep curves for anticipation, strike acceleration, impact settle, and recovery.
- [ ] Add missing clips or generated BPAT variants for:
  - sword slash left/right/overhead,
  - spear thrust high/low/brace,
  - bow draw/release/reload,
  - shield block/bash,
  - melee idle-ready,
  - attack exit to idle,
  - attack exit to walk/run.
- [ ] Add event markers to clips: anticipation start, weapon release, contact, recover unlocked, exit-safe. Damage/VFX/audio should align to markers instead of arbitrary cooldown phase.
- [ ] Ensure template prewarm includes new attack variants and blend pairs to avoid first-use hitches that look like flicker.
- [ ] Keep procedural pose layers small and composable. Each layer should accept immutable pose/context input and return a clear output or mutate only the pose it owns.
- [ ] Remove renderer-specific attack pose hacks once the shared clip/layer system covers those cases.
- [ ] Add cache metrics for blended clips: prepared pair count, hit rate, eviction count, and bytes used.

Acceptance criteria:

- Attack entry and exit blend smoothly at 30 FPS and 60 FPS.
- No visible pop when switching attack sword/spear/bow to idle/walk/run.
- Damage, weapon motion, audio, dust, and hit feedback occur at visually credible contact frames.
- Blend support keeps the old single-clip path simple and fast.

## Phase 5 - Validation, Performance, And Rollout

- [x] Add automated continuity tests for attack phase monotonicity, minimum exit duration, variant stability, and legal interruption paths.
- [ ] Add screenshot/video capture tests in arena for before/after comparison across dense battles.
- [ ] Add performance metrics for per-soldier animation state memory, BPAT blend cost, template cache hit rate, and visible soldier count.
- [ ] Add a budget test or benchmark for dense melee humanoid preparation so future changes cannot silently reintroduce CPU spikes.
- [ ] Add a static cleanup pass after rollout: remove obsolete fallback attack loops, unused transient recovery fields, duplicated phase helpers, and compatibility booleans no longer needed outside boundaries.
- [ ] Add tuning config for combat animation durations, exit blend, per-rank delays, lane probabilities, and attack variant weights.
- [ ] Keep tuning data data-driven and validated at startup. Invalid durations, blend windows, variant weights, or lane tables should fail loudly in tools/tests.
- [ ] Replace the old combat animation path completely. Do not keep a feature flag, compatibility mode, or permanent parallel renderer path.
- [ ] Compare player-facing scenarios: small skirmish, dense infantry line, cavalry charge, archer volley, mixed melee/ranged, commander-controlled RPG combat.
- [ ] Delete the old direct attack fallback in the same migration once the new state machine covers melee lock, ranged attacks, specials, mounted attacks, hold/guard, death, and hit reaction.
- [ ] Remove obsolete tests that assert old attack fallback behavior and replace them with continuity/state-machine tests.
- [ ] Fail CI if old combat animation entry points are reintroduced outside the new state resolver.
- [x] Update architecture docs with the final ownership rule: gameplay emits attack opportunities/events, visual combat state owns animation lifetime, and render preparation consumes immutable resolved state.

Acceptance criteria:

- No regression in large-battle frame time beyond the agreed budget.
- Dense melee CPU cost is equal or better than baseline after cache and cleanup work.
- Dense melee reads as fluid and varied instead of synchronized.
- Exiting combat is smooth in the worst cases: target death, retarget, chase resume, hold/guard exit, stagger, and LOD switch.
- The old flicker/jitter reproduction scene passes without visible snapping.
- There is exactly one combat animation path after migration.
- The final code has fewer duplicated phase/variant decisions and a clearer separation between gameplay combat, visual state, and render preparation.

## Core Design Principle

Gameplay decides when attacks are valid and when damage happens. The visual combat state machine decides how soldiers enter, express, and exit those attacks. Exiting an attack must be a first-class animation state, not the absence of an attack boolean.
