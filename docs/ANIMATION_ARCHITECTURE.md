# Animation & Pose Architecture

How a creature in Standard of Iron goes from _"this unit is attacking"_ to _moving
geometry on screen_ — and why it is fast enough to do for thousands of units at once.

This document is the conceptual map. For the binary file format see
[`CREATURE_BPAT_FORMAT.md`](CREATURE_BPAT_FORMAT.md); for the wider render thread see
[`RENDERING_ARCHITECTURE.md`](RENDERING_ARCHITECTURE.md).

---

## 1. The big idea

Animation is split into two worlds that meet at a thin, cheap seam:

- **Offline (bake time):** the expensive skeletal work — posing every joint of every
  frame of every move — is done once by a tool and packed into a **BPAT** file per
  species (a "motion book").
- **Online (per frame):** the game only answers _which move_ and _how far through it_,
  then either samples the baked book or runs a light procedural shaper. The GPU does the
  final skinning.

```
                       OFFLINE (once, at build)                ONLINE (every frame)
            ┌─────────────────────────────────────┐   ┌────────────────────────────────────┐
            │  SpeciesManifest ──► bpat_baker ──►  │   │  game state ─► intent ─► clip+phase  │
            │  *.bpat  (+ *.bpsm snapshot mesh)    │   │           │                          │
            └─────────────────────────────────────┘   │           ▼                          │
                              │                        │   sample baked pose / shape pose     │
                              └────────  loaded  ──────┼──►        │                          │
                                                       │           ▼                          │
                                                       │   bone palette ─► GPU skinning ─►███ │
                                                       └────────────────────────────────────┘
```

The seam is deliberately tiny: at runtime, _intent_ is essentially `(clip_id, phase)`.
That is what keeps large battles cheap.

---

## 2. The runtime pipeline, layer by layer

Every creature passes through the same five stages each frame. Boxes are the data that
flows; the labels under them are the files that own each step.

```
 ECS components                AnimationInputs              PoseIntent / AnimationStateId
 (AttackComponent,        ┌─────────────────────┐          ┌──────────────────────────┐
  CombatStateComponent,   │  1. INPUT BRIDGE     │          │  2. INTENT / SELECTION    │
  HoldModeComponent,  ───►│  sample_anim_state() ├─────────►│  resolve_pose()           │
  Transform, Stamina,     │  render/gl/humanoid/ │          │  render/creature/         │
  Formation, ...)         │  animation/          │          │   pose_intent.{h,cpp}     │
                          │  animation_inputs.cpp│          │  + combat_visual_state    │
                          └─────────────────────┘          └────────────┬─────────────┘
                                                                         │ clip_id, phase,
                                                                         │ clip_variant
                                                                         ▼
   ███ GPU                 bone palette[]            ResolvedClipPlayback / HumanoidPose
 ┌───────────────┐      ┌────────────────────┐      ┌───────────────────────────────────┐
 │ 5. SKINNING   │◄─────│ 4. POSE PRODUCTION  │◄─────│  3. CLIP RESOLUTION                │
 │ palette texture│     │  • sample baked     │      │  ArchetypeRegistry.bpat_clip[state]│
 │ + vertex      │      │    BPAT frames      │      │  resolve_bpat_clip(variant)        │
 │ shader        │      │  • OR procedural    │      │  resolve_bpat_playback(clip,phase) │
 │ render/gl/... │      │    poser/pose_ctrl  │      │  render/creature/pipeline/         │
 └───────────────┘      └────────────────────┘      └───────────────────────────────────┘
```

### Stage 1 — Input bridge (`render/gl/humanoid/animation/animation_inputs.cpp`)

`sample_anim_state()` reads the ECS and produces one `AnimationInputs` snapshot per
entity per frame (is it attacking? dying? guarding? kneeling? how fast is it moving?).
Per-entity _persistent_ animation memory (filtered speed/turn, locomotion phase
accumulator, guard/hold progress, combat visual state) lives in
`HumanoidAnimationStateComponent` (`render/creature/animation_state_components.h`).

### Stage 2 — Intent / selection (`render/creature/pose_intent.*`, `combat_visual_state.*`)

