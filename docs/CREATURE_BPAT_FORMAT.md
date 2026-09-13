# Creature Bone-Palette Animation Texture Format

BPAT is the baked runtime animation format used for skinned creatures in Standard of Iron. A BPAT file packages the data the runtime needs to animate a creature without rebuilding authored skeletal animation from source definitions every frame.

The format stores pre-baked bone palettes for named animation clips together with clip timing, animation markers, variant metadata, attachment sockets, ground-contact data, bind-pose data, and the skeleton parent table.

At runtime, the game chooses a clip and phase, reads/interpolates the baked pose data, and uses the same asset for rendering, attachment placement, grounding, combat timing, and layered animation where those systems require it.

## Format ownership

The binary contract is defined by:

- `animation/bpat/bpat_format.h` — structures, constants, IDs, sizes, version;
- `animation/bpat/bpat_reader.cpp` — validation and decoded runtime view; and
- `tools/bpat_baker/` — production writer/bake path.

Generated files are outputs of that contract. When the code and an older `.bpat` disagree, the current format/reader code is authoritative.

## Current format version

`animation/bpat/bpat_format.h` defines:

```cpp
k_magic   = {'B', 'P', 'A', 'T'}
k_version = 3
```

The current reader accepts exactly `k_version`.

A file with a different version is rejected with an unsupported-version error rather than interpreted through a best-effort compatibility path.

That strictness keeps the binary layout deterministic: if the structure changes, the version must change with it.

## Species/profile IDs

The current species/profile IDs are:

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

The profile IDs allow several humanoid bake variants to coexist while still using the same BPAT runtime format.

## High-level file layout

A BPAT v3 file is laid out as a header plus referenced sections:

```text
BpatHeader (64 bytes)
BpatHeaderExtV3 (32 bytes)
        │
        ├─ clip table
        ├─ socket table
        ├─ string table
        ├─ frame bone palettes
        ├─ optional/permitted per-frame socket transforms
        ├─ frame contact table
        ├─ bind palette
        └─ bone-parent table
```

The writer aligns sections using `k_section_alignment`, currently 16 bytes.

Offsets in the header are what define the binary section locations. Consumers should not infer section placement from the size of one generated sample file.

## Base header

The 64-byte `BpatHeader` identifies core file facts, including:

- magic/version;
- species/profile ID;
- bone count;
- socket count;
- clip count;
- total frame count;
- string-table size; and
- offsets to clip, socket, string, and palette data.

These fields are sufficient to locate the original BPAT sections and validate the broad dimensions of the asset.

## Version-3 extension

`BpatHeaderExtV3` is 32 bytes and extends v3 with offsets/counts for:

- per-frame contact data;
- the bind palette; and
- the bone-parent table.

These additions let the runtime recover more than GPU skinning matrices. It can derive local poses, bone-global transforms, hierarchy-aware interpolation, and grounding/contact information from the same baked asset.

## Clip table

Each `BpatClipEntry` is 48 bytes.

A clip entry stores:

- clip name offset and length;
- frame count;
- global frame offset;
- playback FPS;
- loop flag;
- clip flags;
- variant family;
- variant ordinal; and
- five authored timing markers.

All clips share one global frame stream. The clip entry identifies the contiguous frame range belonging to that action.

## Animation markers

The five normalized markers are:

| Marker | Meaning |
| --- | --- |
| `anticipation_start` | wind-up begins |
| `weapon_release` | weapon begins travelling toward the target |
| `contact` | impact/contact point used by melee timing |
| `recover_unlocked` | recovery/chaining may begin |
| `exit_safe` | clip can be interrupted/blended out safely |

These markers make action timing authored data rather than a runtime guess based on clip names or fixed frame numbers.

A melee system, for example, can use the baked contact/recovery semantics even if two actions have different frame counts or FPS.

## Marker phases

Markers are stored as normalized clip phases rather than absolute wall-clock times.

This lets runtime code map them consistently onto the current clip duration/playback rate.

An unset marker uses the value produced by the marker source/bake path. Consumers should read marker state from the baked entry instead of assuming every action supplies every timing landmark.

## Clip flags

Bit 0 of the flags byte is:

```text
k_clip_flag_supplies_ground_contact
```

This marks clips whose baked contact data supplies the relevant grounding/contact information for the runtime path.

Flags belong to the binary clip contract; they should be expanded through the format definition rather than through undocumented magic values in a consumer.

## Variant metadata

Variant metadata records:

- a variant family; and
- an ordinal within that family.

`BpatBlob::clip_is_variant_of()` validates that a requested clip really belongs to the expected family before runtime code treats `base + ordinal` as a valid variant.

This avoids relying on accidental clip-table adjacency as the only proof that several actions are interchangeable variants.

## Frame palette data

For every stored global frame, BPAT contains one skinning matrix per bone.

The layout is conceptually:

```text
frame 0: bone 0 ... bone N-1
frame 1: bone 0 ... bone N-1
...
```

