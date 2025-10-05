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
}
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
  QMatrix4x4 model;
  bool selected = false;
  bool hovered = false;
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
