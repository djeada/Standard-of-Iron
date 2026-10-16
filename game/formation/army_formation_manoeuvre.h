#pragma once

#include <QVector3D>

#include <utility>

#include "army_formation_types.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Formation::Manoeuvre {

inline constexpr float k_pressure_share = 0.25F;

inline constexpr float k_yield_seconds = 40.0F;

inline constexpr float k_wheel_trigger = 0.7F;
inline constexpr float k_wheel_seconds = 10.0F;

inline constexpr float k_give_ground_speed = 0.55F;

inline constexpr float k_lane_alarm_distance = 60.0F;
inline constexpr float k_lane_shift_seconds = 5.0F;

struct SlotManoeuvre {
  QVector3D offset;
  float facing{0.0F};
};

[[nodiscard]] auto slot_manoeuvre(const ArmyFormation& formation,
                                  const FormationSlot& slot) -> SlotManoeuvre;

void update(Engine::Core::World& world, ArmyFormation& formation, float elapsed);

void reapply(ArmyFormation& formation);

[[nodiscard]] auto
give_ground_velocity(const ArmyFormation& formation,
                     const Engine::Core::Entity& entity) -> QVector3D;

[[nodiscard]] auto holds_for_manoeuvre(const ArmyFormation& formation,
                                       EntityID entity) -> bool;

[[nodiscard]] auto lane_pitch(const ArmyFormation& formation) -> float;

} // namespace Game::Formation::Manoeuvre
