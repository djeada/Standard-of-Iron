# Creature Bone-Palette Animation Texture (BPAT) Format

BPAT is the game's **motion book** for a creature species.

A horse, elephant, humanoid, or sword-ready humanoid does not figure out every joint from scratch in the middle of battle. Instead, the hard animation work is prepared ahead of time and packed into a BPAT file. During play, the game only needs to answer two easy questions:

- **Which move is this creature doing?**
- **How far through that move is it?**

That is why large battles can stay lively without turning animation into a performance disaster.

## The story

Imagine the animators as drill masters before the campaign begins. They teach each creature its full set of moves: idle, walk, trot, attack, recoil, death, and so on.

Then the game's build tools write those lessons into a single travel book for that species: a BPAT file.

When the battle starts, the game does **not** re-teach the horse how to bend a knee or swing a neck. It simply opens the book, jumps to the right move, picks the right moment, and says:

> "Show me frame 18 of the gallop."

The GPU then turns that stored pose into the moving creature you see on screen.

So BPAT is a promise:

- the expensive pose work happened earlier
- the game only chooses **move + moment**
- the graphics card does the fast final playback

## Why players should care

- **Smoother battles**: more units can animate at once.
- **More reliable looks**: creatures keep the same approved motion every time.
- **Less stutter**: the engine avoids heavy live skeletal work in the shipping build.
- **Cleaner design**: gameplay decides intent, while BPAT provides the body language.

In code, that "intent" is basically: _play this clip at this phase_.

## What lives inside a BPAT file

Think of the file as a chest with a few labeled compartments.

| Part         | Plain-English meaning                                   |
| ------------ | ------------------------------------------------------- |
| Header       | Who this file is for and where the other parts begin    |
| Clip list    | The named moves, such as walk or attack                 |
| Socket list  | Attachment points for gear, riders, or props            |
| String table | The actual text names used by the clip and socket lists |
| Palette data | The real pose data for every frame of every move        |
| Socket data  | Optional pre-baked attachment transforms                |
| Contact data | One ground-contact height per frame (v3)                |
| Bind palette | The rest pose the skinning matrices were built against  |

### Header

The header gives the game the basics:

- this is really a **BPAT** file
- which **version** it uses
- which **species** it belongs to
- how many **bones**, **sockets**, and **clips** exist
- how many total animation frames are stored
- where the clip list, socket list, names, and pose data begin

### Clip list

Each clip entry describes one named move:

- the clip name
- how many frames it has
- where its frames begin inside the big shared frame stream
- how fast it plays
- whether it loops
- **authored timing markers** (see below) — key moments inside the move

In simple terms, the clip list is the table of contents for the motion book.

### Authored markers (v2)

Starting with **version 2**, every clip entry also carries five **timing markers**,
each a normalized phase in `[0, 1]` (or `-1` when unset):

| Marker               | Meaning                                                        |
| -------------------- | -------------------------------------------------------------- |
| `anticipation_start` | the wind-up begins                                             |
| `weapon_release`     | the weapon starts travelling toward the target                 |
| `contact`            | the blade/impact connects — **this is when a melee hit lands** |
| `recover_unlocked`   | the attacker may start recovering / chaining                   |
| `exit_safe`          | the move can be safely interrupted/blended out                 |

These replace the old, fragile habit of guessing key moments from the clip _name_. The
baker authors the values; the runtime reads them directly. The `contact` marker is what
lets a melee hit apply damage **mid-swing** (when the weapon visually connects) instead of
on the trigger frame — see the deferred-melee-strike flow in
[`ANIMATION_ARCHITECTURE.md`](ANIMATION_ARCHITECTURE.md). This stays DPS-neutral
(the cooldown still resets at swing start) and deterministic (the pending strike is
serialized).

### Clip flags and the variant table (v3)

Version 3 stops the runtime from re-deriving facts about a clip that the baker already
knows. Every clip entry now carries:

- a **flags** byte. Bit 0, `supplies_ground_contact`, is set for every clip except the
  `riding_*` and `showcase_*` families. The runtime used to test the clip _name_ for
  those prefixes on every soldier every frame; now it reads one bit.
