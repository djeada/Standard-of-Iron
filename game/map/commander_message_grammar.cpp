#include "commander_message_grammar.h"

#include <QDebug>
#include <QJsonArray>

#include <array>
#include <utility>

namespace Game::Mission {

namespace {

struct TriggerName {
  CommanderMessageTrigger trigger;
  const char* name;
};

constexpr std::array<TriggerName, 13> k_trigger_names{{
    {CommanderMessageTrigger::MissionStart, "mission_start"},
    {CommanderMessageTrigger::MissionVictory, "mission_victory"},
    {CommanderMessageTrigger::MissionDefeat, "mission_defeat"},
    {CommanderMessageTrigger::StructureCaptured, "structure_captured"},
    {CommanderMessageTrigger::CommanderDefeated, "commander_defeated"},
    {CommanderMessageTrigger::AttackLaunched, "attack_launched"},
    {CommanderMessageTrigger::UnderAttack, "under_attack"},
    {CommanderMessageTrigger::FirstContact, "first_contact"},
    {CommanderMessageTrigger::HeavyLosses, "heavy_losses"},
    {CommanderMessageTrigger::NearDefeat, "near_defeat"},
    {CommanderMessageTrigger::OwnerEliminated, "owner_eliminated"},
    {CommanderMessageTrigger::WaveIncoming, "wave_incoming"},
    {CommanderMessageTrigger::WaveCleared, "wave_cleared"},
}};

auto parse_position(const QJsonObject& obj) -> Position {
  Position pos;
  pos.x = static_cast<float>(obj["x"].toDouble(0.0));
  pos.z = static_cast<float>(obj["z"].toDouble(0.0));
  return pos;
}

auto first_present(const QJsonObject& obj,
                   const char* primary,
                   const char* alias) -> QJsonValue {
  if (obj.contains(QLatin1String(primary))) {
    return obj[QLatin1String(primary)];
  }
  return obj[QLatin1String(alias)];
}

} // namespace

auto parse_commander_message_trigger(const QString& value,
                                     CommanderMessageTrigger& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("match_start")) {
    out = CommanderMessageTrigger::MissionStart;
    return true;
  }
  for (const auto& entry : k_trigger_names) {
    if (lowered == QLatin1String(entry.name)) {
      out = entry.trigger;
      return true;
    }
  }
  return false;
}

auto commander_message_trigger_name(CommanderMessageTrigger trigger) -> QString {
  for (const auto& entry : k_trigger_names) {
    if (entry.trigger == trigger) {
      return QLatin1String(entry.name);
    }
  }
  return QStringLiteral("mission_start");
}

auto parse_commander_relationship(const QString& value,
                                  CommanderRelationship& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("enemy")) {
    out = CommanderRelationship::Enemy;
    return true;
  }
  if (lowered == QStringLiteral("ally")) {
    out = CommanderRelationship::Ally;
    return true;
  }
  return false;
}

auto commander_relationship_name(CommanderRelationship relationship) -> QString {
  return relationship == CommanderRelationship::Ally ? QStringLiteral("ally")
                                                     : QStringLiteral("enemy");
}

auto parse_commander_message_owner(const QJsonValue& value,
                                   std::optional<int>& out_owner,
                                   bool& out_is_local,
                                   CommanderMessageRole& out_role) -> bool {
  if (value.isUndefined() || value.isNull()) {
    return true;
  }
  if (!value.isString()) {
    out_owner = value.toInt();
    return true;
  }
  const QString name = value.toString().trimmed().toLower();
  if (name == QStringLiteral("player") || name == QStringLiteral("local")) {
    out_is_local = true;
    return true;
  }
  if (name == QStringLiteral("self")) {
    out_role = CommanderMessageRole::Self;
    return true;
  }
  if (name == QStringLiteral("ally_of_speaker")) {
    out_role = CommanderMessageRole::AllyOfSpeaker;
    return true;
  }
  if (name == QStringLiteral("enemy_of_speaker")) {
    out_role = CommanderMessageRole::EnemyOfSpeaker;
    return true;
  }
  if (name == QStringLiteral("any")) {
    out_role = CommanderMessageRole::Any;
    return true;
  }
  bool parsed = false;
  const int owner = name.toInt(&parsed);
  if (parsed) {
    out_owner = owner;
    return true;
  }
  return false;
}

auto parse_commander_message_condition(const QJsonObject& trigger)
    -> CommanderMessageCondition {
  CommanderMessageCondition condition;

  const QJsonValue subject = first_present(trigger, "owner_id", "subject");
  if (!parse_commander_message_owner(subject,
                                     condition.owner_id,
                                     condition.owner_is_local,
                                     condition.subject_role)) {
    qWarning() << "Unknown commander message owner" << subject.toString()
               << "- ignoring";
  }
  const QJsonValue actor = first_present(trigger, "by_owner_id", "actor");
  if (!parse_commander_message_owner(actor,
                                     condition.by_owner_id,
                                     condition.by_owner_is_local,
                                     condition.actor_role)) {
    qWarning() << "Unknown commander message actor" << actor.toString() << "- ignoring";
  }

  if (trigger.contains("structure_type")) {
    condition.subject_type = trigger["structure_type"].toString();
  } else if (trigger.contains("unit_type")) {
    condition.subject_type = trigger["unit_type"].toString();
  }
  if (trigger.contains("nation")) {
    condition.nation = trigger["nation"].toString();
  }
  if (trigger.contains("final_wave")) {
    condition.final_wave = trigger["final_wave"].toBool();
  }
  if (trigger.contains("at")) {
    condition.at = parse_position(trigger["at"].toObject());
  }
  if (trigger.contains("radius")) {
    condition.radius = static_cast<float>(trigger["radius"].toDouble());
  }
  condition.cooldown = static_cast<float>(trigger["cooldown"].toDouble(0.0));
  return condition;
}

auto parse_commander_message(const QJsonObject& obj) -> CommanderMessage {
  CommanderMessage message;
  message.id = obj["id"].toString();
  message.speaker = obj["speaker"].toString();
  message.pose = obj["pose"].toString();
  message.text = obj["text"].toString();
  message.voice_cue = obj["voice_cue"].toString();

  const QJsonObject trigger = obj["trigger"].toObject();
  const QString trigger_type = trigger["type"].toString();
  if (!parse_commander_message_trigger(trigger_type, message.trigger)) {
    qWarning() << "Unknown commander message trigger" << trigger_type << "in message"
               << message.id << "- defaulting to mission_start";
  }
  message.condition = parse_commander_message_condition(trigger);

  message.delay = static_cast<float>(trigger["delay"].toDouble(message.delay));
  message.duration = static_cast<float>(obj["duration"].toDouble(message.duration));
  message.priority = obj["priority"].toInt(message.priority);
  message.once = obj["once"].toBool(message.once);

  return message;
}

auto parse_commander_voices_policy(const QJsonObject& obj) -> CommanderVoicesPolicy {
  CommanderVoicesPolicy policy;
  policy.generic = obj["generic"].toBool(policy.generic);
  for (const auto value : obj["muted_triggers"].toArray()) {
    CommanderMessageTrigger trigger{};
    if (parse_commander_message_trigger(value.toString(), trigger)) {
      policy.muted_triggers.push_back(trigger);
    } else {
      qWarning() << "commander_voices.muted_triggers names unknown trigger"
                 << value.toString();
    }
  }
  for (const auto value : obj["muted_lines"].toArray()) {
    const QString id = value.toString().trimmed();
    if (!id.isEmpty()) {
      policy.muted_lines.push_back(id);
    }
  }
  return policy;
}

} // namespace Game::Mission
