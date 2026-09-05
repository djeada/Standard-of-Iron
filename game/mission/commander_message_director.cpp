#include "commander_message_director.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "game/map/commander_message_grammar.h"
#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"

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

auto is_muted(const CommanderVoicesPolicy& policy,
              const CommanderVoiceLine& line) -> bool {
  if (std::find(policy.muted_triggers.begin(),
                policy.muted_triggers.end(),
                line.trigger) != policy.muted_triggers.end()) {
    return true;
  }
  return std::find(policy.muted_lines.begin(), policy.muted_lines.end(), line.id) !=
         policy.muted_lines.end();
}

} // namespace

CommanderMessageDirector::~CommanderMessageDirector() {
  unsubscribe();
}

void CommanderMessageDirector::configure(const MissionDefinition& mission,
                                         int local_owner_id,
                                         const MissionPositionToWorld& to_world) {
  CommanderMessageScript script;
  script.mission_lines = mission.commander_messages;
  script.policy = mission.commander_voices;
  configure(script, local_owner_id, to_world);
}

void CommanderMessageDirector::configure(const CommanderMessageScript& script,
                                         int local_owner_id,
                                         const MissionPositionToWorld& to_world) {
  clear();
  m_local_owner_id = local_owner_id;

  add_mission_rules(script.mission_lines, script.speakers, to_world);
  if (script.policy.generic && script.voices != nullptr) {
    add_bank_rules(script, to_world);
  }

  for (const auto& rule : m_rules) {
    if (!rule.cue.speaker_id.isEmpty() &&
        !m_speaker_ids.contains(rule.cue.speaker_id)) {
      m_speaker_ids.push_back(rule.cue.speaker_id);
    }
  }

  if (!m_rules.empty()) {
    subscribe();
  }
}

auto CommanderMessageDirector::make_rule(
    const CommanderMessage& authored, const MissionPositionToWorld& to_world) -> Rule {
  Rule rule;
  rule.authored = authored;

  CommanderMessageCue cue;
  cue.id = authored.id;
  cue.speaker_id = authored.speaker;
  cue.pose = authored.pose.isEmpty() ? QStringLiteral("cynical") : authored.pose;
  cue.text = authored.text;
  cue.voice_cue = authored.voice_cue;
  cue.relationship = commander_relationship_name(CommanderRelationship::Enemy);
  const float authored_seconds = authored.duration > 0.0F
                                     ? authored.duration
                                     : k_default_commander_message_seconds;
  cue.duration = legible_commander_message_seconds(
      static_cast<int>(authored.text.length()), authored_seconds);
  cue.holds_outcome = commander_message_trigger_is_outcome(authored.trigger);

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
  return rule;
}

void CommanderMessageDirector::add_mission_rules(
    const std::vector<CommanderMessage>& lines,
    const std::vector<CommanderSpeaker>& speakers,
    const MissionPositionToWorld& to_world) {
  m_rules.reserve(m_rules.size() + lines.size());
  for (const auto& authored : lines) {
    Rule rule = make_rule(authored, to_world);
    rule.generic = false;
    rule.cue.text_context = Game::Util::k_missions_context;
    for (const auto& speaker : speakers) {
      if (speaker.troop_type == authored.speaker) {
        rule.speaker_owner_id = speaker.owner_id;
        rule.cue.speaker_owner_id = speaker.owner_id;
        rule.cue.relationship = commander_relationship_name(speaker.relationship);
        break;
      }
    }
    m_rules.push_back(std::move(rule));
  }
}

void CommanderMessageDirector::add_bank_rules(const CommanderMessageScript& script,
                                              const MissionPositionToWorld& to_world) {
  int next_group = 0;
  for (const auto& speaker : script.speakers) {
    const auto* bank = script.voices->bank_for(speaker.troop_type);
    if (bank == nullptr) {
      qWarning() << "No commander voice bank for" << speaker.troop_type << "(owner"
                 << speaker.owner_id << ")";
      continue;
    }
    m_chatter_budget[speaker.owner_id] = bank->chatter_per_match;
    for (const auto& line : bank->lines) {
      if (line.relationship != speaker.relationship || is_muted(script.policy, line)) {
        continue;
      }
      const int group = next_group++;
      for (const auto& authored :
           expand_commander_voice_line(line, bank->commander_id, speaker.owner_id)) {
        Rule rule = make_rule(authored, to_world);
        rule.generic = true;
        rule.variant_group = group;
        rule.speaker_owner_id = speaker.owner_id;
        rule.cue.speaker_owner_id = speaker.owner_id;
        rule.cue.relationship = commander_relationship_name(speaker.relationship);
        rule.cue.text_context = Game::Util::k_commander_voices_context;
        m_rules.push_back(std::move(rule));
      }
    }
  }
}

