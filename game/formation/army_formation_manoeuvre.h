#pragma once

#include <QVector3D>

#include <utility>

#include "army_formation_types.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Formation::Manoeuvre {

// The crescent's centre yields only while it is pressed: at least this share of
// its troops locked in melee.
inline constexpr float k_pressure_share = 0.25F;
// Full convex-to-concave travel under even odds takes about this long.
inline constexpr float k_yield_seconds = 40.0F;
// Once the centre has given this much ground the wings wheel inward.
inline constexpr float k_wheel_trigger = 0.7F;
inline constexpr float k_wheel_seconds = 10.0F;
// A pressed centre troop steps back at most this fast while it fights.
inline constexpr float k_give_ground_speed = 0.55F;

// A triplex acies opens straight lanes (principes step behind the hastati)
// when enemy elephants come this close.
inline constexpr float k_lane_alarm_distance = 60.0F;
inline constexpr float k_lane_shift_seconds = 5.0F;

// Per-slot manoeuvre: local offset added to the slot's planned place and the
// change in its facing.
struct SlotManoeuvre {
  QVector3D offset;
  float facing{0.0F};
};

[[nodiscard]] auto slot_manoeuvre(const ArmyFormation& formation,
                                  const FormationSlot& slot) -> SlotManoeuvre;

// Advances the battle-order behaviour of one formation (crescent yield and
// wheel, triplex lanes) and moves its slots accordingly.
void update(Engine::Core::World& world, ArmyFormation& formation, float elapsed);

// Re-applies the current manoeuvre to freshly planned slots (after a replan
// the slots come back at their base positions).
void reapply(ArmyFormation& formation);

// Velocity at which a fighting member of a yielding crescent centre gives
// ground this tick (zero when it should hold).
[[nodiscard]] auto
give_ground_velocity(const ArmyFormation& formation,
                     const Engine::Core::Entity& entity) -> QVector3D;

// True for a crescent wing troop that is holding its ground until the wing
// wheels: it answers blows but does not go looking for a fight.
[[nodiscard]] auto holds_for_manoeuvre(const ArmyFormation& formation,
                                       EntityID entity) -> bool;

// Lane pitch (distance between neighbouring maniple centres of one line) of a
// triplex acies, measured from its hastati; 0 when there are no lanes.
[[nodiscard]] auto lane_pitch(const ArmyFormation& formation) -> float;

} // namespace Game::Formation::Manoeuvre
