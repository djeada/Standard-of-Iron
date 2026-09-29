#include "formation_reform_walk.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../util/planar_math.h"
#include "formation_local_frame.h"

namespace Game::Systems::Combat {
namespace {

using Soldier = Engine::Core::FormationSoldierPresentation;

constexpr float k_reform_walk_speed = 2.4F;
constexpr float k_reform_catch_up_scale = 1.35F;
constexpr float k_reform_turn_degrees = 420.0F;

} // namespace

auto walk_to_new_slot(Engine::Core::SquadReformComponent& reform,
                      const Engine::Core::TransformComponent& actor,
                      float squad_speed,
                      float delta_time,
                      const Soldier* previous,
                      Soldier& directive) -> bool {
  auto walker = std::find_if(
      reform.soldiers.begin(), reform.soldiers.end(), [&directive](auto const& entry) {
        return entry.slot_index == directive.slot_index;
      });
  if (walker == reform.soldiers.end()) {
    return false;
  }
  QVector3D const slot = local_to_world(actor, directive.local_x, directive.local_z);
  float const dx = slot.x() - walker->world_x;
  float const dz = slot.z() - walker->world_z;
  float const distance = std::hypot(dx, dz);
  float const speed =
      std::max(k_reform_walk_speed, squad_speed * k_reform_catch_up_scale);
  float const step = speed * std::max(0.0F, delta_time);
  if (distance <= step) {
    reform.soldiers.erase(walker);
    return false;
  }
  walker->world_x += dx / distance * step;
  walker->world_z += dz / distance * step;

  auto const [local_x, local_z] =
      world_to_local(actor, walker->world_x, walker->world_z);
  auto const heading =
      world_to_local(actor, actor.position.x + dx, actor.position.z + dz);
  float const travel_yaw =
      std::atan2(heading.first, heading.second) * 180.0F / std::numbers::pi_v<float>;
  directive.local_x = local_x;
  directive.local_z = local_z;
  directive.local_yaw =
      turn_yaw_toward(previous != nullptr ? previous->local_yaw : travel_yaw,
                      travel_yaw,
                      k_reform_turn_degrees * std::max(0.0F, delta_time));
  return true;
}

} // namespace Game::Systems::Combat
