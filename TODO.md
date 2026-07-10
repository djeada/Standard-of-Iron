# RPG Combat and Animation Architecture TODO

## Goal

Move RPG/commander combat from the current controller-driven and phase-transition-driven implementation to a clean, animation-synchronized combat action pipeline.

The desired end state is:

- Combat timing is authored by attack/action definitions, not hardcoded in the controller.
- Damage is applied only when a weapon/projectile/body hit volume actually contacts a valid target during the authored active window.
- Rendering reads combat/action state but does not own gameplay decisions.
- Swords, spears, bows, cavalry attacks, commander-vs-commander, and commander-vs-unit fights all use the same core action, hit detection, and damage-resolution pipeline.
- Existing RTS combat continues to work during migration.

## Current Implementation Progress

Implemented so far:

- `CombatActionId`, `WeaponFamily`, `CombatActionEventType`,
  `DamageProfile`, and `HitShapeProfile` exist under
  `game/systems/combat_actions/`.
- `CombatActionService::request_attack()` owns RPG commander attack startup for
  sword, spear, and ranged-preferred bow commanders.
- RPG sword actions, spear actions, and `RpgBowShot` are defined in
  `combat_action_definition.*`.
- `RpgCommanderActionComponent` stores action id, normalized action time, event
  cursor, active trace state, and one-hit-per-action target runtime.
- `combat_state_processor.cpp` advances authored action events.
- `RpgCommanderActionComponent` now separates generic authored active windows
  from weapon-trace windows, so bows/projectile releases, sword/spear traces,
  and mounted body impacts do not share one ambiguous runtime flag.
- Sword and spear commander damage now uses active-window weapon trace contact
  for action-backed FPV attacks.
- Infantry sword and spear traces can now receive previous/current authored
  action time and test contact against swept weapon segments, with the older
  transform/shape trace kept as fallback for unsupported actions.
- Infantry RPG sword actions first sample interpolated baked BPAT `grip_r`
  plus `sword_blade_base_r`/`sword_blade_tip_r` socket frames from
  `humanoid_sword.bpat`, then fall back to deriving the blade segment from the
  grip socket and finally authored pose math for older assets.
- Infantry spear actions first sample interpolated baked BPAT
  `spear_shaft_base_r`, `spear_shaft_tip_r`, and `spear_head_tip_r` socket
  frames from `humanoid_spear.bpat`, then fall back to the authored attack pose
  manifest for older assets.
- Mounted sword actions sample baked BPAT `sword_blade_base_r` and
  `sword_blade_tip_r` sockets from the riding sword strike clip in
  `humanoid_sword.bpat`, so mounted sword damage follows the visible close
  side strike instead of the old broad reach shape.
- Mounted spear now has a dedicated non-looping `riding_spear_thrust` BPAT clip
  with baked spear endpoint sockets. Mounted spear traces sample those sockets,
  so contact and visible rider playback use the same authored asset and phase.
- Mounted action definitions own their rider clip id. Animation input sampling
  carries that clip plus `normalized_action_time` through primary and layered
  render requests, so mounted sword, spear, and charge playback no longer gets
  re-derived from generic attack family/state at draw time.
- `CombatHitContact` and `resolve_commander_action_hit()` route commander
  action contacts through the current RPG damage policy, including authored
  action damage multipliers before combo/guard/unit damage resolution.
- Commander-vs-commander action hits now carry authored posture damage and
  guard pressure through the same resolver path while legacy RPG/RTS damage
  calls keep their existing derived pressure behavior.
- `RpgBowShot` emits a `ProjectileRelease` event and spawns a damaging
  projectile through `ProjectileSystem`.
- Projectile impacts now build contact-aware hit requests and route through
  `resolve_projectile_impact_hit()` before applying RPG HP or unit damage.
- Cursed-arrow projectile impacts now apply/refresh cursed status inside the
  contact-aware projectile resolver; projectile flight no longer owns that
  impact effect policy.
