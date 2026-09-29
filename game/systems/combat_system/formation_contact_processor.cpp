#include "formation_contact_processor.h"

#include "formation_contact_fronts.h"
#include "formation_presentation_publisher.h"

namespace Game::Systems::Combat {

void update_formation_contacts(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  publish_formation_contacts(*world);
  publish_formation_presentation(*world, delta_time);
}

} // namespace Game::Systems::Combat
