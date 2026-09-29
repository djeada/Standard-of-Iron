#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <utility>
#include <vector>

#include "save_sql.h"
#include "save_storage.h"

namespace Game::Systems {

namespace {

using SaveSql::fail;
using SaveSql::now_iso;
using SaveSql::text;
using SaveSql::TransactionGuard;

auto prepare_upsert(QSqlQuery& query, QString* out_error) -> bool {
  if (!query.prepare(QStringLiteral(
          "INSERT INTO saves (slot_name, title, map_name, map_path, mode, "
          "campaign_id, mission_id, difficulty, kind, play_time_seconds, "
          "created_at, updated_at, format_version, snapshot_version, compression, "
          "world_raw_size, world_raw_checksum, world_blob_checksum, metadata, "
          "world_state, screenshot) "
          "VALUES (:slot_name, :title, :map_name, :map_path, :mode, :campaign_id, "
          ":mission_id, :difficulty, :kind, :play_time_seconds, :created_at, "
          ":updated_at, :format_version, :snapshot_version, :compression, "
          ":world_raw_size, :world_raw_checksum, :world_blob_checksum, :metadata, "
          ":world_state, :screenshot) "
          "ON CONFLICT(slot_name) DO UPDATE SET "
          "title = excluded.title, "
          "map_name = excluded.map_name, "
          "map_path = excluded.map_path, "
          "mode = excluded.mode, "
          "campaign_id = excluded.campaign_id, "
          "mission_id = excluded.mission_id, "
          "difficulty = excluded.difficulty, "
          "kind = excluded.kind, "
          "play_time_seconds = excluded.play_time_seconds, "
          "updated_at = excluded.updated_at, "
          "format_version = excluded.format_version, "
          "snapshot_version = excluded.snapshot_version, "
          "compression = excluded.compression, "
          "world_raw_size = excluded.world_raw_size, "
          "world_raw_checksum = excluded.world_raw_checksum, "
          "world_blob_checksum = excluded.world_blob_checksum, "
          "metadata = excluded.metadata, "
          "world_state = excluded.world_state, "
          "screenshot = excluded.screenshot"))) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to prepare save query"),
        query.lastError());
  }
  return true;
}

void bind_record(QSqlQuery& query, const Save::Record& record) {
  const QString timestamp = record.updated_at.isEmpty() ? now_iso() : record.updated_at;
  const QString created = record.created_at.isEmpty() ? timestamp : record.created_at;

  query.bindValue(QStringLiteral(":slot_name"), record.slot_name);
  query.bindValue(QStringLiteral(":title"), text(record.title));
  query.bindValue(QStringLiteral(":map_name"), text(record.map_name));
  query.bindValue(QStringLiteral(":map_path"), text(record.map_path));
  query.bindValue(QStringLiteral(":mode"), text(record.mode));
  query.bindValue(QStringLiteral(":campaign_id"), text(record.campaign_id));
  query.bindValue(QStringLiteral(":mission_id"), text(record.mission_id));
  query.bindValue(QStringLiteral(":difficulty"), text(record.difficulty));
  query.bindValue(QStringLiteral(":kind"), Save::slot_kind_to_string(record.kind));
  query.bindValue(QStringLiteral(":play_time_seconds"), record.play_time_seconds);
  query.bindValue(QStringLiteral(":created_at"), created);
  query.bindValue(QStringLiteral(":updated_at"), timestamp);
  query.bindValue(QStringLiteral(":format_version"), Save::k_format_version);
  query.bindValue(QStringLiteral(":snapshot_version"), record.snapshot_version);
  query.bindValue(QStringLiteral(":compression"),
                  Save::compression_to_string(record.world.compression));
  query.bindValue(QStringLiteral(":world_raw_size"),
                  static_cast<qint64>(record.world.raw_size));
  query.bindValue(QStringLiteral(":world_raw_checksum"),
                  text(record.world.raw_checksum));
  query.bindValue(QStringLiteral(":world_blob_checksum"),
                  text(record.world.blob_checksum));
  query.bindValue(QStringLiteral(":metadata"),
                  QJsonDocument(record.metadata).toJson(QJsonDocument::Compact));
  query.bindValue(QStringLiteral(":world_state"), record.world.blob);
  query.bindValue(QStringLiteral(":screenshot"), record.screenshot);
}

