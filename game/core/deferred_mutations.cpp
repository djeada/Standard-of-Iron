#include "deferred_mutations.h"

#include "world.h"

namespace Engine::Core {

namespace Detail {

auto entity_in(World& world, EntityID entity_id) -> Entity* {
  return world.get_entity(entity_id);
}

void destroy_in(World& world, EntityID entity_id) {
  world.destroy_entity(entity_id);
}

} // namespace Detail

void DeferredMutations::destroy_entity(EntityID entity_id) {
  m_actions.emplace_back(
      [entity_id](World& world) { Detail::destroy_in(world, entity_id); });
}

void DeferredMutations::run(Action action) {
  if (action) {
    m_actions.emplace_back(std::move(action));
  }
}

void DeferredMutations::apply(World& world) {
  if (m_actions.empty()) {
    return;
  }

  m_applying.swap(m_actions);
  m_actions.clear();

  for (Action& action : m_applying) {
    action(world);
  }
  m_applying.clear();
}

} // namespace Engine::Core
