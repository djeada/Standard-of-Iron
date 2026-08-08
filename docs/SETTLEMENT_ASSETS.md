# Settlement and Sanctuary Assets

Reference for the temple building and the settlement world props added for
[issue #1109](https://github.com/djeada/Standard-of-Iron/issues/1109). It covers what
each asset is, where its authoring surface lives, and the constraints a future change
has to respect.

## Temple — a nation building

The temple is a full nation building, in the same class as the marketplace and the home:
it has a spawn type, a per-nation renderer, health, collision, vision, and a builder
construction card. It is **not** a world prop.

| Property                  | Value                             |
| ------------------------- | --------------------------------- |
| Spawn / building type key | `temple`                          |
| Health                    | 900                               |
| Vision range              | 18.0 (the widest of any building) |
| Collision footprint       | 3 × 3                             |
| Resource cost             | 40 wood, 90 stone, 30 gold        |
| Build time                | 18 s                              |

Two nation variants ship, resolved through the usual
`building_renderer_key(nation, "temple")` lookup:

- **Roman** (`render/entity/nations/roman/temple_renderer.cpp`) — a podium temple:
  moulded stone podium with a front stairway, fluted columns with bases and capitals
  around a cella, a painted frieze, a terracotta gable roof with cover tiles running
  down the slope and gilded antefixes, framed tympana on both gable ends, corner
  acroteria and an aquila roof standard.
- **Carthaginian** (`render/entity/nations/carthage/temple_renderer.cpp`) — a walled
  precinct: stepped podium, a recessed porch in antis with basalt columns under a
  timber-and-stone canopy, a pilastered wall carrying a painted indigo and oxblood
  register under the cornice, a basalt-capped merlon parapet, a raised inner sanctum
  crowned with a horned altar, twin gold-capped votive pillars and incense braziers
  flanking the approach.

### Constraints these two models are tuned against

Both temples are authored for the 45 degree RTS camera
(`CameraDefaults::k_default_rts_angle`), and several choices only make sense there:

- **The Roman gable is pitched steeper than a real temple** (`pediment_rise`, ~35
  degrees against a historical ~20). At 45 degrees looking down, a historical pitch
  foreshortens the pediment into a sliver behind its own roof planes, and the roof
  is the dominant surface from every yaw.
- **The tympanum is a stack of bands whose width is sampled at each band's base**, so
  the stack fills solid rather than leaving see-through slivers along the rake. Each
  band therefore overshoots the true rake by its own height; keep bands thinner than
  the roof slab's `half_thick` or the sawtooth pokes through the roof.
- **The painted field inside a tympanum must stay well inside the rake.** Sized to
  the full triangle it erases the stone frame entirely and the gable reads as a hole.
- **Carthaginian pilasters stop below the painted register** rather than running the
  full wall height. Where the register crossed them it broke into one framed
  rectangle per bay and the wall read as a row of lit windows.
- **Merlons are cut from the light stone with a basalt cap.** In the wall's own
  colour they vanish into it.
- Buildings front onto **-X**, which the scene's key light
  (`EnvironmentLightingState::primary_direction`) leaves in shadow. Judge facade
  detail with a mirrored sun, not by brightening the model.

### Damage states

`build_stateful_building_archetype_set` builds Normal, Damaged and Destroyed. Parts
tagged `k_building_state_mask_intact` survive into Damaged only; `BuildingStateMask::
Destroyed` parts appear _only_ in the ruin. Both temples carry a dedicated ruin pass
(`add_roman_temple_ruin`, `add_carthage_temple_ruin`) that adds snapped column stumps
at uneven heights, fallen drums and masonry, jagged wall crests and ash. Without it
the destroyed state is a clean podium with evenly cropped stubs and reads as a
building site. Note that the burn colour is mixed toward the stone (`ash`) — at full
`soot` strength a collapsed floor reads as a hole punched through the model.

### Previewing a change

`build_ninja-debug/bin/building_preview <out_dir>` renders every building offscreen
through `Render::Software::SoftwareRasterizer`, so it needs no display:

- `--only <substring>` limits it to matching types and switches to a four-yaw orbit at
  the game camera's elevation, lighting the -X facade views with a mirrored sun.
- `--states` renders intact / damaged / destroyed instead.

The rasterizer depth-tests per pixel and culls backfaces (`RasterSettings::depth_test`,
`backface_cull`). It is worth knowing why: with those off it falls back to centroid
sorting, and large overlapping slabs — roof planes especially — resolve in the wrong
order and punch whole faces through their neighbours. Several apparent modelling bugs
in these temples were only that. Confirm a suspected geometry fault with a debug colour
before tuning coordinates.

The temple has no unique gameplay system. Its value is the wide vision radius and a
durable, high-value settlement anchor; the production panel tooltip says exactly that,
so it should be kept in sync if a mechanic is added later.

### Where the temple is wired

- `game/units/spawn_type.h`, `game/units/building_type.h` — enum entries and
  string mapping. `SpawnType::Temple` is appended **after** `WallGate`; keep new spawn
  types appended so saved games that store the raw enum ordering stay valid.
- `game/units/temple.{h,cpp}` + `game/units/factory.cpp` — entity construction.
- `game/systems/building_collision_registry.cpp` — footprint.
- `game/systems/construction_cost_catalog.cpp` — resource cost.
- `game/systems/combat_system/structure_combat.cpp` — structure height and the
  "buildings deal no damage" attack profile.
- `app/core/production_manager.cpp` — placement preview, spawn-type mapping, build time.
- `ui/qml/ProductionPanel.qml`, `ui/qml/design/Icons.qml` — builder card, selection
  panel, glyph. There is deliberately no `temple.png`; the card falls back to the glyph.
- `tools/map_editor/*` — `ToolType::Temple`, JSON schema enum, mission structure list.
- `tools/arena/building_panel.cpp`, `tools/arena/arena_scenario_catalog.cpp` — arena
  spawning and showcase scenarios.

## World props: `abandoned_home` and `statue`

Both are `WorldProp::Type` entries, authored in a map's `world_props` array or placed
from the map editor and arena prop panels.

| Prop           | Key              | Render scale | Movement             |
| -------------- | ---------------- | ------------ | -------------------- |
| Abandoned home | `abandoned_home` | 1.90         | blocks its grid cell |
| Statue         | `statue`         | 1.05         | blocks its grid cell |

`Game::Map::is_settlement_world_prop_type()` groups the two. It drives pathfinding
blocking (`game/systems/pathfinding.cpp`) and keeps procedural scatter from growing
through them (`game/map/scatter/scatter_composition_context.h`). Note that a prop blocks
only the cell it stands on — props have no footprint concept, so a large abandoned home
does not wall off its full visual extent.

On the campaign maps neither prop is placed by hand. `scripts/generate-map-landmarks.py`
composes them into sanctuaries, wayside shrines, dead hamlets and pickets from a map's
`landmarks` intent, and keeps them out of roads and settlement rings precisely because
they block. It is also where the campaign's outlying temples come from — the ones that
stand on a ridge or at a wood's edge rather than in a town citadel. See
`scripts/RTS_MAP_DESIGN.md` for the authoring contract.

- **Abandoned home** — a roofless stone-and-daub house shell: rubble foundation, wall
  stubs at varying heights, a collapsed south wall with a lintelled doorway and a
  window opening, a surviving stepped roof over one half, charred rafters over the
  other, a hearth stack, and rubble spilling inside and out.
- **Statue** — a togate figure on a moulded pedestal with a recessed inscription panel:
  contrapposto stance, layered drapery folds, a diagonal swag across the chest, a cloak
  falling to the plinth, one arm raised in an orator's gesture, and a laurel band.

## Renderer plumbing for a new scatter prop

Adding a prop touches a fixed set of places. In order:

1. `game/map/map_definition.h` — enum entry, string mapping both ways, render scale.
2. `render/draw_queue.h` — `TerrainScatterCmd::Species` entry. Props share one
   `PropInstanceGpu` layout and one `PropBatchParams` block, so nothing new is needed
   in `render/decoration_gpu.h`.
3. `render/terrain_scene_types.h` — `ScatterSpeciesId` entry.
4. `render/ground/<prop>_renderer.{h,cpp}` + `render/sources_ground.cmake`. Derive from
   `ScatterRendererBase<PropInstanceGpu, PropBatchParams>`; only `generate_instances()`
   is prop-specific, and `submit()` is a one-line `submit_prop_common()` call.
5. `render/gl/backend/vegetation_pipeline.{h,cpp}` — a `StaticMeshBuffers m_<prop>_mesh`
   member, an entry in `all_meshes()`, `initialize_<prop>_pipeline()`, and the mesh
   itself. There is no per-prop shutdown function: `shutdown()` walks `all_meshes()`.
6. `render/gl/backend/scatter_command_executor.cpp` — one line in `resolve_prop_draw()`
   and the species in the shared prop `case` label list.
7. `render/ground/terrain_scatter_manager.{h,cpp}` — the `unique_ptr` member, one entry
   in the `m_scatter_passes` table, its `configure()` call, and the typed accessor.
   Light direction, clear, the ready check, `chunks()` and `last_sync_stats()` all walk
   the table, so they need no edit. Every scatter renderer implements `IScatterPass`
   (`render/ground/i_scatter_pass.h`), which is what makes the table possible;
   `set_light_direction()` has a no-op default for passes that have no light direction.
8. Map editor `ToolType` and JSON schema; arena `prop_panel` and viewport type mapping.
9. `tests/render/terrain_scene_proxy_test.cpp` asserts exact pass and scatter counts —
   it has to be updated whenever a scatter renderer is added.

### Why props share one type and one draw path

Every world prop is the same thing to the GPU: one static indexed mesh, drawn instanced,
with a per-instance `pos_scale` and `color_rot` vec4 pair and a per-batch light direction
and clock. Nine props used to carry nine byte-identical `<Prop>InstanceGpu` /
`<Prop>BatchParams` struct pairs, nine near-identical renderer classes, nine fields on
`TerrainScatterCmd`, and a 45-line switch in the executor that only chose between a
shader, a VAO and a uniform block. The only genuine per-prop variation left is the
shader, the mesh, the uniform block and the `u_magic_strength` value — which is what
`resolve_prop_draw()` returns, one line per prop.

Two things the shared path deliberately keeps asymmetric: `AbandonedHome` binds the
`ruins_instanced` shader and the ruins uniform block against its own mesh, and iron ore
and the magic shrine are the only props with a non-zero `magic_strength`.

Plant, pine and olive follow the same idea one level up. All three upload three vec4
slots per instance and bind the same three attribute locations, so they share
`FoliageBatchParams`, a `FoliageUniforms` block and a single wind-swayed draw path
resolved by `resolve_foliage_draw()`. Their instance structs stay separate
(`PlantInstanceGpu` vs `TreeInstanceGpu`) because the third slot means different things
— plant type versus silhouette and bark seeds — and a `static_assert` in
`decoration_gpu.h` pins the two layouts together so the shared path stays honest.

The abandoned home reuses the `ruins_instanced` shader, which is a generic weathered
stone shader. The statue has its own `statue_instanced` shader so it reads as marble
rather than picking up the ruins shader's heavy lichen and rain-streak terms; a new
shader also needs registering in `render/gl/shader.cpp`, `render/gl/shader_cache.h`,
`assets.qrc` and the shader list in `tests/render/shader_source_test.cpp`.

`statue_instanced.frag` splits the prop at `v_local_pos.y == 1.175`: below that is
pedestal stone, above it is the figure, which gets whiter albedo, more subsurface wrap
and a tighter specular. The inscription panel is shaded, not modelled — the panel region
is derived from `min(abs(x), abs(z))` (the coordinate that varies across whichever face
is being shaded) plus a height band, and the recess is sold with a mitred chamfer that
lightens the bottom edge and darkens the top. An earlier version modelled the panel frame
as proud boxes; at game distance their lit top faces aliased into a dashed line of white
dots, which is the failure mode to avoid for any thin proud detail on a prop.

Marble needs a much flatter value range than the other stone props. Albedo stays below 1
(clipping to paper white is what made the first pass look like plastic), direct light is
wrapped rather than Lambertian, and a soft highlight shoulder runs after the shadow term.

## Mesh helpers and face normals

The prop geometry helpers live in `render/gl/backend/prop_mesh_builder.{h,cpp}`. They
were split out of `vegetation_pipeline.cpp`, which had grown to ~3000 lines by mixing
three unrelated concerns: pipeline plumbing, this general-purpose geometry library, and
the per-prop `initialize_*_pipeline()` mesh authoring. The helpers are pure functions
over a vertex/index pair and depend on nothing else in the pipeline, so keeping them
file-static made them unusable and untestable from anywhere else.

They append into two named types rather than a bare
`std::vector<std::pair<QVector3D, QVector3D>>`:

- `PropMeshVerts` — interleaved (position, normal) pairs.
- `PropMeshIndices` — `uint16_t` triangle indices.

`append_oriented_box()` and `append_barrel_yaxis()` emit **inward-facing** normals on
some faces. Prop rendering runs with back-face culling disabled, so the geometry is
still drawn — it just shades almost black, which is why the ruins and magic shrine read
so dark.

Replacements with correct outward normals should be preferred for new geometry:

- `append_prop_beam(a, b, half_width, half_depth)` — an oriented box; each face normal
  is flipped to point away from the beam centre.
- `append_prop_taper(cx, y0, cz, r0, r1, height, segs)` — a capped truncated cone,
  useful for limbs, barrels, columns and sacks.
- `append_prop_slab(y0, y1, half_bottom, half_top)` — a square truncated pyramid centred
  on the origin. Its side normals are tilted by the batter, so a stack of slabs reads as
  real architectural mouldings rather than a stack of cubes.
- `append_prop_frustum(cx, y0, cz, rx0, rz0, rx1, rz1, height, segs)` — an elliptical
  truncated cone about Y. Elliptical cross sections are what let a torso be deeper across
  the shoulders than front to back.
- `append_prop_limb(a, b, r0, r1, segs)` — a capped tapered cylinder on an arbitrary
  axis, for arms, legs and cloth folds that are not axis aligned.

The existing helpers were left alone on purpose: ruins, the magic shrine, iron ore and
the dead tree were all authored against their current (dark) appearance, and correcting
the shared helpers would restyle assets outside the scope of this work. `append_box()`
and `append_vert_prism()` have always been correct.

Note that fixing the two inward-normal helpers is now a single-file change with a
visible blast radius: `grep` `prop_mesh_builder.h` to find every prop that would
restyle.

## Supply cart and weapon rack

Both were reworked in the same pass.

- The **supply cart** now carries visible freight — hooped barrels, sacks, an amphora,
  a lashed bed roll and arched tilt hoops with a ridge pole — so its silhouette reads as
  a laden wagon from the game camera, plus larger spoked wheels.
- The **weapon rack** gained a planked backboard behind the weapons, two leaning scuta
  with bosses, and a bundle of pila, so it reads as a rack rather than open scaffolding.

Both prop shaders (`supply_cart_instanced.frag`, `weapon_rack_instanced.frag`) colour by
**local position**, not by vertex colour: they carve out wheel, barrel, crate, canvas,
blade, guard, grip and bow regions from `v_local_pos`. Moving that geometry desynchronises
the colouring. The cart's wheel bands in the shader were updated to match the enlarged
wheels; the rack's weapon geometry was deliberately left where it is, and the additions
sit in regions the shader treats as wood.

## Nation emblems: one eagle, one crescent

Every Roman building carries the **same aquila**, and every Carthaginian building the
**same crescent-and-disc**. They are not per-renderer geometry: both are built once in
`render/entity/building_ornaments.h` and used everywhere.

- `Detail::add_eagle_silhouette` is the aquila itself — four swept wing feathers a side
  over a darker shadow layer, a fanned tail, breast, neck, a head turned to its right and
  a cone beak. That beak is what makes the silhouette read as a bird rather than a cross
  at the game camera, which is what the old box-and-crossbar version looked like.
- `add_roman_aquila_relief` mounts it on a wall inside a laurel wreath on an oxblood
  plaque; `add_roman_roof_standard` mounts the _same function_ on a vexillum standard.
- `Detail::add_tanit_sign` is the Punic sign — a true disc head, upturned arms and a
  stepped triangular body — used by `add_punic_tanit_relief`.
  `add_punic_horned_crown` is the roof piece: pedestal, pole, bronze disc, and a
  continuous crescent arc with horn cones.

Two rules the geometry has to keep:

- **The roof signature survives minimal LOD.** `add_eagle_silhouette` and
  `add_tanit_sign` take a `core_lod`: the relief passes `Full` (a wall carving is not
  worth drawing at distance), the roof standard passes `All`, so the core body, head,
  beak, inner wings and centre tail still draw when the fine feathers are dropped. A
  distant building must still fly its bird. `building_archetype_desc_test` enforces both
  halves of that.
- **Recorder-based renderers use the same source.** The two barracks build their
  archetypes from recorded draw commands rather than from a `BuildingArchetypeDesc`, so
  they would otherwise need a second, drifting copy of the emblem. Instead
  `render/entity/building_ornament_emit.h` replays a desc through an ordinary submitter,
  and the barracks build the shared ornament into a throwaway desc and emit it. Adding a
  feather to the eagle changes every Roman building at once, barracks included.

Walls are deliberately left bare: a timber palisade is not a civic facade, and one
emblem per segment would be both wrong and expensive.

## Arena scenarios

- `world_prop_lineup` — every authored prop side by side, now including
  `abandoned_home` and `statue`.
- `architecture_and_props_showcase` — both temples added at the end of the nation rows.
- `sanctuary_precinct_day` / `_night` / `_storm` — a paved precinct with both temples
  facing each other, a statue-lined way, derelict homes and ruins, and civilians moving
  through it. The three variants lock the environment to midday, night, and a heavy
  rainstorm so lighting and weather regressions are visible.

Arena wall groups must land on even grid cells, so a `WallSegment` group needs an odd
`count` when its origin sits on an even coordinate — `arena_scenarios_test` enforces it.
Any scenario spawning `settlement_resident` groups must also assert
`MovementAnimationObserved` for one of them.

### Where scenario dressing actually lands

`ArenaViewport::place_scenario_resource_patches` does not drop a `resource_patch` prop
exactly where it is authored. Each prop is given a ground radius
(`Game::Map::world_prop_ground_radius`, a per-type fraction of the render scale — camp
props are wide, tree trunks are narrow) and is only placed where it stands clear of:

- a building footprint plus `k_prop_building_clearance`,
- water and bridges plus a dry margin,
- a road plus a small clearance,
- every prop already placed in the same pass, plus `k_prop_gap`.

If the authored spot fails, the prop is nudged outward through three rings of candidate
positions; if none is clear it is **skipped**. That is what stops a tent growing out of
a house wall or a forest standing in a river, and it applies to every scenario, including
new ones — but a patch authored somewhere hopeless still silently loses props, so author
the dressing where it belongs and let the pass tidy the edges rather than relying on it.