`resolve_pose()` collapses all the booleans into a single `PoseIntent` in strict priority
order (dying > dead > hit-react > attacking > … > walk > idle). This replaced the old
scattered if/else chains: **one** resolution per entity per frame. Combat additionally
runs through a small transactional state machine (below).

### Stage 3 — Clip resolution (`render/creature/pipeline/`)

The intent becomes an `AnimationStateId`, which indexes a **precomputed** per-archetype
table to get a `clip_id` — this is O(1), no string lookups on the hot path:

```
PoseIntent ─► AnimationStateId ─► ArchetypeRegistry::bpat_clip[state]  (array index)
                                       │
                                       ▼  + clip_variant (seed / equipment)
                              resolve_bpat_clip()  ─►  uint16 clip_id
```

`BpatRegistry::find_clip(name)` (the by-_name_ utility) is backed by an O(1) per-blob hash
map (`BpatBlob::clip_index`), but it is only used off the hot path; runtime selection uses
the precomputed index above.

### Stage 4 — Pose production (two routes)

- **Baked route (default for shipping creatures):** `resolve_bpat_playback(clip, phase)`
  turns a normalized phase into a frame index + interpolation weight; the bone palette is
  read straight from the BPAT blob. No skeleton is solved at runtime.
- **Procedural route (humanoid locomotion / combat shaping):** `poser.cpp`
  (`compute_locomotion_pose`) and `pose_controller.cpp` shape a `HumanoidPose`
  analytically. This is where the _felt_ realism lives — see §4.

### Stage 5 — Skinning (GPU)

The bone palette (one matrix per bone) is uploaded as a texture and the vertex shader
transforms each vertex by its bone matrices. Thousands of units skin in parallel on the
GPU. See `RENDERING_ARCHITECTURE.md`.

---

## 3. The bake pipeline (offline)

```
 render/<species>/<species>_manifest.cpp
        │   SpeciesManifest { topology, mesh graph, clip descriptors, bake_clip_palette }
        ▼
 tools/bpat_baker  ──► for each clip, for each frame: solve skeleton ► pack bone palette
        │
        ├──►  assets/creatures/<species>.bpat          (animation palettes + markers)
        └──►  assets/creatures/<species>_minimal.bpsm  (snapshot mesh for far LOD; horse/elephant)
```

`*.bpat` files are **generated artifacts** (git-ignored). Regenerate with:

```bash
make bake-bpat          # or: ./build/bin/bpat_baker assets/creatures
```

Six species ship today: `humanoid`, `horse`, `elephant`, `humanoid_sword`,
`humanoid_spear`, `humanoid_skeleton` (ids 0–5).

---

## 4. Humanoid locomotion realism (the procedural shaper)

Walk/run/turn quality is produced by `poser.cpp::compute_locomotion_pose`, driven by a
**velocity-blended, phase-continuous** gait state built in `prepare_animation.cpp`.

```
 ground speed ─► build_locomotion_targets()         (target speed/blend/run/turn/cadence)
                       │
                       ▼   smooth_towards(prev, target, dt, tau)   ← exponential smoothing
        ┌──────────────┴───────────────────────────────────────────┐
        │ filtered_speed   locomotion_blend   run_blend   turn   cadence  │
        └──────────────┬───────────────────────────────────────────┘
                       ▼
       compute_locomotion_pose():  walk_profile ──blend(run_blend)──► run_profile
                       │
        ┌──────────────┼───────────────────────────────────────────┐
        │ stride scaled by speed (anti-slide)   foot plant/toe-off    │
        │ turn lean + torso twist + stride bias  arm swing + counter  │
        │ vertical bob + head stabilization      pelvis weight shift  │
        └───────────────────────────────────────────────────────────┘
```

Key properties that keep it smooth (Phase 6):

- **Eased/spring blends (6.2):** every locomotion channel is exponentially smoothed
  (`smooth_towards`, per-channel `tau`). Combat phases use authored ease curves; guard and
  hold/kneel transitions both ease with the same smoothstep so there is no pop at the ends.
- **Velocity-driven blend (6.3):** stride length, step height, cadence and arm swing scale
  with filtered ground speed; walk↔run is a continuous blend, not a hard switch.
- **Phase continuity:** the locomotion phase is _integrated_ (`phase += dt / cycle_time`),
  never reset on a state change, so feet never teleport between cycles.
