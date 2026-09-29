#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <utility>
#include <vector>

#include "save_sql.h"
#include "save_storage.h"

namespace Game::Systems {

namespace {

using SaveSql::fail;
using SaveSql::TransactionGuard;

struct TableShape {
  const char* table;
  std::vector<const char*> columns;
};

auto required_shape() -> const std::vector<TableShape>& {
  static const std::vector<TableShape> shape = {
      {"saves",
       {"slot_name",
        "title",
        "map_name",
        "map_path",
        "mode",
        "campaign_id",
        "mission_id",
        "difficulty",
        "kind",
        "play_time_seconds",
        "created_at",
        "updated_at",
        "format_version",
        "snapshot_version",
        "compression",
        "world_raw_size",
        "world_raw_checksum",
        "world_blob_checksum",
        "metadata",
        "world_state",
        "screenshot"}},
      {"campaign_progress", {"campaign_id", "completed", "unlocked", "completed_at"}},
      {"campaign_missions",
       {"campaign_id",
        "mission_id",
        "order_index",
        "unlocked",
        "completed",
        "completed_at"}},
      {"mission_results",
       {"mission_id",
        "mode",
        "campaign_id",
        "completed",
        "completion_time",
        "difficulty",
        "result",
        "completed_at",
        "created_at",
        "updated_at"}}};
  return shape;
}
auto schema_statements() -> QStringList {
  return {
      QStringLiteral("CREATE TABLE saves ("
                     "slot_name TEXT PRIMARY KEY NOT NULL, "
                     "title TEXT NOT NULL, "
                     "map_name TEXT NOT NULL, "
                     "map_path TEXT NOT NULL, "
                     "mode TEXT NOT NULL, "
                     "campaign_id TEXT NOT NULL, "
                     "mission_id TEXT NOT NULL, "
                     "difficulty TEXT NOT NULL, "
                     "kind TEXT NOT NULL, "
                     "play_time_seconds REAL NOT NULL, "
                     "created_at TEXT NOT NULL, "
                     "updated_at TEXT NOT NULL, "
                     "format_version INTEGER NOT NULL, "
                     "snapshot_version INTEGER NOT NULL, "
                     "compression TEXT NOT NULL, "
                     "world_raw_size INTEGER NOT NULL, "
                     "world_raw_checksum TEXT NOT NULL, "
                     "world_blob_checksum TEXT NOT NULL, "
                     "metadata BLOB NOT NULL, "
                     "world_state BLOB NOT NULL, "
                     "screenshot BLOB)"),
      QStringLiteral("CREATE INDEX idx_saves_updated_at ON saves (updated_at DESC)"),
      QStringLiteral("CREATE INDEX idx_saves_kind ON saves (kind, updated_at)"),
      QStringLiteral("CREATE TABLE campaign_progress ("
                     "campaign_id TEXT PRIMARY KEY NOT NULL, "
                     "completed INTEGER NOT NULL DEFAULT 0, "
                     "unlocked INTEGER NOT NULL DEFAULT 0, "
                     "completed_at TEXT)"),
      QStringLiteral("CREATE TABLE campaign_missions ("
                     "campaign_id TEXT NOT NULL, "
                     "mission_id TEXT NOT NULL, "
                     "order_index INTEGER NOT NULL, "
                     "unlocked INTEGER NOT NULL DEFAULT 0, "
                     "completed INTEGER NOT NULL DEFAULT 0, "
                     "completed_at TEXT, "
                     "PRIMARY KEY (campaign_id, mission_id))"),
      QStringLiteral("CREATE TABLE mission_results ("
                     "mission_id TEXT NOT NULL, "
                     "mode TEXT NOT NULL, "
                     "campaign_id TEXT NOT NULL, "
                     "completed INTEGER NOT NULL DEFAULT 0, "
                     "completion_time REAL, "
                     "difficulty TEXT, "
                     "result TEXT, "
                     "completed_at TEXT, "
                     "created_at TEXT NOT NULL, "
                     "updated_at TEXT NOT NULL, "
                     "PRIMARY KEY (mission_id, mode, campaign_id))")};
}

} // namespace

auto SaveStorage::passes_integrity_check(QString* out_reason) const -> bool {
  QSqlQuery check(m_database);
  if (!check.exec(QStringLiteral("PRAGMA quick_check(4)"))) {
    if (out_reason != nullptr) {
      *out_reason = check.lastError().text();
    }
    return false;
  }

  QStringList problems;
  while (check.next()) {
    const QString line = check.value(0).toString();
    if (line.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
      problems.append(line);
    }
  }

  if (problems.isEmpty()) {
    return true;
  }
  if (out_reason != nullptr) {
    *out_reason = problems.join(QStringLiteral("; "));
  }
  return false;
}

auto SaveStorage::database_is_empty() const -> bool {
  QSqlQuery query(m_database);
  if (!query.exec(QStringLiteral(
          "SELECT COUNT(*) FROM sqlite_master WHERE type IN ('table', 'view') "
          "AND name NOT LIKE 'sqlite_%'")) ||
      !query.next()) {
    return false;
  }
  return query.value(0).toInt() == 0;
}

auto SaveStorage::schema_shape_is_current() const -> bool {
  for (const auto& [table, columns] : required_shape()) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT * FROM %1 LIMIT 0").arg(table));
    if (!query.exec()) {
      return false;
    }
    const QSqlRecord record = query.record();
    for (const auto* column : columns) {
      if (record.indexOf(QLatin1String(column)) < 0) {
        return false;
      }
    }
  }
  return true;
}

