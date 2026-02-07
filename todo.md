# Render Loop Optimization TODO

## Findings (Perf Report + Code Hotspots)
- Render thread hotspots are still dominated by **per-soldier CPU build**: `Render::GL::Backend::execute` is small, while `QMatrix4x4` multiplies, `QVector3D` ops, and `QQuaternion::rotationTo` (via `Render::Geom::cylinder_between`) consume most time.
- `render/humanoid/rig.cpp` does **formation offsets + jitter + per-soldier matrices + pose solve** every frame; this is the primary bottleneck.
- Repeated component lookups in `sample_anim_state` and render loops keep per-entity overhead high.
- Horses (`render/horse/rig.cpp`) and elephants (`render/elephant/rig.cpp`) are procedural and expensive; they must be cached alongside humanoids.
- The current architecture benefits most from **separating pose templates (local meshes) from placement** (formation offsets + unit transforms).

## Implemented (Current State)
- **Pose template cache**: local-space commands recorded once per `(renderer_id, owner_id, humanoid LOD, mount LOD, anim state + combat phase + frame, variant + attack_variant)` in `render/template_cache.*`.
- **Unit layout cache**: formation offsets + yaw/vertical/phase jitter computed once per unit and reused (`render/humanoid/rig.cpp`).
- **Humanoid render path** now uses cached templates + layout placement; per-soldier work is mainly placement matrix + cull/LOD checks.
- **Mounted units include horses** in cached humanoid templates; forced horse LOD supported for template builds.
- **Elephant renderer** now uses the same template cache (local-space recording + replay) and supports prewarm.
- **Map-load prewarm**: `Renderer::prewarm_unit_templates()` builds all templates for troop types across owners/nations/LODs/variants/animation keys; invoked after load in `app/core/level_orchestrator.cpp`.
- **Cache reset on map load**: `clear_humanoid_caches()` plus `TemplateCache::clear()` to avoid stale pointer reuse.

## Next Steps / Validation
1) Add cache metrics (hits/misses, template count, average commands) and verify **no runtime template builds** after prewarm.
2) Visual verification: idle/move/run/attack/construct/heal/hit states for humanoids, mounted horses, and elephants; check shadows + LOD parity.
3) Reduce placement cost further by **grouping soldiers by template key** per unit and minimizing per-soldier hash lookups.
4) Decide horse LOD policy (currently tied to humanoid LOD) and adjust if separate horse LOD is required.
5) If still slow, move hottest procedural math to POD fast paths (`Render::Geom::cylinder_between` and equipment/horse/humanoid geometries).
