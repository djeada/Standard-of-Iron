#include "render/entity/registry.h"

#include "render/creature/humanoid/runtime/humanoid_renderer.h"

#include <string>
#include <utility>

#include "render/creature/elephant/elephant_spec.h"
#include "render/creature/horse/horse_spec.h"
#include "render/creature/wildlife/sheep_spec.h"
#include "render/creature/wildlife/wolf_spec.h"
#include "render/entity/ballista_renderer.h"
#include "render/entity/barracks_renderer.h"
#include "render/entity/catapult_renderer.h"
#include "render/entity/cursed_gold_vein_flag_renderer.h"
#include "render/entity/defense_tower_renderer.h"
#include "render/entity/elephant_renderer.h"
#include "render/entity/farm_renderer.h"
#include "render/entity/home_renderer.h"
#include "render/entity/marketplace_renderer.h"
#include "render/entity/nations/carthage/archer_renderer.h"
#include "render/entity/nations/carthage/ballista_renderer.h"
#include "render/entity/nations/carthage/builder_renderer.h"
#include "render/entity/nations/carthage/catapult_renderer.h"
#include "render/entity/nations/carthage/elephant_renderer.h"
#include "render/entity/nations/carthage/healer_renderer.h"
#include "render/entity/nations/carthage/horse_archer_renderer.h"
#include "render/entity/nations/carthage/horse_spearman_renderer.h"
#include "render/entity/nations/carthage/horse_swordsman_renderer.h"
#include "render/entity/nations/carthage/spearman_renderer.h"
#include "render/entity/nations/carthage/swordsman_renderer.h"
#include "render/entity/nations/gauls/horse_swordsman_renderer.h"
#include "render/entity/nations/gauls/swordsman_renderer.h"
#include "render/entity/nations/iberians/horse_swordsman_renderer.h"
#include "render/entity/nations/iberians/swordsman_renderer.h"
#include "render/entity/nations/roman/archer_renderer.h"
#include "render/entity/nations/roman/ballista_renderer.h"
#include "render/entity/nations/roman/builder_renderer.h"
#include "render/entity/nations/roman/catapult_renderer.h"
#include "render/entity/nations/roman/healer_renderer.h"
#include "render/entity/nations/roman/horse_archer_renderer.h"
#include "render/entity/nations/roman/horse_spearman_renderer.h"
#include "render/entity/nations/roman/horse_swordsman_renderer.h"
#include "render/entity/nations/roman/spearman_renderer.h"
#include "render/entity/nations/roman/swordsman_renderer.h"
#include "render/entity/siege_assault_renderer.h"
#include "render/entity/skirmisher_renderer.h"
#include "render/entity/temple_renderer.h"
#include "render/entity/wall_renderer.h"
#include "render/entity/wildlife/sheep_renderer.h"
#include "render/entity/wildlife/wolf_renderer.h"
#include "render/frame/frame_submitter.h"

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

  Render::Horse::initialize_horse_asset();
  Render::Elephant::initialize_elephant_asset();
  Render::Wildlife::initialize_sheep_asset();
  Render::Wildlife::initialize_wolf_asset();

  register_skirmisher_renderers(registry);
  Roman::register_archer_renderer(registry);
  Carthage::register_archer_renderer(registry);

  Roman::register_spearman_renderer(registry);
  Carthage::register_spearman_renderer(registry);

  Roman::register_swordsman_renderer(registry);
  Carthage::register_swordsman_renderer(registry);
  Gauls::register_swordsman_renderer(registry);
  Iberians::register_swordsman_renderer(registry);

  Roman::register_mounted_swordsman_renderer(registry);
  Carthage::register_mounted_swordsman_renderer(registry);
  Gauls::register_mounted_swordsman_renderer(registry);
  Iberians::register_mounted_swordsman_renderer(registry);

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
  register_siege_assault_renderers(registry);

  register_barracks_renderer(registry);
  register_cursed_gold_vein_flag_renderer(registry);

  register_defense_tower_renderer(registry);

  register_home_renderer(registry);

  register_wall_renderer(registry);

  register_marketplace_renderer(registry);
  register_temple_renderer(registry);
  register_farm_renderer(registry);

  Wildlife::register_sheep_renderer(registry);
  Wildlife::register_wolf_renderer(registry);
}

void register_humanoid_renderer(EntityRendererRegistry& registry,
                                std::string key,
                                std::shared_ptr<const HumanoidRendererBase> renderer) {
  registry.register_renderer(
      std::move(key),
      [renderer](const DrawContext& ctx, ISubmitter& out) {
        renderer->render(ctx, out);
      },
      renderer);
}

} // namespace Render::GL
