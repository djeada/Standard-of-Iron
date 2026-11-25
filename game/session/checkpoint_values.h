#pragma once

#include <QJsonArray>
#include <QString>

#include <cmath>
#include <limits>
#include <tuple>
#include <type_traits>

namespace Game::Session::CheckpointValues {

template <typename T>
auto encode(T value) -> QJsonValue {
  if constexpr (std::is_enum_v<T>) {
    return encode(static_cast<std::underlying_type_t<T>>(value));
  } else if constexpr (std::is_same_v<T, bool>) {
    return value;
  } else if constexpr (std::is_floating_point_v<T>) {
    return static_cast<double>(value);
  } else if constexpr (std::is_unsigned_v<T> && sizeof(T) == sizeof(qulonglong)) {
    return QString::number(static_cast<qulonglong>(value));
  } else {
    return static_cast<qint64>(value);
  }
}

template <typename T>
auto decode(const QJsonValue& value, T& out) -> bool {
  if constexpr (std::is_enum_v<T>) {
    std::underlying_type_t<T> number{};
    if (!decode(value, number)) {
      return false;
    }
    out = static_cast<T>(number);
  } else if constexpr (std::is_same_v<T, bool>) {
    if (!value.isBool()) {
      return false;
    }
    out = value.toBool();
  } else if constexpr (std::is_floating_point_v<T>) {
    const auto number = value.toDouble(std::numeric_limits<double>::infinity());
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<T>::max()) {
      return false;
    }
    out = static_cast<T>(number);
  } else if constexpr (std::is_unsigned_v<T> && sizeof(T) == sizeof(qulonglong)) {
    bool ok = false;
    const auto number = value.toString().toULongLong(&ok);
    if (!ok) {
      return false;
    }
    out = static_cast<T>(number);
  } else {
    constexpr auto invalid = std::numeric_limits<qint64>::min();
    const auto number = value.toInteger(invalid);
    if (number == invalid ||
        number < static_cast<qint64>(std::numeric_limits<T>::min()) ||
        number > static_cast<qint64>(std::numeric_limits<T>::max())) {
      return false;
    }
    out = static_cast<T>(number);
  }
  return true;
}

template <typename... T>
auto write(T... values) -> QJsonArray {
  return {encode(values)...};
}

template <typename... T>
auto read(const QJsonValue& value, T&... fields) -> bool {
  if (!value.isArray()) {
    return false;
  }
  const auto values = value.toArray();
  qsizetype offset = 0;
  return values.size() == static_cast<qsizetype>(sizeof...(fields)) &&
         (decode(values.at(offset++), fields) && ...);
}

template <typename... T>
auto write_tuple(const std::tuple<T...>& fields) -> QJsonArray {
  return std::apply([](const auto&... values) { return write(values...); }, fields);
}

template <typename... T>
auto read_tuple(const QJsonValue& value, const std::tuple<T...>& fields) -> bool {
  return std::apply([&](auto&... values) { return read(value, values...); }, fields);
}

} // namespace Game::Session::CheckpointValues
