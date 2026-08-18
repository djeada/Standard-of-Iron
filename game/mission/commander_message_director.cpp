#include "commander_message_director.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <optional>
#include <utility>

#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Game::Mission {

namespace {

constexpr float k_default_capture_match_radius = 30.0F;

auto commander_for_speaker(const QString& speaker)
    -> const Game::Units::CommanderDefinition* {
  Game::Units::TroopType troop_type{};
  if (!Game::Units::try_parse_troop_type(speaker, troop_type)) {
    return nullptr;
  }
  return Game::Units::commander_definition(troop_type);
}

struct FallenCommander {
  QString troop_type;
  QString nation;
};

auto fallen_commander_for_spawn(Game::Units::SpawnType spawn_type)
    -> std::optional<FallenCommander> {
  const auto troop_type = Game::Units::spawn_typeToTroopType(spawn_type);
  if (!troop_type.has_value()) {
    return std::nullopt;
  }
  const auto* definition = Game::Units::commander_definition(*troop_type);
  if (definition == nullptr) {
    return std::nullopt;
  }
  return FallenCommander{
      .troop_type = Game::Units::troop_typeToQString(*troop_type),
      .nation = Game::Systems::nation_id_to_qstring(definition->nation_id)};
}

auto within_radius(const QVector3D& a, const QVector3D& b, float radius) -> bool {
  const float dx = a.x() - b.x();
  const float dz = a.z() - b.z();
  return (dx * dx) + (dz * dz) <= radius * radius;
}

} // namespace

CommanderMessageDirector::~CommanderMessageDirector() {
  unsubscribe();
}

void CommanderMessageDirector::configure(const MissionDefinition& mission,
                                         int local_owner_id,
                                         const MissionPositionToWorld& to_world) {
  clear();
  m_local_owner_id = local_owner_id;

  m_rules.reserve(mission.commander_messages.size());
  for (const auto& authored : mission.commander_messages) {
    Rule rule;
    rule.authored = authored;

    CommanderMessageCue cue;
    cue.id = authored.id;
    cue.speaker_id = authored.speaker;
    cue.pose = authored.pose.isEmpty() ? QStringLiteral("cynical") : authored.pose;
    cue.text = authored.text;
    cue.voice_cue = authored.voice_cue;
    cue.duration = authored.duration > 0.0F ? authored.duration
                                            : k_default_commander_message_seconds;
    cue.holds_outcome = authored.trigger == CommanderMessageTrigger::MissionVictory ||
                        authored.trigger == CommanderMessageTrigger::MissionDefeat;

    if (const auto* definition = commander_for_speaker(authored.speaker)) {
      cue.speaker_name = QString::fromStdString(definition->display_name);
      cue.speaker_role = QString::fromStdString(definition->battlefield_role);
      cue.nation = Game::Systems::nation_id_to_qstring(definition->nation_id);
    } else if (!authored.speaker.isEmpty()) {
      qWarning() << "Commander message" << authored.id << "names unknown speaker"
                 << authored.speaker;
      cue.speaker_name = authored.speaker;
    }

    rule.cue = std::move(cue);

    if (authored.condition.at.has_value() && to_world) {
      rule.world_target = to_world(*authored.condition.at);
    }

    m_rules.push_back(std::move(rule));
  }

  if (!m_rules.empty()) {
    subscribe();
  }
}

void CommanderMessageDirector::clear() {
  unsubscribe();
  m_rules.clear();
  m_pending.clear();
  m_active.reset();
  m_active_remaining = 0.0F;
  const std::lock_guard<std::mutex> lock(m_inbox_mutex);
  m_capture_inbox.clear();
  m_death_inbox.clear();
}

void CommanderMessageDirector::subscribe() {
  m_capture_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const auto& event) {
            const std::lock_guard<std::mutex> lock(m_inbox_mutex);
            m_capture_inbox.push_back({.structure_id = event.barrack_id,
                                       .previous_owner_id = event.previous_owner_id,
                                       .new_owner_id = event.new_owner_id});
          });

  m_death_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const auto& event) {
            const auto fallen = fallen_commander_for_spawn(event.spawn_type);
            if (!fallen.has_value()) {
              return;
            }
            const std::lock_guard<std::mutex> lock(m_inbox_mutex);
            m_death_inbox.push_back({.owner_id = event.owner_id,
                                     .killer_owner_id = event.killer_owner_id,
                                     .troop_type = fallen->troop_type,
                                     .nation = fallen->nation});
          });
}

void CommanderMessageDirector::unsubscribe() {
  m_capture_subscription.unsubscribe();
  m_death_subscription.unsubscribe();
}

void CommanderMessageDirector::notify_mission_start() {
  queue_trigger(CommanderMessageTrigger::MissionStart);
}

void CommanderMessageDirector::notify_victory() {
  queue_trigger(CommanderMessageTrigger::MissionVictory);
}

void CommanderMessageDirector::notify_defeat() {
  queue_trigger(CommanderMessageTrigger::MissionDefeat);
}

void CommanderMessageDirector::queue_trigger(CommanderMessageTrigger trigger) {
  for (std::size_t index = 0; index < m_rules.size(); ++index) {
    if (m_rules[index].authored.trigger == trigger) {
      queue_rule(index);
    }
  }
}

