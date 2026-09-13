# Authored Horse and Elephant Pipeline

Horses and elephants use authored, skinned low-poly geometry. Deterministic compiled creature packages carry the production mesh, skinning data, joint hierarchy, and animation clips through the rendering pipeline.

## Production assets

### Horse

The production horse package is `assets/creatures/horse/horse.cmesh`. It contains:

- 4,400 vertices;
- 2,182 triangles;
- 50 skin joints; and
- 13 authored actions.

Walk and gallop use those authored actions directly.

The production transform scales X and Y by `0.59` and Z by `0.5015`. Those values define the reviewed production proportions used by runtime geometry and shape verification.

### Elephant

The production elephant package is `assets/creatures/elephant/elephant.cmesh`. It contains:

- 1,464 vertices;
- 760 triangles;
- 32 skin joints; and
- five authored actions.

Its walk and fast-walk cycles use the authored armature and skin weights. Elephants do not use a horse-style gallop.

The elephant transform is axis-specific:

- X: `0.31625`;
- Y: `0.275`; and
- Z: `0.1925`.

These scales are part of the production-shape contract.

### Deterministic compiled assets

Both `.cmesh` files are deterministic outputs of the repository's creature compiler. Runtime code and verification tools consume the compiled package directly; the repository does not keep a separate imported mesh or editable scene as a second production source of truth.

## Runtime architecture

`render/creature/compiled_creature_assets.cpp` validates and decompresses the generated package, then loads buffers, accessors, materials, skin weights, joint hierarchy, and animation channels.

`MeshSkinning::Authored` preserves four joint indices and four weights per vertex through mesh compilation, BPAT baking, snapshots, and the GPU pipeline. The common palette limit is 64 bones.

Species manifests select the authored mesh and map gameplay clips to authored actions. Mounted attachment frames follow animated bones: the horse saddle and rider frames follow the animated back bone, and the elephant howdah frame follows its corresponding animated back bone.

Rest landmarks are measured from production geometry rather than reconstructed from independent dimensions.

## Mounted horse equipment resolution

`MountedKnightRendererBase`, `HorseSpearmanRendererBase`, and `HorseArcherRendererBase` use `MountedHorseHandles` from `render/entity/mounted_horse_equipment.h` for the shared horse-equipment fields.

`resolve_mounted_horse_handles()` fills those handles from a compatible renderer configuration. `as_array()` returns the ordered handle array consumed by `resolve_horse_equipment_archetype()`.

The array ordering is part of archetype identity because the resolver hashes it. New shared horse-equipment slots must therefore be appended rather than inserted into the existing order.

The rider's weapon, shield, helmet, armour, and shoulder handles remain renderer-specific. Mounted classes expose those fields differently—for example, `has_sword` versus `has_spear`, or `has_cavalry_shield` versus `has_shield`—and each class supplies its own handle set to the humanoid archetype resolver.

## Production-shape verification

The shape-verification tooling inspects generated production geometry directly. It checks:

- package integrity;
- exact topology;
- finite and grounded bounds;
- expected axis extents; and
- non-empty full, torso, legs, and head regions.

It also renders the reviewed regions from left, rear, front-quarter, and top views.

```sh
build/bin/mesh_preview verify-horse-shape artifacts/horse-shape
build/bin/mesh_preview verify-elephant-shape artifacts/elephant-shape
```

Axis extents must remain within `0.00005` game units of the reviewed production contract unless the production contract and its tests are changed together.

## Motion and integration gates

Inspect authored locomotion with:

```sh
build/bin/mesh_preview horse artifacts/horse full walk
build/bin/mesh_preview horse artifacts/horse full gallop
build/bin/mesh_preview elephant artifacts/elephant full walk
build/bin/mesh_preview elephant artifacts/elephant full run
```

Run focused model tests with:

```sh
build/bin/horse_model_tests
build/bin/elephant_model_tests
```

Exercise the production runtime path through Arena with:

```sh
build/bin/arena_app --batch --scenario mounted_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
build/bin/arena_app --batch --scenario elephant_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
```

The focused tests cover production counts, topology, normalized weights, locomotion samples, finite deformation, triangle-tear limits, and rider/howdah socket invariants. Arena covers BPAT playback, preparation, rendering, terrain grounding, start/stop transitions, and reverse motion.
