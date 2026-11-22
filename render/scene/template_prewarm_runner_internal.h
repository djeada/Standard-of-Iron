#pragma once

// Internal to the template prewarm runner: the per-item helpers that
// template_prewarm_runner.cpp defines and template_prewarm_units.cpp calls.

#include <QDebug>
#include <QStringList>
#include <qglobal.h>
#include <qvectornd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

#include "animation/bpat/bpat_registry.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/visuals/building_asset_key.h"
#include "render/backend/backend.h"
#include "render/backend/render_backend_factory.h"
#include "render/backend/software_backend.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/assets/creature_asset_prewarmer.h"
#include "render/creature/combat_animation_diagnostics.h"
#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/elephant_renderer_base.h"
#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_renderer_base.h"
#include "render/creature/humanoid/animation/animation_inputs.h"
#include "render/creature/humanoid/asset/humanoid_asset_prewarmer.h"
#include "render/creature/humanoid/runtime/frame_control.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/runtime_stats.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/creature/pose_intent.h"
#include "render/creature/quadruped/render_stats.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/civilian_actor_prewarm.h"
#include "render/entity/mounted_humanoid_renderer_base.h"
#include "render/entity/registry.h"
#include "render/entity/selection_ring_layout.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_queue.h"
#include "render/frame/draw_resources.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/pass/frame_context.h"
#include "render/frame/pass/frame_pass_runner.h"
#include "render/frame/pipeline/lod_selector.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/submitter.h"
#include "render/frame/visibility_budget.h"
#include "render/frame/world_chunk.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/primitives.h"
#include "render/gl/buffer.h"
#include "render/profiling/asset_counters.h"
#include "render/profiling/frame_profile.h"
#include "render/scene/battle_render_optimizer.h"
#include "render/scene/construction_preview_pass.h"
#include "render/scene/primitive_flush_pass.h"
#include "render/scene/scene_renderer.h"
#include "render/scene/template_prewarm_catalog.h"
#include "render/scene/template_recorder.h"
#include "scene/camera.h"

namespace Render::GL::template_prewarm_runner_detail {

void execute_template_prewarm_item(
    Renderer& renderer,
    std::size_t profile_index,
    Game::Units::SpawnType spawn_type,
    Game::Systems::NationID nation_id,
    int max_health,
    const std::string& renderer_id,
    bool is_mounted,
    bool is_elephant,
    const RenderFunc& fn,
    int owner_id,
    HumanoidLOD lod,
    std::uint8_t variant,
    bool allow_template_cache,
    const AnimKey& anim_key,
    const FacialHairStyle* facial_hair_override = nullptr);

void prewarm_humanoid_assets_for_profile(Renderer& renderer,
                                         const EntityRendererRegistry* entity_registry,
                                         const PrewarmProfile& profile,
                                         HumanoidLOD lod,
                                         std::uint8_t variant);

auto prewarm_humanoid_body_variants(Renderer& renderer,
                                    const EntityRendererRegistry* entity_registry,
                                    const std::vector<PrewarmProfile>& profiles,
                                    bool full_lod_only) -> std::size_t;

void prewarm_creature_species(Renderer& renderer, bool full_lod_only);

} // namespace Render::GL::template_prewarm_runner_detail
