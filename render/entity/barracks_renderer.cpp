#include "barracks_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/barracks_renderer.h"
#include "nations/kingdom/barracks_renderer.h"
#include "nations/roman/barracks_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_barracks_renderer(EntityRendererRegistry &registry) {

  Kingdom::register_barracks_renderer(registry);
  Roman::register_barracks_renderer(registry);
  Carthage::register_barracks_renderer(registry);

  registry.register_renderer(
      "barracks", [&registry](const DrawContext &p, ISubmitter &out) {
        if (p.entity == nullptr) {
          return;
        }

        auto *unit = p.entity->getComponent<Engine::Core::UnitComponent>();
        if (unit == nullptr) {
          return;
        }

        std::string renderer_key;
        switch (unit->nation_id) {
        case Game::Systems::NationID::Carthage:
          renderer_key = "barracks_carthage";
          break;
        case Game::Systems::NationID::RomanRepublic:
          renderer_key = "barracks_roman";
          break;
        case Game::Systems::NationID::KingdomOfIron:
        default:
          renderer_key = "barracks_kingdom";
          break;
        }

        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(p, out);
        }
      });
}

} // namespace Render::GL