Clips select slices of that global frame stream using `frame_offset` and `frame_count`.

`BpatBlob::palette_matrices()` exposes the decoded palette block.

## Skinning vs bone-global transforms

A skinning matrix is not automatically the same thing as the bone's global pose matrix.

`bone_global_matrix(frame, bone)` combines the stored skinning transform with the baked bind-pose information when a consumer needs the actual global bone pose.

This is important for systems such as attachment sockets or analysis that need the transformed bone rather than the GPU-ready skinning matrix alone.

## Bind palette

Version 3 can store one bind-pose matrix per bone.

The bind palette provides the reference needed to recover posed/global/local information from the baked skinning palette.

The reader validates that the bind-palette section fits the file and matches the bone dimensions expected by the header.

## Bone-parent table

Version 3 also stores one parent byte per bone.

The special value:

```text
0xFF
```

identifies a root bone.

The reader rejects a parent index that does not precede its child in the baked ordering.

This ordering constraint lets the runtime reconstruct hierarchy-dependent pose data in a deterministic forward pass.

## Local-pose view

Using the palette, bind pose, and hierarchy, the reader can derive local bone transforms.

`frame_local_pose_view()` exposes decoded local rotations/translations used by interpolation and layered animation code.

This is one reason the v3 asset contains more than the matrix block required for simple GPU skinning: the runtime can work with bone-local pose data without returning to the authoring source.

## Per-frame contact data

`BpatFrameContact` is an 8-byte record with:

- `sole_y`; and
- `foot_y`.

`sole_y` represents the lowest posed sole point relative to the bind-pose reference used by grounding.

`foot_y` records the lowest foot-bone origin used by the preparation/shadow path.

When a contact table is present, the reader requires exactly one record per global frame.

That one-to-one relationship keeps contact sampling aligned with animation sampling.

## Why contact data is baked

Grounding information can be expensive or inconsistent to infer from arbitrary posed geometry at runtime.

Baking it gives the runtime a stable per-frame answer that can be shared by animation preparation and grounding/shadow logic.

The value still belongs to presentation/animation behavior; authoritative unit-root position remains a gameplay/movement fact.

## Sockets

Each `BpatSocketEntry` is 32 bytes.

A socket stores:

- socket name;
- anchor-bone index; and
- local offset.

When sockets are present, the file also contains their per-frame transformed data after the palette section according to the current writer layout.

The reader checks that every socket anchor references a valid bone.

## Socket use

Sockets provide stable attachment points for items/effects that need to follow the animated skeleton.

Typical consumers include weapon/equipment placement and other creature attachments.

The socket contract keeps attachment placement tied to the baked skeleton rather than hard-coded world offsets in individual renderers.

## String table

Clip and socket names are stored through offsets into the file string table.

The reader verifies that names:

- fall inside the string-table bounds; and
- are NUL-terminated within the table.

This prevents malformed offsets from turning a corrupt asset into an unbounded string read.

## Reader validation

`BpatBlob::validate()` rejects a blob when the current binary contract is violated.

Current checks include:

- file shorter than the required header data;
- magic mismatch;
- version not equal to `k_version`;
- species ID outside the supported range;
- `bone_count` equal to zero or above 64;
- zero clips;
- section offsets/ranges outside the file;
- contact-table count different from `frame_total`;
- invalid bone-parent ordering;
- non-contiguous clip frame offsets;
- zero-frame clips;
- clip frame counts that do not sum to `frame_total`;
- clip/socket names outside the string table;
- non-NUL-terminated names; and
- socket anchors outside the valid bone range.

A BPAT file is therefore treated as an untrusted binary blob until those structural checks pass.

## Contiguous clip-frame contract

Clip frame ranges are required to be contiguous in the global frame stream.

That gives the runtime a simple model:

```text
clip 0 frames
clip 1 frames
clip 2 frames
...
```

with no holes or overlapping clip ranges.

The sum of all clip frame counts must equal `frame_total`.

## Bone-count limit

The reader currently accepts at most 64 bones.

That limit is part of the format/runtime contract and should be treated as such when authoring a new creature skeleton.

A source rig with more bones cannot simply be baked and expected to load unless the format/runtime limit is changed coherently.

## Runtime clip selection

Runtime systems select clips using the baked manifest/profile and action state.

The BPAT reader supplies:

- clip identity;
- frame count/FPS;
- loop state;
- markers;
- variants;
- palette access;
- local-pose access;
- contacts; and
- sockets.

Higher-level animation code decides which clip to play and how to blend/layer it. The BPAT file supplies the immutable baked data.

## Interpolation and layered animation

The local-pose view enables runtime interpolation and layered animation without rebuilding the authored animation graph.

For example, a defensive upper-body overlay can compose with locomotion when the runtime has the relevant local bone pose data and masks/overlay rules.

The BPAT format itself does not decide gameplay state; it makes the posed animation data available to the animation/rendering systems that interpret current actions.

## Baking assets

The normal repository command is:

```sh
make bake-bpat
```

The direct executable is:

```sh
./build/bin/bpat_baker [output-directory]
```

The current CLI accepts one optional output directory.

It does **not** accept a species selector.

`tools/bpat_baker/main.cpp` always bakes the complete built-in profile/species set.

## Built-in bake set

The baker iterates six humanoid bake profiles and then bakes horse, elephant, sheep, and wolf.

The resulting BPAT files are:

- `humanoid.bpat`;
- `humanoid_sword.bpat`;
- `humanoid_spear.bpat`;
- `humanoid_skeleton.bpat`;
- `humanoid_caster.bpat`;
- `humanoid_stave_caster.bpat`;
- `horse.bpat`;
- `elephant.bpat`;
- `sheep.bpat`; and
- `wolf.bpat`.

These correspond to the current supported species/profile IDs.

## Minimal snapshot meshes

The same bake target also produces minimal `.bpsm` snapshot meshes for:

- horse;
- elephant;
- sheep; and
- wolf.

Current generated names include:

- `horse_minimal.bpsm`;
- `elephant_minimal.bpsm`;
- `sheep_minimal.bpsm`;
- `wolf_minimal.bpsm`.

These assets belong to the creature presentation/LOD pipeline rather than the BPAT matrix format itself, but they are generated by the same production bake target.

## Rigged body meshes

The bake target also produces full/minimal `.bprm` rigged bodies for the relevant creature profiles, including humanoid/skeleton-humanoid and the four non-humanoid creatures.

This keeps animation and body outputs synchronized through one asset-generation step.

## Source and staged outputs

CMake runs the baker for both:

```text
assets/creatures
```

and:

```text
build/bin/assets/creatures
```

so the repository/source asset tree and the staged runtime asset tree receive the generated outputs expected by the build/run workflow.

## `CreatureBakeRecipe`

A baked creature profile is driven by a `CreatureBakeRecipe` and its runtime manifest.

The recipe provides information such as:

- runtime manifest/profile;
- clip descriptions;
- sockets;
- marker provider where required; and
- the frame-baking function.

The baker then writes the manifest's configured `bpat_file_name` and associated body/snapshot assets.

## Adding a built-in creature profile

Adding a new built-in profile requires more than assigning a new species ID.

The current integration points include:

1. define/update the runtime manifest/profile;
2. provide a `CreatureBakeRecipe`;
3. supply clip definitions and bake function;
4. author required sockets/markers;
5. add the profile/species to the sequence invoked by `tools/bpat_baker/main.cpp`;
6. add generated outputs to `tools/bpat_baker/CMakeLists.txt`; and
7. ensure the runtime species/profile ID and reader limits are updated coherently if the profile expands the supported ID set.

Because the CLI bakes the complete built-in set, the new profile becomes part of the normal bake invocation rather than a separately selected command-line mode.

## Changing the binary format

A binary-format change should be treated as a versioned contract change.

At minimum, review:

- `BpatHeader` / extension structures;
- `k_version`;
- writer offsets/alignment;
- reader range validation;
- decoded runtime views;
- generated asset rebuild; and
- all consumers that assume current section dimensions.

The reader currently accepts only one exact version, so adding an incompatible section/field without a version change is not a safe migration strategy.

## Diagnosing a load failure

A BPAT failure can usually be localized from the reader validation stage.

### Unsupported version

The file was baked with a format version different from the runtime `k_version`.

### Out-of-range section

Header offsets/counts do not fit inside the file. Inspect writer layout or a truncated/corrupt asset.

### Clip-frame continuity failure

Clip offsets/counts no longer form one contiguous frame stream.

### Invalid parent ordering

The baked skeleton parent table violates the parent-before-child ordering required by the decoder.

### Invalid string/socket reference

Inspect string table offsets/NUL termination or socket anchor indices.

Regenerating the asset with the current baker is the first useful check when a stale generated file is suspected.

## Runtime invariants

The current BPAT pipeline depends on these invariants:

- file magic/version are exact;
- species/profile IDs stay inside the declared range;
- bone count is `1..64`;
- clip frame ranges are contiguous and cover `frame_total` exactly;
- string references remain inside the string table;
- socket anchors reference valid bones;
- contact records align one-to-one with global frames when present;
- parent indices precede their children; and
- the production baker/output list agree about the assets generated by `make bake-bpat`.

## Source map

| Concern | Source |
| --- | --- |
| Binary structures/version/IDs | `animation/bpat/bpat_format.h` |
| Reader/validation/decoded views | `animation/bpat/bpat_reader.cpp` |
| Bake executable | `tools/bpat_baker/main.cpp` |
| Generated-output list | `tools/bpat_baker/CMakeLists.txt` |
| Creature manifests/recipes | animation/creature bake sources |
| Runtime creature rendering | renderer/animation creature paths |

The current v3 structures, reader checks, and production baker are the BPAT contract. Older generated-file assumptions or partial output lists should not be carried forward when they disagree with those sources.
