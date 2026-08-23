#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "render/creature/part_graph.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/pipeline/screen_metrics.h"
#include "render/submission_visibility.h"
#include "render/submitter.h"
#include "render/world_view.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Render::Creature::Pipeline {
struct CreaturePreparationResult;
}

namespace Render::Humanoid {
struct HumanoidRuntimeContext;
}

namespace Render::GL {
class ResourceManager;
class Mesh;
class Texture;
class Backend;
class Camera;
struct AnimationInputs;
} // namespace Render::GL

namespace Render::GL {

using HumanoidLOD = ::Render::Creature::CreatureLOD;
using RendererHandle = std::uint32_t;
inline constexpr RendererHandle k_invalid_renderer_handle =
    std::numeric_limits<RendererHandle>::max();

struct DrawContext {
  ResourceManager* resources = nullptr;
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
  class Backend* backend = nullptr;
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
  bool skip_ground_offset = false;
  bool has_variant_override = false;
  std::uint8_t variant_override = 0;
  bool has_attack_variant_override = false;
  std::uint8_t attack_variant_override = 0;

  bool has_facial_hair_override = false;
  FacialHairStyle facial_hair_override = FacialHairStyle::None;
  bool order_markers_visible = false;

  Render::Humanoid::HumanoidRuntimeContext* humanoid_runtime = nullptr;
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

class EntityRendererRegistry {
public:
  struct TransparentStringHash {
    using is_transparent = void;
    auto operator()(std::string_view value) const noexcept -> std::size_t {
      return std::hash<std::string_view>{}(value);
    }
  };

  void register_renderer(const std::string& type,
                         RenderFunc func,
                         std::shared_ptr<const IParallelPreparer> preparer = {});
  auto get(std::string_view type) const -> RenderFunc;
  auto get(RendererHandle handle) const -> const RenderFunc*;
  auto get_preparer(RendererHandle handle) const -> const IParallelPreparer*;
  auto get_handle(std::string_view type) const -> RendererHandle;

private:
  std::
      unordered_map<std::string, RendererHandle, TransparentStringHash, std::equal_to<>>
          m_lookup;
  std::vector<RenderFunc> m_renderers;
  std::vector<std::shared_ptr<const IParallelPreparer>> m_preparers;
};

void register_built_in_entity_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