auto upsert_slot(const QSqlDatabase& database,
                 const Save::Record& record,
                 QString* out_error) -> bool {
  QSqlQuery query(database);
  if (!prepare_upsert(query, out_error)) {
    return false;
  }
  bind_record(query, record);

  if (!query.exec()) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to persist save slot"),
        query.lastError());
  }
  return true;
}

auto select_slot_row(QSqlQuery& query,
                     const QString& slot_name,
                     QString* out_error) -> bool {
  query.prepare(QStringLiteral(
      "SELECT title, map_name, map_path, mode, campaign_id, mission_id, "
      "difficulty, kind, play_time_seconds, created_at, updated_at, "
      "compression, world_raw_size, world_raw_checksum, world_blob_checksum, "
      "metadata, world_state, screenshot, format_version, snapshot_version "
      "FROM saves WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec()) {
    return fail(out_error,
                QCoreApplication::translate("SaveStorage", "Failed to read save slot"),
                query.lastError());
  }

  if (!query.next()) {
    if (out_error != nullptr) {
      *out_error =
          QCoreApplication::translate("SaveStorage", "Save slot '%1' not found")
              .arg(slot_name);
    }
    return false;
  }
  return true;
}

auto check_row_versions(const QSqlQuery& query,
                        const QString& slot_name,
                        QString* out_error) -> bool {
  const int format_version = query.value(18).toInt();
  if (format_version != Save::k_format_version) {
    if (out_error != nullptr) {
      *out_error =
          QCoreApplication::translate(
              "SaveStorage", "Save slot '%1' uses unsupported format version %2")
              .arg(slot_name)
              .arg(format_version);
    }
    return false;
  }

  const int snapshot_version = query.value(19).toInt();
  if (snapshot_version != Save::k_snapshot_version) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
                       "SaveStorage",
                       "'%1' was saved by a different version of Standard of Iron "
                       "and cannot be loaded by this one. It has been left alone.")
                       .arg(slot_name);
    }
    return false;
  }
  return true;
}

auto decode_record(const QSqlQuery& query,
                   const QString& slot_name,
                   Save::Record& out_record,
                   QString* out_error) -> bool {
  const int snapshot_version = query.value(19).toInt();
  Save::Record record;
  record.snapshot_version = snapshot_version;
  record.slot_name = slot_name;
  record.title = query.value(0).toString();
  record.map_name = query.value(1).toString();
  record.map_path = query.value(2).toString();
  record.mode = query.value(3).toString();
  record.campaign_id = query.value(4).toString();
  record.mission_id = query.value(5).toString();
  record.difficulty = query.value(6).toString();
  if (!Save::slot_kind_from_string(query.value(7).toString(), record.kind)) {
    record.kind = Save::SlotKind::Manual;
  }
  record.play_time_seconds = query.value(8).toDouble();
  record.created_at = query.value(9).toString();
  record.updated_at = query.value(10).toString();

  if (!Save::compression_from_string(query.value(11).toString(),
                                     record.world.compression)) {
    if (out_error != nullptr) {
      *out_error =
          QCoreApplication::translate(
              "SaveStorage", "Save slot '%1' uses an unknown compression format")
              .arg(slot_name);
    }
    return false;
  }

  record.world.raw_size = query.value(12).toLongLong();
  record.world.raw_checksum = query.value(13).toString();
  record.world.blob_checksum = query.value(14).toString();
  record.metadata = QJsonDocument::fromJson(query.value(15).toByteArray()).object();
  record.world.blob = query.value(16).toByteArray();
  record.screenshot = query.value(17).toByteArray();

  out_record = record;
  return true;
}

