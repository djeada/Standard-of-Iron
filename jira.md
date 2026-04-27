# JIRA Plan: Shared Creature Framework + Realistic Low-Poly Horse

## Summary

Build a believable low-poly horse directly in this repository, with full code-level control over shape, rig behavior, gait timing, grounding, and rider synchronization.

Do this in **two implementation steps**:

1. Build a **shared large-creature framework** that works for both horse and elephant.
2. Deliver the **real low-poly horse** on top of that framework, while moving elephant onto the same interfaces.

This plan explicitly avoids any external import pipeline. Meshes, rig structure, LOD behavior, and motion rules are all authored in code inside this project.

## Why this plan

The current horse and elephant paths are already parallel in structure, but still too species-specific in implementation. If we improve the horse in isolation, elephant will need a second architecture rewrite later.

The right move is:

1. unify the interfaces first
2. build the better horse on those interfaces

## Constraints

1. No DCC import path
2. No external authoring dependency for the final result
3. Keep `CreatureRenderRequest` high-level
4. Keep the plan to two major steps
5. Make the interfaces reusable for elephants from day one

## Current architecture notes

### Horse

- `render/horse/horse_spec.*` still owns much of the visible horse body construction and reduced pose logic.
- `render/horse/horse_motion.*` computes gait and motion procedurally.
- `render/horse/prepare.*` bridges motion, pose, grounding, and render submission.
- The horse skeleton is still too small for believable equine motion.

### Elephant

- `render/elephant/elephant_spec.*`, `render/elephant/dimensions.h`, and `render/elephant/attachment_frames.h` already mirror much of the horse-side structure.
- Elephant already has richer leg state concepts in its motion/attachment layer, which is a useful sign that a shared framework is feasible.

### Render boundary

- `render/creature/render_request.h` already defines the correct high-level game-to-render contract.
- `render/creature/pipeline/unit_visual_spec.h` is the right place to attach a shared creature visual/runtime definition.

## Target outcome

At the end of this work:

1. horse uses a real internally-built low-poly mesh path
2. horse movement is gait-driven and believable
3. horse supports hoof planting, terrain-aware correction, and rider sync
4. horse and elephant share the same framework interfaces
5. elephant no longer depends on a separate architecture shape

---

## STEP 1 — Build the shared large-creature framework

## Goal

Create one shared framework for internally-built large creatures so horse and elephant use the same runtime contracts for:

- mesh generation
- rig definition
- motion inputs and outputs
- grounding/contact logic
- attachment frames
- LOD behavior

## Epic 1.1: Shared creature visual definition

### Ticket 1.1.1 — Add `CreatureVisualDefinition`

Create a render-side definition object that is referenced from `UnitVisualSpec`.

It should own or resolve:

- species kind
- mesh recipe
- rig definition
- motion controller
- grounding model
- attachment frame extractor
- role-color/material policy
- species-specific LOD strategy

### Acceptance criteria

- horse and elephant can both resolve through the same top-level definition shape
- `UnitVisualSpec` can point to this shared definition without widening `CreatureRenderRequest`

## Epic 1.2: Shared mesh generation contracts

### Ticket 1.2.1 — Add `CreatureMeshRecipe`

Define one interface for species-specific low-poly mesh construction in code.

Responsibilities:

- build full/reduced/minimal LOD body meshes
- preserve species silhouette across LODs
- expose stable naming/regions if later needed for shading or attachments

### Ticket 1.2.2 — Move species mesh building behind the recipe interface

Horse and elephant keep separate implementations, but the runtime sees one contract.

### Acceptance criteria

- the renderer no longer assumes horse/elephant mesh construction through unrelated special paths
- all LOD mesh generation for large creatures can be called through one interface family

## Epic 1.3: Shared rig definition contracts

### Ticket 1.3.1 — Add `CreatureRigDefinition`

Define the shared rig contract for large creatures.

It should describe:

- bone/joint identifiers
- bind pose generation
- runtime deformation channels
- contact points
- optional rider/howdah sync anchors

### Ticket 1.3.2 — Add shared bind-palette access pattern

Horse and elephant should expose bind data through the same runtime shape, even if their bone layouts differ.

### Acceptance criteria

