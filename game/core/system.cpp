#include "system.h"

#include "system_context.h"
#include "world.h"

namespace Engine::Core {

void System::update(World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  SystemContext context(*world, delta_time);
  run(context);
}

void System::run(SystemContext&) {
}

} // namespace Engine::Core
