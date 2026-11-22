#pragma once

#include <QJsonValue>

#include <cstdint>
#include <optional>

namespace Game::Command {

[[nodiscard]] auto u64_to_json(std::uint64_t value) -> QJsonValue;

[[nodiscard]] auto u64_from_json(const QJsonValue& value, bool allow_integer = false)
    -> std::optional<std::uint64_t>;

} // namespace Game::Command
