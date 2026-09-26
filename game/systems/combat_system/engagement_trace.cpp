#include "engagement_trace.h"

#include <QDebug>

#include "../../core/component_combat.h"
#include "../../core/component_core.h"
#include "../../core/component_gameplay.h"

namespace Game::Systems::Combat {

auto engagement_outcome_key(EngagementOutcome outcome) -> std::string_view {
  switch (outcome) {
  case EngagementOutcome::Engaged:
    return "engaged";
  case EngagementOutcome::NoCombatRole:
    return "no_combat_role";
  case EngagementOutcome::Busy:
    return "busy";
  case EngagementOutcome::HoldingTarget:
    return "holding_target";
  case EngagementOutcome::Suppressed:
    return "suppressed";
  case EngagementOutcome::NoCandidateInRange:
    return "no_candidate_in_range";
  case EngagementOutcome::CandidateUnreachable:
    return "candidate_unreachable";
  case EngagementOutcome::AssistedAlly:
    return "assisted_ally";
  case EngagementOutcome::Retaliated:
    return "retaliated";
  }
  return "unknown";
}

auto command_source_key(CommandSource source) -> std::string_view {
  switch (source) {
  case CommandSource::Auto:
    return "auto";
  case CommandSource::PlayerOrder:
    return "player";
  case CommandSource::AIOrder:
    return "ai";
  }
  return "auto";
}

auto command_source_of(const Engine::Core::Entity* entity) -> CommandSource {
  if (entity == nullptr) {
    return CommandSource::Auto;
  }

  auto const commanded_by = [entity]() {
    return entity->has_component<Engine::Core::AIControlledComponent>()
               ? CommandSource::AIOrder
               : CommandSource::PlayerOrder;
  };

  auto const* target = entity->get_component<Engine::Core::AttackTargetComponent>();
  if (target != nullptr && target->target_id != 0 && target->is_player_command) {

    return commanded_by();
  }

  auto const* intent =
      entity->get_component<Engine::Core::PlayerOrderIntentComponent>();
  if (intent != nullptr && intent->kind != Engine::Core::PlayerOrderIntentKind::None) {
    return commanded_by();
  }

  return CommandSource::Auto;
}

EngagementTrace::EngagementTrace() = default;

auto EngagementTrace::instance() -> EngagementTrace& {
  static EngagementTrace trace;
  return trace;
}

void EngagementTrace::set_enabled(bool enabled) {
  m_enabled = enabled;
  if (!m_enabled) {
    m_records.clear();
  }
}

void EngagementTrace::record(const Engine::Core::Entity* entity,
                             const EngagementRecord& record) {
  if (!m_enabled || entity == nullptr) {
    return;
  }

  m_records[entity->get_id()] = record;
}

auto EngagementTrace::find(Engine::Core::EntityID entity_id) const
    -> const EngagementRecord* {
  auto const it = m_records.find(entity_id);
  return it == m_records.end() ? nullptr : &it->second;
}

void EngagementTrace::clear() {
  m_records.clear();
}

void note_engagement(const Engine::Core::Entity* entity,
                     const EngagementRecord& record) {
  auto& trace = EngagementTrace::instance();
  if (!trace.enabled()) {
    return;
  }
  trace.record(entity, record);
}

} // namespace Game::Systems::Combat
