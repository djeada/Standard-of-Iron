#pragma once

#include <QVector3D>

#include <cstdint>
#include <limits>
#include <optional>

#include "game/core/component.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
class RpgCommanderAimComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

struct LockSteerInput {
  float view_yaw{0.0F};
  bool run{false};
  bool backward{false};
  bool dodge_pressed{false};
  float dt{0.0F};
};

class CommanderTargeting {
public:
  static constexpr std::uint16_t k_no_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;

  [[nodiscard]] auto locked_id() const -> Engine::Core::EntityID {
    return m_locked_target_id;
  }
  [[nodiscard]] auto locked_slot() const -> std::uint16_t {
    return m_locked_target_slot;
  }
  [[nodiscard]] auto soft_id() const -> Engine::Core::EntityID {
    return m_soft_target_id;
  }
  [[nodiscard]] auto soft_slot() const -> std::uint16_t { return m_soft_target_slot; }
  [[nodiscard]] auto primary_slot() const -> std::uint16_t {
    return m_primary_target_slot;
  }
  [[nodiscard]] auto observed_hit_confirm_sequence() const -> std::uint32_t {
    return m_observed_hit_confirm_sequence;
  }
  void clear_primary_slot() { m_primary_target_slot = k_no_slot; }

  [[nodiscard]] auto
  locked_position(Engine::Core::World& world) const -> std::optional<QVector3D>;

  auto cycle_lock(Engine::Core::World& world,
                  Engine::Core::EntityID commander_id,
                  int local_owner_id,
                  float view_yaw) -> bool;

  // Lock-on as the player drives it: one press takes the target nearest the
  // centre of the view, the next press lets it go. A lock that could only be
  // cycled left a lone enemy locked until it died or walked out of range.
  enum class LockToggle : std::uint8_t {
    Locked,
    Released,
    NoTarget
  };
  auto toggle_lock(Engine::Core::World& world,
                   Engine::Core::EntityID commander_id,
                   int local_owner_id,
                   float view_yaw) -> LockToggle;

  // Moves an existing lock to the next candidate on that side of the current
  // target (direction < 0 is left, > 0 is right). False when nothing is
  // locked or nothing stands on that side.
  auto switch_lock(Engine::Core::World& world,
                   Engine::Core::EntityID commander_id,
                   int local_owner_id,
                   int direction) -> bool;

  [[nodiscard]] auto steer_view_toward_lock(Engine::Core::World& world,
                                            Engine::Core::Entity& commander,
                                            const LockSteerInput& input) -> float;

  [[nodiscard]] auto find_primary_target(Engine::Core::World& world,
                                         Engine::Core::EntityID commander_id,
                                         int local_owner_id,
                                         float view_yaw,
                                         float extra_reach) -> Engine::Core::EntityID;

  [[nodiscard]] auto resolve_aim_candidate(Engine::Core::World& world,
                                           Engine::Core::Entity& commander,
                                           Engine::Core::EntityID commander_id,
                                           int local_owner_id,
                                           Engine::Core::RpgCommanderAimComponent* aim,
                                           float view_yaw) -> Engine::Core::EntityID;

  [[nodiscard]] auto publish_targets(Engine::Core::Entity& commander,
                                     Engine::Core::EntityID aim_candidate_id,
                                     float dt) -> std::optional<float>;

  void reset();

private:
  struct LockGeometry {
    float dx{0.0F};
    float dz{0.0F};
    float yaw_error{0.0F};
    bool visible{false};
  };

  [[nodiscard]] auto measure_lock(Engine::Core::World& world,
                                  const Engine::Core::TransformComponent& body,
                                  float view_yaw) -> std::optional<LockGeometry>;

  void drop_lock();
  void adopt_lock(Engine::Core::Entity& commander,
                  Engine::Core::EntityID target_id,
                  std::uint16_t soldier_slot);

  void release_lock();
  [[nodiscard]] auto lock_expired(const LockGeometry& geometry, float dt) -> bool;
  [[nodiscard]] auto
  spring_view(const LockGeometry& geometry, float view_yaw, float dt) -> float;

  Engine::Core::EntityID m_locked_target_id = 0;
  std::uint16_t m_locked_target_slot{k_no_slot};
  Engine::Core::EntityID m_soft_target_id = 0;
  std::uint16_t m_soft_target_slot{k_no_slot};
  std::uint16_t m_primary_target_slot{k_no_slot};
  float m_lock_lost_timer = 0.0F;
  float m_lock_spring_yaw = 0.0F;
  bool m_lock_spring_yaw_valid = false;
  float m_lock_manual_override_timer = 0.0F;

  bool m_hit_confirm_adopted = false;
  std::uint32_t m_observed_hit_confirm_sequence = 0;
};

} // namespace App::Core