- a **variant family** and **variant ordinal**. Clip variants (the three sword swings,
  the six ambient idles, the infantry death poses) are still contiguous in the blob and
  still selected as `base_clip + variant`, but the baker records which family every clip
  belongs to and its ordinal inside it, straight from `animation/clip_manifest.h`. The
  runtime validates `base_clip + variant` against that table
  (`BpatBlob::clip_is_variant_of`) and falls back to the base clip when the arithmetic
  would land outside the family. Before v3 the same guard existed only for the idle
  ambient variants and worked by comparing clip names.

### Contact data (v3)

Every frame also gets a `BpatFrameContact` record with two floats:

- `sole_y` — the lowest sole point of the posed feet relative to the bind pose. This is
  what the submit path subtracts from the world matrix so a mid-stride soldier stands on
  the ground instead of floating on its bind-pose feet.
- `foot_y` — the lowest foot-bone origin in palette space, used by the shadow and
  grounding pass in preparation.

Both used to be computed at runtime from the full bone palette (matrix inversions and
sole-point transforms per soldier per frame, twice when frame-lerping). The baker now
runs the exact same functions once per frame
(`Render::Creature::Pipeline::palette_contact_y` / `palette_foot_contact_y`) and the
runtime does two array reads and a lerp. `render_request_test` verifies the baked table
against the runtime computation for every frame of every species blob it can load.

### Socket list

Some creatures need stable attachment points: a saddle, a rider anchor, a banner pole, a carried prop.

Each socket entry says:

- the socket name
- which bone it follows
- the small offset from that bone

### String table

This is just the name drawer. Instead of storing repeated text inside every entry, the file keeps the names together in one block and points to them from the clip and socket lists.

### Palette data

This is the heart of the file.

For every stored frame, BPAT contains one **skinning matrix per bone**: the posed bone
transform already multiplied by the inverse of the bind pose, stored column-major
exactly as the GPU consumes it. The data is laid out in one long run of frames, clip
after clip. The renderer's skin atlas is a view straight into this block
(`RiggedSkinAtlas::palettes` references `BpatBlob::palette_matrices()`), and the palette
UBO is uploaded from it without any per-frame multiply or transposition at load.

If you need a bone's world-space pose rather than its skinning matrix — the rider seat
frame, the preview tool's stick figures, the animation diagnostics — multiply by the
baked bind palette: `BpatBlob::bone_global_matrix(frame, bone)` does exactly
`skin × bind`.

If you like analogies, this is a flipbook where each page contains the finished
deformation for that instant rather than the raw pose.

### Bind palette (v3)

