# Animation & Pose Architecture

How a creature in Standard of Iron goes from *"this unit is attacking"* to *moving
geometry on screen* — and why it is fast enough to do for thousands of units at once.

This document is the conceptual map. For the binary file format see
[`CREATURE_BPAT_FORMAT.md`](CREATURE_BPAT_FORMAT.md); for the wider render thread see
[`RENDERING_ARCHITECTURE.md`](RENDERING_ARCHITECTURE.md).

---

## 1. The big idea

Animation is split into two worlds that meet at a thin, cheap seam:

- **Offline (bake time):** the expensive skeletal work — posing every joint of every
  frame of every move — is done once by a tool and packed into a **BPAT** file per
  species (a "motion book").
- **Online (per frame):** the game only answers *which move* and *how far through it*,
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

The seam is deliberately tiny: at runtime, *intent* is essentially `(clip_id, phase)`.
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
Per-entity *persistent* animation memory (filtered speed/turn, locomotion phase
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

`BpatRegistry::find_clip(name)` (the by-*name* utility) is backed by an O(1) per-blob hash
map (`BpatBlob::clip_index`), but it is only used off the hot path; runtime selection uses
the precomputed index above.

### Stage 4 — Pose production (two routes)
- **Baked route (default for shipping creatures):** `resolve_bpat_playback(clip, phase)`
  turns a normalized phase into a frame index + interpolation weight; the bone palette is
  read straight from the BPAT blob. No skeleton is solved at runtime.
- **Procedural route (humanoid locomotion / combat shaping):** `poser.cpp`
  (`compute_locomotion_pose`) and `pose_controller.cpp` shape a `HumanoidPose`
  analytically. This is where the *felt* realism lives — see §4.

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
- **Phase continuity:** the locomotion phase is *integrated* (`phase += dt / cycle_time`),
  never reset on a state change, so feet never teleport between cycles.
- **Anti-slide (6.1):** stride length is coupled to real ground speed via
  `stride_distance_scale()`, so planted feet track displacement. (A full world-space IK
  foot-lock is a documented future refinement; it needs in-engine visual tuning.)
- **Turn (6.4):** a signed turn amount drives lean, torso twist, stride bias and
  inside/outside foot asymmetry.

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
  unchanged; only the *moment* of application moves to mid-swing.
- **Deterministic:** the pending-strike fields are serialized, so saves/replays reproduce
  the same hit timing.
- Ranged, special projectiles and the first-person/RPG-commander hook are unchanged.

The visual side (`combat_visual_state.cpp`) eases each phase (`eased_combat_phase_progress`)
and applies a lane-driven weight curve (`emphasis_scale` × finisher/amplified multipliers).

---

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

| Concern | File(s) |
|---|---|
| ECS → animation inputs | `render/gl/humanoid/animation/animation_inputs.cpp` |
| Intent resolution | `render/creature/pose_intent.{h,cpp}` |
| Combat visual state | `render/creature/combat_visual_state.{h,cpp}` |
| Clip selection | `render/creature/archetype_registry.cpp`, `pipeline/humanoid_animation_selection.cpp` |
| BPAT playback | `render/creature/pipeline/bpat_playback.cpp` |
| BPAT blob/registry | `render/creature/bpat/bpat_reader.cpp`, `bpat_registry.cpp` |
| Humanoid locomotion | `render/humanoid/poser.cpp`, `prepare_animation.cpp` |
| Humanoid combat poses | `render/humanoid/pose_controller.cpp` |
| Quadruped shared gait | `render/creature/quadruped/gait.{h,cpp}` |
| Horse / elephant motion | `render/horse/horse_motion.cpp`, `render/elephant/elephant_motion.cpp` |
| Melee damage sync | `game/systems/combat_system/attack_processor.cpp` |
| Bake tool | `tools/bpat_baker/`, `render/<species>/<species>_manifest.cpp` |