- **Anti-slide (6.1):** stride length is coupled to real ground speed via
  `stride_distance_scale()`, so planted feet track displacement. (A full world-space IK
  foot-lock is a documented future refinement; it needs in-engine visual tuning.)
- **Turn (6.4):** a signed turn amount drives lean, torso twist, stride bias and
  inside/outside foot asymmetry.

### Walk and run use different body mechanics

The two profiles are not just amplitude variants. Walking keeps each foot planted for
60% of its cycle, creating double support; it uses a wider track, an upright torso,
relaxed arms with a slight elbow bend and a pronounced heel-to-toe roll. Running cuts
ground contact to 32%, narrows the track and leaves two flight windows per cycle. It also
lands near-flat, recovers the swing leg earlier and leans farther over the stride. Free
arms use a compact bent-elbow pump; weapon-ready profiles lower and draw their carry
inward so running does not reuse the walking guard silhouette.

Their centre-of-mass curves are authored separately before blending. A walker rises over
the supporting leg and settles during double support. A runner compresses over the loaded
leg and rises during flight. Blending the two waves, instead of merely reversing one
amplitude, preserves vertical motion through the middle of a walk-to-run transition.

### The swing has to join the stance at the stance's own speed

A planted foot slides backwards relative to the hips at `-stride / planted_fraction` per
unit phase. `swing_travel()` is therefore a Hermite whose **end tangents carry that
slope**, not an ease that starts and finishes at rest: the foot leaves the ground still
travelling backwards, reaches its furthest forward point shortly before touchdown, and
retracts into the plant. Take the tangents away and the foot path has a corner at toe-off
and another at heel strike — one visible hitch per stride, which is what the old
`0.68·smoothstep(t) + 0.32·√t` produced (√ has an _infinite_ slope at `t = 0`).

The retraction near touchdown is also the cheapest foot-lock the stylised gait gets: the
foot is moving with the ground at the moment it lands rather than against it.

### Stand, walk and run are three clips, so the switch is a crossfade

Selection resolves one baked clip, so a unit that starts or stops walking would cut
between `idle` and `walk` on the frame the movement flag flips — mid-stride, both feet in
the wrong place. `resolve_locomotion_crossfade()` (`animation/selection_manifest.cpp`)
instead names a primary clip and a second one to blend against, and
`humanoid_animation_selection.cpp` hangs the second on the request's `full_body_blend`
layer. The played state stays whatever the movement says, so nothing downstream sees a
walking unit reported as idle.

The weight is **`locomotion_presence` / `run_presence`, not `locomotion_blend`**. The
blends carry how fast the unit is going, so driving the clip mix from them would leave
every unit slower than the reference walk permanently diluted with the stand — a builder
at half reference speed would look like it never commits to a step. Presence only answers
"is there a stride at all", eased over the same `tau`, so it settles at one for a crawl
and a sprint alike.

The stride also keeps _cycling_ while it fades: `resolve_humanoid_locomotion_sample()`
integrates the phase at the last walking cadence for as long as presence is above
`k_locomotion_residual_blend`, instead of handing it straight back to the idle free-run.
A walk that froze mid-step and then vanished was the single most visible hitch in
first-person play.

### Gait amplitude and the skate budget

`walk_profile()` / `run_profile()` in `animation/locomotion_manifest.cpp` hold the gait
amplitudes. Note that `stride_length` is the distance the foot sweeps **relative to the
body** over one stance, while the body itself covers `speed * cycle_time` per cycle. At
the reference walk speed those numbers are far apart, so planted feet always slide
somewhat — this is a stylised RTS gait, not a foot-locked one, and
`stride_distance_scale()` only keeps the ratio stable across speeds rather than closing it.

The practical consequence: raising `stride_length` _reduces_ skate, it does not cause it.
The values shipped before the hip-drop solve (0.40 walk / 0.58 run) were small enough that
the legs read as a stiff shuffle from the game camera; the authored stance travel is now
0.95 m for the walk and 0.96 m for the run. The run baker samples the profile at normalized
speed so its canonical clip keeps that skate budget instead of silently inflating it.

