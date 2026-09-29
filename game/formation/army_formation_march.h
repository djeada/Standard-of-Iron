#pragma once

#include <QVector3D>

#include <optional>
#include <vector>

#include "army_formation_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Formation::March {

[[nodiscard]] auto facing_settled(const ArmyFormation& formation) -> bool;

[[nodiscard]] auto is_advancing(const ArmyFormation& formation) -> bool;

void begin(Engine::Core::World& world,
           ArmyFormation& formation,
           std::optional<float> marching_facing);

void advance_group(Engine::Core::World& world,
                   ArmyFormation& formation,
                   float delta_time);

} // namespace Game::Formation::March
