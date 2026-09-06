#pragma once

#include <QJsonObject>

#include <optional>
#include <string_view>

#include "command.h"

namespace Game::Command {

[[nodiscard]] auto to_json(const Command& command) -> QJsonObject;

[[nodiscard]] auto from_json(const QJsonObject& object) -> std::optional<Command>;

[[nodiscard]] auto source_from_name(std::string_view name) -> std::optional<Source>;

} // namespace Game::Command