The ceiling used to be leg reach: at a half-stride approaching `UPPER_LEG_LEN +
LOWER_LEG_LEN` the foot could no longer reach the ground from a fixed pelvis, and
`solve_knee_ik` absorbed the deficit by clamping — which silently stretched the shin.
`resolve_humanoid_locomotion_pose` now closes that loop itself: given the pelvis height,
hip offsets and leg length, it computes the drop each foot needs and sinks the pelvis (and
the whole upper body with it) by the larger of the two. Long strides therefore cost hip
height, exactly as they do on a real walker, instead of costing bone length. The arena's
`NoLimbOverextension` expectation still guards the arms.

### Foot roll

`HumanoidPose` carries `foot_pitch_l` / `foot_pitch_r`, and the `FootL` / `FootR` bones are
built from that pitch instead of always standing square to the world. The gait drives it:
the foot lands toes-up at heel strike, rolls flat through mid-stance, pushes off the toe at
the end of stance, and picks the toe back up for swing clearance. `heel_strike_pitch`,
`toe_off_pitch` and `swing_clearance_pitch` on the profile are the knobs; a runner lands
much flatter and pushes off harder than a walker.

Two things have to move with it or the foot leaves the ground:

- `ankle_lift_for_pitch()` raises the ankle by however much the rotation would otherwise
  drive the heel or the toe under the floor, so the contact point stays at ground level.
- `palette_contact_y()` (`render/creature/pipeline/preparation_common.cpp`) grounds the
  humanoid on the **sole**, not the ankle bone. It transforms a heel point and a toe point
  through the posed and bind foot bones and takes the lower of the two. Grounding on the
  ankle would have cancelled the ankle lift and dragged the model down at every toe-off.
  `Animation::humanoid_foot_contact_lift()` is the same relationship exposed for callers
  that only have a pitch.

Review changes with the `humanoid_gait_review` scenario, or far faster with
`build/bin/humanoid_preview --clip walk --view side --report`, which renders the baked
clip as a phase strip and prints per-frame bone stretch.

---

## 4b. Showcase moves (authored keyframes, not procedural shaping)

Six humanoid clips are **authored as keyframes** rather than shaped from gait
parameters: `showcase_jump`, `showcase_front_flip`, `showcase_handstand`,
`showcase_side_aerial`, `showcase_sword_flourish`, `showcase_spear_throw`. They
exist because an acrobatic move is not a small perturbation of a stance — a
handstand inverts the whole body, so the additive delta vocabulary the ambient
idles use (`ambient_pose_manifest.cpp`) cannot express it.

`animation/showcase_pose_manifest.{h,cpp}` holds the keys; the forward kinematics
that turn them into a complete `HumanoidPose` live in `animation/rig/pose_fk.h`,
shared with the death collapses (§4c):

```
 ShowcaseKey  { root, body_pitch/roll/yaw, spine_*, head_pitch, blade_*, 4 limb aims }
        │  interpolated between keys with smoothstep
        ▼
 FK: spine rotation places neck/shoulders/head; each limb is
     Ry(yaw) · Rz(±splay) · Rx(-pitch) applied to "down", with the joint bend
     folded into the second segment's pitch, so segment lengths are exact by
     construction
        │
        ▼
 whole-body rotation about the pelvis, then translation to the authored root
```

A limb aim is `(pitch, splay, yaw, bend)` in degrees: pitch swings the limb
forward, splay outward, yaw around the body axis, and bend is joint flexion —
knees fold backward, elbows forward. Because both the segment and its bend are
rotations about the same local X axis they simply add, which is why the FK needs
no IK solve and cannot produce a stretched limb.

Two things about these clips do not follow the usual rules:

- **Ground contact is authored, not derived.** Every other clip is re-grounded
  at draw time by `contact_y_for_playback`, which pushes the model down until
  the lowest foot bone sits on the terrain. In a handstand the feet are two
  metres in the air, so that rule would bury the character. `showcase_` clips
  are excluded from grounding alongside `riding_` clips, and their keys are
  therefore authored in absolute model space with the standing feet at the rig's
  foot offset.
- **Root motion is carried by the entity, not the pose.** A front flip travels
  1.55 m and a side aerial 2.62 m. The keys stay in place; the authored travel
  comes from `humanoid_showcase_root_travel(move, phase)` and
  `ShowcaseRoutineSystem` integrates it into the transform, rotated into the
  performer's facing and scaled by the entity's render scale. Pose plus entity
  motion equals the world motion, with no snap when the clip ends.

