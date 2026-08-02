#include "save_format.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLatin1String>
#include <QStringLiteral>

#include <cstring>

namespace Game::Systems::Save {

namespace {

constexpr const char* k_magic = "SOISAVE\x01";
constexpr int k_magic_size = 8;
constexpr int k_compression_level = 6;
constexpr qint64 k_max_package_blob = 512LL * 1024LL * 1024LL;

auto make_error(QString* out_error, const QString& message) -> bool {
  if (out_error != nullptr) {
    *out_error = message;
  }
  return false;
}

} // namespace

auto compression_to_string(Compression compression) -> QString {
  switch (compression) {
  case Compression::None:
    return QStringLiteral("none");
  case Compression::Zlib:
    return QStringLiteral("zlib");
  }
  return QStringLiteral("none");
}

auto compression_from_string(const QString& value, Compression& out) -> bool {
  if (value == QStringLiteral("none")) {
    out = Compression::None;
    return true;
  }
  if (value == QStringLiteral("zlib")) {
    out = Compression::Zlib;
    return true;
  }
  return false;
}

auto slot_kind_to_string(SlotKind kind) -> QString {
  switch (kind) {
  case SlotKind::Manual:
    return QStringLiteral("manual");
  case SlotKind::Quicksave:
    return QStringLiteral("quicksave");
  case SlotKind::Autosave:
    return QStringLiteral("autosave");
  }
  return QStringLiteral("manual");
}

auto slot_kind_from_string(const QString& value, SlotKind& out) -> bool {
  if (value == QStringLiteral("manual")) {
    out = SlotKind::Manual;
    return true;
  }
  if (value == QStringLiteral("quicksave")) {
    out = SlotKind::Quicksave;
    return true;
  }
  if (value == QStringLiteral("autosave")) {
    out = SlotKind::Autosave;
    return true;
  }
  return false;
}

auto checksum_of(const QByteArray& bytes) -> QString {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

auto pack(const QByteArray& raw, Compression compression) -> Payload {
  Payload payload;
  payload.compression = compression;
  payload.raw_size = raw.size();
  payload.raw_checksum = checksum_of(raw);
  payload.blob =
      compression == Compression::Zlib ? qCompress(raw, k_compression_level) : raw;
  payload.blob_checksum = checksum_of(payload.blob);
  return payload;
}

auto verify_blob(const Payload& payload, QString* out_error) -> bool {
  if (payload.blob_checksum.isEmpty()) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Save payload has no checksum"));
  }
  if (checksum_of(payload.blob) != payload.blob_checksum) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile",
                                    "Save payload is corrupted (stored checksum "
                                    "mismatch)"));
  }
  return true;
}

auto unpack(const Payload& payload, QByteArray& out_raw, QString* out_error) -> bool {
  if (!verify_blob(payload, out_error)) {
    return false;
  }

  QByteArray raw;
  if (payload.compression == Compression::Zlib) {
    raw = qUncompress(payload.blob);
    if (raw.isEmpty() && payload.raw_size != 0) {
      return make_error(
          out_error,
          QCoreApplication::translate("SaveFile",
                                      "Save payload is corrupted (decompression "
                                      "failed)"));
    }
  } else {
    raw = payload.blob;
  }

  if (raw.size() != payload.raw_size) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile",
                                    "Save payload is corrupted (expected %1 bytes, "
                                    "got %2)")
            .arg(payload.raw_size)
            .arg(raw.size()));
  }

  if (checksum_of(raw) != payload.raw_checksum) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile",
                                    "Save payload is corrupted (content checksum "
                                    "mismatch)"));
  }

  out_raw = raw;
  return true;
}

auto package_file_suffix() -> QString {
  return QStringLiteral("soisave");
}

