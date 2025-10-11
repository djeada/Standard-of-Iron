#pragma once

#include "../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Engine {
namespace Core {
class Entity;
class World;
} // namespace Core
} // namespace Engine
namespace Render {
namespace GL {
class ResourceManager;
class Mesh;
class Texture;
} // namespace GL
} // namespace Render

namespace Render::GL {

struct DrawContext {
  ResourceManager *resources = nullptr;
  Engine::Core::Entity *entity = nullptr;
  Engine::Core::World *world = nullptr;
  QMatrix4x4 model;
  bool selected = false;
  bool hovered = false;
  float animationTime = 0.0f;
};

using RenderFunc = std::function<void(const DrawContext &, ISubmitter &out)>;

class EntityRendererRegistry {
public:
  void registerRenderer(const std::string &type, RenderFunc func);
  RenderFunc get(const std::string &type) const;

private:
  std::unordered_map<std::string, RenderFunc> m_map;
};

void registerBuiltInEntityRenderers(EntityRendererRegistry &registry);

} // namespace Render::GL
