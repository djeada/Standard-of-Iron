#include "registry.h"
#include "archer_renderer.h"
#include "../gl/renderer.h"
#include "../../engine/core/entity.h"
#include "../../engine/core/component.h"

namespace Render::GL {

void EntityRendererRegistry::registerRenderer(const std::string& type, RenderFunc func) {
    m_map[type] = std::move(func);
}

RenderFunc EntityRendererRegistry::get(const std::string& type) const {
    auto it = m_map.find(type);
    if (it != m_map.end()) return it->second;
    return {};
}

void registerBuiltInEntityRenderers(EntityRendererRegistry& registry) {
    registerArcherRenderer(registry);
}

} // namespace Render::GL