auto slot_summary(const QSqlQuery& query) -> QVariantMap {
  QVariantMap slot;
  slot.insert(QStringLiteral("slot_name"), query.value(0).toString());
  slot.insert(QStringLiteral("title"), query.value(1).toString());
  slot.insert(QStringLiteral("map_name"), query.value(2).toString());
  slot.insert(QStringLiteral("mode"), query.value(3).toString());
  slot.insert(QStringLiteral("campaign_id"), query.value(4).toString());
  slot.insert(QStringLiteral("mission_id"), query.value(5).toString());
  slot.insert(QStringLiteral("difficulty"), query.value(6).toString());
  slot.insert(QStringLiteral("kind"), query.value(7).toString());
  slot.insert(QStringLiteral("play_time_seconds"), query.value(8).toDouble());
  slot.insert(QStringLiteral("timestamp"), query.value(9).toString());
  slot.insert(QStringLiteral("uncompressed_size"), query.value(10).toLongLong());
  slot.insert(QStringLiteral("stored_size"), query.value(11).toLongLong());
  slot.insert(
      QStringLiteral("metadata"),
      QJsonDocument::fromJson(query.value(12).toByteArray()).object().toVariantMap());

  const QByteArray screenshot = query.value(13).toByteArray();
  slot.insert(QStringLiteral("thumbnail"),
              screenshot.isEmpty() ? QString()
                                   : QString::fromLatin1(screenshot.toBase64()));

  const int snapshot_version = query.value(14).toInt();
  slot.insert(QStringLiteral("snapshot_version"), snapshot_version);

  slot.insert(QStringLiteral("loadable"), snapshot_version == Save::k_snapshot_version);
  slot.insert(QStringLiteral("mission_id"), query.value(15).toString());
  slot.insert(QStringLiteral("created_at"), query.value(16).toString());
  return slot;
}

} // namespace

auto SaveStorage::check_slot_writable(const Save::Record& record,
                                      QString* out_error) const -> bool {
  if (const QString rejection = Save::slot_name_rejection(record.slot_name);
      !rejection.isEmpty() && record.kind == Save::SlotKind::Manual) {
    if (out_error != nullptr) {
      *out_error = rejection;
    }
    return false;
  }
  if (record.slot_name.trimmed().isEmpty()) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
          "SaveStorage", "Refusing to write a save with an empty slot name");
    }
    return false;
  }

  if (record.kind != Save::SlotKind::Manual) {
    Save::SlotKind existing = Save::SlotKind::Manual;
    if (slot_kind(record.slot_name, existing) && existing != record.kind) {
      if (out_error != nullptr) {
        *out_error = QCoreApplication::translate(
                         "SaveStorage",
                         "'%1' holds a save that was not written by this "
                         "rotation, so it will not be overwritten automatically. "
                         "Delete it from the Load menu to free the slot.")
                         .arg(record.slot_name);
      }
      return false;
    }
  }
  return true;
}

auto SaveStorage::write_slot(const Save::Record& record, QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  if (!check_slot_writable(record, out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  if (!upsert_slot(m_database, record, out_error)) {
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  if (!read_back_matches(record, out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::read_back_matches(const Save::Record& record,
                                    QString* out_error) const -> bool {
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("SELECT world_state, world_raw_checksum, "
                               "snapshot_version FROM saves WHERE "
                               "slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), record.slot_name);

  if (!query.exec() || !query.next()) {
    return fail(out_error,
                QCoreApplication::translate(
                    "SaveStorage", "The save was written but could not be read back"),
                query.lastError());
  }

  const QByteArray stored = query.value(0).toByteArray();
  if (stored != record.world.blob) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
          "SaveStorage",
          "The save did not survive being written to disk. Nothing was lost in "
          "the match; try saving again.");
    }
    return false;
  }
  if (query.value(1).toString() != record.world.raw_checksum ||
      query.value(2).toInt() != record.snapshot_version) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
          "SaveStorage", "The save was written with the wrong header; try again.");
    }
    return false;
  }
  return true;
}

auto SaveStorage::read_slot(const QString& slot_name,
                            Save::Record& out_record,
                            QString* out_error) const -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  if (!select_slot_row(query, slot_name, out_error)) {
    return false;
  }
  if (!check_row_versions(query, slot_name, out_error)) {
    return false;
  }
  return decode_record(query, slot_name, out_record, out_error);
}

