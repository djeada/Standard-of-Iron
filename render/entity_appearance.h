#pragma once

#include <QVector3D>

#include "../game/units/spawn_type.h"

namespace Engine::Core {
class Entity;
}

namespace Render {

[[nodiscard]] auto team_color(int owner_id) -> QVector3D;

[[nodiscard]] auto coat_color(Game::Units::SpawnType spawn_type) -> QVector3D;

[[nodiscard]] auto entity_color(const Engine::Core::Entity& entity) -> QVector3D;

} // namespace Render
