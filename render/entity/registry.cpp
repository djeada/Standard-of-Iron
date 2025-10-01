#include "registry.h"
#include "archer_renderer.h"
#include "barracks_renderer.h"
#include "../gl/renderer.h"
#include "../../game/core/entity.h"
#include "../../game/core/component.h"

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
    registerBarracksRenderer(registry);
}

} // namespace Render::GL
