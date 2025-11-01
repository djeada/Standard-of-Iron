#include "registry.h"
#include "../scene_renderer.h"
#include "barracks_renderer.h"
#include "nations/carthage/archer_renderer.h"
#include "nations/carthage/swordsman_renderer.h"
#include "nations/carthage/horse_swordsman_renderer.h"
#include "nations/carthage/spearman_renderer.h"
#include "nations/kingdom/archer_renderer.h"
#include "nations/kingdom/swordsman_renderer.h"
#include "nations/kingdom/horse_swordsman_renderer.h"
#include "nations/kingdom/spearman_renderer.h"
#include "nations/roman/archer_renderer.h"
#include "nations/roman/swordsman_renderer.h"
#include "nations/roman/horse_swordsman_renderer.h"
#include "nations/roman/spearman_renderer.h"
#include <string>
#include <utility>

namespace Render::GL {

void EntityRendererRegistry::registerRenderer(const std::string &type,
                                              RenderFunc func) {
  m_map[type] = std::move(func);
}

auto EntityRendererRegistry::get(const std::string &type) const -> RenderFunc {
  auto it = m_map.find(type);
  if (it != m_map.end()) {
    return it->second;
  }
  return {};
}

void registerBuiltInEntityRenderers(EntityRendererRegistry &registry) {
  Kingdom::registerArcherRenderer(registry);
  Roman::registerArcherRenderer(registry);
  Carthage::registerArcherRenderer(registry);

  Kingdom::registerSpearmanRenderer(registry);
  Roman::registerSpearmanRenderer(registry);
  Carthage::registerSpearmanRenderer(registry);

  Kingdom::registerKnightRenderer(registry);
  Roman::registerKnightRenderer(registry);
  Carthage::registerKnightRenderer(registry);

  Kingdom::registerMountedKnightRenderer(registry);
  Roman::registerMountedKnightRenderer(registry);
  Carthage::registerMountedKnightRenderer(registry);
  registerBarracksRenderer(registry);
}

} // namespace Render::GL
