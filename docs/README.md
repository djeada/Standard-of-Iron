# Documentation index

Entry point to the design and tooling documents. Files stay flat in `docs/` so
existing links keep working; this page groups them by topic. Start with
[ARCHITECTURE.md](ARCHITECTURE.md) for the module map and the enforced
boundaries between `game/`, `render/`, `app/`, `ui/`, `tools/` and `tests/`.

## Architecture

- [ARCHITECTURE.md](ARCHITECTURE.md) — module layout and dependency rules
- [PATHFINDING_ARCHITECTURE.md](PATHFINDING_ARCHITECTURE.md) — how pathfinding works
- [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md) — AI decision-making
- [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) — formations and march behaviour
- [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md) — mission scripting framework
- [MISSION_STARTUP.md](MISSION_STARTUP.md) — mission startup sequence

## Gameplay systems

- [COMBAT_SYSTEM.md](COMBAT_SYSTEM.md) — combat rules
- [UNIT_BALANCE.md](UNIT_BALANCE.md) — unit statistics and balance
- [ECONOMY_GUIDANCE.md](ECONOMY_GUIDANCE.md) — economy tuning
- [FOOD_AND_FARMS.md](FOOD_AND_FARMS.md) — food, farms, settlement economy
- [marketplace.md](marketplace.md) — marketplace
- [VICTORY_SYSTEM.md](VICTORY_SYSTEM.md) — victory conditions
- [DIFFICULTY.md](DIFFICULTY.md) — difficulty presets
- [RPG_PLAYABILITY.md](RPG_PLAYABILITY.md) — direct-control commander play
- [SKIRMISH_BASES.md](SKIRMISH_BASES.md) — choosing a starting base
- [CAMPAIGN_MISSIONS.md](CAMPAIGN_MISSIONS.md) — campaign mission roster
- [IRON_SEPULCHER.md](IRON_SEPULCHER.md), [CURSED_GOLD_VEIN.md](CURSED_GOLD_VEIN.md),
  [ROCKFALL.md](ROCKFALL.md) — individual mission write-ups
- [AMBIENT_WILDLIFE.md](AMBIENT_WILDLIFE.md), [farm_activity.md](farm_activity.md),
  [home_activity.md](home_activity.md) — ambient life
- [siege_presentation.md](siege_presentation.md) — siege engine presentation

## Rendering and animation

- [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) — render pipeline
- [BAKED_MESHES.md](BAKED_MESHES.md) — build-time baked meshes
- [ANIMATION_ARCHITECTURE.md](ANIMATION_ARCHITECTURE.md) — poses and clips
- [CREATURE_BPAT_FORMAT.md](CREATURE_BPAT_FORMAT.md) — creature animation texture format
- [HORSE_MODEL_ARCHITECTURE.md](HORSE_MODEL_ARCHITECTURE.md) — horse and elephant pipeline
- [FRAME_PACING.md](FRAME_PACING.md) — battle frame pacing

## UI and accessibility

- [UI_DESIGN_SYSTEM.md](UI_DESIGN_SYSTEM.md) — Iron and Ember design system
- [TYPOGRAPHY.md](TYPOGRAPHY.md) — type system
- [ACCESSIBILITY.md](ACCESSIBILITY.md) — accessibility and input customisation
- [CAMERA_CONTROLS.md](CAMERA_CONTROLS.md) — camera controls
- [MINIMAP.md](MINIMAP.md) — minimap

## Maps and content authoring

- [MAP_OBJECT_PLACEMENT.md](MAP_OBJECT_PLACEMENT.md) — placing objects on maps
- [HILL_SHAPES.md](HILL_SHAPES.md) — hills and raised terrain

## Audio

- [AUDIO_SYSTEM.md](AUDIO_SYSTEM.md) — audio cues and mixing
- [AUDIO_LICENSES.md](AUDIO_LICENSES.md) — audio licences

## Performance and tooling

- [PERFORMANCE.md](PERFORMANCE.md) — profiling and budgets
- [BATTLEFIELD_CAPTURE.md](BATTLEFIELD_CAPTURE.md) — battlefield capture runner
- [PROMO_CAPTURE.md](PROMO_CAPTURE.md), [TRAILER.md](TRAILER.md) — promo and trailer capture

## Save and replay

- [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md) — save/load format

## Platform and release

- [CI_POLICY.md](CI_POLICY.md) — CI lanes and policy
- [MACOS_SIGNING.md](MACOS_SIGNING.md) — macOS signing and notarisation
- [WINDOWS_CODE_SIGNING.md](WINDOWS_CODE_SIGNING.md) — Windows code signing