- both species can provide rig and bind-pose data through one common interface
- future large creatures can reuse the same pattern

## Epic 1.4: Shared motion contracts

### Ticket 1.4.1 — Add `CreatureMotionInputs`

Provide one high-level runtime input model for large creatures.

Suggested fields:

- locomotion speed
- normalized speed
- turn amount
- stop intent
- combat/overlay intent
- terrain response inputs
- current facing/velocity relationship

### Ticket 1.4.2 — Add `CreatureMotionOutput`

Provide one shared output model for large creatures.

Suggested fields:

- phase
- bob
- locomotion mode
- body sway
- body pitch
- spine bend
- contact state per leg
- attachment sync channels

### Ticket 1.4.3 — Add `CreatureMotionState`

Provide a persistent runtime state object for phase continuity, gait transitions, and contact memory.

### Acceptance criteria

- horse and elephant can publish motion through the same contract family
- no species-specific motion data needs to be pushed through `CreatureRenderRequest`

## Epic 1.5: Shared grounding and contact framework

### Ticket 1.5.1 — Add `CreatureGroundingModel`

Define one grounding/contact interface for large creatures.

It should support:

- contact windows per leg
- planted-foot targets
- stride warping inputs
- root/chest/pelvis correction
- terrain-alignment hooks

### Ticket 1.5.2 — Add shared terrain correction entry point

Unify the handoff between preparation, terrain probing, and creature contact solving.

### Acceptance criteria

- horse and elephant can use the same grounding framework
- species-specific gait timing remains pluggable

## Epic 1.6: Shared attachment frame contracts

### Ticket 1.6.1 — Add `CreatureAttachmentFrameSet`

Unify mount/howdah-facing attachment output.

It should support:

- seat anchor
- frame basis: forward/right/up
- tack/howdah anchors
- ground offset

### Ticket 1.6.2 — Adapt horse mount frames and elephant howdah frames

Keep species-specific extraction logic, but publish one common result shape.

### Acceptance criteria

- horse rider sync and elephant howdah sync can consume one shared attachment-frame contract

## Epic 1.7: Framework integration

### Ticket 1.7.1 — Extend `UnitVisualSpec`

Let `UnitVisualSpec` resolve a shared creature definition for large creatures.

### Ticket 1.7.2 — Add compatibility adapters

Current horse and elephant behavior should continue to function through temporary adapters until Step 2 replaces horse with the new full path.

### Acceptance criteria

- current behavior still runs
- new framework exists and is wired
- no gameplay-side contract expansion is required

## Step 1 definition of done

Step 1 is complete when:

1. horse and elephant both resolve through one shared framework shape
2. mesh, rig, motion, grounding, and attachment contracts are shared
3. `CreatureRenderRequest` remains high-level
4. current species code can run through compatibility layers

---

## STEP 2 — Deliver the real low-poly horse on the shared framework

## Goal

Build the realistic low-poly horse fully in code on top of the new framework, while also moving elephant onto the same interfaces so architectural coverage is complete.

## Epic 2.1: Replace the current horse body with a real low-poly mesh recipe

### Ticket 2.1.1 — Design the horse silhouette language

Define the intended horse read:

- chest vs barrel separation
- pelvis/croup shape
- neck mass and taper
- head wedge and muzzle block
- leg segment proportions
- mane/tail simplification style

### Ticket 2.1.2 — Implement the horse full-LOD mesh recipe

Build a stable, internally-authored low-poly horse mesh in code.

Requirements:

- readable low-poly silhouette similar to the target reference
- stable topology generation
- no dependence on external model import

### Ticket 2.1.3 — Implement reduced and minimal LOD recipes

The reduced and minimal LODs must preserve the same horse identity rather than becoming unrelated placeholders.

### Acceptance criteria

- horse silhouette reads as intentional low-poly art, not a primitive placeholder
- all three LODs come from one coherent mesh recipe family

## Epic 2.2: Upgrade the horse rig

### Ticket 2.2.1 — Define the horse runtime rig

Expand beyond the current tiny skeleton.

Minimum internal control structure should support:

- root
- pelvis/hind body
- chest/front body
- spine or body-bend channel
- neck chain
- head control
- tail chain
- front and hind leg articulation
- rider sync anchors

