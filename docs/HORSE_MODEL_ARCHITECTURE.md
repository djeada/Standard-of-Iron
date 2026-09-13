# Authored Horse and Elephant Pipeline

Horses and elephants use authored, skinned low-poly geometry. The engine does not reconstruct either animal from primitive proportions at runtime. Instead, deterministic compiled creature packages carry the production mesh, skinning data, joint hierarchy, and animation clips all the way through the rendering pipeline.

This page describes those production assets, the runtime path that consumes them, how mounted equipment is resolved, and the tests that protect the reviewed shapes and motion.

## Production assets

### Horse

The production horse package is `assets/creatures/horse/horse.cmesh`. It contains:

- 4,400 vertices;
- 2,182 triangles;
- 50 skin joints; and
- 13 authored actions.

Walk and gallop use those authored actions directly.

The production transform scales X and Y by `0.59` and Z by `0.5015`. This makes the current longitudinal extent 15% shorter while preserving the reviewed width and height relationship.

### Elephant

The production elephant package is `assets/creatures/elephant/elephant.cmesh`. It contains:

- 1,464 vertices;
- 760 triangles;
- 32 skin joints; and
- five authored actions.

Its walk and fast-walk cycles use the same armature and skin weights. Elephants intentionally do not have a horse-style gallop.

The elephant transform is axis-specific:

- X: `0.31625`;
- Y: `0.275`; and
- Z: `0.1925`.

Relative to the earlier model, the resulting production shape is 50% of the previous height, 35% of the previous length, and 57.5% of the previous width.

### Deterministic compiled assets

Both `.cmesh` files are deterministic outputs of the repository's creature compiler. The repository does not keep an imported mesh, editable source scene, or comparison OBJ alongside them. The compiled package itself is the production artifact that runtime code and verification tools consume.

## Runtime architecture

`render/creature/compiled_creature_assets.cpp` validates and decompresses a generated package, then loads its buffers, accessors, materials, skin weights, joint hierarchy, and animation channels.

`MeshSkinning::Authored` preserves four joint indices and four weights per vertex through mesh compilation, BPAT baking, snapshots, and the GPU pipeline. The shared palette limit is 64 bones.

Species manifests choose the authored mesh and map gameplay clips to authored actions. Mounted attachment frames also follow the animated skeleton rather than static approximations: the horse saddle and rider frames follow the animated back bone, and the elephant howdah frame follows its corresponding animated back bone.

Rest landmarks are measured from production geometry instead of being reconstructed from independently guessed dimensions.

## Mounted horse equipment has one resolution path

Every mounted renderer base—`MountedKnightRendererBase`, `HorseSpearmanRendererBase`, and `HorseArcherRendererBase`—uses the same seven horse equipment slots:

- saddle;
- bridle;
- reins;
- barding;
- crupper; and
- decoration slots represented by the corresponding `HorseTack`, `HorseArmor`, and `HorseDecoration` handles.

These renderers previously stored seven independent `EquipmentHandle` members and repeated the same resolution logic in each constructor. That shared behavior now lives in `render/entity/mounted_horse_equipment.h`.

`MountedHorseHandles` stores the seven handles, `resolve_mounted_horse_handles()` fills them from any configuration that provides the required fields, and `as_array()` returns the ordered array expected by `resolve_horse_equipment_archetype()`.

The ordering is part of the archetype identity: `as_array()` is hashed by the resolver, so changing the order changes the archetype a mount resolves to. **New horse-equipment slots must therefore be appended rather than inserted.**

The rider's own weapon, shield, helmet, armour, and shoulder handles remain renderer-specific. Mounted classes expose those fields differently—for example, `has_sword` versus `has_spear`, or `has_cavalry_shield` versus `has_shield`—and each class supplies its own five-handle array to the humanoid archetype resolver.

## Verifying production shape

The shape-verification tooling inspects generated production geometry directly. It checks:

- package integrity;
- exact topology;
- finite and grounded bounds;
- expected axis extents; and
- non-empty full, torso, legs, and head regions.

It also renders all four regions from left, rear, front-quarter, and top views for visual inspection.

Run the checks with:

```sh
build/bin/mesh_preview verify-horse-shape artifacts/horse-shape
build/bin/mesh_preview verify-elephant-shape artifacts/elephant-shape
```

Axis extents must remain within `0.00005` game units of the reviewed production contract unless that contract is intentionally changed together with its tests.

## Motion and integration gates

Use `mesh_preview` to inspect the authored locomotion clips:

```sh
build/bin/mesh_preview horse artifacts/horse full walk
build/bin/mesh_preview horse artifacts/horse full gallop
build/bin/mesh_preview elephant artifacts/elephant full walk
build/bin/mesh_preview elephant artifacts/elephant full run
```

Run the focused model tests:

```sh
build/bin/horse_model_tests
build/bin/elephant_model_tests
```

Finally, exercise the real runtime path through Arena:

```sh
build/bin/arena_app --batch --scenario mounted_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
build/bin/arena_app --batch --scenario elephant_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
```

The focused tests validate production counts, topology, normalized weights, 64 samples per locomotion cycle, finite deformation, triangle-tear limits, and animated rider/howdah socket invariants. Arena then exercises the actual BPAT, preparation, renderer, terrain-grounding, start/stop, and reverse-motion paths.

Together, these gates protect both sides of the contract: the authored creature must retain its reviewed production shape, and the runtime must continue to animate and attach equipment to that shape correctly.
