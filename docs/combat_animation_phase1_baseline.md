# Combat Animation Phase 1 Baseline

## Current attack flow

1. `game/systems/combat_system/attack_processor.cpp`
   - `begin_attack_animation()` creates or reuses `CombatStateComponent`.
   - New attacks start in `Advance`, seed `attack_offset`, seed `attack_variant`, and lock `attack_family`.
   - Gameplay damage timing still lives in combat/cooldown code outside render preparation.

2. `game/systems/combat_system/combat_state_processor.cpp`
   - `process_combat_state()` advances `CombatStateComponent::animation_state` with short fixed durations.
   - The phase machine is gameplay-owned and resets `state_time` to zero whenever a phase boundary is crossed.

3. `render/gl/humanoid/animation/animation_inputs.cpp`
   - `sample_anim_state()` converts gameplay state into render inputs.
   - Attack ownership is currently shared across three sources:
     - active `CombatStateComponent`,
     - melee-lock fallback,
     - target/range/recent-fire fallback when combat state is absent.
   - The sampled unit state now records which source produced the visible attack intent.

4. `render/creature/pose_intent.cpp`
   - `resolve_animation_intent()` and `resolve_pose_intent()` translate sampled booleans plus locomotion into a higher-level action/pose.
   - This is the first render-only decision layer that can disagree with raw gameplay booleans.

5. `render/humanoid/prepare.cpp`
   - `prepare_humanoid_instances()` expands one unit sample into per-soldier layouts, locomotion, attack offsets, and final `HumanoidAnimationContext`.
   - `combat_phase_to_attack_phase()` maps gameplay phase plus per-soldier offsets into clip phase.
   - `apply_transient_attack_recovery()` can override sampled inputs during render preparation to keep the outgoing attack clip alive after gameplay already exited the attack.
   - `resolve_humanoid_animation_selection()` picks the final `AnimationStateId` and clip variant per visible soldier.

## Duplicated combat-animation decisions

- `sample_anim_state()` decides whether the unit is attacking.
- `resolve_animation_intent()` decides whether locomotion or action wins.
- `combat_phase_to_attack_phase()` independently maps gameplay state to clip phase.
- `resolve_humanoid_animation_selection()` can change the final `AnimationStateId` again through manifests and variant tables.
- `render/creature/pipeline/creature_prepared_state.cpp::resolve_elephant_animation_state()` re-derives attack intent from combat state and melee lock after `sample_anim_state()`.

## Diagnostics added in Phase 1

- Arena combat debug overlay now shows per-unit inputs and per-soldier resolved state.
- The overlay reports:
  - `combat_phase`,
  - `combat_phase_progress`,
  - derived `attack_phase`,
  - `attack_variant`,
  - `is_attacking`,
  - `is_in_melee_lock`,
  - locomotion state,
  - chosen `AnimationStateId`,
  - cull reason,
  - churn counters,
  - likely pop sources (`phase-reset`, `variant`, `lod`, `move`, `recovery`).
- The arena also supports freezing the selected unit's combat state and scrubbing attack phase from `0.0` to `1.0` without bypassing the normal combat-state path.
- Arena quick setup now includes named Phase 1 scenario presets for sword, spear, bow, mounted, melee lock, chase/retarget/death transitions, hold/guard exit, and LOD switching.

## Profiling baseline hooks

- `combat_state_update_us` wraps `process_combat_state()`.
- `animation_input_sampling_us` wraps `sample_anim_state()`.
- `humanoid_preparation_us` wraps `prepare_humanoid_instances()`.
- `soldier_layout_generation_us` wraps layout cache lookup/build.
- `render_asset_cache_lookup_us` counts render-asset handle cache lookups and hit/miss totals.
- `bpat_playback_us` wraps runtime clip playback resolution in `creature_pipeline.cpp`.
- The profiling HUD and arena stats overlay now report rolling average frame time, p95 frame time, visible soldier count, cache misses, and animation preparation time.

## Owner notes on current code smells

- **Shared attack ownership:** attack booleans currently come from gameplay combat state, melee lock fallback, and attack-target fallback.
- **Magic timing constants:** gameplay phase durations and transient recovery duration still live as scattered constants.
- **Mixed gameplay/render state:** render preparation still needs to reinterpret gameplay timing instead of consuming a stable visual attack transaction.
- **Render-time mutation:** `apply_transient_attack_recovery()` mutates sampled inputs during preparation to keep clips alive.
- **Hidden fallback loops:** when gameplay stops producing combat state, render code can still re-enter attack visuals through melee-lock or recent-fire fallback.

## Phase 1 tuning result

- `k_transient_attack_recovery_seconds` is now routed through a named Phase 1 tuning constant and temporarily increased to `0.32s` to make exit churn easier to inspect before the larger Phase 2 refactor.

## Phase 4/5 rollout notes

- Gameplay still decides when attacks are valid and when contact/damage happens, but the render path now consumes `CombatVisualResolvedState` as the authoritative visual lifetime for entry, strike, recovery, and exit.
- Humanoid animation selection now emits fixed-size blend descriptors:
  - full-body blends for attack entry/exit continuity,
  - upper-body overlays for walking melee attacks,
  - stable clip ids/phases/variants without per-frame heap growth.
- BPAT phase normalization and frame lookup now go through shared playback helpers so preparation and submit paths do not duplicate clip wrapping logic.
- Full-detail rigged submission can mix palettes safely with queue-owned blended palettes, while snapshot/minimal LOD stays on a dominant-clip collapse path to avoid cache-key explosion.
- Submit stats now report layered/full-body blend request counts so blend-heavy scenes can be profiled without reintroducing render-path guesswork.