void CommanderMessageDirector::clear() {
  unsubscribe();
  m_speaker_ids.clear();
  m_rules.clear();
  m_pending.clear();
  m_active.reset();
  m_active_rule.reset();
  m_active_remaining = 0.0F;
  m_elapsed = 0.0F;
  m_last_line_ended_at = -1.0e9F;
  m_outcome_reached = false;
  m_chatter_budget.clear();
  m_chatter_spent.clear();
  m_speaker_trigger_fired_at.clear();
  const std::lock_guard<std::mutex> lock(m_inbox_mutex);
  m_inbox.clear();
}

void CommanderMessageDirector::subscribe() {
  m_capture_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::StructureCaptured,
                         .subject_owner_id = event.previous_owner_id,
                         .actor_owner_id = event.new_owner_id,
                         .structure_id = event.barrack_id});
          });

  m_death_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const auto& event) {
            const auto fallen = fallen_commander_for_spawn(event.spawn_type);
            if (!fallen.has_value()) {
              return;
            }
            notify_fact({.trigger = CommanderMessageTrigger::CommanderDefeated,
                         .subject_owner_id = event.owner_id,
                         .actor_owner_id = event.killer_owner_id,
                         .subject_type = fallen->troop_type,
                         .nation = fallen->nation});
          });

  using Engine::Core::ScopedEventSubscription;
  m_attack_subscription = ScopedEventSubscription<Engine::Core::AiAttackLaunchedEvent>(
      [this](const auto& event) {
        notify_fact({.trigger = CommanderMessageTrigger::AttackLaunched,
                     .subject_owner_id = event.target_owner_id,
                     .actor_owner_id = event.attacker_owner_id});
      });
  m_under_attack_subscription =
      ScopedEventSubscription<Engine::Core::OwnerUnderAttackEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::UnderAttack,
                         .subject_owner_id = event.owner_id,
                         .actor_owner_id = event.attacker_owner_id});
          });
  m_contact_subscription =
      ScopedEventSubscription<Engine::Core::OwnersFirstContactEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::FirstContact,
                         .subject_owner_id = event.owner_id,
                         .actor_owner_id = event.other_owner_id});
          });
  m_losses_subscription = ScopedEventSubscription<Engine::Core::OwnerHeavyLossesEvent>(
      [this](const auto& event) {
        notify_fact({.trigger = CommanderMessageTrigger::HeavyLosses,
                     .subject_owner_id = event.owner_id,
                     .actor_owner_id = event.killer_owner_id});
      });
  m_near_defeat_subscription =
      ScopedEventSubscription<Engine::Core::OwnerNearDefeatEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::NearDefeat,
                         .subject_owner_id = event.owner_id});
          });
  m_eliminated_subscription =
      ScopedEventSubscription<Engine::Core::OwnerEliminatedEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::OwnerEliminated,
                         .subject_owner_id = event.owner_id,
                         .actor_owner_id = event.by_owner_id});
          });
  m_wave_incoming_subscription =
      ScopedEventSubscription<Engine::Core::MissionWaveIncomingEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::WaveIncoming,
                         .subject_owner_id = event.owner_id,
                         .final_wave = event.final_wave});
          });
  m_wave_cleared_subscription =
      ScopedEventSubscription<Engine::Core::MissionWaveClearedEvent>(
          [this](const auto& event) {
            notify_fact({.trigger = CommanderMessageTrigger::WaveCleared,
                         .subject_owner_id = event.owner_id,
                         .final_wave = event.final_wave});
          });
}

void CommanderMessageDirector::unsubscribe() {
  m_capture_subscription.unsubscribe();
  m_death_subscription.unsubscribe();
  m_attack_subscription.unsubscribe();
  m_under_attack_subscription.unsubscribe();
  m_contact_subscription.unsubscribe();
  m_losses_subscription.unsubscribe();
  m_near_defeat_subscription.unsubscribe();
  m_eliminated_subscription.unsubscribe();
  m_wave_incoming_subscription.unsubscribe();
  m_wave_cleared_subscription.unsubscribe();
}

void CommanderMessageDirector::notify_fact(const CommanderMessageFact& fact) {
  const std::lock_guard<std::mutex> lock(m_inbox_mutex);
  m_inbox.push_back(fact);
}

void CommanderMessageDirector::notify_mission_start() {
  queue_fact({.trigger = CommanderMessageTrigger::MissionStart});
}

