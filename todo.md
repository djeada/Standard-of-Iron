# Standard of Iron — Horse & Elephant Pipeline Cleanup Issues

## Target Architecture

Use the same three-layer split as humanoids, but share as much as possible through a generic quadruped layer.

```text
1. QUADRUPED / SPECIES SCHEMA
   immutable
   ├─ skeleton topology
   ├─ bone semantics
   ├─ socket semantics
   ├─ clip IDs
   ├─ species capabilities
   ├─ static scale / bind metadata
   └─ asset identifiers

          ↓

2. BAKE / ASSET BUILD
   once
   ├─ compile .cmesh
   ├─ deterministic geometry fixups
   ├─ generate derived geometry
   ├─ map source actions → gameplay clips
   ├─ generate BPAT
   ├─ derive rest attachment frames
   └─ serialize runtime metadata

          ↓

3. RUNTIME QUADRUPED
   dynamic
   ├─ current locomotion state
   ├─ current clip + phase
   ├─ current bone palette
   ├─ current saddle/howdah transform
   ├─ rider state
   ├─ terrain grounding
   ├─ team/material variation
   ├─ LOD
   ├─ shadows
   └─ submission
```

The horse and elephant should then be thin species adapters over the same runtime quadruped machinery.

## Progress

Every issue's premise was checked against the tree before any work started,
because several of them describe code that is no longer there.

| Issue | Title                                                       | Status       |
| ----- | ----------------------------------------------------------- | ------------ |
| 1     | Extend horse/elephant tests into parity tests               | done         |
| 2     | Split `SpeciesManifest` into bake recipe + runtime manifest | done         |
| 3     | Move horse geometry out of `horse_manifest.cpp`             | already done |
| 4     | Remove legacy procedural elephant geometry                  | already done |
| 5     | Bake elephant eye mirroring into `elephant.cmesh`           | already done |
| 6     | Replace horse/elephant `template_prewarm` rendering         | done         |
| 7     | Remove source animation sampling from runtime headers       | partly done  |
| 8     | Deduplicate the two source-asset status structs             | done         |
| 9     | Remove elephant asset ownership from `Render::Horse`        | done         |
| 10    | Bake procedural elephant locomotion into BPAT               | blocked      |
| 11    | Generic animated sockets for mount and howdah               | partly done  |
| 12    | Remove the hardcoded elephant back-bone index               | done         |
| 13    | Extract common `prepare_quadruped_render()`                 | done         |
| 14    | Replace `HorseLOD` with `CreatureLOD`                       | done         |
| 15    | Stop compiling mesh LODs on first use                       | done         |
| 16    | Generic `QuadrupedAssetKey` / `QuadrupedInstanceState`      | done         |
| 17    | Split `elephant/attachment_frames.h` into three concerns    | done         |
| 18    | Horse attachments follow the same socket model              | done         |
| 19    | Move horse/elephant render stats into a runtime context     | done         |
| 20    | Stop using the entity pointer address as horse seed         | already done |
| 21    | Remove or use `shared_mount`                                | done         |
| 22    | One skeleton topology definition per species                | done         |
| 23    | Isolate legacy procedural animal systems                    | done         |
| 24    | Reduce the species renderers to adapters                    | done         |

### Issues whose premise no longer holds

These were checked by searching for the exact symbols each issue names. Nothing
was done for them because there is nothing left to do:

- **Issue 3** — `horse_manifest.cpp` is 174 lines, not ~1,247. `horse_hoof_mesh()`,
  `build_horse_full_leg_overlays()` and `k_horse_full_leg_overlays` do not exist
  anywhere in the tree.
- **Issue 4** — `elephant_manifest.cpp` is 149 lines. `build_elephant_whole_nodes()`,
  `k_elephant_whole_nodes` and `k_elephant_full_leg_overlays` do not exist.
- **Issue 5** — there is no `build_elephant_production_nodes()` and no eye-mirroring
  code in render. The `.cmesh` already carries both eyes.
- **Issue 20** — horse preparation already seeds from
  `Render::Creature::stable_entity_seed(ctx.entity->get_id())`. No pointer
  address is involved.
- **Issue 22** — `elephant_topology_storage()`/`elephant_topology_ref()` are gone;
  the manifest points at `elephant_spec.cpp`'s topology. What remains is the
  weaker version of the problem: the runtime topology in `*_spec.cpp` and the
  compiled bone definitions from the `.cmesh` are still two hand-kept lists that
  happen to agree.

### A correction to the plan's premise

The issue-1 case list asks for `idle / walk / trot / canter / gallop` as if the
horse had four locomotion animations. It does not — the horse animates on walk
vs run exactly like a humanoid, and trot, canter and gallop all resolve to
`AnimationStateId::Run` and produce byte-identical bone palettes. They are gait
_bands_: they differ only in the procedural gait descriptor (cycle time, stride
swing and lift, bob, sway). The harness records all five bands plus a `motion`
line carrying that descriptor, so the band mapping is pinned even though the
drawn clip cannot distinguish them.

### Issue 10 is blocked, and the plan's ordering is why

The procedural elephant walk is **not** dead code — it is the production walk.
The shipped `elephant.cmesh` has no authored `Walk` or `Run` clip, so
`sample_elephant_locomotion()` falls through to `synthesise_elephant_locomotion()`
every time. I confirmed this by making the synthesis print on entry: it fires 405
times across the render suite. (An earlier reading of mine said the authored
clips existed; that was wrong — `elephant_source_sample_clip("Walk")` returns
true _because_ the synthesis succeeds.)

The generator has two callers:

