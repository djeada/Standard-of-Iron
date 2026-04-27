# Render Pipeline Cleanup Plan

## Goal

Move the runtime creature pipeline to the intended end state:

- body and loadout geometry are baked once and cached;
- runtime rendering chooses a prebaked pose/frame instead of driving live bone palettes for normal gameplay states;
- the gameplay-facing render contract stays high-level (`state`, `phase`, `direction`, `fight`, `world`, `lod`, loadout/archetype);
- individual limbs, sockets, bows, accessories, and other body-part coordinates do not exist in the hot runtime body-render path;
- each gameplay entity is rendered from one world transform, with mounted units handled as a single render concept instead of ad hoc parent/child body composition.

## Current Verified State

### Already achieved

- `CreatureRenderRequest` is a clean high-level contract and does not expose anatomy, sockets, or pose data.
- Static equipment/loadout geometry is merged into the baked body mesh through `StaticAttachmentSpec` and `RiggedMeshCache`.
- `SnapshotMeshCache` already supports true prebaked frame meshes for snapshot states.
- Snapshot cache keys now distinguish clip identity/variant, so future prebaked-state expansion will not alias different clips onto one mesh entry.
- Idle/dead snapshot selection is wired into `CreaturePipeline::submit_requests()`.
- Main-pass body submission is now request-centric; normal visible humanoid/horse/elephant body draws no longer depend on row submission.
- Humanoid main-pass requests now project to terrain during prepare and defer only clip-contact grounding to submit, so the visible path avoids the old prepare-time pose grounding solve.
- Mounted rider requests now carry final absolute world transforms; runtime request submission no longer composes mounted parent/child links through `parent_local_world`.

### Gaps that still remain

- Only `Idle` and `Dead` are marked snapshot states today; locomotion and combat still go through rigged draw submission with per-frame bone palettes.
- Shadow/prewarm humanoid paths still compute full per-soldier pose state every frame, and equivalent hot-path reductions still need to happen for other species/states.
- `PreparedCreatureRenderRow` still stores runtime pose data (`HumanoidPose`, `HorseSpecPose`, `ElephantSpecPose`) and keeps the old row-based body path alive.

## Detailed Work Plan

### Phase 1 - Establish guardrails and migration targets

1. Inventory every visible body-render entrypoint and classify it as:
   - request-path body submission;
   - row-path body submission;
   - post-body draw/shadow/effect work;
   - mounted parent/child composition.
2. Add temporary instrumentation so we can measure:
   - how often main-pass bodies are emitted through `submit_requests()`;
   - how often main-pass bodies still touch `submit_row_body()`;
   - how often snapshot meshes are used vs rigged palette draws.
3. Lock in migration tests before changing behavior:
   - visible bodies should flow through the request pipeline;
   - row submission should not become the main body path again;
   - snapshot states must use identity palettes;
   - mounted render requests must stay logically grouped per gameplay entity.

**Acceptance criteria**

- We can prove which runtime paths are still live.
- We have regression coverage for request-path rendering, snapshot rendering, and mounted grouping.

### Phase 2 - Make request submission the only main body path

1. Route humanoid, horse, and elephant visible body submission exclusively through `CreatureRenderBatch::requests()` and `CreaturePipeline::submit_requests()`.
2. Reduce `PreparedCreatureRenderRow` to a transitional/internal structure used only where strictly necessary:
   - shadow placement;
   - post-body special-case draws;
   - migration-only compatibility paths.
3. Remove dependence on `PreparedCreatureSubmitBatch::submit()` / `submit_row_body()` for normal main-pass body rendering.
4. Update preparation code so the output contract is:
   - body render requests;
   - optional non-body post draws.

**Primary files**

- `render/creature/pipeline/prepared_submit.*`
- `render/creature/pipeline/creature_pipeline.*`
- `render/humanoid/prepare.cpp`
- `render/horse/prepare.cpp`
- `render/elephant/prepare.cpp`

**Acceptance criteria**

- Main-pass body rendering no longer depends on row submission.
- Body rendering is request-centric across humanoids, horses, elephants, and mounted units.

### Phase 3 - Expand prebaked snapshot coverage to real gameplay states

1. Define the supported prebaked-state matrix per species:
   - humanoid: idle, hold, walk, run, attack sword, attack spear, attack bow, dead;
   - horse: idle, walk, trot, canter, gallop, attack;
   - elephant: idle, walk, run, attack, dead;
   - rider-mounted states: riding idle, riding charge, riding reining, riding bow shot.
2. Decide frame quantization policy for each state:
   - exact BPAT frame reuse;
   - reduced frame sets for distant LODs;
   - whether attack clips need denser sampling than locomotion clips.
3. Extend snapshot cache identity so it cannot alias different motion sources:
   - include clip variant and, if needed, explicit clip id in the snapshot cache key;
   - keep archetype/variant/state/frame in the key.
4. Update archetype/snapshot policy:
   - replace the current `Idle`/`Dead`-only snapshot table with the real intended coverage;
   - allow fallback to rigged draws only for states not yet migrated.