void CommanderMessageDirector::notify_victory() {
  m_outcome_reached = true;
  drop_pending_chatter();
  queue_fact({.trigger = CommanderMessageTrigger::MissionVictory});
}

void CommanderMessageDirector::notify_defeat() {
  m_outcome_reached = true;
  drop_pending_chatter();
  queue_fact({.trigger = CommanderMessageTrigger::MissionDefeat});
}

auto CommanderMessageDirector::is_chatter(const Rule& rule) const -> bool {
  return commander_message_trigger_is_chatter(rule.authored.trigger) &&
         rule.authored.priority < k_commander_chatter_exempt_priority;
}

void CommanderMessageDirector::drop_pending_chatter() {
  m_pending.erase(std::remove_if(m_pending.begin(),
                                 m_pending.end(),
                                 [this](const Pending& pending) {
                                   return is_chatter(m_rules[pending.rule_index]);
                                 }),
                  m_pending.end());
  if (m_active_rule.has_value() && is_chatter(m_rules[*m_active_rule])) {
    end_active();
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

auto CommanderMessageDirector::matches_role(CommanderMessageRole role,
                                            int owner_id,
                                            int speaker_owner_id) const -> bool {
  switch (role) {
  case CommanderMessageRole::Unset:
  case CommanderMessageRole::Any:
    return true;
  case CommanderMessageRole::Self:
    return speaker_owner_id >= 0 && owner_id == speaker_owner_id;
  case CommanderMessageRole::Player:
    return owner_id == m_local_owner_id;
  case CommanderMessageRole::AllyOfSpeaker:
    if (speaker_owner_id < 0 || owner_id == speaker_owner_id || owner_id < 0) {
      return false;
    }
    return m_are_allies ? m_are_allies(owner_id, speaker_owner_id) : false;
  case CommanderMessageRole::EnemyOfSpeaker:
    if (speaker_owner_id < 0 || owner_id == speaker_owner_id || owner_id < 0) {
      return false;
    }
    return m_are_allies ? !m_are_allies(owner_id, speaker_owner_id) : true;
  }
  return false;
}

auto CommanderMessageDirector::rule_matches(
    const Rule& rule, const CommanderMessageFact& fact) const -> bool {
  if (rule.authored.trigger != fact.trigger) {
    return false;
  }
  const auto& condition = rule.authored.condition;
  if (!matches_owner(condition, fact.subject_owner_id, fact.actor_owner_id)) {
    return false;
  }
  if (!matches_role(
          condition.subject_role, fact.subject_owner_id, rule.speaker_owner_id) ||
      !matches_role(condition.actor_role, fact.actor_owner_id, rule.speaker_owner_id)) {
    return false;
  }
  if (condition.subject_type.has_value() &&
      *condition.subject_type != fact.subject_type) {
    return false;
  }
  if (condition.nation.has_value() && *condition.nation != fact.nation) {
    return false;
  }
  if (condition.final_wave.has_value() &&
      (!fact.final_wave.has_value() || *condition.final_wave != *fact.final_wave)) {
    return false;
  }
  if (rule.world_target.has_value()) {
    if (!m_structure_position || !fact.structure_id.has_value()) {
      return false;
    }
    const auto position = m_structure_position(*fact.structure_id);
    const float radius = condition.radius.value_or(k_default_capture_match_radius);
    if (!position.has_value() ||
        !within_radius(*position, *rule.world_target, radius)) {
      return false;
    }
  }
  return true;
}

auto CommanderMessageDirector::is_queued(std::size_t index) const -> bool {
  if (m_active_rule.has_value() && *m_active_rule == index) {
    return true;
  }
  return std::any_of(
      m_pending.begin(), m_pending.end(), [index](const Pending& pending) {
        return pending.rule_index == index;
      });
}

auto CommanderMessageDirector::rule_is_available(std::size_t index) const -> bool {
  const auto& rule = m_rules[index];
  if (rule.authored.once && rule.fired) {
    return false;
  }
  if (is_queued(index)) {
    return false;
  }
  if (m_outcome_reached &&
      !commander_message_trigger_is_outcome(rule.authored.trigger)) {
    return false;
  }
  const float cooldown = rule.authored.condition.cooldown;
  if (cooldown > 0.0F) {
    if (rule.fired && m_elapsed - rule.last_fired_at < cooldown) {
      return false;
    }
    if (rule.speaker_owner_id >= 0) {
      const auto it = m_speaker_trigger_fired_at.find(
          {.owner_id = rule.speaker_owner_id, .trigger = rule.authored.trigger});
      if (it != m_speaker_trigger_fired_at.end() && m_elapsed - it->second < cooldown) {
        return false;
      }
    }
  }
  if (rule.generic && is_chatter(rule) && rule.speaker_owner_id >= 0) {
    const auto budget = m_chatter_budget.find(rule.speaker_owner_id);
    const auto spent = m_chatter_spent.find(rule.speaker_owner_id);
    if (budget != m_chatter_budget.end() && spent != m_chatter_spent.end() &&
        spent->second >= budget->second) {
      return false;
    }
  }
  return true;
}

auto CommanderMessageDirector::pick_variant(const std::vector<std::size_t>& group) const
    -> std::optional<std::size_t> {

  for (const std::size_t index : group) {
    if (!m_rules[index].fired) {
      return index;
    }
  }
  std::optional<std::size_t> oldest;
  for (const std::size_t index : group) {
    if (!oldest.has_value() ||
        m_rules[index].last_fired_at < m_rules[*oldest].last_fired_at) {
      oldest = index;
    }
  }
  return oldest;
}

void CommanderMessageDirector::queue_fact(const CommanderMessageFact& fact) {
  std::vector<std::size_t> mission_hits;
  std::map<int, std::map<int, std::vector<std::size_t>>> generic_by_speaker_group;

  for (std::size_t index = 0; index < m_rules.size(); ++index) {
    const auto& rule = m_rules[index];
    if (!rule_matches(rule, fact)) {
      continue;
    }
    if (!rule.generic) {
      if (rule_is_available(index)) {
        mission_hits.push_back(index);
      }
      continue;
    }
    generic_by_speaker_group[rule.speaker_owner_id][rule.variant_group].push_back(
        index);
  }

  std::set<QString> mission_speakers;
  for (const std::size_t index : mission_hits) {
    mission_speakers.insert(m_rules[index].cue.speaker_id);
    queue_rule(index);
  }

  std::vector<std::size_t> generic_choices;
  for (const auto& [speaker_owner, groups] : generic_by_speaker_group) {
    std::optional<std::size_t> best;
    for (const auto& [group, variants] : groups) {
      if (mission_speakers.contains(m_rules[variants.front()].cue.speaker_id)) {
        continue;
      }
      const auto choice = pick_variant(variants);
      if (!choice.has_value() || !rule_is_available(*choice)) {
        continue;
      }
      if (!best.has_value() ||
          m_rules[*choice].authored.priority > m_rules[*best].authored.priority) {
        best = choice;
      }
    }
    if (best.has_value()) {
      generic_choices.push_back(*best);
    }
  }
  if (generic_choices.empty()) {
    return;
  }
  if (fact.trigger == CommanderMessageTrigger::MissionStart) {
    for (const std::size_t index : generic_choices) {
      queue_rule(index);
    }
    return;
  }
  std::size_t best = generic_choices.front();
  for (const std::size_t index : generic_choices) {
    const auto& candidate = m_rules[index];
    const auto& current = m_rules[best];
    if (candidate.authored.priority > current.authored.priority ||
        (candidate.authored.priority == current.authored.priority &&
         candidate.speaker_owner_id < current.speaker_owner_id)) {
      best = index;
    }
  }
  queue_rule(best);
}

void CommanderMessageDirector::queue_rule(std::size_t index) {
  auto& rule = m_rules[index];
  if (is_queued(index)) {
    return;
  }

  rule.last_fired_at = m_elapsed;
  if (rule.speaker_owner_id >= 0 && rule.authored.condition.cooldown > 0.0F) {
    m_speaker_trigger_fired_at[{.owner_id = rule.speaker_owner_id,
                                .trigger = rule.authored.trigger}] = m_elapsed;
  }
  m_pending.push_back(
      {.rule_index = index,
       .delay_remaining = std::max(0.0F, rule.authored.delay),
       .expires_in = is_chatter(rule)
                         ? std::optional<float>{k_commander_chatter_expiry_seconds}
                         : std::nullopt});
}

auto CommanderMessageDirector::update(float delta_time) -> bool {
  if (m_rules.empty()) {
    return false;
  }
  m_elapsed += std::max(0.0F, delta_time);

  std::vector<CommanderMessageFact> facts;
  {
    const std::lock_guard<std::mutex> lock(m_inbox_mutex);
    facts.swap(m_inbox);
  }
  for (const auto& fact : facts) {
    queue_fact(fact);
  }

  for (auto& pending : m_pending) {
    if (pending.delay_remaining > 0.0F) {
      pending.delay_remaining = std::max(0.0F, pending.delay_remaining - delta_time);
    } else if (pending.expires_in.has_value()) {
      *pending.expires_in -= delta_time;
    }
  }
  m_pending.erase(std::remove_if(m_pending.begin(),
                                 m_pending.end(),
                                 [](const Pending& pending) {
                                   return pending.expires_in.has_value() &&
                                          *pending.expires_in < 0.0F;
                                 }),
                  m_pending.end());

  bool changed = false;
  if (m_active.has_value()) {
    m_active_remaining -= delta_time;
    const bool outcome_waiting =
        std::any_of(m_pending.begin(), m_pending.end(), [this](const Pending& pending) {
          return pending.delay_remaining <= 0.0F &&
                 m_rules[pending.rule_index].cue.holds_outcome;
        });
    if (m_active_remaining <= 0.0F || (outcome_waiting && !m_active->holds_outcome)) {
      end_active();
      changed = true;
    }
  }

  if (!m_active.has_value() && promote_next()) {
    changed = true;
  }
  return changed;
}

void CommanderMessageDirector::end_active() {
  m_active.reset();
  m_active_rule.reset();
  m_active_remaining = 0.0F;
  m_last_line_ended_at = m_elapsed;
}

auto CommanderMessageDirector::promote_next() -> bool {
  const bool gap_open =
      m_elapsed - m_last_line_ended_at >= k_commander_chatter_min_gap_seconds;
  auto best = m_pending.end();
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->delay_remaining > 0.0F) {
      continue;
    }
    const auto& rule = m_rules[it->rule_index];
    if (!gap_open && is_chatter(rule)) {
      continue;
    }
    if (best == m_pending.end() ||
        rule.authored.priority > m_rules[best->rule_index].authored.priority) {
      best = it;
    }
  }
  if (best == m_pending.end()) {
    return false;
  }

  auto& rule = m_rules[best->rule_index];
  rule.fired = true;
  rule.last_fired_at = m_elapsed;
  if (rule.generic && is_chatter(rule) && rule.speaker_owner_id >= 0) {
    ++m_chatter_spent[rule.speaker_owner_id];
  }
  m_active = rule.cue;
  m_active_rule = best->rule_index;
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
  end_active();
  promote_next();
  return true;
}

