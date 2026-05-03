# Creature Bone-Palette Animation Texture (BPAT) Format

Status: **v1**, draft 2026-04-24.

## The short version

BPAT is the game's **motion book** for a creature species.

A horse, elephant, humanoid, or sword-ready humanoid does not figure
out every joint from scratch in the middle of battle. Instead, the hard
animation work is prepared ahead of time and packed into a BPAT file.
During play, the game only needs to answer two easy questions:

- **Which move is this creature doing?**
- **How far through that move is it?**

That is why large battles can stay lively without turning animation
into a performance disaster.

## The story

Imagine the animators as drill masters before the campaign begins.
They teach each creature its full set of moves: idle, walk, trot,
attack, recoil, death, and so on.

Then the game's build tools write those lessons into a single travel
book for that species: a BPAT file.

When the battle starts, the game does **not** re-teach the horse how
to bend a knee or swing a neck. It simply opens the book, jumps to the
right move, picks the right moment, and says:

> "Show me frame 18 of the gallop."

The GPU then turns that stored pose into the moving creature you see on
screen.

So BPAT is really a promise:

- the expensive pose work happened earlier
- the game only chooses **move + moment**
- the graphics card does the fast final playback

## Why players should care

- **Smoother battles**: more units can animate at once.
- **More reliable looks**: creatures keep the same approved motion every
  time.
- **Less stutter**: the engine avoids heavy live skeletal work in the
  shipping build.
- **Cleaner design**: gameplay decides intent, while BPAT provides the
  body language.

In code, that "intent" is basically: *play this clip at this phase*.

## What lives inside a BPAT file

Think of the file as a chest with a few labeled compartments.

| Part | Plain-English meaning |
|---|---|
| Header | Who this file is for and where the other parts begin |
| Clip list | The named moves, such as walk or attack |
| Socket list | Attachment points for gear, riders, or props |
| String table | The actual text names used by the clip and socket lists |
| Palette data | The real pose data for every frame of every move |
| Socket data | Optional pre-baked attachment transforms |

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

In simple terms, the clip list is the table of contents for the motion
book.

### Socket list

Some creatures need stable attachment points: a saddle, a rider anchor,
a banner pole, a carried prop.

Each socket entry says:

- the socket name
- which bone it follows
- the small offset from that bone

### String table

This is just the name drawer. Instead of storing repeated text inside
every entry, the file keeps the names together in one block and points
to them from the clip and socket lists.

### Palette data

This is the heart of the file.

For every stored frame, BPAT contains the full pose of the creature:
where each bone should be and how it should be oriented. The data is
laid out in one long run of frames, clip after clip.

If you like analogies, this is a flipbook where each page contains the
full bone pose for that instant.

### Socket data

This part is optional.

If present, it stores ready-to-use attachment transforms so the game
does not have to combine the bone pose with that extra socket offset
every time.
That saves extra work during rendering.

## How playback works without the scary math

1. **Before the game ships**, `tools/bpat_baker` bakes creature motion
   into BPAT files.
2. **During play**, gameplay chooses a clip and a phase inside it.
3. **The engine looks up the right frame** in the BPAT data.
4. **The GPU reads the stored pose** and bends the creature mesh into
   place.

No live "solve the whole skeleton from scratch" step runs in the
shipping build.

That is the key idea behind the format.

## How to use the prebaker

If you want the game to regenerate the current creature BPAT assets, use
the prebaker.

### Recommended

```bash
make bake-bpat
```

That runs the built baker and writes the generated creature assets into
`assets/creatures/`.

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

| Output | Notes |
|---|---|
| `humanoid.bpat` | default humanoid animation set |
| `humanoid_sword.bpat` | sword-ready humanoid animation set |
| `horse.bpat` | horse creature BPAT |
| `horse_minimal.bpsm` | horse minimal snapshot mesh |
| `elephant.bpat` | elephant creature BPAT |
| `elephant_minimal.bpsm` | elephant minimal snapshot mesh |

So this is not a pick-one-species command yet. The current CLI bakes the
whole built-in set.

### How a new species plugs into the prebaker

For whole-creature species such as horse and elephant, the current
prebaker path is driven by `SpeciesManifest`.

To hook a new species in, provide a manifest with:

- `species_id`
- `bpat_file_name`
- `minimal_snapshot_file_name`
- clip descriptors
- `bind_palette`
- `creature_spec`
- `bake_clip_palette`

Then expose that manifest and call `bake_species_manifest(...)` from the
baker entrypoint.

## The texture trick, explained simply

The engine uploads the baked pose data as a texture on the GPU.

Why a texture? Because GPUs are extremely good at reading texture data
quickly and in parallel. Instead of treating the texture like a picture,
the engine treats it like a shelf full of pose rows.

So when the creature is drawn, the graphics card reads the right bone
rows from the texture and bends the mesh into the correct pose.

In player terms: the creature animation is packed like an image so the
graphics card can replay it fast.

## A friendly glossary

| Term | Easy meaning |
|---|---|
| Bone palette | The full creature pose for one frame |
| Clip | One named move, like idle or attack |
| Frame | One step inside that move |
| Socket | A named attach point for equipment or props |
| Phase | How far through the move the creature currently is |
| Species | Which body plan this file belongs to |

## The firm rules of the format

For readers who want the important hard facts without drowning in byte
offset tables:

- BPAT v1 is **little-endian**.
- Floating-point values are **32-bit IEEE 754 floats**.
- Bone matrices are stored **row-major**.
- Each section begins on a **16-byte boundary**.
- Variable-sized data lives in trailing blocks referenced by **absolute
  file offsets**.
- Reserved and padding bytes must be **zero**.
- The file magic must be **`BPAT`**.
- Current supported species ids are **0 = humanoid, 1 = horse, 2 =
  elephant, 3 = humanoid_sword**.

## How the frame data is packed

The frame data is flat and simple:

- all frames from all clips are stored in one long sequence
- frames from the same clip stay together
- each frame stores all bones for that species

So a clip entry does not own a separate chunked mini-file. It simply
points to its starting place inside the full shared stream.

## Validation in plain language

The current reader accepts a BPAT file when:

1. it starts with the `BPAT` magic
2. it uses version `1`
3. its species id is known (`0..3` today)
4. it has at least one clip
5. its bone count is in range
6. its clip frame offsets are contiguous and its frame counts add up
   correctly
7. its offsets stay inside the file
8. every referenced clip or socket name really exists inside the string
   table and ends with `NUL`
9. every socket anchor bone points at a real bone

The writer still emits zeroed padding and reserved fields, but the
current reader does **not** actively reject non-zero reserved bytes.

## If you need the exact binary contract

This page now tells the story first.

For the exact C++ layout used by the engine, see:

- `render/creature/bpat/bpat_format.h`

For the broader rendering flow, see:

- `docs/RENDERING_ARCHITECTURE.md`
