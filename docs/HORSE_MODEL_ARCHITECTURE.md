# Authored Horse and Elephant Pipeline

Horses and elephants use authored, skinned low-poly geometry compiled into deterministic creature packages. The production pipeline preserves mesh topology, skinning weights, joint hierarchy, authored animation channels, attachment landmarks, and reviewed production proportions from asset generation through BPAT baking and rendering.

The important architectural rule is that production geometry comes from the compiled creature package. Runtime rendering, shape verification, locomotion inspection, and attachment placement all consume that same asset rather than maintaining independent hand-entered body dimensions.

## Pipeline overview

The current path is:

```text
authored creature source
        │
        ▼
creature compiler
        │
        ▼
.cmesh production package
        │
        ├─ vertices / indices
        ├─ skin joints + weights
        ├─ joint hierarchy
        ├─ materials
        └─ authored animation channels
        │
        ├────────► BPAT bake / runtime animation data
        │
        ▼
compiled creature asset loader
        │
        ▼
renderer preparation + equipment/attachment resolution
        │
        ▼
production rendering / Arena / shape verification
```

The compiler output is deterministic, which means topology/count/extent tests can treat the generated package as a reproducible production contract.

# Production horse

The production horse package is:

```text
assets/creatures/horse/horse.cmesh
```

It contains:

- 4,400 vertices;
- 2,182 triangles;
- 50 skin joints; and
- 13 authored actions.

Walk and gallop use those authored actions directly.

## Horse production transform

The reviewed production transform scales:

- X: `0.59`;
- Y: `0.59`; and
- Z: `0.5015`.

These values are part of the production shape contract used by runtime geometry and verification tooling.

A scale change is therefore not merely a cosmetic transform tweak. It changes the body extents consumed by rendering, grounding, rider placement, screenshots, and shape-verification expectations.

## Horse locomotion

The horse uses authored skeletal locomotion rather than procedural leg placement as its production path.

The runtime selects the appropriate authored clip through the creature manifest/BPAT path, samples the baked skeleton state, and deforms the skinned mesh through the same joint/weight data stored in the compiled package.

The focused tests and Arena locomotion matrix exercise walking, galloping, stopping/starting, reverse movement, terrain grounding, and rider attachment behavior through the production renderer.

# Production elephant

The production elephant package is:

```text
assets/creatures/elephant/elephant.cmesh
```

It contains:

- 1,464 vertices;
- 760 triangles;
- 32 skin joints; and
- five authored actions.

Its movement uses authored walk and fast-walk cycles. Elephants do not use the horse gallop model.

## Elephant production transform

The elephant uses axis-specific production scaling:

- X: `0.31625`;
- Y: `0.275`; and
- Z: `0.1925`.

These values are likewise part of the reviewed production-shape contract.

The non-uniform transform is intentional and is verified against the generated package dimensions rather than reconstructed from separate “expected elephant size” constants in each renderer.

# Compiled creature package

`render/creature/compiled_creature_assets.cpp` loads the generated package used at runtime.

The loader validates and decompresses the package, then resolves data such as:

- vertex/index buffers;
- accessors;
- materials;
- per-vertex skin joint indices;
- per-vertex skin weights;
- joint hierarchy; and
- authored animation channels.

This is the renderer-facing production source for the horse/elephant body geometry.

## Deterministic asset ownership

The repository does not keep a second manually maintained runtime horse mesh or elephant mesh alongside `.cmesh` and then choose between them.

That matters because shape tests, attachment locations, renderer deformation, and promo/Arena review all need to inspect the same geometry the shipped game draws.

If a production body changes, regenerate the compiled asset and update the tests/shape contract that intentionally depends on it.

# Skinning contract

`MeshSkinning::Authored` preserves four joint indices and four weights per vertex through the creature pipeline.

The authored skinning data survives:

1. mesh compilation;
2. creature/BPAT baking where the animation path needs it;
3. runtime snapshots/preparation; and
4. GPU skinning.

The common bone-palette limit is 64 bones.

The horse's 50 joints and the elephant's 32 joints fit inside that current runtime limit.

## Weight normalization

Focused model tests check normalized skin weights and finite deformation.

