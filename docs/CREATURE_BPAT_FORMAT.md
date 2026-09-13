# Creature Bone-Palette Animation Texture Format

BPAT is the runtime animation format used for skinned creatures. A BPAT file stores pre-baked bone palettes for named animation clips together with clip timing, attachment sockets, ground-contact data, bind-pose data, and the skeleton parent table.

Runtime animation selects a clip and frame/phase from this baked data instead of rebuilding the authored skeletal animation from source definitions every frame.

## Current format version

`animation/bpat/bpat_format.h` defines:

```cpp
k_magic   = {'B', 'P', 'A', 'T'}
k_version = 3
```

The reader in `animation/bpat/bpat_reader.cpp` accepts exactly `k_version`. A blob with another version is rejected as `unsupported version`.

The current species IDs are:

| ID | Species/profile |
| ---: | --- |
| 0 | humanoid |
| 1 | horse |
| 2 | elephant |
| 3 | humanoid sword-ready |
| 4 | humanoid spear-ready |
| 5 | humanoid skeleton |
| 6 | humanoid caster |
| 7 | humanoid stave-caster |
| 8 | sheep |
| 9 | wolf |

`k_species_count` is 10 and `k_max_species_id` is the wolf ID.

## File layout

The binary begins with a 64-byte `BpatHeader`, followed by the 32-byte `BpatHeaderExtV3` used by version 3.

The header identifies:

- format version and species;
- bone, socket, clip, and frame counts;
- string-table size; and
- offsets to clip, socket, string, and palette data.

The v3 extension adds offsets/counts for:

- per-frame contact data;
- the bind palette; and
- the bone-parent table.

Section offsets are aligned by the writer with `k_section_alignment`, currently 16 bytes.

## Clip entries

Each `BpatClipEntry` is 48 bytes and stores:

- clip name offset and length;
- frame count and global frame offset;
- playback FPS;
- loop flag;
- clip flags;
- variant family and ordinal; and
- five authored timing markers.

The markers are normalized clip phases:

| Marker | Meaning |
| --- | --- |
| `anticipation_start` | wind-up begins |
| `weapon_release` | weapon begins travelling toward the target |
| `contact` | impact/contact point used by melee timing |
| `recover_unlocked` | recovery/chaining may begin |
| `exit_safe` | clip can be interrupted or blended out safely |

An unset marker is represented by the value produced by the clip marker source; runtime consumers read the values from the baked entry rather than inferring timing from clip names.

### Flags and variants

Bit 0 of the flags byte is `k_clip_flag_supplies_ground_contact`.

Variant metadata records a family and ordinal for clips that belong to a baked variant group. `BpatBlob::clip_is_variant_of()` validates a requested variant against this metadata before runtime code treats `base + ordinal` as a valid member of the family.

## Palette data

For every stored frame, BPAT contains one skinning matrix per bone. Frames from all clips are stored in one contiguous global frame stream; each clip entry names its starting frame and frame count.

`BpatBlob::palette_matrices()` exposes the decoded palette block. `bone_global_matrix(frame, bone)` combines a skinning matrix with the baked bind palette when a consumer needs a bone's global pose rather than the GPU skinning transform.

## Bind palette and parent table

Version 3 can carry one bind-pose matrix per bone and one parent byte per bone.

The parent table uses `0xFF` for a root. The reader rejects a parent index that does not precede its child.

The reader derives local bone poses from the stored palette, bind palette, and hierarchy. `frame_local_pose_view()` exposes those decoded local rotations/translations for interpolation and layered animation.

## Per-frame contact data

`BpatFrameContact` is an 8-byte record containing:

- `sole_y` — the lowest posed sole point relative to the bind-pose reference used by grounding; and
- `foot_y` — the lowest foot-bone origin used by the preparation/shadow path.

When a contact table is present, the reader requires exactly one contact record per global frame.

## Sockets

Each `BpatSocketEntry` is 32 bytes. It stores:

- socket name;
- anchor-bone index; and
- local offset.

When sockets are present, the file also contains per-frame socket transforms after the palette data. The reader verifies every socket anchor against `bone_count`.

## Reader validation

`BpatBlob::validate()` rejects a blob when any of the following current contracts fail:

- file shorter than the required header data;
- magic mismatch;
- version not equal to `k_version`;
- species ID outside the supported range;
- `bone_count` equal to zero or above 64;
- zero clips;
- clip, socket, string, palette, contact, bind-palette, or parent data outside the file;
- contact-table count different from `frame_total`;
- invalid bone-parent ordering;
- non-contiguous clip frame offsets;
- zero-frame clips;
- clip frame counts that do not sum to `frame_total`;
- clip/socket names that fall outside the string table or are not NUL-terminated; or
- socket anchors outside the bone range.

The format structures and validation code are the binary contract. Claims about byte layout should be taken from `animation/bpat/bpat_format.h` and `animation/bpat/bpat_reader.cpp` rather than inferred from an older generated file.

## Baking assets

The normal repository command is:

```sh
make bake-bpat
```

The direct executable is:

```sh
./build/bin/bpat_baker [output-directory]
```

The command-line interface accepts an optional output directory. It does not accept a species selector.

`tools/bpat_baker/main.cpp` always bakes the complete built-in set by iterating all six humanoid bake profiles and then baking horse, elephant, sheep, and wolf.

The generated asset list in `tools/bpat_baker/CMakeLists.txt` includes:

### BPAT animation blobs

- `humanoid.bpat`
- `humanoid_sword.bpat`
- `humanoid_spear.bpat`
- `humanoid_skeleton.bpat`
- `humanoid_caster.bpat`
- `humanoid_stave_caster.bpat`
- `horse.bpat`
- `elephant.bpat`
- `sheep.bpat`
- `wolf.bpat`

### Minimal snapshot meshes

- `horse_minimal.bpsm`
- `elephant_minimal.bpsm`
- `sheep_minimal.bpsm`
- `wolf_minimal.bpsm`

### Rigged body meshes

The same bake target also produces full/minimal `.bprm` bodies for humanoid, skeleton-humanoid, horse, elephant, sheep, and wolf.

CMake runs the baker for both `assets/creatures` in the source tree and `build/bin/assets/creatures` in the staged runtime tree.

## Adding a baked creature profile

A baked creature is driven by a `CreatureBakeRecipe` and its runtime manifest. The recipe supplies the runtime manifest, clip descriptions, sockets, marker provider when needed, and the frame-baking function.

The baker writes the manifest's `bpat_file_name` and any configured minimal snapshot/body assets. A new built-in species/profile must also be added to the set invoked by `tools/bpat_baker/main.cpp` and to the generated-output list in `tools/bpat_baker/CMakeLists.txt` so the build knows which files the bake target produces.

The current CLI always processes that built-in set in one invocation.
