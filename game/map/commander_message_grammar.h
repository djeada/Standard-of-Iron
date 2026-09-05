#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>

#include "mission_definition.h"

namespace Game::Mission {

[[nodiscard]] auto
parse_commander_message_trigger(const QString& value,
                                CommanderMessageTrigger& out) -> bool;

[[nodiscard]] auto
commander_message_trigger_name(CommanderMessageTrigger trigger) -> QString;

[[nodiscard]] auto parse_commander_relationship(const QString& value,
                                                CommanderRelationship& out) -> bool;

[[nodiscard]] auto
commander_relationship_name(CommanderRelationship relationship) -> QString;

[[nodiscard]] auto
parse_commander_message_owner(const QJsonValue& value,
                              std::optional<int>& out_owner,
                              bool& out_is_local,
                              CommanderMessageRole& out_role) -> bool;

[[nodiscard]] auto parse_commander_message_condition(const QJsonObject& trigger)
    -> CommanderMessageCondition;

[[nodiscard]] auto parse_commander_message(const QJsonObject& obj) -> CommanderMessage;

[[nodiscard]] auto
parse_commander_voices_policy(const QJsonObject& obj) -> CommanderVoicesPolicy;

} // namespace Game::Mission