- Fireball projectile direct impacts now apply/refresh burning status inside the
  contact-aware projectile resolver; projectile flight no longer owns that
  per-hit burning policy.
- Fireball projectile area impacts now route splash target selection, per-target
  damage, and fire patch creation through `resolve_projectile_area_impact_hit()`.
- Fire patch contact refreshes now call `apply_fire_patch_contact_effect()`, so
  target filtering and burning-status refresh policy live beside the projectile
  hit resolver.
- `process_combat_status_effects()` now owns cursed-status expiry, burning tick
  damage, and fire patch lifetime/contact iteration in
  `game/systems/combat_system/combat_status_effect_processor.*`. Burning ticks
  route damage through `resolve_projectile_impact_hit()` so RPG commanders and
  unit targets keep the same damage boundary as projectile impacts.
- `CombatStatusEffectSystem` is registered in the world update order before
  `ProjectileSystem`, so long-lived projectile-created effects tick outside
  projectile flight while preserving the previous before-impact cadence.
- `process_spear_brace_state()` now converts explicit, hold, guard, and FPV
  commander-guard intent into one `SpearBraceComponent` runtime. The component
  records its intent source, entry/exit progress, and readiness; a brace becomes
  gameplay-active only after 85% of pose entry, while rendering reads the same
  requested/active state for its guard transition.
- Braced spear thrust contacts against fast mounted targets can interrupt
  charge motion through the contact-aware resolver by applying knockdown-tier
  stagger and stopping target movement. The resolver now consumes only the
  explicit brace runtime instead of independently inferring hold/guard state.
- `MountedChargeImpact` exists as an authored action definition, and mounted
  charge impact contacts can route through `resolve_mounted_charge_impact_hit()`
  for speed-scaled damage/stagger policy.
- `MountedSwordSlash` and `MountedSpearThrust` exist as authored action
  definitions, and FPV mounted commanders select them from the same action
  service used by infantry sword/spear attacks.
- `find_body_impact_contact()` can scan mount/body charge volumes, and
  action-backed `MountedChargeImpact` runtime can apply body impacts during the
  authored active window with same-target suppression.
- Moving cavalry can now start the authored `MountedChargeImpact` action from
  combat-state processing when current movement velocity and body contact meet
  charge conditions.
- `MountedChargeComponent` tracks mounted charge runtime state
  (`Ready`, `Charging`, `ImpactActive`, `Cooldown`), player/AI/contact-auto
  intent source, speed-loss grace, cancellation reason, cooldown, active target,
  and last impacted target. The action begins before contact, becomes damaging
  on authored `ActiveStart`, and cancels immediately on spear knockdown or after
  sustained speed loss.
- Mounted vanguard input requests player charge intent; non-FPV cavalry with a
  forward attack target can request AI charge intent. Contact-auto remains as a
  migration fallback for cavalry without explicit behavior integration.
- Authored actions own duration and elapsed time. Event crossing, weapon traces,
  projectile release, charge activation, animation phase, and clip phase now use
  `RpgCommanderActionComponent` time rather than deriving normalized time from
  `CombatStateComponent` phase durations.
- `combat_action_processor.cpp` advances authored actions independently across
  entities with `RpgCommanderActionComponent` and owns projectile release,
  weapon traces, body impacts, hit-list updates, and resolver dispatch.
  `combat_state_processor.cpp` now contains only legacy phase transitions and
  the no-action-id commander contact fallback.
- `melee_attack_style` and `legacy_rpg_melee_style` have been removed. Stable
  action ids and definition-owned clips are the only RPG/mounted clip selectors.
- Commander-vs-commander acceptance coverage now includes perfect guard, guard
  break, authored posture/guard pressure, dodge invulnerability, finisher punish,
  and same-swing target suppression through the shared hit resolver.

Deferred compatibility cleanup:

- Weapon tracing is no longer only transform/shape based for action-backed RPG
  infantry and mounted weapon attacks. Compatibility transform/shape fallback
  still exists for unsupported actions/assets.