`tools/arena/promos/humanoid_showcase.json` and the `promo_humanoid_showcase`
scenario are what these were authored for; `ShowcaseRoutineComponent` is the
scripted playlist that drives them (move id, duration, hold, loop) and it is the
only production consumer.

### Driving one at runtime

```
 ShowcaseRoutineComponent  ──ShowcaseRoutineSystem──►  active / move / phase
        │                                                      │
        │                                    world.cpp presentation sync
        ▼                                                      ▼
 transform root motion                       CreaturePresentationComponent
                                                               │
                                             animation_inputs.cpp bridges to
                                             AnimationInputs::showcase_clip
                                                               │
                                     humanoid_animation_selection.cpp forces
                                     the clip and phase after every other rule
```

The presentation `revision` counter includes the showcase fields. It has to:
the render signature in `world.cpp` is built from that revision, and a routine
that changed only its phase would otherwise be treated as an unchanged entity
and drawn from the cached preparation.

---

## 4c. Dying (`animation/death_pose_manifest.{h,cpp}`)

A death is authored the same way a showcase move is, and for the same reason. It
used to be a list of per-joint offsets faded in with a smoothstep, and that has
two failure modes that a fall exposes immediately:

- **Segments came apart.** The offsets moved the shoulders and the hands but not
  the elbows, and the head further than the neck. By the end of the clip the
  forearms were **3.27× their bind length** (`humanoid_preview --report`) and the
  head had visibly left the neck behind.
- **Nobody reached the ground.** The pelvis only sank 0.42 m, so the body settled
  into a floating half-crouch with its head hanging in the air — a shape the
  corpse then held for as long as it was on the field.

The falls are now `DeathKey` sequences resolved through the same forward
kinematics as the showcase moves, shared in `animation/rig/pose_fk.h`: bone
lengths are exact by construction, and the settled pose is authored where a body
actually lies.

```
 DeathKey { root, body_pitch/roll/yaw, spine_*, head_*, foot_pitch, 4 limb aims }
        │  keys eased per segment: falling segments ease *in* only (t²), so the
        │  body accelerates into the ground instead of arriving softly
        ▼
 PoseFk  spine → neck/shoulders/head, limb aims → elbows/hands/knees/feet
        ▼
 whole-body rotation about the pelvis, then translation to the authored root
```

### Which way a man goes down

Four collapses are authored. Three are reachable by an infantry casualty and are
baked as **clip variants**, so `resolve_bpat_clip` picks one by adding the
`death_variant` to the base clip index — hence `die_infantry`,
`die_infantry_face`, `die_infantry_side` are contiguous in `clip_manifest.h`, and
so are the three `dead_infantry*` clips that hold the corpses.

| collapse        | what it is                                                | chosen when                  |
| --------------- | --------------------------------------------------------- | ---------------------------- |
| `BackSprawl`    | trunk arches, knees fold, hips land, shoulders whip down  | struck from the front        |
| `FacePlant`     | folds over its own knees, lands face-down                 | cut down from behind         |
| `SideCrumple`   | legs go out sideways, comes to rest twisted onto one side | taken on the flank           |
| `MountedUnseat` | carried clear of the saddle before gravity gets him       | rider profile, not a variant |

`infantry_death_variant()` (`damage_application.cpp`) takes the dot product of
the blow direction against the casualty's facing. It is not a die roll: a man
shot in the chest must not land on his face. Slot zero of a volley always takes
the fall the blow argues for; the men behind him may be substituted onto the side
crumple, which is where identical bodies would otherwise show.

The trunk roll on the side falls deliberately stops short of 90°. The rig carries
its shoulders as two points half a metre apart on a rigid spine, so a body laid
exactly on its side puts the lower shoulder underground — and from the game
camera a three-quarter roll reads as "dropped" anyway.

Each collapse also owns its own length
(`humanoid_death_collapse_duration`), and that one number drives both the baked
frame count and the runtime `DeathAnimationComponent::state_duration`. They must
agree or the body would still be moving when the clip runs out.

`tests/render/creature/death_collapse_test.cpp` guards both original defects:
every segment holds its bind length across every sampled phase of every fall, and
no joint is driven through the ground. Review a change with
`humanoid_preview --clip die_infantry --view iso --report`.