void SaveStorage::backup_before_migration(int from_version) const {
  if (is_memory_database() || !QFile::exists(m_database_path)) {
    return;
  }

  const QString backup_path =
      QStringLiteral("%1.v%2-backup").arg(m_database_path).arg(from_version);
  QFile::remove(backup_path);
  if (QFile::copy(m_database_path, backup_path)) {
    qInfo() << "SaveStorage: copied the pre-migration database to" << backup_path;
  } else {
    qWarning() << "SaveStorage: could not back up" << m_database_path << "before "
               << "migrating from version" << from_version;
  }
}

auto SaveStorage::quarantine_and_recreate(const QString& reason,
                                          QString* out_error) const -> bool {
  qWarning() << "SaveStorage: the save database at" << m_database_path
             << "cannot be used:" << reason;

  if (is_memory_database()) {

    close_connection();
    return open(out_error) && create_fresh_schema(out_error);
  }

  if (!move_database_aside(reason, out_error)) {
    return false;
  }

  return open(out_error) && create_fresh_schema(out_error);
}

auto SaveStorage::move_database_aside(const QString& reason,
                                      QString* out_error) const -> bool {
  close_connection();

  const QString stamp =
      QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
  QString quarantine = QStringLiteral("%1.unreadable-%2").arg(m_database_path, stamp);
  for (int attempt = 2; QFile::exists(quarantine) && attempt < 100; ++attempt) {
    quarantine =
        QStringLiteral("%1.unreadable-%2-%3").arg(m_database_path, stamp).arg(attempt);
  }

  if (!QFile::rename(m_database_path, quarantine)) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate(
                       "SaveStorage",
                       "The save database is unreadable and could not be moved "
                       "aside: %1")
                       .arg(reason);
    }
    return false;
  }

  for (const QString& sidecar :
       {QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")}) {
    const QString source = m_database_path + sidecar;
    if (QFile::exists(source) && !QFile::rename(source, quarantine + sidecar)) {
      QFile::remove(source);
    }
  }

  m_quarantined_path = quarantine;
  qWarning() << "SaveStorage: moved it to" << quarantine << "and started a fresh one";

  return true;
}

auto SaveStorage::create_fresh_schema(QString* out_error) const -> bool {
  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }
  if (!create_schema(out_error) || !stamp_schema_version(out_error)) {
    transaction.rollback();
    return false;
  }
  return transaction.commit(out_error);
}

auto SaveStorage::stamp_schema_version(QString* out_error) const -> bool {
  QSqlQuery set_version(m_database);
  if (!set_version.exec(QStringLiteral("PRAGMA user_version = %1")
                            .arg(Save::k_database_schema_version))) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to record schema version"),
        set_version.lastError());
  }
  return true;
}

auto SaveStorage::read_schema_version(int& version, QString* out_error) const -> bool {
  QSqlQuery version_query(m_database);
  if (!version_query.exec(QStringLiteral("PRAGMA user_version")) ||
      !version_query.next()) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to read schema version"),
        version_query.lastError());
  }
  version = version_query.value(0).toInt();

  version_query.finish();
  return true;
}

auto SaveStorage::ensure_schema(QString* out_error) const -> bool {
  QString integrity_reason;
  if (!passes_integrity_check(&integrity_reason)) {
    return quarantine_and_recreate(
        QCoreApplication::translate("SaveStorage", "integrity check failed: %1")
            .arg(integrity_reason),
        out_error);
  }

  int version = 0;
  if (!read_schema_version(version, out_error)) {
    return false;
  }

  if (version == 0 && database_is_empty()) {
    return create_fresh_schema(out_error);
  }

  if (version > Save::k_database_schema_version || version <= 0) {
    return quarantine_and_recreate(
        version > Save::k_database_schema_version
            ? QCoreApplication::translate(
                  "SaveStorage",
                  "it was written by a newer version of the game (schema %1)")
                  .arg(version)
            : QCoreApplication::translate("SaveStorage",
                                          "it is not a Standard of Iron save database"),
        out_error);
  }

  if (version < Save::k_database_schema_version) {
    backup_before_migration(version);
    if (!migrate_schema(version, out_error)) {
      return quarantine_and_recreate(
          QCoreApplication::translate("SaveStorage",
                                      "it could not be upgraded from schema %1: %2")
              .arg(version)
              .arg(out_error != nullptr ? *out_error : QString()),
          out_error);
    }
  }

  if (!schema_shape_is_current()) {
    return quarantine_and_recreate(
        QCoreApplication::translate("SaveStorage", "its tables do not match schema %1")
            .arg(Save::k_database_schema_version),
        out_error);
  }

  return true;
}

auto SaveStorage::migrate_schema(int from_version, QString* out_error) const -> bool {
  qInfo() << "SaveStorage: upgrading the save database from schema" << from_version
          << "to" << Save::k_database_schema_version;

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  int version = from_version;

  if (version < 3) {
    QSqlQuery add_column(m_database);
    if (!add_column.exec(QStringLiteral("ALTER TABLE saves ADD COLUMN "
                                        "snapshot_version INTEGER NOT NULL DEFAULT %1")
                             .arg(version))) {
      fail(out_error,
           QCoreApplication::translate("SaveStorage",
                                       "Failed to add the snapshot version column"),
           add_column.lastError());
      transaction.rollback();
      return false;
    }
    version = 3;
  }

  QSqlQuery set_version(m_database);
  if (!set_version.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to record schema version"),
         set_version.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::create_schema(QString* out_error) const -> bool {
  const QStringList statements = schema_statements();

  for (const QString& statement : statements) {
    QSqlQuery query(m_database);
    if (!query.exec(statement)) {
      return fail(
          out_error,
          QCoreApplication::translate("SaveStorage", "Failed to create save schema"),
          query.lastError());
    }
  }

  return true;
}

} // namespace Game::Systems
