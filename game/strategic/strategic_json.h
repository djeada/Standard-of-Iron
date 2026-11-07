#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>

#include "strategic_world.h"

namespace Game::Strategic {

[[nodiscard]] auto
definition_from_json(const QByteArray& json,
                     QString* error = nullptr) -> std::optional<WorldDefinition>;
[[nodiscard]] auto load_definition_file(const QString& path, QString* error = nullptr)
    -> std::optional<WorldDefinition>;
[[nodiscard]] auto definition_to_json(const WorldDefinition& def) -> QJsonObject;
[[nodiscard]] auto definition_to_bytes(const WorldDefinition& def) -> QByteArray;

[[nodiscard]] auto state_body_to_json(const WorldState& state) -> QJsonObject;
[[nodiscard]] auto
state_body_from_json(const QJsonObject& body,
                     QVector<LoadIssue>* issues = nullptr) -> WorldState;

[[nodiscard]] auto save_state(const WorldDefinition& def,
                              const WorldState& state) -> QByteArray;
[[nodiscard]] auto load_state(const WorldDefinition& def,
                              const QByteArray& json,
                              QString* error = nullptr) -> std::optional<WorldState>;

[[nodiscard]] auto effect_to_json(const Effect& effect) -> QJsonObject;
[[nodiscard]] auto effect_from_json(const QJsonObject& json,
                                    QString* error = nullptr) -> std::optional<Effect>;

[[nodiscard]] auto pretty_json(const QJsonValue& value,
                               int wrap_width = 96) -> QByteArray;

[[nodiscard]] auto definition_fingerprint(const WorldDefinition& def) -> QString;

} // namespace Game::Strategic