---

## 5. Combat: visual state machine + marker-driven damage

Melee combat is animated by a small per-swing state machine and, crucially, the **HP hit
lands when the blade visually connects** — not on the trigger frame.

```
 Advance ─► WindUp ─► Strike ─► Impact ─► Recover ─► Reposition ─► Idle
    │          │         │
    │          │         └── BPAT marker: contact  ───────────────┐
    │          └────────────  marker: weapon_release              │ phase where the
    └───────────────────────  marker: anticipation_start          │ weapon connects
                                                                   ▼
                              ┌──────────────────────────────────────────────┐
                              │ Deferred-melee-strike (DPS-neutral)            │
                              │ At swing start: snapshot damage+target onto    │
                              │ AttackComponent (pending_melee_*), reset       │
                              │ cooldown. When elapsed ≥ contact_time, apply    │
                              │ the snapshotted hit (revalidated: alive/enemy/  │
                              │ in range), else cancel.                        │
                              │ contact_time = k_melee_contact_fraction*cooldown│
                              │ game/systems/combat_system/attack_processor.cpp │
                              └──────────────────────────────────────────────┘
```

- **Authored markers (BPAT v2):** each clip carries 5 normalized-phase markers —
  `anticipation_start`, `weapon_release`, `contact`, `recover_unlocked`, `exit_safe` —
  baked by the tool and read directly (no runtime name-substring guessing).
- **DPS-neutral:** cooldown is still reset at swing start, so steady-state damage output is
  unchanged; only the _moment_ of application moves to mid-swing.
- **Deterministic:** the pending-strike fields are serialized, so saves/replays reproduce
  the same hit timing.
- Ranged, special projectiles and the first-person/RPG-commander hook are unchanged.

The visual side (`combat_visual_state.cpp`) eases each phase (`eased_combat_phase_progress`)
and applies a lane-driven weight curve (`emphasis_scale` × finisher/amplified multipliers).

### The stance bleeds through the whole swing

`combat_attack_visual_weight()` never reaches 1: a swing peaks at 0.95 of the attack clip
with the stance showing through the rest. That weight has to be _applied_ everywhere it is
below one, not only during the wind-up and the exit. Blending the stance during
Enter/Anticipation and nowhere else put a step in the mix exactly where anticipation hands
over to the strike — the stance went from three tenths of the pose to none of it on one
frame, right as the blade started to move.

The authored RPG timeline had a second hole in the same curve: `exit_blend_progress` was
pinned at zero, so `ExitBlend` evaluated to a _constant_ 0.80 for the whole recovery and
then cut to the stance when the action ended. It is now read off the authored phase, so
the weight actually walks down to zero and `use_base_selection` takes over cleanly before
the move finishes.

### A swing is one arc, not five lunges

`sample_authored_sword_pose_key()` interpolates the authored keys with Hermite/Catmull-Rom
tangents (central differences, zero at the first and last key). Easing each _segment_
separately with its own smoothstep — the old behaviour — drove the blade's velocity to
zero at every key, so a cut read as a series of short lunges with a stop between each.

Blade direction is **slerped**, not lerp-and-normalise. The keys either side of a cut are
up to 135° apart, and the chord path crawls near both ends and whips through the middle;
slerp gives the cut a constant angular rate, which is what makes the arc readable.

Every RPG sword move also starts and finishes on one shared `rpg_sword_guard_key()`. It
has to be literally the same key, not five near-copies: the moves chain into one another
and fall back to the stance when they end, and an 8 cm hand offset between the last frame
of one and the first frame of the next is a snap at exactly the moment the player is
watching the blade.

Finally, the swing trail in `sword_renderer.cpp` is no longer gated off for the authored
blade. Without it an RPG cut is a thin prism crossing the screen in five frames;
`sword_trail_window()` takes the window from the move, because each RPG attack puts its
cut in a different slice of its own timeline.

### Arms are solved, not stretched

Bone matrices are rigid — `make_bone_basis()` builds a rotation and a translation and no
scale — so a pose that puts a hand further from the shoulder than the arm is long does not
produce a long arm. It produces a forearm that ends in mid-air and a hand (and the weapon
welded to it) floating away from the body. The authored RPG sword keys used to do this: at
the strike frame the right hand sat 1.29 m from a 0.59 m arm, and the clips rendered with a
detached hand and a sword planted in the ground.

