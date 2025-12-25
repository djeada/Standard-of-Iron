#include "defense_tower_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/defense_tower_renderer.h"
#include "nations/roman/defense_tower_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_defense_tower_renderer(EntityRendererRegistry &registry) {

  Roman::register_defense_tower_renderer(registry);
  Carthage::register_defense_tower_renderer(registry);

  registry.register_renderer(
      "defense_tower", [&registry](const DrawContext &p, ISubmitter &out) {
        if (p.entity == nullptr) {
          return;
        }

        auto *unit = p.entity->get_component<Engine::Core::UnitComponent>();
        if (unit == nullptr) {
          return;
        }

        std::string renderer_key;
        switch (unit->nation_id) {
        case Game::Systems::NationID::Carthage:
          renderer_key = "troops/carthage/defense_tower";
          break;
        case Game::Systems::NationID::RomanRepublic:
          renderer_key = "troops/roman/defense_tower";
          break;
        default:
          renderer_key = "troops/roman/defense_tower";
          break;
        }

        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(p, out);
        }
      });
}

} 
