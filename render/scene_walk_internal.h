#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>

#include "game/core/component.h"
#include "game/units/spawn_type.h"
#include "render/battle_render_optimizer.h"
#include "render/entity/building_collapse.h"
#include "render/entity/registry.h"
#include "render/gl/resources.h"
#include "render/pipeline/screen_metrics.h"
#include "render/scene_renderer.h"
#include "render/submission_visibility.h"
#include "render/submitter.h"
#include "render/unit_render_cache.h"

namespace Render::GL {

struct UnitRenderEntry {
  Render::CachedUnitData* cache{nullptr};
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  Engine::Core::MotionPresentationComponent* motion{nullptr};
  std::string renderer_key;
  Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
  Engine::Core::EntityID entity_id{0};
  bool selected{false};
  bool hovered{false};
  bool combat_active{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool fog_unseen{false};
  Game::Systems::UnitActivity activity{};
  int owner_id{0};
  float indicator_height{0.0F};
  float distance_sq{0.0F};

  float view_distance_sq{0.0F};
  float cull_radius{0.0F};
  Render::GL::BuildingCollapse collapse{};
};

struct RenderEntry {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  std::string renderer_key;
  Render::GL::RendererHandle renderer_handle{Render::GL::k_invalid_renderer_handle};
  Engine::Core::EntityID entity_id{0};
  bool selected{false};
  bool hovered{false};
  float distance_sq{0.0F};
};

struct UnitSubmitContext {
  Engine::Core::World* world{nullptr};
  ResourceManager* resources{nullptr};
  ISubmitter* batch_submitter{nullptr};

  const Render::BattleRenderOptimizer::FrameSnapshot* optimizer{nullptr};

  Render::BattleRenderOptimizer::FrameStats* optimizer_stats{nullptr};
  Render::Pipeline::ScreenMetrics screen_metrics{};
  float batching_ratio{0.0F};
  float full_shader_max_distance_sq{0.0F};
  std::uint32_t optimizer_frame{0};
  int visible_unit_count{0};
  bool force_batching{false};
  bool never_batch{false};
  bool full_creature_detail{false};
  bool visibility_enabled{false};
};

struct UnitDrawPlan {
  const RenderFunc* fn{nullptr};
  const IParallelPreparer* preparer{nullptr};
  DrawContext draw_ctx{};
  bool use_batching{false};
  bool tier_is_minimal{false};
  int lod_tier{0};
};

[[nodiscard]] inline auto
non_local_fog_mode(const Engine::Core::UnitComponent& unit) -> SubmissionFogMode {
  return Game::Units::is_building_spawn(unit.spawn_type)
             ? SubmissionFogMode::Revealed
             : SubmissionFogMode::VisibleOnly;
}

} // namespace Render::GL
