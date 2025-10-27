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
} // namespace Engine::Core

namespace Render::GL {
class ResourceManager;
class Mesh;
class Texture;
class Backend;
} // namespace Render::GL

namespace Render::GL {

struct DrawContext {
  ResourceManager *resources = nullptr;
  Engine::Core::Entity *entity = nullptr;
  Engine::Core::World *world = nullptr;
  QMatrix4x4 model;
  bool selected = false;
  bool hovered = false;
  float animationTime = 0.0F;
  class Backend *backend = nullptr;
};

using RenderFunc = std::function<void(const DrawContext &, ISubmitter &out)>;

class EntityRendererRegistry {
public:
  void registerRenderer(const std::string &type, RenderFunc func);
  auto get(const std::string &type) const -> RenderFunc;

private:
  std::unordered_map<std::string, RenderFunc> m_map;
};

void registerBuiltInEntityRenderers(EntityRendererRegistry &registry);

} // namespace Render::GL
