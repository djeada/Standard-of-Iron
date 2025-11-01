#include "barracks_renderer.h"
#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "nations/carthage/barracks_renderer.h"
#include "nations/roman/barracks_renderer.h"
#include "registry.h"

namespace Render::GL {

void registerBarracksRenderer(EntityRendererRegistry &registry) {
  // Register nation-specific renderers first
  Roman::registerBarracksRenderer(registry);
  Carthage::registerBarracksRenderer(registry);
  
  // Register a dispatcher that routes based on nation_id
  registry.registerRenderer("barracks", [&registry](const DrawContext &p, ISubmitter &out) {
    if (p.entity == nullptr) {
      return;
    }

    auto *unit = p.entity->getComponent<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      return;
    }

    // Route to nation-specific renderer based on nation_id
    std::string renderer_key;
    switch (unit->nation_id) {
    case Game::Systems::NationID::Carthage:
      renderer_key = "barracks_carthage";
      break;
    case Game::Systems::NationID::RomanRepublic:
    case Game::Systems::NationID::KingdomOfIron:
    default:
      renderer_key = "barracks_roman";
      break;
    }

    // Delegate to the nation-specific renderer
    auto renderer = registry.get(renderer_key);
    if (renderer) {
      renderer(p, out);
    }
  });
}

} // namespace Render::GL
