#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

#include "../../core/component.h"
#include "../../core/entity.h"

namespace Game::Systems::RpgCombat {

struct SoldierTarget {
  Engine::Core::Entity* entity{nullptr};
  Engine::Core::EntityID entity_id{0};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  QVector3D position{0.0F, 0.0F, 0.0F};
  float yaw_degrees{0.0F};
  float body_radius{0.5F};

  [[nodiscard]] auto has_soldier_slot() const noexcept -> bool {
    return soldier_slot != Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  }
};

[[nodiscard]] auto
live_soldier_targets(Engine::Core::Entity& entity) -> std::vector<SoldierTarget>;

[[nodiscard]] auto
resolve_soldier_target(Engine::Core::Entity& entity,
                       std::uint16_t soldier_slot) -> std::optional<SoldierTarget>;

[[nodiscard]] auto resolve_damage_carrier(Engine::Core::Entity& entity,
                                          Engine::Core::EntityID opponent_id)
    -> std::optional<SoldierTarget>;

inline constexpr float k_hurt_body_min_height = 0.30F;
inline constexpr float k_hurt_body_max_height = 1.70F;
inline constexpr float k_hurt_body_chest_height = 1.05F;

[[nodiscard]] auto hurt_body_contact_point(const SoldierTarget& target,
                                           const QVector3D& from) noexcept -> QVector3D;

[[nodiscard]] auto horizontal_distance(const QVector3D& lhs,
                                       const QVector3D& rhs) noexcept -> float;

[[nodiscard]] auto target_in_melee_envelope(const Engine::Core::Entity& attacker,
                                            const SoldierTarget& target,
                                            float weapon_reach) -> bool;

} // namespace Game::Systems::RpgCombat
