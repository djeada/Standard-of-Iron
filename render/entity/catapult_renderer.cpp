#include "catapult_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/catapult_renderer.h"
#include "nations/roman/catapult_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_catapult_renderer(EntityRendererRegistry &registry) {

  Roman::register_catapult_renderer(registry);
  Carthage::register_catapult_renderer(registry);

  registry.register_renderer(
      "catapult", [&registry](const DrawContext &p, ISubmitter &out) {
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
          renderer_key = "troops/carthage/catapult";
          break;
        case Game::Systems::NationID::RomanRepublic:
          renderer_key = "troops/roman/catapult";
          break;
        default:
          renderer_key = "troops/roman/catapult";
          break;
        }

        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(p, out);
        }
      });
}

} // namespace Render::GL
