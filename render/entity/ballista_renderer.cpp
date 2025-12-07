#include "ballista_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/ballista_renderer.h"
#include "nations/roman/ballista_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_ballista_renderer(EntityRendererRegistry &registry) {

  Roman::register_ballista_renderer(registry);
  Carthage::register_ballista_renderer(registry);

  registry.register_renderer(
      "ballista", [&registry](const DrawContext &p, ISubmitter &out) {
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
          renderer_key = "troops/carthage/ballista";
          break;
        case Game::Systems::NationID::RomanRepublic:
          renderer_key = "troops/roman/ballista";
          break;
        default:
          renderer_key = "troops/roman/ballista";
          break;
        }

        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(p, out);
        }
      });
}

} // namespace Render::GL
