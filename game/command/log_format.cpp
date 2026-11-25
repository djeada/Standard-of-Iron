#include "log_format.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QtEndian>

#include <array>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

#include "command_codec.h"
#include "json_u64.h"

namespace Game::Command {
namespace {

constexpr std::uint32_t k_max_frame_bytes = 32U * 1024U * 1024U;
using FrameHeader = std::array<char, 8>;

constexpr auto k_crc_table = [] {
  std::array<std::uint32_t, 256> table{};
  for (std::uint32_t i = 0; i < table.size(); ++i) {
    std::uint32_t value = i;
    for (int bit = 0; bit < 8; ++bit) {
      value = (value & 1U) != 0U ? (value >> 1U) ^ 0xEDB88320U : value >> 1U;
    }
    table[i] = value;
  }
  return table;
}();

auto crc32(const QByteArray& bytes) -> std::uint32_t {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const unsigned char byte : bytes) {
    crc = (crc >> 8U) ^ k_crc_table[(crc ^ byte) & 0xFFU];
  }
  return crc ^ 0xFFFFFFFFU;
}

using Digests = Game::Session::SubsystemDigests;
constexpr std::array k_digest_fields{std::pair{"root", &Digests::root},
                                     std::pair{"identity", &Digests::identity},
                                     std::pair{"movement", &Digests::movement},
                                     std::pair{"combat", &Digests::combat},
                                     std::pair{"status", &Digests::status},
                                     std::pair{"economy", &Digests::economy},
                                     std::pair{"wildlife", &Digests::wildlife},
                                     std::pair{"session", &Digests::session},
                                     std::pair{"systems", &Digests::systems}};

auto read_payload(const QString& kind,
                  const QJsonObject& body) -> std::optional<LogPayload> {
  if (kind == QLatin1String("command")) {
    if (auto command = from_json(body)) {
      return std::move(command->payload);
    }
  } else if (kind == QLatin1String("commander_input")) {
    if (auto frame = commander_input_from_json(body)) {
      return *frame;
    }
  } else if (kind == QLatin1String("commander_action")) {
    if (auto action = commander_action_from_json(body)) {
      return *action;
    }
  } else if (kind == QLatin1String("clock")) {
    ClockChange change;
    const auto paused = body.value(QLatin1String("paused"));
    const auto speed = body.value(QLatin1String("speed"));
    if (!paused.isUndefined()) {
      if (!paused.isBool()) {
        return std::nullopt;
      }
      change.paused = paused.toBool();
    }
    if (!speed.isUndefined()) {
      const double scale = speed.toDouble(-1.0);
      if (!std::isfinite(scale) || scale <= 0.0) {
        return std::nullopt;
      }
      change.speed = scale;
    }
    if (change.paused || change.speed) {
      return change;
    }
  } else if (kind == QLatin1String("digest")) {
    Digests digests;
    for (const auto& [key, field] : k_digest_fields) {
      const auto value = u64_from_json(body.value(QLatin1String(key)));
      if (!value) {
        return std::nullopt;
      }
      digests.*field = *value;
    }
    return digests;
  } else if (kind == QLatin1String("checkpoint")) {
    const auto snapshot = QByteArray::fromBase64Encoding(
        body.value(QLatin1String("snapshot")).toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors);
    if (snapshot && !snapshot.decoded.isEmpty()) {
      return Checkpoint{snapshot.decoded};
    }
  } else if (kind == QLatin1String("tick_complete") && body.isEmpty()) {
    return TickComplete{};
  } else if (kind == QLatin1String("match_end") && body.isEmpty()) {
    return MatchEnd{};
  }
  return std::nullopt;
}

} // namespace

auto write_log_frame(QIODevice& output, const QJsonObject& object) -> bool {
  const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
  if (bytes.size() > k_max_frame_bytes) {
    return false;
  }
  FrameHeader header{};
  qToLittleEndian<std::uint32_t>(static_cast<std::uint32_t>(bytes.size()),
                                 header.data());
  qToLittleEndian(crc32(bytes), header.data() + 4);
  return output.write(header.data(), header.size()) ==
             static_cast<qint64>(header.size()) &&
         output.write(bytes) == bytes.size();
}

