#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <QMatrix4x4>
#include <QVector3D>

namespace Engine { namespace Core { class Entity; } }
namespace Render { namespace GL { class Renderer; class ResourceManager; class Mesh; class Texture; } }

namespace Render::GL {

struct DrawParams {
    Renderer* renderer = nullptr;
    ResourceManager* resources = nullptr;
    Engine::Core::Entity* entity = nullptr;
    QMatrix4x4 model;
    bool selected = false;
};

using RenderFunc = std::function<void(const DrawParams&)>;

class EntityRendererRegistry {
public:
    void registerRenderer(const std::string& type, RenderFunc func);
    RenderFunc get(const std::string& type) const;
private:
    std::unordered_map<std::string, RenderFunc> m_map;
};

// Registers built-in entity renderers (e.g., archer)
void registerBuiltInEntityRenderers(EntityRendererRegistry& registry);

} // namespace Render::GL