- `elephant_bake_recipe.cpp`, which is correct — this is what writes the
  synthesised walk into the BPAT at build time;
- `elephant_source_pose_howdah()`, at runtime, which resamples the source clip
  purely to work out where the howdah sits.

So "runtime does not contain `sample_elephant_locomotion()`" cannot happen until
the howdah stops resampling — which is Issue 11's second half. The plan sequences
7 → 10 → 11; it has to be 11 → 10 → 7.

Issue 11's second half needs the howdah and mount sockets to resolve from the
bone palette the renderer already computed, rather than from a fresh source
sample. The BPAT format supports per-frame socket transforms (the humanoid uses
them), but the horse and elephant recipes declare no sockets, so their BPATs
carry no socket track — and `evaluate_elephant_motion()` runs _before_ playback
resolves, so it has no palette to read. Closing this means baking socket tracks
for both species and moving motion evaluation after playback: a real
architectural change, not cleanup, and it needs an asset re-bake.

What did land for Issue 11: the duplicated "apply a bone's pose-vs-bind delta to
a set of attachment points" logic is now one implementation
(`render/creature/quadruped/attachment_resolver.h`), and the horse's seven mount
sockets are declared as data in `render/horse/schema/mounted_sockets.h`, which is
Issue 18.

### Notes on what landed

**Issue 7** is partly done. `CreatureBakeRecipe` is a separate library
(`creature_bake`) that only the baker and the test binaries link, and the game
executable contains none of the bake symbols — verified with `nm` on
`build/bin/standard_of_iron`: `bake_horse_manifest_clip_frame`,
`bake_elephant_manifest_clip_frame`, `horse_bake_recipe`, `elephant_bake_recipe`,
`horse_source_sample_clip` and `elephant_source_sample_clip` are all absent. What
has _not_ happened is the physical move of the source-animation sampler into
`tools/`: `compiled_creature_assets.cpp` still holds it, and it still compiles
into `render_gl` even though nothing in the game pulls it in.

**Issue 2** split horse and elephant into `<species>_manifest.cpp` (runtime) and
`<species>_bake_recipe.cpp` (bake). Sheep, wolf and the humanoid expose a
`CreatureBakeRecipe` through the same interface but still define their bake
tables inside their manifest translation unit; the humanoid's is 2,391 lines with
templated bake profiles and is its own job.
`tests/architecture/humanoid_layering_test.cpp` names those three as exemptions
so the boundary test stays meaningful.

**Issue 15** replaced the two function-local statics per species with a
`CreatureLodGeometrySlot`. `register_built_in_entity_renderers()` now initializes
all four species at load, and every compile is counted, so
`CreatureAssetPrewarm.SpeciesSpecCompilesNoGeometryOncePrewarmed` can assert that
reading a species spec compiles nothing. A slot that is read before it is
initialized still fills itself — a missing preload must not blank the creature —
but reports it once, the same way a missing preloaded rigged mesh does.

**Issue 22** turned out to be mostly satisfied already: `horse_topology()` and
`elephant_topology()` both take their bone table from the compiled asset, so
there is one authoritative parent table. What was missing was the check. The BPAT
already stores its bone parents, so `bone_parents_match()` compares them against
the runtime topology during asset initialization and reports a mismatch loudly —
no format change needed.

**Issue 23** classified the procedural anatomy rather than moving it. Both
species' procedural pose chains — `make_*_spec_pose`, `evaluate_*_skeleton`,
`compute_*_bone_palette` — have no callers anywhere in `render/`, `game/`, `app/`
or `tools/`; only their own unit tests reach them, and both bind palettes forward
to the compiled asset. `tests/architecture/creature_procedural_pose_test.cpp`
fails if any of that acquires a production caller. The physical move under a
`fallback/` directory is the remaining half; the code is authored art with a
working test suite, so it was not deleted.

**Issue 24** removed the last lazily-baked visual specs in the codebase. Both
quadruped bases carried the same `m_visual_spec_baked` / `m_visual_spec_cache`
pattern the humanoid base had; they now take an immutable spec at construction.
No renderer of any species lazily bakes anything, and
`CreatureBakeBoundary.NoRendererLazilyBakesItsVisualSpec` keeps it that way.

**Cross-species coupling.** `mount_model_scale()` lived in `horse_motion.h`,
which is why elephant preparation included a horse header. It is now
`Render::Creature::Quadruped::mount_model_scale()`, and `render/elephant/`
references `render/horse/` in exactly one place: a comment.

**Issue 13** left one species-shaped step in the shared path: `emit_body`, which
carries the species' own variant type into `add_quadruped()`. Everything around
it — graph inputs, base graph output, spec and seed stamping, world position,
camera distance, shadow inputs, shadow emission — is now
`build_quadruped_body()` and `add_quadruped_shadow()` in
`render/creature/quadruped/quadruped_prepare.*`.

**Issue 19** installs the quadruped runtime context for the render thread in
`Renderer::initialize()`. The humanoid equivalent instead threads its context
through `DrawContext`, because humanoid preparation runs on worker threads; the
quadruped stats are only touched from the render thread, so the scoped install is
enough. If quadruped preparation is ever parallelised, it needs the DrawContext
treatment too.

**A clip-selection finding.** `horse_clip_for_motion()` picks among the six baked
horse clips (idle/walk/trot/canter/gallop/fight) but is only ever used to look up
a grounding contact height. The submitted request carries an animation _state_,
and the archetype maps that state to a clip — so trot, canter and gallop all draw
whatever `Run` maps to, and the separately baked trot/canter/gallop BPAT clips
are never played. That matches the intended walk-vs-run behaviour, so nothing was
changed; the golden pins it either way.