auto read_log_frame(QIODevice& input, QJsonObject& object) -> FrameStatus {
  FrameHeader header{};
  const qint64 count = input.read(header.data(), header.size());
  if (count == 0 && input.atEnd()) {
    return FrameStatus::End;
  }
  if (count != static_cast<qint64>(header.size())) {
    return count > 0 && input.atEnd() ? FrameStatus::Truncated : FrameStatus::Invalid;
  }
  const auto length = qFromLittleEndian<std::uint32_t>(header.data());
  if (length == 0 || length > k_max_frame_bytes) {
    return FrameStatus::Invalid;
  }
  if (!input.isSequential() && input.bytesAvailable() < length) {
    return FrameStatus::Truncated;
  }
  const auto bytes = input.read(length);
  if (bytes.size() != length) {
    return input.atEnd() ? FrameStatus::Truncated : FrameStatus::Invalid;
  }
  if (crc32(bytes) != qFromLittleEndian<std::uint32_t>(header.data() + 4)) {
    return FrameStatus::Invalid;
  }
  const auto document = QJsonDocument::fromJson(bytes);
  if (!document.isObject()) {
    return FrameStatus::Invalid;
  }
  object = document.object();
  return FrameStatus::Valid;
}

auto to_json(const LogEntry& entry) -> QJsonObject {
  const auto [kind, body] = std::visit(
      [&entry](const auto& payload) -> std::pair<const char*, QJsonObject> {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, Payload>) {
          // Reuse the command codec. The outer envelope owns the ordering and source.
          auto command =
              to_json(Command{entry.source, entry.owner, entry.tick, payload});
          command.remove(QLatin1String("source"));
          command.remove(QLatin1String("owner"));
          command.remove(QLatin1String("tick"));
          return {"command", std::move(command)};
        } else if constexpr (std::is_same_v<T, CommanderInputFrame>) {
          return {"commander_input", to_json(payload)};
        } else if constexpr (std::is_same_v<T, CommanderActionRecord>) {
          return {"commander_action", to_json(payload)};
        } else if constexpr (std::is_same_v<T, ClockChange>) {
          QJsonObject clock;
          if (payload.paused) {
            clock["paused"] = *payload.paused;
          }
          if (payload.speed) {
            clock["speed"] = *payload.speed;
          }
          return {"clock", clock};
        } else if constexpr (std::is_same_v<T, Digests>) {
          QJsonObject digests;
          for (const auto& [key, field] : k_digest_fields) {
            digests[QLatin1String(key)] = u64_to_json(payload.*field);
          }
          return {"digest", digests};
        } else if constexpr (std::is_same_v<T, Checkpoint>) {
          return {"checkpoint",
                  {{"snapshot", QString::fromLatin1(payload.snapshot.toBase64())}}};
        } else if constexpr (std::is_same_v<T, TickComplete>) {
          return {"tick_complete", {}};
        } else {
          static_assert(std::is_same_v<T, MatchEnd>);
          return {"match_end", {}};
        }
      },
      entry.payload);
  return {{"kind", QLatin1String(kind)},
          {"tick", u64_to_json(entry.tick)},
          {"seq", u64_to_json(entry.seq)},
          {"owner", entry.owner},
          {"source", QLatin1String(source_name(entry.source))},
          {"body", body}};
}

auto log_entry_from_json(const QJsonObject& object) -> std::optional<LogEntry> {
  const auto tick = u64_from_json(object.value(QLatin1String("tick")));
  const auto seq = u64_from_json(object.value(QLatin1String("seq")));
  const auto source_value = object.value(QLatin1String("source"));
  const auto source = source_from_name(source_value.toString().toStdString());
  const auto owner = object.value(QLatin1String("owner"));
  const auto body = object.value(QLatin1String("body"));
  const auto kind = object.value(QLatin1String("kind")).toString();
  if (!tick || !seq || *seq == 0 || *seq == std::numeric_limits<std::uint64_t>::max() ||
      !source || !owner.isDouble() || owner.toDouble() != owner.toInt() ||
      !body.isObject()) {
    return std::nullopt;
  }
  auto payload_object = body.toObject();
  if (kind == QLatin1String("command")) {
    payload_object["source"] = source_value;
    payload_object["tick"] = u64_to_json(*tick);
    payload_object["owner"] = owner;
  }
  auto payload = read_payload(kind, payload_object);
  if (!payload) {
    return std::nullopt;
  }
  return LogEntry{*tick, *seq, owner.toInt(), *source, std::move(*payload)};
}

} // namespace Game::Command
