#pragma once

#include "../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine::Core {
class Entity;
class World;
} 

namespace Render::GL {
class ResourceManager;
class Mesh;
class Texture;
class Backend;
class Camera;
} 

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
};

using RenderFunc = std::function<void(const DrawContext &, ISubmitter &out)>;

class EntityRendererRegistry {
public:
  void register_renderer(const std::string &type, RenderFunc func);
  auto get(const std::string &type) const -> RenderFunc;

private:
  std::unordered_map<std::string, RenderFunc> m_map;
};

void registerBuiltInEntityRenderers(EntityRendererRegistry &registry);

} 
