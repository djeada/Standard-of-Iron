#pragma once

#include <QVector3D>

#include <cstdint>

#include "../../core/component_combat.h"
#include "../../core/entity.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "combat_action_definition.h"
#include "weapon_trace.h"

namespace Game::Systems::CombatActions {

struct AttackerFrame {
  bool valid{false};
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D right{-1.0F, 0.0F, 0.0F};
};

struct PresentedAttackerFrame {
  AttackerFrame frame{};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
};

[[nodiscard]] auto attacker_frame(Engine::Core::Entity& attacker) -> AttackerFrame;

[[nodiscard]] auto
presented_attacker_frame(Engine::Core::Entity& attacker,
                         Engine::Core::EntityID target_id) -> PresentedAttackerFrame;

[[nodiscard]] auto to_world(const AttackerFrame& frame,
                            const QVector3D& local) -> QVector3D;

[[nodiscard]] auto normalized_or(QVector3D value,
                                 const QVector3D& fallback) -> QVector3D;

[[nodiscard]] auto anchor_intent_for(const CombatActionDefinition& definition)
    -> Engine::Core::MeleeIntent;

[[nodiscard]] auto
live_intent_of(const Engine::Core::Entity& attacker,
               const CombatActionDefinition& definition) -> Engine::Core::MeleeIntent;

[[nodiscard]] auto
sample_segment_in_frame(const AttackerFrame& frame,
                        const CombatActionDefinition& definition,
                        const Engine::Core::MeleeIntent& intent,
                        WeaponTraceTimeSpan time_span) -> WeaponTraceSegment;

} // namespace Game::Systems::CombatActions