### The parity harness

`tests/render/creature/quadruped_golden_parity_test.cpp` with its recorded values
in `tests/render/creature/golden/quadruped_pipeline_golden.txt`. Sixteen fixtures
— eight horse, two mounted, six elephant — pinning per prepared request and per
submitted draw: archetype, creature asset, LOD, pass, animation state, clip id
and variant, seed, role colours, base colour, wear, world transform, mesh
topology and skinning hashes, and every bone matrix. Plus, per case, the resolved
gait/motion descriptor and the elephant howdah socket. Regenerate with
`SOI_WRITE_QUADRUPED_GOLDEN=1 ./build/bin/render_tests --gtest_filter=QuadrupedGoldenParity.*`
from the repo root and read the diff.

---

---

# P0 — Protect Current Functionality

## Issue 1 — Extend Existing Horse/Elephant Tests Into Refactor Parity Tests

### Problem

The repo already has unusually good production-asset verification:

```text
horse_model_tests
elephant_model_tests
mounted_locomotion_matrix
elephant_locomotion_matrix
```

and checks topology, skin weights, deformation, locomotion samples and animated rider/howdah attachment invariants.

That should become the safety net for the cleanup rather than creating a new testing system.

### Change

Add explicit old-vs-new runtime snapshots for:

### Horse

```text
idle
walk
trot
canter
gallop
fight
death
mounted walk
mounted gallop
```

### Elephant

```text
idle
walk
fast walk
fight
death
howdah occupied
```

Capture:

```cpp
struct QuadrupedGoldenFrame {
    AssetKey asset;
    AnimationStateId animation;
    float phase;

    std::vector<QMatrix4x4> bone_palette;

    QMatrix4x4 world;
    AttachmentFrame mount_or_howdah;

    CreatureLOD lod;
    uint32_t draw_count;
};
```

### Acceptance Criteria

Before and after every cleanup:

- identical compiled asset IDs;
- identical clip selection;
- bone matrices within tolerance;
- same horse rider socket;
- same elephant howdah socket;
- same grounding;
- same LOD;
- same submitted geometry/materials.

No visual redesign during this work.

---

# P0 — Split Bake Metadata From Runtime Metadata

## Issue 2 — Split `SpeciesManifest` Into `CreatureBakeRecipe` and `CreatureRuntimeManifest`

### Problem

The horse and elephant manifests currently mix things needed only while producing assets with things needed while rendering.

For example, `horse_manifest.cpp` contains `BakeClipDescriptor` entries and a `bake_horse_manifest_clip_palettes()` callback that maps walk/trot to `"Walk"`, canter/gallop to `"Gallop"`, combat to `"Attack_Kick"`, and death to `"Death"`.

The elephant manifest does the same kind of bake mapping for walk/run/fight/death.

That is not runtime renderer configuration.

### Change

Create:

```cpp
struct CreatureBakeRecipe {
    SpeciesId species;

    std::span<const BakeClipDescriptor> clips;
    SourceAssetId source;
    BakePaletteFn bake_palette;

    GeometryFixupFn geometry_fixup;
};
```

And separately:

```cpp
struct CreatureRuntimeManifest {
    SpeciesId species;

    CompiledMeshAssetId mesh;
    BpatAssetId animation;

    SkeletonSchemaId skeleton;
    CreatureSpecId spec;
    AttachmentSchemaId attachments;
};
```

Runtime links only:

```text
CreatureRuntimeManifest
```

The baker/tool links:

```text
CreatureBakeRecipe
+
CreatureRuntimeManifest/schema
```

### Acceptance Criteria

The game executable does not need:

```text
BakeClipDescriptor
source animation sampler
bake_*_manifest_clip_palettes()
procedural bake geometry functions
```

to draw a horse or elephant.

---

# P0 — Remove Procedural Geometry From Runtime-Facing Manifests

## Issue 3 — Move Horse Geometry Construction Out of `horse_manifest.cpp`

### Problem

`horse_manifest.cpp` is currently about **1,247 lines** and contains large amounts of geometry-generation code.

It even constructs runtime `GL::Mesh` objects such as `horse_hoof_mesh()` using hard-coded vertices and indices and static `unique_ptr<Mesh>` storage.

Later it constructs a full set of procedural leg overlays:

```cpp
const auto k_horse_full_leg_overlays =
    build_horse_full_leg_overlays();
```

immediately before the actual animation-bake code.

But the documented production horse is an authored `.cmesh`; production geometry should not need render-manifest procedural construction.

### Change

Move all functions that create:

```text
torso meshes
leg span meshes
hoof meshes
procedural full-leg overlays
other deterministic horse geometry
```

to either:

```text
tools/creature_baker/horse/
```

if still required for production generation,

or:

```text
tools/mesh_preview/legacy/
```

if they are only retained for previews/debugging.

If repository-wide references confirm `k_horse_full_leg_overlays` is no longer consumed by the production manifest, delete the entire overlay branch after parity tests.

### Target `horse_manifest.cpp`

It should become approximately:

```cpp
const HorseBakeRecipe& horse_bake_recipe();
const HorseRuntimeManifest& horse_runtime_manifest();
```

not a geometry library.

---

## Issue 4 — Remove Legacy Procedural Elephant Geometry From `elephant_manifest.cpp`

### Problem

The elephant manifest has the same issue.

`build_elephant_whole_nodes()` procedurally constructs an elephant body and stores it in:

```cpp
const auto k_elephant_whole_nodes =
    build_elephant_whole_nodes();
```

even though the actual production elephant is an authored `.cmesh`.

