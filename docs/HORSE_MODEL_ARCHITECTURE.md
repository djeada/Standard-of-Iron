# Authored horse and elephant pipeline

Horse and elephant production geometry is skinned low-poly art. The engine does
not reconstruct either animal from primitive proportions.

## Production assets

The internal horse package is `assets/creatures/horse/horse.cmesh`. It has 4,400
vertices, 2,182 triangles, 50 skin joints and 13 actions. Walk and gallop use
its actions directly.

The horse production transform uses `0.59` on X/Y and `0.5015` on Z, making
its current longitudinal extent 15% shorter while preserving width and height.

The internal elephant package is `assets/creatures/elephant/elephant.cmesh`. It
has 1,464 vertices, 760 triangles, 32 skin joints and five authored actions. Its
walk and fast-walk cycles operate on that armature and its skin weights;
elephants intentionally have no horse-style gallop.

These `.cmesh` files are deterministic outputs of the repository's creature
compiler. No imported mesh, editable source scene, or comparison OBJ is kept in
the repository.

The elephant's production transform is axis-specific: X uses `0.31625`, Y uses
`0.275`, and Z uses `0.1925`. Relative to the earlier model, height is 50%,
length is 35% (50% followed by a further 30% reduction), and width is 57.5%
(the half-width result widened by 15%).

## Runtime architecture

`render/creature/compiled_creature_assets.cpp` validates and decompresses the
generated package, then loads its buffers, accessors, materials, skin weights,
joint hierarchy and animation channels. `MeshSkinning::Authored` carries four
joint indices and weights per vertex through mesh compilation, BPAT baking,
snapshots and the GPU pipeline. The common palette ceiling is 64 bones.

Species manifests select the authored meshes and map gameplay clips to actions.
Horse saddle/rider frames follow the animated back bone; the elephant howdah
frame does the same. Rest landmarks are measured from production geometry rather
than independently guessed dimensions.

### Mount tack is resolved in one place

Every mounted renderer base — `MountedKnightRendererBase`,
`HorseSpearmanRendererBase`, `HorseArcherRendererBase` — carries the same seven
horse equipment slots: saddle, bridle and reins (`HorseTack`), barding and crupper
(`HorseArmor`), and decoration (`HorseDecoration`). They used to hold seven separate
`EquipmentHandle` members each and repeat the same thirty-line resolution block in
their constructors. That now lives in `render/entity/mounted_horse_equipment.h`:
`MountedHorseHandles` holds the seven, `resolve_mounted_horse_handles()` fills them
from any config that names the fields, and `as_array()` produces the ordered array
that `resolve_horse_equipment_archetype()` expects.

The order in `as_array()` is what the archetype resolver hashes, so changing it
changes which archetype a mount resolves to. Add new slots at the end.

The rider's own weapon, shield, helmet, armour and shoulder handles stay per-class,
because each mounted type names them differently (`has_sword` versus `has_spear`,
`has_cavalry_shield` versus `has_shield`) and feeds a different five-handle array to
the humanoid archetype resolver.

## Shape verification

The verification tool checks the generated production geometry directly. It
asserts package integrity, exact topology, finite and grounded bounds, expected
axis extents, and non-empty full/torso/legs/head regions. It renders all four
regions from left, rear, front-quarter and top views for visual inspection.

```bash
build/bin/mesh_preview verify-horse-shape artifacts/horse-shape
build/bin/mesh_preview verify-elephant-shape artifacts/elephant-shape
```

Axis extents must remain within `0.00005` game units of the reviewed production
contract unless that contract is deliberately changed alongside its tests.

## Motion and integration gates

```bash
build/bin/mesh_preview horse artifacts/horse full walk
build/bin/mesh_preview horse artifacts/horse full gallop
build/bin/mesh_preview elephant artifacts/elephant full walk
build/bin/mesh_preview elephant artifacts/elephant full run

build/bin/horse_model_tests
build/bin/elephant_model_tests

build/bin/arena_app --batch --scenario mounted_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
build/bin/arena_app --batch --scenario elephant_locomotion_matrix \
  --fps 60 --capture-interval 2 --artifact-dir artifacts/arena
```

Focused tests validate production counts, topology, normalized weights, 64
samples per locomotion cycle, finite deformation, triangle tear limits, and
animated rider/howdah socket invariants. Arena exercises the real BPAT,
preparation, renderer, terrain grounding, start/stop and reverse paths.