5. Prebuild or lazily bake snapshot meshes from the already baked rigged mesh source plus clip palettes.
6. Prewarm the commonly used snapshot states so gameplay does not build them on first encounter.

**Acceptance criteria**

- Normal locomotion/combat states select prebaked frame meshes instead of submitting live bone palettes.
- Snapshot cache entries are unique per real clip variant.
- Rigged palette submission becomes a temporary fallback path, not the default path.

### Phase 4 - Eliminate runtime body-pose computation from the hot path

1. Stop computing/storing full body-part pose structures for visible body rendering:
   - `HumanoidPose`
   - `HorseSpecPose`
   - `ElephantSpecPose`
2. Remove those pose structs from `PreparedCreatureRenderRow` once request submission owns the main path.
3. Move any data still needed for grounding or alignment into lightweight prebaked metadata:
   - contact height;
   - root transform corrections;
   - seat/root offsets for mounted states;
   - other clip-derived placement metadata.
4. Restrict detailed attachment/body-frame math to:
   - asset build/bake code;
   - authoring helpers;
   - tests that verify bake outputs.
5. Ensure `prepare_humanoid_instances()` and similar functions no longer solve elbow/hand/knee/foot/body-frame data for visible runtime body submission.

**Acceptance criteria**

- No per-soldier limb coordinates are generated for main body rendering.
- Runtime body rendering works from request metadata plus prebaked mesh/frame selection.
- Detailed pose math survives only where it is needed for authoring/bake validation, not every-frame rendering.

### Phase 5 - Unify mounted rendering under a single-transform model

1. Replace runtime rider-via-`parent_local_world` composition for mounted units with one of these end states:
   - **recommended:** mounted archetypes/snapshots baked and rendered as a single mounted render family;
   - **temporary fallback:** separate rider/mount draws remain internal, but the gameplay/render contract exposes only one mounted entity transform.
2. Prebake rider seating alignment per mounted clip/state instead of deriving it from runtime attachment frames.
3. Ensure mounted attack/rein/bow states preserve rider-horse alignment without rebuilding seat transforms each frame.
4. Remove mounted runtime dependence on pose-derived attachment frames in the main body path.

**Acceptance criteria**

- A mounted gameplay unit is driven by one world transform in the runtime render contract.
- Rider placement is no longer derived from per-frame seat/body-frame math in the hot path.

### Phase 6 - Remove legacy body-runtime interfaces

1. Delete or shrink fields in `PreparedCreatureRenderRow` that are no longer needed.
2. Remove dead compatibility helpers around row-based body submission.
3. Prune legacy pose/frame APIs that remain reachable only from the retired runtime body path.
4. Tighten tests so future changes cannot reintroduce:
   - anatomy data into `CreatureRenderRequest`;
   - row-path body submission as the main route;
   - mounted parent/child body composition into the gameplay-facing contract.

**Acceptance criteria**

- The live runtime body-render API surface is request-based and minimal.
- Unused row/pose plumbing is removed instead of left to rot.

## Validation Checklist

### Functional validation

- Humanoid states: idle, hold, walk, run, sword attack, spear attack, bow attack, dead.
- Horse states: idle, walk, trot/canter/gallop, attack.
- Elephant states: idle, walk, run, attack, dead.
- Mounted states: riding idle, charge, reining, mounted bow shot.
- Visual parity checks for:
   - grounding/contact with terrain;
   - role colors and loadout colors;
   - shadow placement;
   - LOD transitions;
   - mounted rider alignment.

### Contract validation

- `CreatureRenderRequest` remains free of body-part coordinates and socket/frame data.
- Main body rendering uses one request/world transform per gameplay entity.
- Runtime systems do not need direct access to bow/leg/hand/accessory coordinates for normal body drawing.

### Performance validation

- Snapshot/prebaked mesh selection dominates normal gameplay states.
- Main-pass body rendering does not build or solve full limb poses every frame.
- No first-use hitch remains after prewarm for common troop/loadout/state combinations.

## Open Design Decisions

1. **Mounted representation**
   - Prefer a true mounted archetype/snapshot family if memory growth is acceptable.
   - If not, keep rider/mount split internal only and forbid it from leaking into the gameplay-facing render contract.

2. **Snapshot memory budget**
   - Full-frame prebake for every state/variant may be too large.
   - We may need per-LOD frame reduction, selective prewarm, or state-specific sampling density.

3. **Fallback policy during migration**
   - Until all states are migrated, keep the rigged palette path available as an explicit fallback.
   - Remove it only after state coverage and visual parity are complete.

4. **Clip identity**
   - Snapshot cache keying must distinguish state plus clip variant (and possibly clip id), otherwise later snapshot expansion will be incorrect.

## Recommended Execution Order

1. Phase 1 guardrails and instrumentation.
2. Phase 2 request-path ownership of all main body submits.
3. Phase 3 snapshot key fix plus state coverage expansion.
4. Phase 4 runtime pose removal from the hot path.
5. Phase 5 mounted single-transform cleanup.
6. Phase 6 dead-code removal and contract hardening.