- `CombatStateComponent` remains for RTS combat, legacy commander attacks, input
  buffer adaptation, and older animation consumers. It no longer owns authored
  action timing or action-backed damage. Remove it from RPG action startup only
  after RTS and presentation state have their own replacement contracts.

This plan is based on the current code paths:

- Player RPG input and attack start: `app/core/commander_control_controller.cpp`
- Commander mode setup/tick: `app/core/commander_mode_coordinator.cpp`
- Shared combat phase state and contact damage timing: `game/systems/combat_system/combat_state_processor.cpp`
- RTS attack processing and enemy damage into RPG commander: `game/systems/combat_system/attack_processor.cpp`
- RPG engagement/stagger/telegraph processing: `game/systems/rpg_combat_system/rpg_combat_processor.cpp`
- RPG damage/guard/combo resolution: `game/systems/rpg_combat_system/rpg_commander_damage.cpp`
- RPG HP/guard/action/combat components: `game/core/component.h`
- Animation input sampling: `render/gl/humanoid/animation/animation_inputs.cpp`
- Clip and RPG sword animation mapping: `animation/clip_manifest.cpp`, `animation/clip_manifest.h`
- Animation core build list: `animation/CMakeLists.txt`
- Game systems build list: `game/CMakeLists.txt`

## Current State

### Attack start

`CommanderControlController::primary_action()` resolves player target/input hints
and forwards an `AttackRequest` to `CombatActionService`. The service selects the
action definition, initializes generic action runtime, writes the temporary
presentation adapter, and spends definition-authored stamina cost. The controller
does not select attack phases, clips, or action runtime fields.

### Damage timing

`process_combat_state()` still advances this compatibility state machine:

`Advance -> WindUp -> Strike -> Impact -> Recover -> Reposition -> Idle`

For legacy FPV commander attacks without an authored action id, damage can still
be applied when the state transitions from `Strike` to `Impact`:

- `deal_commander_contact_damage()`
- `resolve_commander_contact_target()`
- `target_in_swing_arc()`
- `RpgCombat::deal_commander_attack_damage()`

For action-backed attacks, `advance_combat_action_events()` advances definition
duration using action-owned elapsed time. Sword/spear traces run while
`weapon_trace_active` is true, bow shots release on `ProjectileRelease`, and
mounted body impacts run during their authored active window. The phase-transition
damage path executes only for legacy FPV attacks without an action id.

### Damage resolution

`rpg_commander_damage.cpp` contains the useful RPG gameplay rules:

- combo multiplier,
- finisher multiplier,
- power strike multiplier,
- punish-opening multiplier,
- RPG HP routing,
- unit HP routing,
- perfect guard,
- normal guard,
- posture pressure,
- guard break,
- stagger,
- hit feedback event publishing.

This should remain mostly as the damage-resolution layer, but it should receive precise hit/contact information from a separate hit detection layer.

### Animation selection

Rendering samples `CombatStateComponent` and `RpgCommanderActionComponent` in `animation_inputs.cpp`.

For FPV sword attacks, `CombatActionId` maps directly to:

- `RpgSlashLeft`
- `RpgSlashRight`
- `RpgOverhead`
- `RpgThrust`
- `RpgFinisher`

Those map to clips in `animation/clip_manifest.cpp`.

Mounted definitions additionally own explicit rider BPAT clip ids. Render requests
preserve explicit clips and normalized action phase through primary/full-body/
upper-body playback instead of resolving them again from generic state.

### Enemy behavior around commander

`rpg_combat_processor.cpp` assigns nearby enemies into RPG engagement roles:

- front attacker,
- left threat,
- right threat,
- support.

It also keeps active attackers at an ideal distance and support enemies circling. This is useful and should remain separate from hit detection and damage.

## Desired Architecture

### Layer 1: Input and Intent

Owner:

- `app/core/commander_control_controller.cpp`
- later possibly AI behavior code for non-player commanders.

Responsibility:

- Convert input into requests such as:
  - light sword attack,
  - heavy sword attack,
  - spear thrust,
  - bow shot,
  - guard,
  - dodge,
  - jump,
  - shield bash,
  - vanguard rush,
  - cavalry slash,
  - cavalry charge.

Non-responsibility:

- It should not decide combat phase durations.
- It should not know exact damage contact timing.
- It should not hardcode clip-specific hit windows.
- It should not run hit detection.

Target API shape:

```cpp
CombatActionService::request_attack(world, attacker_id, AttackRequest{
    .action_id = CombatActionId::RpgSwordSlashLeft,
    .target_hint_id = target_id,
    .input_direction = AttackInputDirection::Left,
    .charged = false,
});
```

### Layer 2: Combat Action State

Owner:

- new files under `game/systems/combat_actions/` or `game/systems/combat_system/action_*`.

Initial integration:

- reuse `CombatStateComponent` where practical,
- introduce focused runtime helpers before replacing the component.

Desired state data:

- current action id,
- action family: sword, spear, bow, shield, horse, body,
- clip id or clip selector,
- normalized action time,
- current authored phase,
- event cursor,
- input buffer state,
- cancel window state,
- target hint,
- already-hit entity ids for the current swing,
- hit trace runtime data.

Migration note:

- Do not rename `CombatStateComponent` immediately. It is used by render sampling and tests.
- First add an action definition service that fills existing `CombatStateComponent`.
- Later split or replace it with `CombatActionComponent` once the new pipeline is stable.

### Layer 3: Action Definitions

Owner:

- new `game/systems/combat_actions/combat_action_definition.*`
- possibly mirrored animation metadata in `animation/action_manifest.*` or `animation/clip_manifest.*`

An action definition describes gameplay timing and hit behavior.

Target data shape:

```cpp
struct CombatActionDefinition {
  CombatActionId id;
  WeaponFamily weapon_family;
  Animation::SwordAttackAnimation sword_clip;
  Engine::Core::CombatAttackFamily attack_family;
  Engine::Core::AttackDirection attack_direction;
  DamageProfile damage_profile;
  HitShapeProfile hit_shape;
  std::vector<CombatActionEvent> events;
  float stamina_cost;
  float posture_damage;
  float guard_pressure;
  int max_targets;
  bool can_hit_same_target_once;
  bool requires_projectile_release;
};
```

Initial definitions:

- `RpgSwordSlashLeft`
- `RpgSwordSlashRight`
- `RpgSwordOverhead`
- `RpgSwordThrust`
- `RpgSwordFinisher`

Future definitions:

- `RpgSpearThrust`
- `RpgSpearSweep`
- `RpgBowShot`
- `MountedSwordSlash`
- `MountedSpearThrust`
- `CavalryChargeImpact`
- `ShieldBash`
- `VanguardRushImpact`

### Layer 4: Animation-Authored Events

Owner:

- new combat action definition files,
- later optionally generated or data-loaded from asset metadata.

Events should be crossed by normalized action time. Example events:

```cpp
enum class CombatActionEventType {
  WindupStart,
  ActiveStart,
  WeaponTraceStart,
  WeaponTraceEnd,
  ProjectileRelease,
  RecoveryStart,
  CancelWindowStart,
  CancelWindowEnd,
  ExitSafe
};
```

For current sword attacks, seed event windows from existing clip marker knowledge in `animation/clip_manifest.cpp`:

- `anticipation_start`
- `weapon_release`
- `contact`
- `recover_unlocked`
- `exit_safe`

Important:

- Current gameplay damage happens at one phase transition.
- Desired gameplay damage happens while `WeaponTraceStart <= action_time <= WeaponTraceEnd`, and only if the weapon hit volume contacts a target.

### Layer 5: Hit Detection

Owner:

- new `game/systems/combat_actions/weapon_trace.*`
- use existing transform/unit/combat utility code from `game/systems/combat_system/combat_utils.*`

Hit detection should produce contact facts, not apply damage directly.

Target result shape:

```cpp
struct CombatHitContact {
  Engine::Core::EntityID attacker_id;
  Engine::Core::EntityID target_id;
  WeaponFamily weapon_family;
  QVector3D contact_point;
  QVector3D contact_normal;
  float action_time;
  float relative_speed;
  bool from_projectile;
  bool from_mount_charge;
};
```

Initial sword implementation:

- Start with a swept capsule/arc in front of the commander using:
  - `TransformComponent::position`,
  - `TransformComponent::rotation.y`,
  - current action progress,
  - action definition reach/radius.
- Use `UnitComponent` target validity and `OwnerRegistry` enemy checks.
- Use `combat_radius()` from `combat_utils` for target size.
- Track already-hit ids per swing.

Later sword implementation:

- Replace approximate arc with real weapon socket tracing.
- Use animation pose/bone/socket data where available.
- Trace previous sword blade segment to current sword blade segment.

Spear implementation:

- Use a long forward capsule during thrust active frames.
- Narrower side tolerance than sword.
- Higher anti-charge posture/interrupt value.

Bow implementation:

- Do not deal damage from animation impact.
- On `ProjectileRelease`, spawn projectile from bow/hand socket.
- Let projectile collision apply damage.
- Existing projectile flow already routes projectile damage to `RpgCombat::deal_damage_to_rpg_commander()` in `game/systems/projectile_system.cpp`.

Cavalry implementation:

- Use two contact sources:
  - rider weapon trace,
  - mount/charge body volume.
- Include relative speed in `CombatHitContact`.
- Apply interrupt/stagger based on charge speed and target bracing/weapon family.

### Layer 6: Damage Resolution

Owner:

- keep and refine `game/systems/rpg_combat_system/rpg_commander_damage.*`
- possibly add `game/systems/combat_system/combat_hit_resolver.*`

Desired direction:

- Hit detection calls a resolver with `CombatHitContact`.
- Resolver decides the outcome.

Target API shape:

```cpp
CombatHitResult CombatHitResolver::resolve_contact(
    Engine::Core::World& world,
    const CombatHitContact& contact,
    const CombatActionDefinition& action);
```

Routing rules:

- Commander target with active `RpgHealthComponent`:
  - use RPG HP,
  - guard/perfect guard,
  - posture,
  - punish windows,
  - stagger,
  - commander hit feedback.
- Soldier/unit target:
  - use `apply_unit_damage()`,
  - resolve multi-soldier health/count effects,
  - apply stagger/flinch as appropriate.
- Mounted/cavalry target:
  - initially unit damage plus stagger,
  - later rider/horse zones if the entity model supports it.
- Projectile target:
  - route from projectile collision into the same resolver when possible.

Keep from current implementation:

- combo multiplier,
- finisher multiplier,
- power-strike multiplier,
- punish multiplier,
- perfect guard and guard-break logic,
- `CombatHitEvent` publishing,
- `HitFeedbackComponent` integration.

Change over time:

- `deal_commander_attack_damage()` should eventually take an action/contact context instead of raw damage only.

### Layer 7: Presentation and Render Sampling

Owner:

- `render/gl/humanoid/animation/animation_inputs.cpp`
- `render/humanoid/*`
- `animation/*`

Desired direction:

- Render reads the selected action/clip/progress.
- Render does not infer gameplay timing.
- The same action definition id should select:
  - gameplay hit windows,
  - animation clip,
  - sword/spear/bow pose family,
  - VFX trails,
  - audio cues.

Compatibility adapter:

- Continue using `CombatStateComponent` fields:
  - `animation_state`,
  - `state_time`,
  - `state_duration`,
  - `attack_family`,
  - `attack_direction`,
  - `finisher_attack`.
- Action ids and definition-owned clip ids are authoritative in render sampling.
- `CombatStateComponent` values are populated only for older presentation and RTS
  consumers while those systems migrate.

## Phased Implementation Plan

### Phase 0: Stabilize vocabulary and tests