It also builds another procedural set:

```cpp
const auto k_elephant_full_leg_overlays =
    build_elephant_full_leg_overlays();
```

in the same file as the BPAT bake callback.

### Change

Determine which procedural pieces are still reachable from production.

For each:

```text
used by production compiler
    → move to tools/creature_baker/elephant/

used only by preview/tests
    → move to tools/mesh_preview/legacy/

unused
    → delete
```

### Acceptance Criteria

`render/elephant/elephant_manifest.cpp` contains no `GL::Mesh` construction.

Runtime elephant code never synthesizes body/leg geometry.

---

# P0 — Move Deterministic Production Fixups Into the Compiler

## Issue 5 — Bake Elephant Eye Mirroring Into `elephant.cmesh`

### Problem

The elephant manifest reads the production mesh nodes, searches specifically for:

```text
"elephant.production.eyes"
```

then transforms the vertices through the bind matrix, mirrors them in X, converts them back into local space, and produces another mesh node.

That is a deterministic asset correction.

It should not happen when render-side C++ static data is constructed.

### Change

Move the mirroring operation into the creature compiler:

```text
source geometry
    ↓
production transform
    ↓
mirror missing eye
    ↓
validate
    ↓
write elephant.cmesh
```

The generated package should already contain both eyes.

### Acceptance Criteria

Runtime `elephant_source_mesh_nodes()` returns final production geometry directly.

There is no:

```cpp
build_elephant_production_nodes()
```

in render code.

Topology and shape tests remain identical.

---

# P0 — Stop Rendering From Being a Prewarm Mechanism

## Issue 6 — Replace Horse/Elephant `template_prewarm` Rendering With Asset Preloading

### Problem

Horse rendering currently does:

```cpp
ctx.template_prewarm
    ? make_runtime_prewarm_ctx(ctx)
    : ctx;
```

and can force Minimal LOD during that fake render.

Elephant rendering uses the same fake runtime-prewarm mechanism.

This means:

```text
asset preparation
    ↓
pretend to render animal
    ↓
normal runtime prepare code
    ↓
populate whatever caches happen to be touched
```

That is exactly the bake/runtime boundary problem being removed from humanoids.

### Change

Create one generic API:

```cpp
class CreatureAssetPrewarmer {
public:
    void prewarm(const CreatureAssetKey&);
};
```

Examples:

```cpp
prewarm({
    .species = SpeciesId::Horse,
    .lod = CreatureLOD::Full,
    .equipment = mounted_equipment
});

prewarm({
    .species = SpeciesId::Elephant,
    .lod = CreatureLOD::Full,
    .equipment = howdah_variant
});
```

### Acceptance Criteria

- Horse render never checks `template_prewarm`.
- Elephant render never checks `template_prewarm`.
- Prewarming requires no fake entity.
- Prewarming requires no animation input.
- Prewarming requires no camera.
- Same assets are warm before battle begins.

---

# P1 — Separate Source/Baker API From Runtime Asset API

## Issue 7 — Remove Source Animation Sampling From Runtime-Facing Species Headers

### Problem

`horse_source_asset.h` exposes all of these together:

```cpp
horse_source_mesh_nodes()
horse_source_bind_palette()
horse_source_bone_defs()
horse_source_sample_clip()
horse_source_pose_mount_frame()
horse_source_asset_status()
```

The elephant header exposes essentially the same API, including:

```cpp
elephant_source_sample_clip()
elephant_source_pose_howdah()
```

Some of that is needed by the **baker**.

Runtime should normally consume already compiled `.cmesh` + BPAT.

### Change

Split into:

```text
creature/assets/compiled_creature_asset.h
```

Runtime-safe:

```cpp
mesh()
bind_palette()
skeleton()
runtime_metadata()
status()
```

And:

```text
tools/creature_baker/source_animation_sampler.h
```

Baker-only:

```cpp
sample_source_clip()
build_bpat_clip()
derive_rest_landmarks()
```

### Acceptance Criteria

Runtime horse/elephant modules cannot call:

```text
*_source_sample_clip()
```

because those functions are not linked into the runtime layer.

---

## Issue 8 — Deduplicate `HorseSourceAssetStatus` and `ElephantSourceAssetStatus`

### Problem

Horse defines:

```cpp
struct HorseSourceAssetStatus {
    bool loaded;
    size_t vertex_count;
    size_t triangle_count;
    size_t clip_count;
    string error;
};
```

Elephant defines essentially the same structure.

There is no species-specific information in either type.

### Change

Create:

```cpp
struct CompiledCreatureAssetStatus {
    bool loaded{false};
    std::size_t vertex_count{0};
    std::size_t triangle_count{0};
    std::size_t joint_count{0};
    std::size_t clip_count{0};
    std::string error;
};
```

Both species return it.

### Acceptance Criteria

Delete:

```text
HorseSourceAssetStatus
ElephantSourceAssetStatus
```

and use one generic definition.

---

# P1 — Clean Up the Shared Compiled-Asset Loader

## Issue 9 — Remove Elephant Asset Ownership From `Render::Horse`

### Problem

`compiled_creature_assets.cpp` currently exposes elephant data through things living under horse implementation state.

For example:

```cpp
return Render::Horse::elephant_asset().mesh_nodes;
```

and elephant clip sampling calls:

```cpp
Render::Horse::sample_elephant_locomotion(...)
Render::Horse::sample_source_clip(...)
Render::Horse::k_elephant_config
```

That is a strong sign that the original horse loader became a generic creature loader without being structurally extracted.

### Change

Create:

```text
render/creature/assets/
    compiled_creature_package.*
    compiled_creature_loader.*
    source_animation_sampler.*   // baker side if possible
```

Generic representation:

```cpp
class CompiledCreaturePackage {
public:
    MeshData mesh;
    std::vector<BoneDef> bones;
    std::vector<QMatrix4x4> bind_palette;
    std::vector<AnimationClip> source_clips;
};
```

Species wrappers become thin:

```cpp
horse_asset()
elephant_asset()
```

owned by their own namespaces but using the same loader.

### Acceptance Criteria

No elephant implementation refers to:

```cpp
Render::Horse::
```

except code genuinely interacting with an actual horse.

---

# P1 — Bake Elephant Locomotion Instead of Keeping Its Generator in Runtime Asset Code

## Issue 10 — Move Procedural Elephant Walk Generation Into BPAT Baking

### Problem

The source-asset implementation has special elephant locomotion logic.

For `"Walk"` and `"Run"`, it does not simply sample an authored source animation. It:

- starts from bind palette;
- rotates four upper-leg subtrees;
- rotates lower-leg subtrees;
- adds sinusoidal body bob;
- adds body sway;
- animates another subtree;
- returns the resulting palette.

The important point is **where this belongs**.

This algorithm produces deterministic animation samples. Runtime already has BPAT precisely so those samples can be generated once.

### Change

Move:

```cpp
sample_elephant_locomotion()
rotate_elephant_subtree()
```

into:

```text
tools/creature_baker/elephant/elephant_locomotion_bake.cpp
```

Then:

```text
procedural locomotion generator
       ↓ during build
BPAT walk/run frames
       ↓ runtime
plain BPAT playback
```

### Preserve

Do not change:

- current stride angles;
- knee angles;
- leg phase offsets;
- bob;
- sway;
- trunk/body movement.

This issue is relocation/precomputation only.

### Acceptance Criteria

The resulting BPAT matrices match the old generated samples within tolerance at all current validation phases.

Runtime does not contain `sample_elephant_locomotion()`.

---

# P1 — Unify Attachment Handling

## Issue 11 — Replace Separate Horse Mount and Elephant Howdah Pose Samplers With Generic Animated Sockets

### Problem

Horse attachment animation currently samples the whole source pose, computes deltas for its back/head bones, then manually transforms saddle, stirrups, reins and bridle points.

Elephant does nearly the same idea for its howdah:

1. sample whole pose;
2. retrieve bind palette;
3. take a back-bone transform;
4. apply delta to howdah center and seat axes.

This duplicates functionality that a skeletal socket system should provide.

### Change

Bake immutable socket definitions:

### Horse

```text
MountSeat      → SourceBack
SaddleCenter   → SourceBack
StirrupLeft    → SourceBack
StirrupRight   → SourceBack
ReinBitLeft    → SourceHead
ReinBitRight   → SourceHead
Bridle         → SourceHead
```

### Elephant

```text
HowdahCenter   → Back
HowdahSeat     → Back
```

Runtime already has the current bone palette.

Resolve:

```cpp
QMatrix4x4 socket_world =
    instance_world
    * current_bone_palette[socket.bone]
    * socket.local_bind_transform;
```

No source animation should be resampled just to locate equipment.

### Acceptance Criteria

Existing rider/howdah socket tests pass without tolerance changes.

Delete:

```text
horse_source_pose_mount_frame()
elephant_source_pose_howdah()
```

from the runtime API.

---

## Issue 12 — Remove Hardcoded Elephant Back-Bone Index

### Problem

Current elephant howdah resolution explicitly uses:

```cpp
constexpr std::size_t k_back_bone = 1U;
```

before calculating the pose/bind delta.

That silently assumes the compiled skeleton maintains a particular numerical bone ordering.

### Change

Use a semantic schema value:

```cpp
enum class ElephantBone {
    Root,
    Back,
    ...
};
```

or better:

```cpp
SocketSemantic::MountBack
```

The baked asset resolves that semantic to its actual bone index.

### Acceptance Criteria

Changing source joint ordering while preserving semantic mapping does not break the howdah.

---

# P1 — Share Horse/Elephant Runtime Preparation

## Issue 13 — Extract Common `prepare_quadruped_render()`

### Problem

The horse runtime preparation performs:

```text
resolve motion
build DrawContext
resolve world/grounding
build CreatureGraphInputs
build base graph
set visual spec
set seed
create quadruped body state
calculate world position
calculate camera distance
prepare quadruped shadow
submit
```

The elephant path follows essentially the same generic creature-preparation/submission architecture and uses the same generic quadruped render stats/shadow machinery.

The species-specific parts should not require two almost-complete render-preparation pipelines.

### Change

Introduce:

```cpp
struct QuadrupedRuntimeInput {
    const DrawContext& ctx;
    const UnitVisualSpec& spec;

    CreatureKind kind;
    CreatureLOD lod;

    AnimationStateId animation;
    float phase;

    QMatrix4x4 world;
    uint32_t seed;

    bool standing_idle;
    float surface_world_y;
};
```

Then:

```cpp
prepare_quadruped_render(
    const QuadrupedRuntimeInput&,
    QuadrupedPreparation&);
```

Species adapters do only:

### Horse

```text
evaluate HorseMotionSample
choose horse clip/state
resolve horse grounding
resolve saddle sockets
```

### Elephant

```text
evaluate ElephantMotionSample
choose elephant clip/state
resolve elephant ground offset
resolve howdah socket
```

Everything after that is generic.

### Acceptance Criteria

Horse and elephant no longer duplicate:

- graph creation;
- basic body-state creation;
- camera-distance lookup;
- shadow input construction;
- generic body submission.

---

# P1 — Fix Horse-Specific LOD Naming Leaking Into Elephant

## Issue 14 — Replace `HorseLOD` With `CreatureLOD` Everywhere Outside Compatibility Boundaries

### Problem

The elephant renderer currently accepts:

```cpp
HorseLOD lod
```

and checks:

```cpp
render_ctx.force_horse_lod
render_ctx.forced_horse_lod
```

It switches over `HorseLOD::Full`, `Minimal`, and `Billboard`.

An elephant having a “horse LOD” is clear evidence that a horse-specific abstraction became generic without being renamed.

### Change

Canonical type:

```cpp
enum class CreatureLOD {
    Full,
    Minimal,
    Billboard
};
```

Canonical debug override:

```cpp
struct CreatureLodOverride {
    bool enabled;
    CreatureLOD value;
};
```

In `DrawContext`:

```cpp
std::optional<CreatureLOD> forced_creature_lod;
```

Remove:

```text
HorseLOD
force_horse_lod
forced_horse_lod
```

once call sites are migrated.

### Acceptance Criteria

There is no occurrence of `HorseLOD` in elephant code.

---

# P1 — Remove Hidden Runtime Geometry Compilation

## Issue 15 — Stop `horse_spec.cpp` From Compiling Mesh LODs on First Use

### Problem

`horse_spec.cpp` currently defines:

```cpp
static_minimal_parts()
static_full_parts()
```

and each calls:

```cpp
compile_whole_mesh_lod(horse_manifest().lod_...)
```

inside function-static initialization.

This is once-per-process rather than once-per-frame, but it is still asset compilation hidden inside runtime species configuration.

### Change

Either:

### Best

Serialize the already compiled part graph into the creature asset metadata.

or at minimum:

### Acceptable intermediate

Build it explicitly during asset initialization:

```cpp
HorseAssetRuntime initialize_horse_asset(...);
```

Then `horse_creature_spec()` references immutable loaded data.

### Acceptance Criteria

Calling:

```cpp
horse_creature_spec()
```

does not compile geometry.

Apply the same rule to elephant or future species anywhere this pattern exists.

---

# P1 — Clarify Animal Asset Key Versus Runtime State

## Issue 16 — Introduce Generic `QuadrupedAssetKey` and `QuadrupedInstanceState`

### Problem

Current code passes profiles, animation state, LOD, shared motion, mount/howdah information and renderer state through species-specific APIs.

That makes it difficult to tell which values can legitimately change the cached mesh.

### Change

Use:

```cpp
struct QuadrupedAssetKey {
    SpeciesId species;
    CreatureLOD geometry_lod;
    GeometryVariantId body_variant;
    EquipmentGeometrySetId equipment;
};
```

And:

```cpp
struct QuadrupedInstanceState {
    QMatrix4x4 world;

    AnimationStateId animation;
    float phase;

    MaterialVariantId material;
    uint32_t visual_seed;

    bool selected;
    bool hit_reacting;
};
```

Species extension:

```cpp
struct HorseRuntimeState {
    HorseGait gait;
    MountedAttachmentFrame mount;
};

struct ElephantRuntimeState {
    ElephantGait gait;
    HowdahAttachmentFrame howdah;
    float trunk_swing;
    float ear_flap;
};
```

### Rule

These must **not** alter asset identity:

```text
animation phase
movement speed
current gait
rider position
howdah current transform
team tint
hit state
selection
world position
```

---

# P2 — Clean Species Runtime State

## Issue 17 — Split `elephant/attachment_frames.h` Into Three Concerns

### Problem

`attachment_frames.h` currently contains all of these:

### Attachment representation

```text
ElephantAttachmentFrame
ElephantBodyFrames
HowdahAttachmentFrame
```

### Current animation sample

```text
ElephantMotionSample
```

### Persistent procedural gait state

```text
LegIndex
ElephantLegState
ElephantGaitState
ElephantLegPose
```

Those are three different lifetimes.

### Change

Split:

```text
elephant/schema/attachment_schema.h
    socket semantics
    rest attachment definitions

elephant/runtime/motion_sample.h
    ElephantMotionSample
    current howdah transform

elephant/runtime/gait_state.h
    ElephantLegState
    ElephantGaitState
    ElephantLegPose
```

### Important

`HowdahAttachmentFrame` itself remains dynamic.

The **rest socket definition** is baked.

The current world/pose-adjusted howdah transform is runtime.

### Acceptance Criteria

Asset/bake code never includes `ElephantGaitState`.

Runtime gait code does not own immutable attachment schema.

---

## Issue 18 — Make Horse Attachment Data Follow the Same Socket Model

### Problem

The horse has several live attachment concepts—seat, saddle, stirrups, reins, bridle—and the repository already centralizes mounted equipment handles separately. The architecture documentation confirms the shared seven-slot mounted-equipment abstraction is already in place.

The next cleanup is to do the same for **attachment transforms**, not just equipment handles.

### Change

Introduce:

```cpp
struct MountedSocketSet {
    SocketId seat;
    SocketId saddle;
    SocketId stirrup_left;
    SocketId stirrup_right;
    SocketId rein_left;
    SocketId rein_right;
    SocketId bridle;
};
```

Horse runtime produces current socket transforms from its current palette.

Mounted rider code consumes those sockets.

### Acceptance Criteria

Mounted renderer classes do not need horse-specific pose-sampling functions.

Equipment selection and attachment placement remain separate concerns:

```text
MountedHorseHandles
    = WHAT equipment exists

MountedSocketSet
    = WHERE it attaches this frame
```

