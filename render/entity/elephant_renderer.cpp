#include "elephant_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/elephant_renderer.h"
#include "registry.h"

namespace Render::GL {

void register_elephant_renderer(EntityRendererRegistry &registry) {

  Carthage::register_elephant_renderer(registry);

  registry.register_renderer(
      "elephant", [&registry](const DrawContext &p, ISubmitter &out) {
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
          renderer_key = "troops/carthage/elephant";
          break;
        case Game::Systems::NationID::RomanRepublic:
          // Romans didn't use war elephants historically, default to Carthage
          renderer_key = "troops/carthage/elephant";
          break;
        default:
          renderer_key = "troops/carthage/elephant";
          break;
        }

        auto renderer = registry.get(renderer_key);
        if (renderer) {
          renderer(p, out);
        }
      });
}

} // namespace Render::GL