Two rules keep that from happening again:

- `PosePrimitives::solve_arm_ik()` is a real two-bone solve, so `|shoulder→elbow|` and
  `|elbow→hand|` are the bind lengths by construction. The old `elbow_bend_torso()`
  heuristic placed the elbow a fraction along the shoulder-hand line and let the segments
  come out however they came out — up to 35% over bind on the spear's offhand grip.
- `place_hand_at()` clamps the request to the arm's reach before solving, so no authored
  key can ask for an impossible pose. The reach fractions live together in
  `pose_primitives.h` — relaxed (0.985), braced two-handed grip (0.96), committed melee
  swing (0.94), seated rider (0.75) — because they are four deliberate policies, not four
  copies of one number.

Body deltas are applied **before** the hands are placed. Solving an arm against the shoulder
it had last frame is how the bow ended up clamped short of full draw: the draw pose pushes
the shoulder 0.20 m forward, and the hand target was only out of reach relative to where the
shoulder had not moved to yet.

### The blade has to travel

A weapon is a static attachment welded to the `HandR` bone (`sword_make_static_attachment`),
which means its direction in the world is entirely the hand bone's Y axis. The hand bone
takes that axis from `pose.grip_axis_r`, and nothing set it for sword clips — so through
every RTS sword swing the blade pointed at the sky and only the arm moved.

`resolve_sword_pose()` now authors a blade direction alongside each hand key (guard →
chambered behind the shoulder → apex → through the cut → follow-through → guard) and
`aim_held_weapon()` converts it into the grip axis. `k_sword_blade_axis_in_grip` in
`sword_renderer.h` is the single definition of where the blade sits in the grip frame; the
static attachment, the runtime renderer and the bake-time aim all read it. The three
infantry sword variants are a right-to-left cut, its mirror, and an overhead chop, and they
now look like three different attacks.

The infantry spear thrust had the same shape of problem in a different place:
`resolve_infantry_spear_thrust_pose()` ignored `inputs.variant` entirely, so
`attack_spear_a/b/c` baked byte-identical clips. It now offsets hand height, crouch, shaft
pitch and reach per variant — a level thrust, a low one, and one over the shield rim.

---

### The body moves with the blow: root motion and reactions

Everything above picks a clip and a phase; the model matrix is built from the
transform alone. That left a melee fight looking like two men tapping each
other with spoons — the arms moved and nothing else did. Two pieces now move the
root, both resolved by `Animation::resolve_combat_root_motion`
(`animation/combat_root_motion_manifest.cpp`) and applied in
`render/humanoid/runtime/instance_prepare.cpp` after the model has been grounded:

- **The lunge.** A melee swing carries the body: `melee_lunge_offset` loads the
  weight back a few centimetres through the wind-up, drives forward 0.24 m
  (sword) or 0.30 m (spear) into contact and eases back through the recovery,
  with a forward lean of up to 8°. The curve is a function of the visual
  `attack_phase`, so every soldier's lunge lands on its own cut; formation ranks
  use a shorter step (they already carry the lane depth pulses), an evaded swing
  overextends, and a heavy one drives deeper. First-person commanders are
  excluded — the chase camera hangs off the simulation transform and the
  controller already steps them in.
- **The reaction.** `HitReactionKind` (flinch, block, evade, stagger, recoil)
  drives a recoil along the blow, a pitch about the feet, a roll and a squash,
  each with its own out-and-back envelope. A single body the simulation has
  already knocked back gets a smaller visual recoil so the two do not add up.
  An attacker's `Recoil` — the bounce off a blocked blow — is layered over the
  swing rather than interrupting it, and so is any light reaction that lands
  while the blade is already in its strike.

Three clips back this up, baked per profile so a swordsman raises his shield
where a spearman turns his shaft:

- `combat_ready` — the fighting stance. It is authored on the **first frame of
  the swing** (`HumanoidPoseController::combat_ready_stance` samples the attack
  pose at a small phase), with knees bent (`crouch`), torso forward, the shield
  half up and a slow breathing bob. Locked single bodies use it as their base
  between swings instead of the parade-rest idle, and formation ranks blend
  their swings over it. The selection happens in `apply_combat_ready_clip`.