auto CommanderMessageDirector::serialize() const -> QJsonObject {
  QJsonArray fired;
  QJsonObject last_fired;
  for (const auto& rule : m_rules) {
    if (rule.fired) {
      fired.append(rule.authored.id);
      last_fired[rule.authored.id] = static_cast<double>(rule.last_fired_at);
    }
  }
  QJsonObject spent;
  for (const auto& [owner, count] : m_chatter_spent) {
    spent[QString::number(owner)] = count;
  }
  QJsonObject state;
  state["fired"] = fired;
  state["last_fired"] = last_fired;
  state["chatter_spent"] = spent;
  state["elapsed"] = static_cast<double>(m_elapsed);
  state["last_line_ended_at"] = static_cast<double>(m_last_line_ended_at);
  state["outcome_reached"] = m_outcome_reached;
  return state;
}

void CommanderMessageDirector::restore(const QJsonObject& state) {
  m_pending.clear();
  m_active.reset();
  m_active_rule.reset();
  m_active_remaining = 0.0F;
  m_chatter_spent.clear();
  m_speaker_trigger_fired_at.clear();

  m_elapsed = static_cast<float>(state["elapsed"].toDouble(0.0));
  m_last_line_ended_at =
      static_cast<float>(state["last_line_ended_at"].toDouble(-1.0e9));
  m_outcome_reached = state["outcome_reached"].toBool(false);

  const QJsonArray fired = state["fired"].toArray();
  const QJsonObject last_fired = state["last_fired"].toObject();
  for (auto& rule : m_rules) {
    rule.fired = false;
    rule.last_fired_at = -1.0e9F;
  }
  for (const auto value : fired) {
    const QString id = value.toString();
    for (auto& rule : m_rules) {
      if (rule.authored.id != id) {
        continue;
      }
      rule.fired = true;
      rule.last_fired_at = static_cast<float>(last_fired[id].toDouble(0.0));
      if (rule.speaker_owner_id >= 0 && rule.authored.condition.cooldown > 0.0F) {
        auto& stamp = m_speaker_trigger_fired_at[{.owner_id = rule.speaker_owner_id,
                                                  .trigger = rule.authored.trigger}];
        stamp = std::max(stamp, rule.last_fired_at);
      }
    }
  }
  const QJsonObject spent = state["chatter_spent"].toObject();
  for (auto it = spent.begin(); it != spent.end(); ++it) {
    bool ok = false;
    const int owner = it.key().toInt(&ok);
    if (ok) {
      m_chatter_spent[owner] = it.value().toInt();
    }
  }
}

} // namespace Game::Mission
