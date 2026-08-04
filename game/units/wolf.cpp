#include "wolf.h"

#include <memory>

#include "../core/component.h"
#include "../core/world.h"
#include "wildlife_unit_common.h"

namespace Game::Units {

Wolf::Wolf(Engine::Core::World& world)
    : Unit(world, TroopType::Wolf) {
}

auto Wolf::Create(Engine::Core::World& world,
                  const SpawnParams& params) -> std::unique_ptr<Wolf> {
  auto unit = std::unique_ptr<Wolf>(new Wolf(world));
  unit->init(params);
  return unit;
}

void Wolf::init(const SpawnParams& params) {
  auto* entity = m_world->create_entity();
  m_id = entity->get_id();

  auto components = setup_wildlife_unit(
      *entity, params, Game::Wildlife::Species::Wolf, TroopType::Wolf);
  m_t = components.transform;
  m_r = components.renderable;
  m_u = components.unit;
  m_mv = components.movement;
  m_atk = components.attack;
}

} // namespace Game::Units