auto SaveStorage::verify_slot(const QString& slot_name,
                              QString* out_error) const -> bool {
  Save::Record record;
  if (!read_slot(slot_name, record, out_error)) {
    return false;
  }

  QByteArray world_bytes;
  if (!Save::unpack(record.world, world_bytes, out_error)) {
    return false;
  }

  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(world_bytes, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
                       "SaveStorage", "'%1' does not contain a readable world: %2")
                       .arg(slot_name, parse_error.errorString());
    }
    return false;
  }
  if (!document.object().contains(QStringLiteral("entities"))) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
                       "SaveStorage", "'%1' is missing the units it should contain")
                       .arg(slot_name);
    }
    return false;
  }
  return true;
}

auto SaveStorage::slot_kind(const QString& slot_name,
                            Save::SlotKind& out_kind,
                            QString* out_error) const -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("SELECT kind FROM saves WHERE slot_name = :slot_name LIMIT 1"));
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec() || !query.next()) {
    return false;
  }
  if (!Save::slot_kind_from_string(query.value(0).toString(), out_kind)) {
    out_kind = Save::SlotKind::Manual;
  }
  return true;
}

auto SaveStorage::list_slots(QString* out_error) const -> QVariantList {
  QVariantList result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  if (!query.exec(QStringLiteral(
          "SELECT slot_name, title, map_name, mode, campaign_id, mission_id, "
          "difficulty, kind, play_time_seconds, updated_at, world_raw_size, "
          "length(world_state), metadata, screenshot, snapshot_version, "
          "mission_id, created_at "
          "FROM saves ORDER BY datetime(updated_at) DESC"))) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to enumerate save slots"),
         query.lastError());
    return result;
  }

  while (query.next()) {
    result.append(slot_summary(query));
  }

  return result;
}

auto SaveStorage::slot_names_by_kind(Save::SlotKind kind,
                                     QString* out_error) const -> QStringList {
  QStringList result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("SELECT slot_name FROM saves WHERE kind = :kind "
                               "ORDER BY datetime(updated_at) ASC"));
  query.bindValue(QStringLiteral(":kind"), Save::slot_kind_to_string(kind));

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to enumerate save slots"),
         query.lastError());
    return result;
  }

  while (query.next()) {
    result.append(query.value(0).toString());
  }
  return result;
}

auto SaveStorage::slot_exists(const QString& slot_name,
                              QString* out_error) const -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("SELECT 1 FROM saves WHERE slot_name = :slot_name LIMIT 1"));
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec()) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to look up save slot"),
        query.lastError());
  }
  return query.next();
}

auto SaveStorage::update_screenshot(const QString& slot_name,
                                    const QByteArray& screenshot,
                                    QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "UPDATE saves SET screenshot = :screenshot WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":screenshot"), screenshot);
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to store save preview"),
         query.lastError());
    transaction.rollback();
    return false;
  }

  if (query.numRowsAffected() == 0) {
    if (out_error != nullptr) {
      *out_error =
          QCoreApplication::translate("SaveStorage", "Save slot '%1' not found")
              .arg(slot_name);
    }
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::delete_slot(const QString& slot_name, QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("DELETE FROM saves WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to delete save slot"),
         query.lastError());
    transaction.rollback();
    return false;
  }

  if (query.numRowsAffected() == 0) {
    if (out_error != nullptr) {
      *out_error =
          QCoreApplication::translate("SaveStorage", "Save slot '%1' not found")
              .arg(slot_name);
    }
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

} // namespace Game::Systems
