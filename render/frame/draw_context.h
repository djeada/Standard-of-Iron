#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <functional>
#include <limits>
#include <string>

#include "render/frame/creature_lod.h"
#include "render/frame/pipeline/screen_metrics.h"
#include "render/frame/submission_visibility.h"
#include "render/frame/submitter.h"
#include "render/frame/world_view.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Render::Creature::Pipeline {
struct CreaturePreparationResult;
}

namespace Render {
struct EnvironmentLightingState;
}

namespace Render::Humanoid {
struct HumanoidRuntimeContext;
}

namespace Render::GL {
class DrawResources;
class Mesh;
class Texture;
class Camera;
struct AnimationInputs;
} // namespace Render::GL

namespace Render::GL {

using HumanoidLOD = ::Render::Creature::CreatureLOD;
using RendererHandle = std::uint32_t;
inline constexpr RendererHandle k_invalid_renderer_handle =
    std::numeric_limits<RendererHandle>::max();

struct FarmActivity;
struct HomeActivity;

struct DrawContext {
  const DrawResources* resources = nullptr;
  Engine::Core::Entity* entity = nullptr;
  Engine::Core::World* world = nullptr;

  Render::WorldView world_view;
  QMatrix4x4 model;
  bool selected = false;
  bool hovered = false;
  float animation_time = 0.0F;
  float distance_sq = 0.0F;
  std::string renderer_id;
  RendererHandle renderer_handle = k_invalid_renderer_handle;

  const Render::EnvironmentLightingState* environment_lighting = nullptr;
  const Camera* camera = nullptr;

  Render::Pipeline::ScreenMetrics screen_metrics{};

  const SubmissionVisibilityPolicy* submission_visibility = nullptr;
  SubmissionFogMode submission_fog_mode = SubmissionFogMode::Ignore;
  float alpha_multiplier = 1.0F;
  bool animation_throttled = false;
  const AnimationInputs* animation_override = nullptr;
  bool allow_template_cache = true;
  bool force_humanoid_lod = false;
  HumanoidLOD forced_humanoid_lod = HumanoidLOD::Full;

  bool force_quadruped_lod = false;
  Render::Creature::CreatureLOD forced_quadruped_lod{
      Render::Creature::CreatureLOD::Full};
  bool has_seed_override = false;
  uint32_t seed_override = 0;
  bool template_prewarm = false;

  bool prewarming_via_runtime_path = false;
  bool suppress_animation_state_persistence = false;
  bool force_single_soldier = false;

  int max_rendered_individuals = 0;
  std::uint16_t soldier_index = 0;
  bool skip_ground_offset = false;
  bool has_variant_override = false;
  std::uint8_t variant_override = 0;
  bool has_attack_variant_override = false;
  std::uint8_t attack_variant_override = 0;

  bool has_facial_hair_override = false;
  FacialHairStyle facial_hair_override = FacialHairStyle::None;
  bool order_markers_visible = false;

  Render::Humanoid::HumanoidRuntimeContext* humanoid_runtime = nullptr;
  FarmActivity* farm_activity = nullptr;
  HomeActivity* home_activity = nullptr;
};

[[nodiscard]] inline auto
should_persist_animation_state(const DrawContext& ctx) noexcept -> bool {
  return !ctx.template_prewarm && !ctx.suppress_animation_state_persistence;
}

using RenderFunc = std::function<void(const DrawContext&, ISubmitter& out)>;

class IParallelPreparer {
public:
  virtual ~IParallelPreparer() = default;

  virtual void ensure_prepare_components(Engine::Core::Entity& entity) const = 0;

  virtual void
  prepare(const DrawContext& ctx,
          Render::Creature::Pipeline::CreaturePreparationResult& out) const = 0;
};

} // namespace Render::GL