---

# P2 — Remove Hidden Global Runtime State

## Issue 19 — Move Horse and Elephant Render Stats Into Renderer Runtime Context

### Problem

Horse currently has:

```cpp
static HorseRenderStats s_horseRenderStats;
```

inside `prepare.cpp`.

Elephant follows the same global-stat pattern in its preparation file.

These are runtime diagnostics, not species globals.

### Change

Create generic:

```cpp
struct QuadrupedRenderStats {
    uint64_t total;
    uint64_t rendered;

    uint64_t lod_full;
    uint64_t lod_minimal;
    uint64_t skipped_lod;
};
```

Store per renderer/session:

```cpp
struct CreatureRuntimeContext {
    QuadrupedRenderStats horse;
    QuadrupedRenderStats elephant;
};
```

Or tagged by `CreatureKind`.

### Acceptance Criteria

No namespace-scope mutable stats in horse or elephant code.

Independent test/renderer contexts do not share counters.

---

# P2 — Fix Nondeterministic Horse Runtime Identity

## Issue 20 — Stop Using the Entity Pointer Address as Horse Visual Seed

### Problem

Horse preparation currently derives its default request seed from:

```cpp
reinterpret_cast<std::uintptr_t>(ctx.entity)
```

and truncates that pointer address to 32 bits.

That means visual identity depends on memory placement.

It can change between:

- runs;
- save/load;
- allocator behavior;
- platforms.

It is also conceptually wrong for an asset/runtime boundary.

### Change

Use stable data:

```cpp
uint32_t horse_visual_seed(
    EntityId entity,
    uint32_t formation_member,
    MatchSeed match_seed);
```

For example:

```cpp
hash(entity.value(), formation_member, match_seed);
```

### Acceptance Criteria

The same horse receives the same visual variant across:

```text
save/load
replay
headless/rendered runs
different allocator layouts
```

---

# P2 — Remove Unused Runtime Parameters

## Issue 21 — Remove or Actually Use `shared_mount` in Horse Preparation

### Problem

`prepare_horse_impl()` currently receives:

```cpp
const MountedAttachmentFrame* shared_mount
```

and then explicitly does:

```cpp
(void)shared_mount;
```

So the API advertises a dependency that the body-preparation path does not use.

### Change

After the socket refactor:

- remove `shared_mount` from body preparation entirely;
- have mounted-rider preparation consume resolved mount sockets separately.

Target:

```text
horse body preparation
      ↓
bone palette
      ↓
mount socket resolver
      ↓
rider preparation
```

instead of passing mount data into functions that ignore it.

### Acceptance Criteria

No unused attachment parameters in horse body preparation.

---

# P2 — Consolidate Topology Ownership

## Issue 22 — Give Each Species Exactly One Skeleton Topology Definition

### Problem

Elephant manifest code currently contains a generic quadruped topology builder:

```cpp
elephant_topology_storage()
elephant_topology_ref()
```

with a `TrunkTip` appendage setting.

At the same time the production source asset exposes its compiled bone definitions, and the runtime spec also has elephant skeleton knowledge.

This makes it too easy to have:

```text
compiled skeleton
manifest-generated topology
runtime topology
```

that happen to agree today.

### Change

The creature compiler emits:

```cpp
CompiledSkeletonSchema {
    bone_names
    parents
    semantic_bones
    sockets
    schema_hash
};
```

That becomes authoritative.

Horse and elephant runtime use that schema.

Any manually written species topology becomes a **baker validation contract**, not a second runtime skeleton.

### Acceptance Criteria

There is one authoritative parent table at runtime.

BPAT contains/checks the same skeleton-schema hash.

Wrong skeleton + BPAT combination fails loudly.

---

# P2 — Isolate Legacy Procedural Animal Systems

## Issue 23 — Move Procedural Horse/Elephant Fallback Art Into an Explicit Legacy/Tool Layer

### Problem

The current production architecture says both animals are authored skinned `.cmesh` assets.

However, horse code still contains large procedural pose/anatomy calculations. `horse_spec.cpp`, for example, calculates shoulders, knees, feet, leg bend hints, leg radius and hoof scaling procedurally before later defining the production creature spec.

The elephant manifest similarly retains complete procedural elephant construction.

Keeping these beside production runtime code obscures which system actually renders shipping creatures.

### Change

First classify every procedural function using repository-wide call analysis.

Then:

```text
required by current BPAT bake
    → tools/creature_baker/<species>/

required by mesh-preview/debug tool
    → tools/mesh_preview/procedural/

fallback asset intentionally supported
    → render/creature/fallback/

no callers
    → delete
```

### Do Not

Delete the code simply because it looks old.

Move/delete only after Issue 1 proves production parity.

### Acceptance Criteria

A developer reading:

```text
render/horse/
render/elephant/
```

can immediately identify the production runtime path without mentally separating legacy mesh generation from authored-asset rendering.

---

# P2 — Make Species Renderers Thin

## Issue 24 — Reduce `HorseRendererBase` and `ElephantRendererBase` to Species Adapters

### Target Horse Responsibility

```cpp
HorseMotionSample evaluate_horse_motion(...);

HorseRuntimeOutput resolve_horse_runtime(
    const HorseRuntimeInput&);
```

Horse-specific knowledge:

```text
gait classification
horse contact grounding
saddle/head socket semantics
horse material variation
```

### Target Elephant Responsibility

```cpp
ElephantMotionSample evaluate_elephant_motion(...);

ElephantRuntimeOutput resolve_elephant_runtime(
    const ElephantRuntimeInput&);
```

