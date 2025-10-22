#include "registry.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../scene_renderer.h"
#include "archer_renderer.h"
#include "barracks_renderer.h"
#include "knight_renderer.h"
#include "mounted_knight_renderer.h"
#include "spearman_renderer.h"

namespace Render::GL {

void EntityRendererRegistry::registerRenderer(const std::string &type,
                                              RenderFunc func) {
  m_map[type] = std::move(func);
}

RenderFunc EntityRendererRegistry::get(const std::string &type) const {
  auto it = m_map.find(type);
  if (it != m_map.end())
    return it->second;
  return {};
}

void registerBuiltInEntityRenderers(EntityRendererRegistry &registry) {
  registerArcherRenderer(registry);
  registerKnightRenderer(registry);
  registerMountedKnightRenderer(registry);
  registerSpearmanRenderer(registry);
  registerBarracksRenderer(registry);
}

} 
