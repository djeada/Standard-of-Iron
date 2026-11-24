#pragma once

#include <QJsonObject>

#include <optional>

#include "command_log.h"

class QIODevice;

namespace Game::Command {

inline constexpr char k_log_magic[] = "SOIWAL\r\n";

enum class FrameStatus {
  Valid,
  End,
  Truncated,
  Invalid
};

auto write_log_frame(QIODevice& output, const QJsonObject& object) -> bool;
auto read_log_frame(QIODevice& input, QJsonObject& object) -> FrameStatus;

auto to_json(const LogEntry& entry) -> QJsonObject;
auto log_entry_from_json(const QJsonObject& object) -> std::optional<LogEntry>;

} // namespace Game::Command