`BpatHeaderExtV3::bind_palette_offset` points at `bone_count` column-major matrices:
the rest pose the skinning matrices were built against. It exists so consumers can
recover global bone poses without linking the species rig code (the baker writes it
from the manifest's `bind_palette` provider), and so a blob is self-describing.

### Socket data

This part is optional. If present, it stores ready-to-use attachment transforms so the game does not have to combine the bone pose with that extra socket offset every time. That saves extra work during rendering.

## How playback works without the scary math

1. **Before the game ships**, `tools/bpat_baker` bakes creature motion into BPAT files.
2. **During play**, gameplay chooses a clip and a phase inside it.
3. **The engine looks up the right frame** in the BPAT data.
4. **The GPU reads the stored pose** and bends the creature mesh into place.

No live "solve the whole skeleton from scratch" step runs in the shipping build.

That is the idea behind the format.

## How to use the prebaker

If you want the game to regenerate the current creature BPAT assets, use the prebaker.

### Recommended

```bash
make bake-bpat
```

That runs the built baker and writes the generated creature assets into `assets/creatures/`.

### Direct CLI

```bash
./build/bin/bpat_baker
```

The tool takes one optional argument: the output directory.

```bash
./build/bin/bpat_baker assets/creatures
./build/bin/bpat_baker /tmp/creature_bakes
```

### What it writes today

At the moment, the prebaker writes all built-in species in one pass:

| Output                   | Notes                              |
| ------------------------ | ---------------------------------- |
| `humanoid.bpat`          | default humanoid animation set     |
| `humanoid_sword.bpat`    | sword-ready humanoid animation set |
| `humanoid_spear.bpat`    | spear-ready humanoid animation set |
| `humanoid_skeleton.bpat` | skeleton humanoid animation set    |
| `horse.bpat`             | horse creature BPAT                |
| `horse_minimal.bpsm`     | horse minimal snapshot mesh        |
| `elephant.bpat`          | elephant creature BPAT             |
| `elephant_minimal.bpsm`  | elephant minimal snapshot mesh     |

So this is not a pick-one-species command yet. The current CLI bakes the whole built-in set.

### How a new species plugs into the prebaker

For whole-creature species such as horse and elephant, the current prebaker path is driven by `SpeciesManifest`.

To hook a new species in, provide a manifest with:

- `species_id`
- `bpat_file_name`
- `minimal_snapshot_file_name`
- clip descriptors
- `bind_palette`
- `creature_spec`
- `bake_clip_palette`

Then expose that manifest and call `bake_species_manifest(...)` from the baker entrypoint.

## The texture trick, explained simply

The engine uploads the baked pose data as a texture on the GPU.

Why a texture? Because GPUs are extremely good at reading texture data quickly and in parallel. Instead of treating the texture like a picture, the engine treats it like a shelf full of pose rows.

So when the creature is drawn, the graphics card reads the right bone rows from the texture and bends the mesh into the correct pose.

In player terms: the creature animation is packed like an image so the graphics card can replay it fast.

## A friendly glossary

| Term         | Easy meaning                                       |
| ------------ | -------------------------------------------------- |
| Bone palette | The full creature pose for one frame               |
| Clip         | One named move, like idle or attack                |
| Frame        | One step inside that move                          |
| Socket       | A named attach point for equipment or props        |
| Phase        | How far through the move the creature currently is |
| Species      | Which body plan this file belongs to               |

## The firm rules of the format

For readers who want the important hard facts without drowning in byte offset tables:

- BPAT v3 is **little-endian**.
- Floating-point values are **32-bit IEEE 754 floats**.
- Bone skinning matrices and the bind palette are stored **column-major** (GPU
  layout); the 3×4 socket transforms stay **row-major**.
- Each section begins on a **16-byte boundary**.
- Variable-sized data lives in trailing blocks referenced by **absolute file offsets**.
- Reserved and padding bytes must be **zero**.
- The file magic must be **`BPAT`**.
- The header is **64 bytes** and is immediately followed by a **32-byte v3 extension
  header** (contact table offset and count); each clip entry is **48 bytes** (5 marker
  floats, flags, variant family and ordinal); each socket entry is **32 bytes**; each
  contact record is **8 bytes**; the bind palette is `bone_count × 64` bytes.
- Current supported species ids are **0 = humanoid, 1 = horse, 2 = elephant, 3 = humanoid_sword, 4 = humanoid_spear, 5 = humanoid_skeleton, 6 = humanoid_caster, 7 = humanoid_stave_caster, 8 = sheep, 9 = wolf**.
- Blobs are build output (`make bake-bpat`), never checked in, so a version bump simply
  re-bakes every species; the reader accepts exactly the current version and nothing else.

## How the frame data is packed

The frame data is flat and simple:

- all frames from all clips are stored in one long sequence
- frames from the same clip stay together
- each frame stores all bones for that species

So a clip entry does not own a separate chunked mini-file. It simply points to its starting place inside the full shared stream.

## Validation in plain language

The current reader accepts a BPAT file when:

1. it starts with the `BPAT` magic
2. it uses version `3`
3. its species id is known (`0..9` today)
4. it has at least one clip
5. its bone count is in range
6. its clip frame offsets are contiguous and its frame counts add up correctly
7. its offsets stay inside the file
8. every referenced clip or socket name really exists inside the string table and ends with `NUL`
9. every socket anchor bone points at a real bone
10. if a contact table is present it holds exactly one record per frame and stays inside the file
11. if a bind palette is present it holds exactly one matrix per bone and stays inside the file

The writer still emits zeroed padding and reserved fields, but the current reader does **not** actively reject non-zero reserved bytes.

## If you need the exact binary contract

This page now tells the story first.

For the exact C++ layout used by the engine, see:

- `animation/bpat/bpat_format.h`

For the broader rendering flow, see:

- `docs/RENDERING_ARCHITECTURE.md`