- Add combat action vocabulary:
  - `CombatActionId`
  - `WeaponFamily`
  - `CombatActionEventType`
  - `HitShapeProfile`
  - `DamageProfile`
- Add unit tests for the vocabulary/definition lookup.
- No gameplay behavior change.

Files likely touched:

- new `game/systems/combat_actions/combat_action_types.h`
- new `game/systems/combat_actions/combat_action_definition.*`
- `game/CMakeLists.txt`
- `tests/systems/*`

Acceptance criteria:

- Current commander attacks still behave the same.
- New action definitions can be looked up by id.
- Definitions exist for current five RPG sword attacks.

### Phase 1: Extract attack request service from controller

- Add `CombatActionService::request_attack()`.
- Move the attack-start code out of `CommanderControlController::primary_action()` into the service.
- The service should still populate existing `CombatStateComponent` and `RpgCommanderActionComponent`.
- Keep target hint selection in the controller at first, or pass current soft/lock target into the service.

Files likely touched:

- `app/core/commander_control_controller.cpp`
- new `game/systems/combat_actions/combat_action_service.*`
- `game/CMakeLists.txt`
- tests under `tests/core/commander_control_controller_test.cpp`

Acceptance criteria:

- Existing tests pass.
- Controller no longer manually selects phase durations or writes most attack fields.
- Existing RPG sword animations still play.

### Phase 2: Introduce action event progression

- Add event cursor/runtime helper for action state.
- During `process_combat_state()`, detect crossed action events.
- Initially only log/record crossed events or expose them to tests.
- Do not change damage timing yet.

Files likely touched:

- `game/systems/combat_system/combat_state_processor.cpp`
- new `game/systems/combat_actions/combat_action_events.*`
- tests under `tests/systems/combat_mode_test.cpp`

Acceptance criteria:

- For each RPG sword action, tests can prove expected events are crossed in order.
- No gameplay behavior change yet.

### Phase 3: Add sword weapon tracing behind a feature-compatible path

- Implement approximate sword trace using commander transform, action progress, reach, and sweep width.
- Track already-hit targets for one swing.
- Run trace during action active window.
- At first, compare trace-selected target with existing cone-selected target in diagnostics/tests.

Files likely touched:

- new `game/systems/combat_actions/weapon_trace.*`
- `game/systems/combat_system/combat_state_processor.cpp`
- `game/systems/combat_system/combat_utils.*`
- tests under `tests/systems/*`

Acceptance criteria:

- Sword trace detects enemies in front/side according to swing direction.
- Same target cannot be damaged multiple times in one swing.
- Trace can hit no target even when attack animation plays.

### Phase 4: Switch commander sword damage to trace contact

- Replace `Strike -> Impact` single-point damage with active-window trace contact damage.
- Keep the old function temporarily as fallback or test helper.
- Route contacts through `RpgCombat::deal_commander_attack_damage()` initially.

Files likely touched:

- `game/systems/combat_system/combat_state_processor.cpp`
- `game/systems/rpg_combat_system/rpg_commander_damage.*`
- `tests/core/commander_control_controller_test.cpp`
- `tests/systems/combat_mode_test.cpp`

Acceptance criteria:

- Damage only applies if trace contacts a valid target.
- Visual swing direction matters.
- Combo/finisher/power-strike behavior remains intact.
- Misses do not advance combo.
- Existing damage numbers and hit events still work.

### Phase 5: Add contact-aware damage context

- Introduce `CombatHitContact` and `CombatHitResult`.
- Add resolver that takes contact/action context.
- Migrate `deal_commander_attack_damage()` to use context internally.
- Preserve the old raw-damage API as a wrapper while projectile/RTS code migrates.

Files likely touched:

- new `game/systems/combat_system/combat_hit_types.h`
- new `game/systems/combat_system/combat_hit_resolver.*`
- `game/systems/rpg_combat_system/rpg_commander_damage.*`
- `game/systems/projectile_system.cpp`
- `game/systems/combat_system/attack_processor.cpp`

Acceptance criteria:

- Commander-vs-commander and commander-vs-unit route through the same resolver.
- Guard/perfect guard has access to contact direction.
- Relative speed and weapon family are available for future cavalry logic.

### Phase 6: Data-drive animation/action mapping (implemented)

- Use `CombatActionId` and definition-owned clip ids as the only selectors.
- Remove the style compatibility field.
- Render animation sampling maps action id to clip.
- Action definitions own clip selection and gameplay event windows.

Files likely touched:

- `game/core/component.h`
- `render/gl/humanoid/animation/animation_inputs.cpp`
- `animation/clip_manifest.*`
- `animation/action_manifest.*`
- serialization tests if component state is serialized.

Acceptance criteria:

- Adding a new RPG sword animation means adding one definition and one clip mapping, not changing controller logic.
- Tests prove action id selects expected RPG clip.

### Phase 7: Spears

- Add spear action definitions.
- Add spear thrust/sweep hit shape.
- Add anti-charge resolver hooks:
  - higher posture damage to cavalry,
  - interrupt if contact occurs in frontal brace/thrust window.
- Derive a shared brace runtime from player/AI hold and guard intent, and gate
  anti-charge readiness on pose entry progress.

Files likely touched:

- `game/systems/combat_actions/combat_action_definition.*`
- `game/systems/combat_actions/weapon_trace.*`
- `game/systems/combat_system/spear_brace_processor.*`
- `render/gl/humanoid/animation/animation_inputs.cpp`
- `animation/clip_manifest.*`

Acceptance criteria:

- Spear attack damage applies only during spear contact.
- Spear reach is longer and narrower than sword.
- Spear can interrupt a fast charging mounted entity when conditions match.

### Phase 8: Bows and projectile release events

- Add bow action definitions with `ProjectileRelease`.
- Move commander bow shot timing to action event release.
- Spawn projectile from action event.
- Projectile collision routes through contact-aware resolver.

Files likely touched:

- `game/systems/projectile_system.*`
- `game/systems/combat_status_effect_system.*`
- `game/systems/combat_system/combat_status_effect_processor.*`
- `game/systems/combat_actions/combat_action_events.*`
- `game/systems/combat_actions/combat_action_service.*`
- `animation/clip_manifest.*`

Acceptance criteria:

- Arrow is created when visual bow release occurs.
- Damage happens on projectile collision.
- Commander and non-commander RPG targets use same projectile damage path.
- Ongoing projectile-created statuses and fire patches update outside projectile
  flight logic.

### Phase 9: Mounted/cavalry combat

- Add mounted attack definitions.
- Give each mounted action an authored rider clip and carry its normalized
  action phase through primary and layered render playback.
- Bake a dedicated mounted spear thrust clip with trace sockets.
- Add charge state/contact shape.
- Add charge runtime/cooldown state so body-impact actions cannot restart
  continuously while cavalry remains in contact.
- Use relative speed in damage/stagger resolution.
- Add cavalry interruption rules for timed sword/spear hits.

Files likely touched:

- `game/core/component.h` if a mounted/charge runtime component is needed.
- `game/systems/combat_actions/weapon_trace.*`
- `game/systems/combat_system/combat_hit_resolver.*`
- mounted render/animation files under `render/entity/*mounted*`, `render/horse/*`, and `render/humanoid/*`.

Acceptance criteria:

- Mounted sword/spear attacks use action definitions.
- Charge impact can damage/stagger valid targets.
- Commander sword/spear contact can interrupt or stop cavalry under defined conditions.

### Phase 10: Commander-vs-commander polish

- Ensure both commanders use RPG HP/posture/guard/stagger consistently.
- Add lock-on/facing/contact rules specific to commander duels.
- Add tests for:
  - perfect guard against commander sword,
  - guard break,
  - finisher punish,
  - dodge invulnerability,
  - same-swing one-hit-per-target.

Files likely touched:

- `game/systems/rpg_combat_system/rpg_commander_damage.*`
- `game/systems/combat_system/combat_hit_resolver.*`
- `app/core/commander_control_controller.cpp`
- `tests/systems/combat_mode_test.cpp`

Acceptance criteria:

- Commander-vs-commander is not a special one-off path.
- It uses the same action, hit contact, and resolver path as commander-vs-unit.

## Cleanup Targets

### `CommanderControlController`

Desired cleanup:

- Keep:
  - input state,
  - camera control,
  - movement,
  - lock-on target selection,
  - ability request forwarding.
- Move out:
  - attack phase setup,
  - attack duration choices,
  - action clip/style selection,
  - stamina cost calculation for attacks,
  - direct writes to combat action fields.

### `CombatStateComponent`

Desired cleanup:

- Short term: keep as compatibility state for animation sampling.
- Medium term: add action id and hit runtime state.
- Long term: split into:
  - `CombatActionComponent`,
  - `CombatAnimationPlaybackComponent` if needed,
  - `CombatHitRuntimeComponent` if the hit-list/runtime grows too large.

### `RpgCommanderActionComponent`

Desired cleanup:

- Current: owns stable action id, duration/elapsed/normalized time, event cursor,
  active/cancel windows, input-buffer flag, target hint, and per-action hit list.
- Long term: rename/generalize the component after all non-commander authored
  actions use it; no gameplay field is now specific to sword animation style.

### `combat_state_processor.cpp`

Desired cleanup:

- Completed for authored actions: action event advancement/contact dispatch lives
  in `combat_action_processor.cpp`.
- Remaining phase-triggered commander damage is explicitly the no-action-id
  compatibility fallback and can be removed with legacy commander saves/callers.

### `rpg_commander_damage.cpp`

Desired cleanup:

- Keep RPG damage policy.
- Accept contact/action context.
- Avoid doing target acquisition or hit detection.

### Render animation sampling

Desired cleanup:

- Read action id/clip id.
- Map action id to clip and pose families.
- Keep guard/jump/hit-reaction sampling separate from gameplay logic.

## Test Strategy

Add tests in small layers:

- Definition lookup tests:
  - every current RPG sword action exists,
  - clip mapping is correct,
  - event order is valid.
- Event progression tests:
  - action time crossing emits expected events,
  - buffered input works during recover/reposition.
- Weapon trace tests:
  - target inside swing arc is hit,
  - target outside swing arc is missed,
  - same target is hit once per swing,
  - multiple targets respect `max_targets`.
- Damage resolver tests:
  - guard reduces damage,
  - perfect guard staggers attacker,
  - guard break opens punish window,
  - dodge invulnerability ignores contact,
  - commander target uses RPG HP,
  - unit target uses Unit HP.
- Integration tests:
  - current sword attacks still advance combo,
  - misses do not advance combo,
  - finisher still knocks/staggers,
  - projectile release spawns arrow once.

Existing relevant test areas:

- `tests/core/commander_control_controller_test.cpp`
- `tests/systems/combat_mode_test.cpp`
- `tests/systems/rpg_engagement_system_test.cpp`
- `tests/render/creature/humanoid_prepare_test.cpp`
- `tests/render/bpat/bpat_registry_test.cpp`

## Implementation Rules

- Do not rewrite all combat at once.
- Keep current RTS combat stable.
- Do not move rendering decisions into gameplay systems.
- Do not make projectile, spear, and cavalry bespoke one-off paths.
- New features should be expressible as action definitions plus hit shapes plus resolver policy.
- Every migration phase should preserve current player sword behavior unless that phase explicitly changes hit timing.
- Favor small adapters/wrappers first, then remove old paths once tests cover the replacement.

## First Practical Milestone

The first code milestone should be:

1. Add action ids and action definitions for existing RPG sword attacks.
2. Add `CombatActionService::request_attack()`.
3. Move `CommanderControlController::primary_action()` attack setup into that service.
4. Keep current phase-transition damage unchanged.
5. Add tests proving current behavior is preserved.

This gives the codebase a professional extension point before changing damage timing.
