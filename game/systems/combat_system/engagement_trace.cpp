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

EngagementTrace::EngagementTrace() {
  m_log_to_console = !qEnvironmentVariableIsEmpty("SOI_ENGAGEMENT_TRACE");
  m_enabled = m_log_to_console;
}

auto EngagementTrace::instance() -> EngagementTrace& {
  static EngagementTrace trace;
  return trace;
}

void EngagementTrace::set_enabled(bool enabled) {
  m_enabled = enabled || m_log_to_console;
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

  if (!m_log_to_console) {
    return;
  }

  auto const reason = engagement_outcome_key(record.outcome);
  auto const source = command_source_key(record.source);
  qInfo().nospace() << "SOI_ENGAGEMENT unit=" << entity->get_id()
                    << " candidate=" << record.candidate_id
                    << " target=" << record.target_id
                    << " range=" << record.acquisition_range << " reason="
                    << QLatin1StringView(reason.data(),
                                         static_cast<qsizetype>(reason.size()))
                    << " source="
                    << QLatin1StringView(source.data(),
                                         static_cast<qsizetype>(source.size()));
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
