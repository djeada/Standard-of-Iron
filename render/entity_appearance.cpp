#include "entity_appearance.h"

#include "../game/core/component.h"
#include "../game/core/entity.h"
#include "../game/visuals/team_colors.h"

namespace Render {

auto team_color(int owner_id) -> QVector3D {

  return Game::Visuals::team_colorForOwner(owner_id);
}

auto coat_color(Game::Units::SpawnType spawn_type) -> QVector3D {
  switch (spawn_type) {
  case Game::Units::SpawnType::Sheep:
    return {0.88F, 0.86F, 0.80F};
  case Game::Units::SpawnType::Wolf:
    return {0.47F, 0.43F, 0.39F};
  default:
    break;
  }
  return {0.8F, 0.8F, 0.8F};
}

auto entity_color(const Engine::Core::Entity& entity) -> QVector3D {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return {1.0F, 1.0F, 1.0F};
  }
  if (Game::Units::is_wildlife_spawn(unit->spawn_type)) {
    return coat_color(unit->spawn_type);
  }
  return team_color(unit->owner_id);
}

} // namespace Render
