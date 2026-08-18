#include "registry.h"

#include <string>
#include <utility>

#include "ballista_renderer.h"
#include "barracks_renderer.h"
#include "catapult_renderer.h"
#include "defense_tower_renderer.h"
#include "elephant_renderer.h"
#include "farm_renderer.h"
#include "home_renderer.h"
#include "marketplace_renderer.h"
#include "nations/carthage/archer_renderer.h"
#include "nations/carthage/ballista_renderer.h"
#include "nations/carthage/builder_renderer.h"
#include "nations/carthage/catapult_renderer.h"
#include "nations/carthage/elephant_renderer.h"
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
#include "render/scene_renderer.h"
#include "temple_renderer.h"
#include "wall_renderer.h"
#include "wildlife/sheep_renderer.h"
#include "wildlife/wolf_renderer.h"

namespace Render::GL {

void EntityRendererRegistry::register_renderer(
    const std::string& type,
    RenderFunc func,
    std::shared_ptr<const IParallelPreparer> preparer) {
  auto it = m_lookup.find(type);
  if (it != m_lookup.end()) {
    m_renderers[it->second] = std::move(func);
    m_preparers[it->second] = std::move(preparer);
    return;
  }

  const auto handle = static_cast<RendererHandle>(m_renderers.size());
  m_renderers.push_back(std::move(func));
  m_preparers.push_back(std::move(preparer));
  m_lookup.emplace(type, handle);
}

auto EntityRendererRegistry::get(std::string_view type) const -> RenderFunc {
  if (const auto handle = get_handle(type); handle != k_invalid_renderer_handle) {
    return m_renderers[handle];
  }
  return {};
}

auto EntityRendererRegistry::get(RendererHandle handle) const -> const RenderFunc* {
  if (handle >= m_renderers.size()) {
    return nullptr;
  }
  return &m_renderers[handle];
}

auto EntityRendererRegistry::get_preparer(RendererHandle handle) const
    -> const IParallelPreparer* {
  return handle < m_preparers.size() ? m_preparers[handle].get() : nullptr;
}

auto EntityRendererRegistry::get_handle(std::string_view type) const -> RendererHandle {
  auto it = m_lookup.find(type);
  if (it == m_lookup.end()) {
    return k_invalid_renderer_handle;
  }
  return it->second;
}

void register_built_in_entity_renderers(EntityRendererRegistry& registry) {
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
  Roman::register_civilian_renderer(registry);
  Carthage::register_civilian_renderer(registry);

  Carthage::register_skeleton_swordsman_renderer(registry);
  Carthage::register_skeleton_archer_renderer(registry);
  Carthage::register_grave_priest_renderer(registry);

  register_catapult_renderer(registry);

  register_elephant_renderer(registry);

  register_ballista_renderer(registry);

  register_barracks_renderer(registry);

  register_defense_tower_renderer(registry);

  register_home_renderer(registry);

  register_wall_renderer(registry);

  register_marketplace_renderer(registry);
  register_temple_renderer(registry);
  register_farm_renderer(registry);

  Wildlife::register_sheep_renderer(registry);
  Wildlife::register_wolf_renderer(registry);
}

} // namespace Render::GL