- `react_flinch`, `react_block`, `react_evade`, `react_stagger` — non-looping
  reactions driven by the reaction's own progress (`apply_melee_reaction_clip`),
  so a 0.34 s block and a 0.60 s stagger each play end to end. Their curves live
  in `animation/reaction_pose_manifest.cpp`; the pose controller turns them into
  crouch, torso tilt, flinch, shield raise and hand offsets.

`tests/render/creature/combat_root_motion_test.cpp` pins the lunge shape, the
reaction envelopes and that no reaction tilts the torso far enough to read as a
fall; `humanoid_preview --clip combat_ready --weapon sword` shows the stance.

## 6. Quadrupeds: one shared gait evaluator

Horse and elephant share a single parametric gait core instead of each carrying its own
copy of the phase/bob/leg math (Phase 7).

```
        render/creature/quadruped/gait.{h,cpp}
        ┌───────────────────────────────────────────────┐
        │  Quadruped::evaluate_cycle_motion(dims, gait,   │
        │     MotionConfig, SwayConfig, time, motion)     │
        │   • phase = wrap(time/cycle_time + offset)      │
        │   • bob   = Σ harmonics × amplitude × scale     │
        │   • per-leg swing_target / default_foot_position│
        │   • body_sway, swing_ease, swing_arc            │
        └───────────────┬─────────────────┬──────────────┘
                        │                  │
         HorseGait : Gait                ElephantGait : Gait
         (MotionConfig knobs)            (MotionConfig knobs)
         render/horse/horse_motion.cpp   render/elephant/elephant_motion.cpp
         + rider phase selection         + trunk/ears/howdah extras
```

The shared evaluator gained **behaviour-exact config knobs** (bob harmonic
weights/frequencies, bob base/intensity scale, cycle-time floor, optional unclamped
swing ease/arc, optional non-mirrored swing target) so each species reproduces its prior
output numerically — the consolidation deleted duplicate math without changing the gait
feel. Mount/howdah attachment frames remain per-species (their anchor geometry differs).

---

## 7. Performance notes

- **No per-frame skeleton solves** on the baked route — just a frame lookup + lerp.
- **No per-frame heap allocations** in submission: bone palettes use a pooled, thread-local
  allocator.
- **O(1) clip resolution** on the hot path (precomputed `bpat_clip[state]` index); the
  by-name registry lookup is an O(1) hash map for off-hot-path use.
- **Eager palette decode:** BPAT palettes are decoded to matrices once at load
  (`bpat_reader.cpp`) to make per-frame reads branch-free; this is intentional (lazy decode
  would add mutable state + thread-safety risk for marginal memory savings).
- **One intent resolution per entity per frame** — no redundant re-resolution downstream.

---

## 8. Where to look

| Concern                 | File(s)                                                                               |
| ----------------------- | ------------------------------------------------------------------------------------- |
| ECS → animation inputs  | `render/gl/humanoid/animation/animation_inputs.cpp`                                   |
| Intent resolution       | `render/creature/pose_intent.{h,cpp}`                                                 |
| Combat visual state     | `render/creature/combat_visual_state.{h,cpp}`                                         |
| Clip selection          | `render/creature/archetype_registry.cpp`, `pipeline/humanoid_animation_selection.cpp` |
| BPAT playback           | `animation/bpat/bpat_playback.cpp`                                                    |
| BPAT blob/registry      | `animation/bpat/bpat_reader.cpp`, `bpat_registry.cpp`                                 |
| Humanoid locomotion     | `render/humanoid/runtime/poser.cpp`, `runtime/animation_runtime.cpp`                  |
| Humanoid combat poses   | `render/humanoid/runtime/pose_controller.cpp`                                         |
| Quadruped shared gait   | `render/creature/quadruped/gait.{h,cpp}`                                              |
| Horse / elephant motion | `render/horse/horse_motion.cpp`, `render/elephant/elephant_motion.cpp`                |
| Melee damage sync       | `game/systems/combat_system/attack_processor.cpp`                                     |
| Bake tool               | `tools/bpat_baker/`, `render/<species>/<species>_manifest.cpp`                        |