That makes the skinning data itself part of the model contract. A mesh can have the correct static topology and still be invalid for runtime use if the skin weights are malformed.

# Animation and BPAT integration

Species manifests map gameplay/presentation actions to authored animation clips.

The BPAT system carries pre-baked bone palettes, markers, sockets, contact data, bind pose, and hierarchy information used by the runtime animation path. The horse/elephant renderer then consumes the current clip/phase rather than rebuilding skeletal animation from source channels every frame.

See [CREATURE_BPAT_FORMAT.md](CREATURE_BPAT_FORMAT.md) for the binary animation contract.

## Root motion vs mesh pose

Creature animation deforms the visible body and attachment frames, but gameplay movement still owns the logical entity/root motion.

The authored gait must therefore visually match the movement speed/cadence well enough to avoid skating while remaining presentation data rather than a second navigation simulation.

The locomotion matrix and focused tests exist to inspect that integration.

# Attachment frames

Mounted presentation depends on animated attachment frames rather than fixed world-space offsets.

Current production attachment behavior includes:

- horse saddle/rider frames following the animated back bone; and
- elephant howdah frames following the corresponding animated back bone.

This keeps the rider/howdah attached to the posed creature as the back rises, falls, pitches, and rolls through locomotion.

## Rest landmarks

Rest landmarks are measured from production geometry.

They are not reconstructed from independently maintained body-height/length constants. This reduces drift between mesh revisions and attachment/grounding assumptions.

# Mounted horse equipment resolution

Mounted horse renderer families share a subset of horse-equipment configuration through `MountedHorseHandles` in:

```text
render/entity/mounted_horse_equipment.h
```

The mounted renderer bases include:

- `MountedKnightRendererBase`;
- `HorseSpearmanRendererBase`; and
- `HorseArcherRendererBase`.

`resolve_mounted_horse_handles()` fills the shared handles from the compatible renderer configuration.

## Archetype handle ordering

`MountedHorseHandles::as_array()` returns the ordered handle array consumed by:

```text
resolve_horse_equipment_archetype()
```

The resolver hashes that ordered array as part of archetype identity.

The ordering is therefore a compatibility contract. New shared horse-equipment slots should be appended rather than inserted into the middle of the existing array, because insertion would reinterpret the positional meaning of existing handles and alter hashes for existing archetypes.

## Shared vs rider-specific equipment

The horse-side shared handles are intentionally separate from rider-specific equipment.

The rider's:

- weapon;
- shield;
- helmet;
- armour; and
- shoulder equipment

remain renderer-family-specific.

Mounted classes expose different concepts—for example `has_sword` vs `has_spear`, or `has_cavalry_shield` vs `has_shield`—and each renderer supplies its own humanoid equipment handle set to the rider archetype resolver.

That separation avoids forcing every mounted troop into one oversized equipment schema with many meaningless fields.

# Horse and rider composition

A mounted troop is visually composed from at least two authored systems:

1. the horse creature body/animation/equipment archetype; and
2. the humanoid rider body/animation/equipment archetype.

The saddle/rider frame links the two presentations.

The horse equipment resolver should therefore own horse-side presentation identity, while humanoid equipment resolution owns rider-side identity.

Gameplay troop identity remains outside both renderer archetype caches.

# Elephant and howdah composition

The elephant follows the same broad principle: the creature body is authored/skinned and the howdah or mounted presentation follows an animated attachment frame.

The elephant does not share the horse's locomotion assumptions or production transforms. Its package, action catalogue, joint hierarchy, and reviewed extents are verified independently.

# Production-shape verification

Shape verification inspects generated production geometry directly.

The verification path checks:

- package integrity;
- exact topology;
- finite bounds;
- grounded bounds;
- expected axis extents; and
- non-empty full, torso, legs, and head regions.

It also renders review views from:

- left;
- rear;
- front-quarter; and
- top.

## Verification commands

Horse:

```sh
build/bin/mesh_preview verify-horse-shape artifacts/horse-shape
```

Elephant:

```sh
build/bin/mesh_preview verify-elephant-shape artifacts/elephant-shape
```

Axis extents must remain within `0.00005` game units of the reviewed production contract unless the production contract and corresponding tests are intentionally changed together.

