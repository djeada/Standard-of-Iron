# Creature Bone-Palette Animation Texture (BPAT) Format

Status: **v1**, draft 2026-04-24.

## Purpose

The BPAT format stores **pre-baked bone palettes** for one creature
species across a fixed set of named animation clips. A BPAT file is
produced once at build time by `tools/bpat_baker` and loaded immutably
at runtime; **no skeletal evaluation runs in shipping builds**.

This is the data backbone of the architectural separation between
*anatomy/articulation* (built once into BPATs) and *game intent*
(`PoseIntent { clip, phase }` at runtime). See
`docs/RENDERING_ARCHITECTURE.md` for how BPATs are sampled from the
GPU.

## Design contract

- **Endianness**: little-endian throughout. Producer and current
  runtime are both little-endian; loaders MUST refuse on mismatch.
- **Floats**: IEEE 754 32-bit. Matrices are stored **row-major**
  (matches `QMatrix4x4` byte order on disk after a `memcpy` of the
  underlying `float[16]`).
- **Alignment**: every section starts at a 16-byte boundary.
- **No pointers, no strings inside structs.** All variable-length data
  lives in trailing blocks referenced by absolute file offsets.
- **Mmap-friendly**: a loader can `mmap` the file and reinterpret
  spans directly; no allocation is required to read palettes.
- **Forward compat**: every struct ends with `reserved` words so
  future versions can add fields without changing on-disk layout.
  Unknown reserved values must be zero.

## File layout

```
+------------------------------------------+
| BpatHeader                       (64 B)  |  fixed
+------------------------------------------+
| BpatClipEntry × clip_count       (32 B ea)
+------------------------------------------+
| BpatSocketEntry × socket_count   (32 B ea)
+------------------------------------------+
| String table                             |  UTF-8, null-terminated
+------------------------------------------+
| pad to 16-byte boundary                  |
+------------------------------------------+
| Palette data                             |  float × frame_total × bone_count × 16
+------------------------------------------+
| Socket data (optional)                   |  float × frame_total × socket_count × 12
+------------------------------------------+
```

The palette and socket data blocks are flat: each frame is a
contiguous `bone_count × 16` floats (or `socket_count × 12` floats),
and frames belonging to the same clip are contiguous in file order.
Clip `c`'s frame `f` lives at byte offset:

```
header.palette_data_offset
  + (clip[c].frame_offset + f) * bone_count * 16 * sizeof(float)
```

## Structures (binary)

All structs are POD, packed to natural alignment. Sizes are checked at
compile time in `bpat_format.h` via `static_assert`.

### `BpatHeader` — 64 bytes

| Offset | Type        | Field                  | Notes                              |
|-------:|-------------|------------------------|------------------------------------|
|      0 | `u8[4]`     | `magic`                | ASCII `'B','P','A','T'`            |
|      4 | `u32`       | `version`              | currently `1`                      |
|      8 | `u32`       | `species_id`           | 0=humanoid, 1=horse, 2=elephant    |
|     12 | `u32`       | `bone_count`           | matches species' rig               |
|     16 | `u32`       | `socket_count`         | 0 if no sockets baked              |
|     20 | `u32`       | `clip_count`           | ≥ 1                                |
|     24 | `u32`       | `frame_total`          | Σ frame_count across clips         |
|     28 | `u32`       | `string_table_size`    | bytes, including final NUL         |
|     32 | `u64`       | `clip_table_offset`    | absolute file offset               |
|     40 | `u64`       | `socket_table_offset`  | 0 if `socket_count == 0`           |
|     48 | `u64`       | `string_table_offset`  | absolute file offset               |
|     56 | `u64`       | `palette_data_offset`  | absolute file offset               |

> Note: `socket_data_offset` is computed as
> `palette_data_offset + frame_total*bone_count*16*4` when
> `socket_count > 0`. It is not stored in the header (it is fully
> derivable; saves one field).

### `BpatClipEntry` — 32 bytes

| Offset | Type   | Field          | Notes                                           |
|-------:|--------|----------------|-------------------------------------------------|
|      0 | `u32`  | `name_offset`  | byte offset into string table                   |
|      4 | `u32`  | `name_length`  | excluding trailing NUL                          |
|      8 | `u32`  | `frame_count`  | ≥ 1                                             |
|     12 | `u32`  | `frame_offset` | starting frame index in the flat palette stream |
|     16 | `f32`  | `fps`          | playback frames per second                      |
|     20 | `u8`   | `loops`        | 1 if loops, 0 if one-shot                       |
|     21 | `u8[3]`| `_pad`         | zero                                            |
|     24 | `u32[2]`| `reserved`    | zero                                            |

### `BpatSocketEntry` — 32 bytes

| Offset | Type    | Field           | Notes                            |
|-------:|---------|-----------------|----------------------------------|
|      0 | `u32`   | `name_offset`   | byte offset into string table    |
|      4 | `u32`   | `name_length`   |                                  |
|      8 | `u32`   | `anchor_bone`   | bone index this socket follows   |
|     12 | `f32[3]`| `local_offset`  | offset in anchor-bone local space|
|     24 | `u32[2]`| `reserved`      | zero                             |

> The runtime-visible socket world transform for clip `c`, frame `f`
> is `palette[c,f][anchor_bone] * translate(local_offset)`. The
> `socket_data` block stores this product pre-baked when the writer
> chooses to (saves a multiply per equipment vertex shader call).

## GPU representation (informative)

A loaded BPAT is uploaded as a single 2D `RGBA32F` texture:

- **Width**  = `bone_count * 4` texels (4 texels per `mat4`, one per row).
- **Height** = `frame_total` texels.

The vertex shader samples four texels for each bone index referenced
by the vertex's skinning weights and reconstructs the `mat4`. Frame
index is delivered as a per-instance vertex attribute. No CPU work.

Socket data, when present, is uploaded as a separate `RGB32F` texture
with width `socket_count * 4` and height `frame_total` — the same
sampling pattern but only 3 rows per matrix (translation + rotation
basis).

## Versioning

- `version == 1` (this document)
- A loader MUST refuse files with `version > 1` and warn on `version < 1`.
- Future incompatible changes MUST bump `version` to 2 and the spec
  MUST add a new section describing the delta.

## Validation rules (loader-enforced)

A BPAT file is well-formed iff all of the following hold:

1. `magic == 'BPAT'`.
2. `version == 1`.
3. `species_id ∈ {0, 1, 2}`.
4. `bone_count ∈ [1, 64]`. (`kMaxCreatureBones` = 24 today; the loose
   ceiling allows future skeletons.)
5. `clip_count ≥ 1`.
6. `frame_total = Σ clip[i].frame_count`.
7. `clip[i].frame_offset + clip[i].frame_count ≤ frame_total` for all `i`.
8. All offsets fit within file size; sections do not overlap.
9. Each `name_offset + name_length + 1 ≤ string_table_size`.
10. The byte at `string_table_offset + name_offset + name_length` is `0x00`.
11. All `reserved` and `_pad` bytes are zero.

## Reference C++ types

See `render/creature/bpat/bpat_format.h`.