auto encode_preview(const QImage& frame, int width) -> QByteArray {
  if (frame.isNull() || width <= 0) {
    return {};
  }

  const QImage thumbnail = frame.width() > width
                               ? frame.scaledToWidth(width, Qt::SmoothTransformation)
                               : frame;

  QByteArray png;
  QBuffer buffer(&png);
  if (!buffer.open(QIODevice::WriteOnly) || !thumbnail.save(&buffer, "PNG")) {
    return {};
  }
  buffer.close();
  return png;
}

auto sanitize_file_stem(const QString& name) -> QString {
  QString sanitized;
  sanitized.reserve(name.size());
  for (const QChar character : name) {
    if (character.isLetterOrNumber() || character == QLatin1Char('_') ||
        character == QLatin1Char('-')) {
      sanitized.append(character);
    } else {
      sanitized.append(QLatin1Char('_'));
    }
  }
  return sanitized;
}

auto encode_package(const Record& record) -> QByteArray {
  QJsonObject header;
  header[QStringLiteral("format_version")] = k_format_version;
  header[QStringLiteral("slot_name")] = record.slot_name;
  header[QStringLiteral("title")] = record.title;
  header[QStringLiteral("map_name")] = record.map_name;
  header[QStringLiteral("map_path")] = record.map_path;
  header[QStringLiteral("mode")] = record.mode;
  header[QStringLiteral("campaign_id")] = record.campaign_id;
  header[QStringLiteral("mission_id")] = record.mission_id;
  header[QStringLiteral("difficulty")] = record.difficulty;
  header[QStringLiteral("kind")] = slot_kind_to_string(record.kind);
  header[QStringLiteral("play_time_seconds")] = record.play_time_seconds;
  header[QStringLiteral("created_at")] = record.created_at;
  header[QStringLiteral("updated_at")] = record.updated_at;
  header[QStringLiteral("metadata")] = record.metadata;
  header[QStringLiteral("compression")] =
      compression_to_string(record.world.compression);
  header[QStringLiteral("world_raw_size")] = static_cast<double>(record.world.raw_size);
  header[QStringLiteral("world_raw_checksum")] = record.world.raw_checksum;
  header[QStringLiteral("world_blob_checksum")] = record.world.blob_checksum;
  header[QStringLiteral("world_blob_size")] =
      static_cast<double>(record.world.blob.size());
  header[QStringLiteral("screenshot_size")] =
      static_cast<double>(record.screenshot.size());
  header[QStringLiteral("screenshot_checksum")] = checksum_of(record.screenshot);

  const QByteArray header_bytes = QJsonDocument(header).toJson(QJsonDocument::Compact);

  QByteArray package;
  QDataStream stream(&package, QIODevice::WriteOnly);
  stream.setByteOrder(QDataStream::BigEndian);
  stream.writeRawData(k_magic, k_magic_size);
  stream << static_cast<quint32>(k_format_version);
  stream << static_cast<quint32>(header_bytes.size());
  stream.writeRawData(header_bytes.constData(), static_cast<int>(header_bytes.size()));
  stream.writeRawData(record.world.blob.constData(),
                      static_cast<int>(record.world.blob.size()));
  stream.writeRawData(record.screenshot.constData(),
                      static_cast<int>(record.screenshot.size()));
  return package;
}

