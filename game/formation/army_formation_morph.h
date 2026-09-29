#pragma once

#include <QVector3D>

#include <optional>

#include "army_formation_types.h"

namespace Engine::Core {
class World;
}

namespace Game::Formation::Morph {

[[nodiscard]] auto rotate_yaw(const QVector3D& local, float yaw_degrees) -> QVector3D;

[[nodiscard]] auto
point(const FormationMorph& morph, std::size_t index, float t) -> QVector3D;

[[nodiscard]] auto start(Engine::Core::World& world,
                         ArmyFormation& formation,
                         std::optional<float> marching_facing) -> bool;

void advance(ArmyFormation& formation, float delta_time);

[[nodiscard]] auto target(const ArmyFormation& formation,
                          EntityID entity) -> std::optional<QVector3D>;

[[nodiscard]] auto pace(const ArmyFormation& formation,
                        EntityID entity,
                        const QVector3D& position,
                        float full_speed) -> float;

} // namespace Game::Formation::Morph
