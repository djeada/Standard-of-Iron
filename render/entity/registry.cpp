#include "registry.h"
#include "../scene_renderer.h"
#include "ballista_renderer.h"
#include "barracks_renderer.h"
#include "catapult_renderer.h"
#include "defense_tower_renderer.h"
#include "home_renderer.h"
#include "nations/carthage/archer_renderer.h"
#include "nations/carthage/ballista_renderer.h"
#include "nations/carthage/builder_renderer.h"
#include "nations/carthage/catapult_renderer.h"
#include "nations/carthage/healer_renderer.h"
#include "nations/carthage/horse_archer_renderer.h"
#include "nations/carthage/horse_spearman_renderer.h"
#include "nations/carthage/horse_swordsman_renderer.h"
#include "nations/carthage/spearman_renderer.h"
#include "nations/carthage/swordsman_renderer.h"
#include "nations/roman/archer_renderer.h"
#include "nations/roman/ballista_renderer.h"
#include "nations/roman/builder_renderer.h"
#include "nations/roman/catapult_renderer.h"
#include "nations/roman/healer_renderer.h"
#include "nations/roman/horse_archer_renderer.h"
#include "nations/roman/horse_spearman_renderer.h"
#include "nations/roman/horse_swordsman_renderer.h"
#include "nations/roman/spearman_renderer.h"
#include "nations/roman/swordsman_renderer.h"
#include <string>
#include <utility>

namespace Render::GL {

void EntityRendererRegistry::register_renderer(const std::string &type,
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
  Roman::register_archer_renderer(registry);
  Carthage::register_archer_renderer(registry);

  Roman::register_spearman_renderer(registry);
  Carthage::register_spearman_renderer(registry);

  Roman::register_knight_renderer(registry);
  Carthage::register_knight_renderer(registry);

  Roman::register_mounted_knight_renderer(registry);
  Carthage::register_mounted_knight_renderer(registry);

  Roman::register_horse_archer_renderer(registry);
  Carthage::register_horse_archer_renderer(registry);

  Roman::register_horse_spearman_renderer(registry);
  Carthage::register_horse_spearman_renderer(registry);

  Roman::register_healer_renderer(registry);
  Carthage::register_healer_renderer(registry);

  Roman::register_builder_renderer(registry);
  Carthage::register_builder_renderer(registry);

  register_catapult_renderer(registry);

  register_ballista_renderer(registry);

  register_barracks_renderer(registry);

  register_defense_tower_renderer(registry);

  register_home_renderer(registry);
}

} 