Elephant-specific knowledge:

```text
elephant gait
trunk/ear procedural secondary motion
howdah socket semantics
elephant material variation
```

### Generic Quadruped Responsibility

```text
asset lookup
BPAT clip playback
palette upload
LOD
base graph creation
common submission
camera-distance calculation
shadows
visibility
GPU resources
```

### Acceptance Criteria

Neither species renderer:

- constructs meshes;
- compiles LOD graphs;
- performs asset prewarming through fake rendering;
- loads source animation clips;
- owns global stats.

---

# Bake-Time Versus Runtime Contract

## Horse — Bake Once

```text
✓ authored horse geometry
✓ production scaling
✓ bone hierarchy
✓ skin weights
✓ bind palette
✓ static material roles
✓ static LOD geometry
✓ gameplay → source clip mapping
✓ BPAT walk/trot/canter/gallop/fight/death
✓ rest saddle socket
✓ rest seat socket
✓ rest stirrup sockets
✓ rest rein/bridle sockets
✓ deterministic geometry corrections
```

## Horse — Runtime

```text
✗ current gait
✗ gait phase
✗ current BPAT interpolation
✗ current bone palette
✗ saddle world transform
✗ rider world transform
✗ terrain grounding
✗ current animation state
✗ current material/team variation
✗ LOD decision
✗ shadow
✗ visibility
```

---

## Elephant — Bake Once

```text
✓ authored elephant geometry
✓ production scaling
✓ mirrored/fixed production geometry
✓ skeleton hierarchy
✓ skin weights
✓ bind palette
✓ static material roles
✓ static LOD geometry
✓ gameplay clip mapping
✓ generated walk/fast-walk BPAT samples
✓ fight/death BPAT
✓ rest howdah socket
✓ rest rider-seat socket
✓ trunk socket semantics
```

## Elephant — Runtime

```text
✗ current movement speed
✗ current gait state
✗ current animation phase
✗ current palette
✗ current howdah transform
✗ current rider transform
✗ current trunk/ear secondary motion
✗ terrain grounding
✗ LOD decision
✗ shadow
✗ visibility
```

---

# Recommended Shared Directory Shape

```text
render/
├── creature/
│   ├── schema/
│   │   ├── skeleton_schema.*
│   │   ├── attachment_schema.*
│   │   └── creature_runtime_manifest.*
│   │
│   ├── assets/
│   │   ├── compiled_creature_asset.*
│   │   ├── compiled_creature_loader.*
│   │   └── creature_asset_cache.*
│   │
│   ├── quadruped/
│   │   ├── runtime/
│   │   │   ├── quadruped_prepare.*
│   │   │   ├── quadruped_submission.*
│   │   │   ├── attachment_resolver.*
│   │   │   ├── shadow.*
│   │   │   └── runtime_stats.*
│   │   │
│   │   └── schema/
│   │       └── quadruped_semantics.*
│
├── horse/
│   ├── schema/
│   │   └── horse_schema.*
│   │
│   └── runtime/
│       ├── horse_motion.*
│       ├── horse_runtime.*
│       └── horse_renderer.*
│
└── elephant/
    ├── schema/
    │   └── elephant_schema.*
    │
    └── runtime/
        ├── elephant_motion.*
        ├── elephant_gait_state.*
        ├── elephant_runtime.*
        └── elephant_renderer.*

tools/
└── creature_baker/
    ├── common/
    │   ├── source_asset_loader.*
    │   └── bpat_builder.*
    │
    ├── horse/
    │   ├── horse_bake_recipe.*
    │   └── horse_geometry_tools.*
    │
    └── elephant/
        ├── elephant_bake_recipe.*
        ├── elephant_locomotion_bake.*
        └── elephant_geometry_fixups.*
```

---

# Recommended Implementation Order

```text
1. Extend existing parity tests
               ↓
2. Introduce generic runtime/bake manifests
               ↓
3. Introduce QuadrupedAssetKey
               ↓
4. Extract generic compiled-creature loader
               ↓
5. Remove elephant ownership from Render::Horse
               ↓
6. Split source animation sampling from runtime assets
               ↓
7. Bake elephant locomotion into BPAT
               ↓
8. Move elephant eye fixup into compiler
               ↓
9. Move/delete procedural manifest geometry
               ↓
10. Replace fake render prewarm
               ↓
11. Introduce generic animated sockets
               ↓
12. Remove mount/howdah source-pose samplers
               ↓
13. Extract common quadruped preparation
               ↓
14. Replace HorseLOD with CreatureLOD
               ↓
15. Remove hidden mesh compilation
               ↓
16. Remove globals / pointer seed / unused parameters
               ↓
17. Isolate remaining legacy procedural paths
```

# Desired Final Runtime Paths

## Horse

```text
Horse entity
    ↓
HorseRuntimeState
    ↓
loaded horse asset + BPAT
    ↓
select clip / evaluate phase
    ↓
bone palette
    ↓
resolve saddle/rein sockets
    ↓
resolve rider
    ↓
generic quadruped preparation
    ↓
submit
```

## Elephant

```text
Elephant entity
    ↓
ElephantRuntimeState
    ↓
loaded elephant asset + BPAT
    ↓
select clip / evaluate phase
    ↓
bone palette
    ↓
resolve howdah socket
    ↓
resolve riders
    ↓
generic quadruped preparation
    ↓
submit
```

The critical rule should be:

> **Runtime horse/elephant rendering may transform and animate already-built data, but it may never manufacture production geometry or source animation data.**

That gives you one clean generic quadruped pipeline with horse and elephant providing only the behavior that is genuinely species-specific.
