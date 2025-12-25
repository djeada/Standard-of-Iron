#include "home_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/home_renderer.h"
#include "nations/roman/home_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_home_renderer(EntityRendererRegistry &registry) {

  Roman::register_home_renderer(registry);
  Carthage::register_home_renderer(registry);

  registry.register_renderer(
      "home", [&registry](const DrawContext &p, ISubmitter &out) {
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
          renderer_key = "troops/carthage/home";
          break;
        case Game::Systems::NationID::RomanRepublic:
          renderer_key = "troops/roman/home";
          break;
        default:
          renderer_key = "troops/roman/home";
          break;
        }

        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(p, out);
        }
      });
}

} 
