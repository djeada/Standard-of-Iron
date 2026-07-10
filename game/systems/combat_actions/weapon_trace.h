#pragma once

#include <QVector3D>

#include <cstdint>
#include <span>

#include "../../core/entity.h"
#include "combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::CombatActions {

struct WeaponTraceContact {
  Engine::Core::EntityID attacker_id{0};
  Engine::Core::EntityID target_id{0};
  QVector3D contact_point{0.0F, 0.0F, 0.0F};
  float distance{0.0F};
  float local_forward{0.0F};
  float local_right{0.0F};
};

struct WeaponTraceTimeSpan {
  float previous_normalized_time{0.0F};
  float current_normalized_time{0.0F};
};

enum class WeaponTraceSegmentSource : std::uint8_t {
  None = 0,
  AuthoredPose,
  BakedSocket,
};

struct WeaponTraceSegment {
  bool valid{false};
  WeaponTraceSegmentSource source{WeaponTraceSegmentSource::None};
  QVector3D previous_base{0.0F, 0.0F, 0.0F};
  QVector3D previous_tip{0.0F, 0.0F, 0.0F};
  QVector3D current_base{0.0F, 0.0F, 0.0F};
  QVector3D current_tip{0.0F, 0.0F, 0.0F};
  float radius{0.0F};
};

[[nodiscard]] auto sample_authored_weapon_trace_segment(
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    WeaponTraceTimeSpan time_span) -> WeaponTraceSegment;

[[nodiscard]] auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    WeaponTraceTimeSpan time_span,
    Engine::Core::EntityID target_hint_id = 0,
    std::span<const Engine::Core::EntityID> ignored_target_ids = {})
    -> WeaponTraceContact;

[[nodiscard]] auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id = 0,
    std::span<const Engine::Core::EntityID> ignored_target_ids = {})
    -> WeaponTraceContact;

[[nodiscard]] auto find_sword_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id = 0,
    std::span<const Engine::Core::EntityID> ignored_target_ids = {})
    -> WeaponTraceContact;

} // namespace Game::Systems::CombatActions