auto decode_package(const QByteArray& bytes, Record& out, QString* out_error) -> bool {
  if (bytes.size() < k_magic_size + 8) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Not a Standard of Iron save file"));
  }
  if (std::memcmp(bytes.constData(), k_magic, k_magic_size) != 0) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Not a Standard of Iron save file"));
  }

  QDataStream stream(bytes);
  stream.setByteOrder(QDataStream::BigEndian);
  stream.skipRawData(k_magic_size);

  quint32 format_version = 0;
  quint32 header_size = 0;
  stream >> format_version;
  stream >> header_size;

  if (format_version != static_cast<quint32>(k_format_version)) {
    return make_error(out_error,
                      QCoreApplication::translate(
                          "SaveFile", "Unsupported save file version %1 (expected %2)")
                          .arg(format_version)
                          .arg(k_format_version));
  }

  if (header_size == 0 || header_size > k_max_package_blob ||
      static_cast<qint64>(header_size) > bytes.size() - k_magic_size - 8) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Save file header is truncated"));
  }

  QByteArray header_bytes(static_cast<int>(header_size), Qt::Uninitialized);
  if (stream.readRawData(header_bytes.data(), static_cast<int>(header_size)) !=
      static_cast<int>(header_size)) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Save file header is truncated"));
  }

  QJsonParseError parse_error{};
  const QJsonDocument header_doc = QJsonDocument::fromJson(header_bytes, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !header_doc.isObject()) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Save file header is corrupted: %1")
            .arg(parse_error.errorString()));
  }
  const QJsonObject header = header_doc.object();

  Record record;
  record.slot_name = header.value(QStringLiteral("slot_name")).toString();
  record.title = header.value(QStringLiteral("title")).toString();
  record.map_name = header.value(QStringLiteral("map_name")).toString();
  record.map_path = header.value(QStringLiteral("map_path")).toString();
  record.mode = header.value(QStringLiteral("mode")).toString();
  record.campaign_id = header.value(QStringLiteral("campaign_id")).toString();
  record.mission_id = header.value(QStringLiteral("mission_id")).toString();
  record.difficulty = header.value(QStringLiteral("difficulty")).toString();
  if (!slot_kind_from_string(header.value(QStringLiteral("kind")).toString(),
                             record.kind)) {
    record.kind = SlotKind::Manual;
  }
  record.play_time_seconds =
      header.value(QStringLiteral("play_time_seconds")).toDouble(0.0);
  record.created_at = header.value(QStringLiteral("created_at")).toString();
  record.updated_at = header.value(QStringLiteral("updated_at")).toString();
  record.metadata = header.value(QStringLiteral("metadata")).toObject();

  if (!compression_from_string(header.value(QStringLiteral("compression")).toString(),
                               record.world.compression)) {
    return make_error(out_error,
                      QCoreApplication::translate(
                          "SaveFile", "Save file uses an unknown compression format"));
  }
  record.world.raw_size =
      static_cast<qint64>(header.value(QStringLiteral("world_raw_size")).toDouble(0.0));
  record.world.raw_checksum =
      header.value(QStringLiteral("world_raw_checksum")).toString();
  record.world.blob_checksum =
      header.value(QStringLiteral("world_blob_checksum")).toString();

  const auto blob_size = static_cast<qint64>(
      header.value(QStringLiteral("world_blob_size")).toDouble(0.0));
  const auto screenshot_size = static_cast<qint64>(
      header.value(QStringLiteral("screenshot_size")).toDouble(0.0));
  if (blob_size < 0 || screenshot_size < 0 || blob_size > k_max_package_blob ||
      screenshot_size > k_max_package_blob) {
    return make_error(out_error,
                      QCoreApplication::translate(
                          "SaveFile", "Save file declares implausible sizes"));
  }

  const qint64 body_offset = k_magic_size + 8 + static_cast<qint64>(header_size);
  if (bytes.size() - body_offset != blob_size + screenshot_size) {
    return make_error(
        out_error,
        QCoreApplication::translate("SaveFile", "Save file body is truncated"));
  }

  record.world.blob =
      bytes.mid(static_cast<int>(body_offset), static_cast<int>(blob_size));
  record.screenshot = bytes.mid(static_cast<int>(body_offset + blob_size),
                                static_cast<int>(screenshot_size));

  if (!verify_blob(record.world, out_error)) {
    return false;
  }

  const QString screenshot_checksum =
      header.value(QStringLiteral("screenshot_checksum")).toString();
  if (!screenshot_checksum.isEmpty() &&
      checksum_of(record.screenshot) != screenshot_checksum) {
    return make_error(out_error,
                      QCoreApplication::translate(
                          "SaveFile", "Save file preview image is corrupted"));
  }

  out = record;
  return true;
}

} // namespace Game::Systems::Save
