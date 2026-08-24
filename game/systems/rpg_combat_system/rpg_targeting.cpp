#include "rpg_targeting.h"

#include <algorithm>
#include <cmath>

#include "../formation_combat_geometry.h"

namespace Game::Systems::RpgCombat {

auto live_soldier_targets(Engine::Core::Entity& entity) -> std::vector<SoldierTarget> {
  std::vector<SoldierTarget> result;
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (transform == nullptr || unit == nullptr || unit->health <= 0) {
    return result;
  }

  auto const layout = FormationCombat::resolve_layout(entity);
  if (FormationCombat::has_formation_slots(entity)) {
    auto const anchors = FormationCombat::soldier_spatial_anchors(entity, layout);
    result.reserve(anchors.size());
    for (auto const& anchor : anchors) {
      result.push_back({
          .entity = &entity,
          .entity_id = entity.get_id(),
          .soldier_slot = anchor.slot_index,
          .position = {anchor.world_x, transform->position.y, anchor.world_z},
          .yaw_degrees = transform->rotation.y + anchor.local_yaw,
          .body_radius = layout.body_radius,
      });
    }
    return result;
  }

  result.push_back({
      .entity = &entity,
      .entity_id = entity.get_id(),
      .position = {transform->position.x, transform->position.y, transform->position.z},
      .yaw_degrees = transform->rotation.y,
      .body_radius = std::max(0.05F, layout.body_radius),
  });
  return result;
}

auto resolve_soldier_target(Engine::Core::Entity& entity, std::uint16_t soldier_slot)
    -> std::optional<SoldierTarget> {
  auto targets = live_soldier_targets(entity);
  if (targets.empty()) {
    return std::nullopt;
  }
  if (soldier_slot == Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot) {
    return targets.front();
  }
  auto const found =
      std::find_if(targets.begin(), targets.end(), [soldier_slot](auto const& target) {
        return target.soldier_slot == soldier_slot;
      });
  return found != targets.end() ? std::optional<SoldierTarget>(*found)
                                : std::optional<SoldierTarget>(targets.front());
}

auto resolve_damage_carrier(Engine::Core::Entity& entity,
                            Engine::Core::EntityID opponent_id)
    -> std::optional<SoldierTarget> {
  auto const* presentation =
      entity.get_component<Engine::Core::FormationPresentationComponent>();
  if (presentation == nullptr || opponent_id == 0U) {
    return std::nullopt;
  }
  auto const carrier = std::find_if(presentation->soldiers.begin(),
                                    presentation->soldiers.end(),
                                    [opponent_id](auto const& soldier) {
                                      return soldier.alive && soldier.damage_carrier &&
                                             soldier.opponent_id == opponent_id;
                                    });
  if (carrier == presentation->soldiers.end()) {
    return std::nullopt;
  }
  return resolve_soldier_target(entity, carrier->slot_index);
}

auto horizontal_distance(const QVector3D& lhs, const QVector3D& rhs) noexcept -> float {
  return std::hypot(rhs.x() - lhs.x(), rhs.z() - lhs.z());
}

auto target_in_melee_envelope(const Engine::Core::Entity& attacker,
                              const SoldierTarget& target,
                              float weapon_reach) -> bool {
  auto const* transform = attacker.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr || target.entity == nullptr) {
    return false;
  }
  QVector3D const origin(
      transform->position.x, transform->position.y, transform->position.z);
  return horizontal_distance(origin, target.position) <=
         std::max(0.0F, weapon_reach) + std::max(0.0F, target.body_radius);
}

} // namespace Game::Systems::RpgCombat
