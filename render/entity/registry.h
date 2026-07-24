#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../creature/part_graph.h"
#include "../submission_visibility.h"
#include "../submitter.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

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
using HorseLOD = ::Render::Creature::CreatureLOD;
using RendererHandle = std::uint32_t;
inline constexpr RendererHandle k_invalid_renderer_handle =
    std::numeric_limits<RendererHandle>::max();

struct DrawContext {
  ResourceManager* resources = nullptr;
  Engine::Core::Entity* entity = nullptr;
  Engine::Core::World* world = nullptr;
  QMatrix4x4 model;
  bool selected = false;
  bool hovered = false;
  float animation_time = 0.0F;
  float distance_sq = 0.0F;
  std::string renderer_id;
  RendererHandle renderer_handle = k_invalid_renderer_handle;
  class Backend* backend = nullptr;
  const Camera* camera = nullptr;

  const SubmissionVisibilityPolicy* submission_visibility = nullptr;
  SubmissionFogMode submission_fog_mode = SubmissionFogMode::Ignore;
  float alpha_multiplier = 1.0F;
  bool animation_throttled = false;
  const AnimationInputs* animation_override = nullptr;
  bool allow_template_cache = true;
  bool force_humanoid_lod = false;
  HumanoidLOD forced_humanoid_lod = HumanoidLOD::Full;
  bool force_horse_lod = false;
  HorseLOD forced_horse_lod = HorseLOD::Full;
  bool has_seed_override = false;
  uint32_t seed_override = 0;
  bool template_prewarm = false;
  bool suppress_animation_state_persistence = false;
  bool force_single_soldier = false;

  int max_rendered_individuals = 0;
  bool skip_ground_offset = false;
  bool has_variant_override = false;
  std::uint8_t variant_override = 0;
  bool has_attack_variant_override = false;
  std::uint8_t attack_variant_override = 0;
};

[[nodiscard]] inline auto
should_persist_animation_state(const DrawContext& ctx) noexcept -> bool {
  return !ctx.template_prewarm && !ctx.suppress_animation_state_persistence;
}

using RenderFunc = std::function<void(const DrawContext&, ISubmitter& out)>;

class EntityRendererRegistry {
public:
  void register_renderer(const std::string& type, RenderFunc func);
  auto get(const std::string& type) const -> RenderFunc;
  auto get(RendererHandle handle) const -> const RenderFunc*;
  auto get_handle(const std::string& type) const -> RendererHandle;

private:
  std::unordered_map<std::string, RendererHandle> m_lookup;
  std::vector<RenderFunc> m_renderers;
};

void register_built_in_entity_renderers(EntityRendererRegistry& registry);

} // namespace Render::GL
