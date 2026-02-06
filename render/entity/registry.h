#pragma once

#include "../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

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

enum class HumanoidLOD : uint8_t {
  Full = 0,
  Reduced = 1,
  Minimal = 2,
  Billboard = 3
};

enum class HorseLOD : uint8_t {
  Full = 0,
  Reduced = 1,
  Minimal = 2,
  Billboard = 3
};

struct DrawContext {
  ResourceManager *resources = nullptr;
  Engine::Core::Entity *entity = nullptr;
  Engine::Core::World *world = nullptr;
  QMatrix4x4 model;
  bool selected = false;
  bool hovered = false;
  float animation_time = 0.0F;
  std::string renderer_id;
  class Backend *backend = nullptr;
  const Camera *camera = nullptr;
  float alpha_multiplier = 1.0F;
  bool animation_throttled = false;
  const AnimationInputs *animation_override = nullptr;
  bool allow_template_cache = true;
  bool force_humanoid_lod = false;
  HumanoidLOD forced_humanoid_lod = HumanoidLOD::Full;
  bool force_horse_lod = false;
  HorseLOD forced_horse_lod = HorseLOD::Full;
  bool has_seed_override = false;
  uint32_t seed_override = 0;
  bool template_prewarm = false;
  bool force_single_soldier = false;
  bool skip_ground_offset = false;
  bool has_variant_override = false;
  std::uint8_t variant_override = 0;
  bool has_attack_variant_override = false;
  std::uint8_t attack_variant_override = 0;
};

using RenderFunc = std::function<void(const DrawContext &, ISubmitter &out)>;

class EntityRendererRegistry {
public:
  void register_renderer(const std::string &type, RenderFunc func);
  auto get(const std::string &type) const -> RenderFunc;

private:
  std::unordered_map<std::string, RenderFunc> m_map;
};

void register_built_in_entity_renderers(EntityRendererRegistry &registry);

} // namespace Render::GL
