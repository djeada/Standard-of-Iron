#pragma once

#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QString>

namespace Game::Systems::Save {

inline constexpr int k_format_version = 1;
inline constexpr int k_schema_version = 1;

enum class Compression {
  None,
  Zlib
};

enum class SlotKind {
  Manual,
  Quicksave,
  Autosave
};

auto compression_to_string(Compression compression) -> QString;
auto compression_from_string(const QString& value, Compression& out) -> bool;

auto slot_kind_to_string(SlotKind kind) -> QString;
auto slot_kind_from_string(const QString& value, SlotKind& out) -> bool;

auto checksum_of(const QByteArray& bytes) -> QString;

struct Payload {
  QByteArray blob;
  Compression compression = Compression::Zlib;
  qint64 raw_size = 0;
  QString raw_checksum;
  QString blob_checksum;

  [[nodiscard]] auto is_empty() const -> bool { return blob.isEmpty(); }
};

auto pack(const QByteArray& raw,
          Compression compression = Compression::Zlib) -> Payload;

auto unpack(const Payload& payload, QByteArray& out_raw, QString* out_error) -> bool;

auto verify_blob(const Payload& payload, QString* out_error) -> bool;

struct Record {
  QString slot_name;
  QString title;
  QString map_name;
  QString map_path;
  QString mode;
  QString campaign_id;
  QString mission_id;
  QString difficulty;
  SlotKind kind = SlotKind::Manual;
  double play_time_seconds = 0.0;
  QString created_at;
  QString updated_at;
  QJsonObject metadata;
  Payload world;
  QByteArray screenshot;
};

auto encode_package(const Record& record) -> QByteArray;
auto decode_package(const QByteArray& bytes, Record& out, QString* out_error) -> bool;

auto package_file_suffix() -> QString;

inline constexpr int k_preview_width = 320;

auto encode_preview(const QImage& frame, int width = k_preview_width) -> QByteArray;

auto sanitize_file_stem(const QString& name) -> QString;

} // namespace Game::Systems::Save