## Why exact topology is checked

Topology checks make the production asset deterministic at more than the visual-review level.

A changed vertex/triangle count can indicate:

- an unintended compiler/source change;
- an accidentally different export;
- missing/duplicated geometry; or
- a deliberate model revision that needs its verification contract updated.

The test cannot decide whether the change is artistically good; it can ensure the change is explicit.

# Region verification

The verifier checks that important regions such as torso, legs, and head are non-empty.

This prevents a malformed package from passing only because its global bounding box remains plausible.

A creature body with a missing head or collapsed leg region can have finite overall bounds and still be unusable.

# Locomotion inspection

Use `mesh_preview` to inspect authored actions directly.

Horse:

```sh
build/bin/mesh_preview horse artifacts/horse full walk
build/bin/mesh_preview horse artifacts/horse full gallop
```

Elephant:

```sh
build/bin/mesh_preview elephant artifacts/elephant full walk
build/bin/mesh_preview elephant artifacts/elephant full run
```

These commands are useful for reviewing deformation, gait silhouette, grounding, and attachment motion without running a full battle.

# Focused model tests

Run:

```sh
build/bin/horse_model_tests
build/bin/elephant_model_tests
```

The focused tests cover areas including:

- production counts/topology;
- normalized skin weights;
- locomotion samples;
- finite deformation;
- triangle-tear limits; and
- rider/howdah socket invariants.

A shape preview and the focused tests serve different purposes: one is visual review of the production body; the other locks structural/deformation properties.

# Arena integration

Use Arena to exercise the complete runtime path.

Horse/mounted matrix:

```sh
build/bin/arena_app --batch --scenario mounted_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
```

Elephant matrix:

```sh
build/bin/arena_app --batch --scenario elephant_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
```

Arena adds integration that a static mesh preview cannot provide:

- BPAT playback;
- runtime preparation;
- renderer integration;
- terrain grounding;
- movement start/stop transitions;
- reverse motion; and
- rider/howdah attachment in the production scene.

# Diagnosing mounted-model problems

The pipeline is easier to debug when the failing layer is identified explicitly.

## Static shape is wrong

Inspect the generated `.cmesh`, production transform, bounds, topology, and compiler/source model.

## Static shape is correct but deformation tears

Inspect joint indices, normalized weights, hierarchy, and authored animation channels/BPAT output.

## Creature animates but rider/howdah drifts

Inspect the animated attachment frame, anchor bone, rest landmark, and rider/howdah local transform.

## Rider equipment is wrong but horse is correct

Inspect the rider-specific humanoid equipment handles rather than changing `MountedHorseHandles`.

## Horse equipment archetypes alias unexpectedly

Inspect `MountedHorseHandles::as_array()` ordering and the hashed handle values.

## Preview looks correct but Arena/runtime differs

Inspect manifest clip mapping, BPAT runtime selection, renderer preparation, terrain grounding, and production render configuration.

# Architectural invariants

The current horse/elephant pipeline depends on these invariants:

- compiled `.cmesh` packages are the production geometry source;
- skinning preserves four joints/four weights per vertex;
- joint count remains within the common 64-bone palette limit;
- animation comes from authored/baked skeletal actions;
- rider/howdah attachments follow animated bones;
- production transforms/extents are verified directly from generated geometry;
- shared horse-equipment handle ordering remains stable; and
- preview/tests/Arena inspect the same production assets used by runtime.

# Source map

| Concern | Source |
| --- | --- |
| Compiled creature loading | `render/creature/compiled_creature_assets.cpp` |
| Horse production package | `assets/creatures/horse/horse.cmesh` |
| Elephant production package | `assets/creatures/elephant/elephant.cmesh` |
| Shared horse-equipment handles | `render/entity/mounted_horse_equipment.h` |
| BPAT format/runtime | `animation/bpat/` |
| Mesh/shape preview | `mesh_preview` tool sources |
| Focused tests | horse/elephant model test targets |
| Runtime integration | Arena locomotion matrix scenarios |

The production packages, runtime loader, authored animation data, attachment frames, and verification tools define the current mounted-creature model contract. Historical refactor stories are not required to explain that pipeline.