auto CommanderMessageDirector::matches_owner(const CommanderMessageCondition& condition,
                                             int owner_id,
                                             int by_owner_id) const -> bool {
  const auto expected_owner = condition.owner_is_local
                                  ? std::optional<int>{m_local_owner_id}
                                  : condition.owner_id;
  const auto expected_actor = condition.by_owner_is_local
                                  ? std::optional<int>{m_local_owner_id}
                                  : condition.by_owner_id;

  if (expected_owner.has_value() && *expected_owner != owner_id) {
    return false;
  }
  if (expected_actor.has_value() && *expected_actor != by_owner_id) {
    return false;
  }
  return true;
}

void CommanderMessageDirector::queue_capture(const CaptureFact& fact) {
  for (std::size_t index = 0; index < m_rules.size(); ++index) {
    const auto& rule = m_rules[index];
    if (rule.authored.trigger != CommanderMessageTrigger::StructureCaptured) {
      continue;
    }
    if (!matches_owner(
            rule.authored.condition, fact.previous_owner_id, fact.new_owner_id)) {
      continue;
    }
    if (rule.world_target.has_value()) {
      if (!m_structure_position) {
        continue;
      }
      const auto position = m_structure_position(fact.structure_id);
      const float radius =
          rule.authored.condition.radius.value_or(k_default_capture_match_radius);
      if (!position.has_value() ||
          !within_radius(*position, *rule.world_target, radius)) {
        continue;
      }
    }
    queue_rule(index);
  }
}

void CommanderMessageDirector::queue_commander_death(const CommanderDeathFact& fact) {
  for (std::size_t index = 0; index < m_rules.size(); ++index) {
    const auto& rule = m_rules[index];
    if (rule.authored.trigger != CommanderMessageTrigger::CommanderDefeated) {
      continue;
    }
    if (!matches_owner(rule.authored.condition, fact.owner_id, fact.killer_owner_id)) {
      continue;
    }
    const auto& subject_type = rule.authored.condition.subject_type;
    if (subject_type.has_value() && *subject_type != fact.troop_type) {
      continue;
    }
    const auto& nation = rule.authored.condition.nation;
    if (nation.has_value() && *nation != fact.nation) {
      continue;
    }
    queue_rule(index);
  }
}

void CommanderMessageDirector::queue_rule(std::size_t index) {
  auto& rule = m_rules[index];
  if (rule.authored.once && rule.fired) {
    return;
  }
  const bool already_queued =
      std::any_of(m_pending.begin(), m_pending.end(), [index](const Pending& pending) {
        return pending.rule_index == index;
      });
  if (already_queued) {
    return;
  }

  rule.fired = true;
  m_pending.push_back(
      {.rule_index = index, .delay_remaining = std::max(0.0F, rule.authored.delay)});
}

auto CommanderMessageDirector::update(float delta_time) -> bool {
  if (m_rules.empty()) {
    return false;
  }

  std::vector<CaptureFact> captures;
  std::vector<CommanderDeathFact> deaths;
  {
    const std::lock_guard<std::mutex> lock(m_inbox_mutex);
    captures.swap(m_capture_inbox);
    deaths.swap(m_death_inbox);
  }
  for (const auto& capture : captures) {
    queue_capture(capture);
  }
  for (const auto& death : deaths) {
    queue_commander_death(death);
  }

  for (auto& pending : m_pending) {
    pending.delay_remaining = std::max(0.0F, pending.delay_remaining - delta_time);
  }

  bool changed = false;
  if (m_active.has_value()) {
    m_active_remaining -= delta_time;
    if (m_active_remaining <= 0.0F) {
      m_active.reset();
      m_active_remaining = 0.0F;
      changed = true;
    }
  }

  if (!m_active.has_value() && promote_next()) {
    changed = true;
  }
  return changed;
}

auto CommanderMessageDirector::promote_next() -> bool {
  auto best = m_pending.end();
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->delay_remaining > 0.0F) {
      continue;
    }
    if (best == m_pending.end() || m_rules[it->rule_index].authored.priority >
                                       m_rules[best->rule_index].authored.priority) {
      best = it;
    }
  }
  if (best == m_pending.end()) {
    return false;
  }

  m_active = m_rules[best->rule_index].cue;
  m_active_remaining = m_active->duration;
  m_pending.erase(best);
  return true;
}

auto CommanderMessageDirector::active() const -> const CommanderMessageCue& {
  static const CommanderMessageCue k_empty{};
  return m_active.has_value() ? *m_active : k_empty;
}

auto CommanderMessageDirector::dismiss_active() -> bool {
  if (!m_active.has_value()) {
    return false;
  }
  m_active.reset();
  m_active_remaining = 0.0F;
  promote_next();
  return true;
}

auto CommanderMessageDirector::serialize() const -> QJsonObject {
  QJsonArray fired;
  for (const auto& rule : m_rules) {
    if (rule.fired) {
      fired.append(rule.authored.id);
    }
  }
  QJsonObject state;
  state["fired"] = fired;
  return state;
}

void CommanderMessageDirector::restore(const QJsonObject& state) {
  m_pending.clear();
  m_active.reset();
  m_active_remaining = 0.0F;

  const QJsonArray fired = state["fired"].toArray();
  for (auto& rule : m_rules) {
    rule.fired = false;
  }
  for (const auto value : fired) {
    const QString id = value.toString();
    for (auto& rule : m_rules) {
      if (rule.authored.id == id) {
        rule.fired = true;
      }
    }
  }
}

} // namespace Game::Mission