### Ticket 2.2.2 — Implement bind pose and runtime palette generation

Generate the new horse bind palette and runtime pose evaluation through the shared rig interface.

### Acceptance criteria

- horse can express convincing front/hind body separation
- rig is rich enough for gait timing and grounding correction

## Epic 2.3: Implement explicit horse gait logic

### Ticket 2.3.1 — Add horse locomotion states

At minimum:

- idle
- walk
- trot
- canter
- gallop
- start/stop transition logic

### Ticket 2.3.2 — Add lead and turning behavior

Support:

- turn bias
- lateral lead behavior
- better phase handling through transitions

### Ticket 2.3.3 — Remove the "generic run" simplification

Fast horse motion must not collapse into one state that hides gait differences.

### Acceptance criteria

- walk, trot, canter, and gallop are visually distinct
- transitions remain phase-consistent and readable

## Epic 2.4: Add hoof planting and terrain-aware correction

### Ticket 2.4.1 — Add stance/swing timing per hoof

Track each hoof through stance and swing windows.

### Ticket 2.4.2 — Add planted-foot targets

Prevent obvious skating by holding contact targets during stance.

### Ticket 2.4.3 — Add body compensation

Adjust root, chest, and pelvis based on terrain and active contacts.

### Ticket 2.4.4 — Add stride warping

Adjust step placement on uneven surfaces and during speed changes.

### Acceptance criteria

- visible hoof skating is greatly reduced
- body grounding reads as weight-bearing rather than floating

## Epic 2.5: Add rider synchronization

### Ticket 2.5.1 — Publish rider sync channels from horse motion

Required outputs:

- seat bounce
- frame basis
- pitch/roll transfer
- rein response
- stop/start anticipation

### Ticket 2.5.2 — Consume sync channels in mounted rendering

Mounted pose overlays should stay coherent with horse motion.

### Acceptance criteria

- rider no longer feels detached from horse gait
- mounted combat/aim overlays remain believable

## Epic 2.6: Move elephant onto the same framework

### Ticket 2.6.1 — Adapt elephant mesh generation to `CreatureMeshRecipe`

### Ticket 2.6.2 — Adapt elephant rig/bind path to `CreatureRigDefinition`

### Ticket 2.6.3 — Adapt elephant motion output to shared motion contracts

### Ticket 2.6.4 — Adapt elephant grounding/howdah output to shared contracts

### Acceptance criteria

- elephant uses the same framework interfaces as horse
- elephant does not require a separate architecture pass later

## Epic 2.7: Validation and debugging

### Ticket 2.7.1 — Add framework-level tests

Cover:

- shared definition resolution
- rig contract validity
- grounding/contact contract shape
- attachment frame publication

### Ticket 2.7.2 — Add horse behavior tests

Cover:

- gait selection
- phase continuity
- hoof contact state
- terrain grounding invariants
- mounted sync output

### Ticket 2.7.3 — Add debug views

Useful overlays:

- gait state
- phase
- active contacts
- planted targets
- rider sync anchors
- terrain correction offsets

### Acceptance criteria

- the new path is debuggable and regression-safe

## Step 2 definition of done

Step 2 is complete when:

1. horse uses the shared framework end-to-end
2. horse is represented by a real internally-built low-poly mesh path
3. horse motion is explicitly gait-driven and believable
4. hoof grounding and terrain correction are active
5. rider synchronization is stable
6. elephant is already aligned to the same framework interfaces

---

## Out of scope

1. Any import-based mesh/animation pipeline
2. Expanding `CreatureRenderRequest` with low-level pose or skeleton payloads
3. Solving horse only and deferring elephant architecture to a future rewrite

## Primary risks

1. Improving horse visuals without shared framework changes
2. Keeping horse locomotion compressed into coarse idle/walk/run logic
3. Building a better silhouette without fixing rig richness and contact behavior
4. Letting elephant stay on a different architecture while horse moves forward

## Recommended implementation order inside the two steps

1. Step 1.1 through 1.7
2. Step 2.1 mesh recipe
3. Step 2.2 rig
4. Step 2.3 gait controller
5. Step 2.4 grounding/contact
6. Step 2.5 rider sync
7. Step 2.6 elephant alignment
8. Step 2.7 tests/debugging
