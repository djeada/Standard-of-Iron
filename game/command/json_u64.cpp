#include "json_u64.h"

#include <QString>

#include <cmath>

namespace Game::Command {

auto u64_to_json(std::uint64_t value) -> QJsonValue {
  return QString::number(value);
}

auto u64_from_json(const QJsonValue& value,
                   bool allow_integer) -> std::optional<std::uint64_t> {
  if (value.isString()) {
    const QString text = value.toString();
    if (text.isEmpty()) {
      return std::nullopt;
    }
    for (const QChar ch : text) {
      if (ch < QLatin1Char('0') || ch > QLatin1Char('9')) {
        return std::nullopt;
      }
    }
    bool ok = false;
    const quint64 parsed = text.toULongLong(&ok, 10);
    if (!ok) {
      return std::nullopt;
    }
    return static_cast<std::uint64_t>(parsed);
  }

  if (allow_integer && value.isDouble()) {
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0 || std::trunc(number) != number) {
      return std::nullopt;
    }
    const qint64 as_int = value.toInteger(-1);
    if (as_int >= 0 && static_cast<double>(as_int) == number) {
      return static_cast<std::uint64_t>(as_int);
    }
  }

  return std::nullopt;
}

} // namespace Game::Command
