#include "save_storage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

#include <utility>
#include <vector>

#include "../map/campaign_definition.h"
#include "../map/campaign_loader.h"
#include "../util/asset_text.h"

namespace Game::Systems {

namespace {
constexpr const char* k_driver_name = "QSQLITE";

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

auto build_connection_name(const SaveStorage* instance) -> QString {
  return QStringLiteral("SaveStorage_%1")
      .arg(reinterpret_cast<quintptr>(instance), 0, 16);
}

auto last_error_string(const QSqlError& error) -> QString {
  if (error.type() == QSqlError::NoError) {
    return {};
  }
  return error.text();
}

auto fail(QString* out_error, const QString& context, const QSqlError& error) -> bool {
  if (out_error != nullptr) {
    *out_error = QStringLiteral("%1: %2").arg(context, last_error_string(error));
  }
  return false;
}

auto now_iso() -> QString {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

auto text(const QString& value) -> QString {
  return value.isNull() ? QString::fromLatin1("") : value;
}

class TransactionGuard {
public:
  explicit TransactionGuard(QSqlDatabase& database)
      : m_database(database) {}

  TransactionGuard(const TransactionGuard&) = delete;
  auto operator=(const TransactionGuard&) -> TransactionGuard& = delete;

  auto begin(QString* out_error) -> bool {
    if (!m_database.transaction()) {
      return fail(
          out_error,
          QCoreApplication::translate("SaveStorage", "Failed to begin transaction"),
          m_database.lastError());
    }
    m_active = true;
    return true;
  }

  auto commit(QString* out_error) -> bool {
    if (!m_active) {
      return true;
    }

    if (!m_database.commit()) {
      const bool result = fail(
          out_error,
          QCoreApplication::translate("SaveStorage", "Failed to commit transaction"),
          m_database.lastError());
      rollback();
      return result;
    }

    m_active = false;
    return true;
  }

  void rollback() {
    if (m_active) {
      m_database.rollback();
      m_active = false;
    }
  }

  ~TransactionGuard() { rollback(); }

private:
  QSqlDatabase& m_database;
  bool m_active = false;
};

auto load_campaign_definitions() -> std::vector<Game::Campaign::CampaignDefinition> {
  std::vector<Game::Campaign::CampaignDefinition> campaigns;

  const QStringList search_paths = {
      QStringLiteral("assets/campaigns"),
      QStringLiteral("../assets/campaigns"),
      QStringLiteral("../../assets/campaigns"),
      QCoreApplication::applicationDirPath() + QStringLiteral("/assets/campaigns"),
      QCoreApplication::applicationDirPath() + QStringLiteral("/../assets/campaigns")};

  for (const QString& campaigns_path : search_paths) {
    QDir const campaigns_dir(campaigns_path);
    if (!campaigns_dir.exists()) {
      continue;
    }

    const QStringList campaign_files =
        campaigns_dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    if (campaign_files.isEmpty()) {
      continue;
    }

    qInfo() << "Loading campaigns from filesystem:" << campaigns_dir.absolutePath();
    for (const auto& campaign_file : campaign_files) {
      Game::Campaign::CampaignDefinition campaign;
      QString error;
      if (!Game::Campaign::CampaignLoader::load_from_json_file(
              campaigns_dir.filePath(campaign_file), campaign, &error)) {
        qWarning() << "Failed to load campaign" << campaign_file << ":" << error;
        continue;
      }
      campaigns.push_back(std::move(campaign));
    }

    if (!campaigns.empty()) {
      return campaigns;
    }
  }

  qInfo() << "Loading campaigns from Qt resources";
  const QStringList known_campaigns = {QStringLiteral("second_punic_war")};
  for (const auto& campaign_name : known_campaigns) {
    const QString campaign_path =
        QStringLiteral(":/assets/campaigns/%1.json").arg(campaign_name);
    if (!QFile::exists(campaign_path)) {
      qWarning() << "Campaign resource does not exist:" << campaign_path;
      continue;
    }

    Game::Campaign::CampaignDefinition campaign;
    QString error;
    if (!Game::Campaign::CampaignLoader::load_from_json_file(
            campaign_path, campaign, &error)) {
      qWarning() << "Failed to load campaign from resources" << campaign_name << ":"
                 << error;
      continue;
    }
    campaigns.push_back(std::move(campaign));
  }

  return campaigns;
}

auto build_campaign_entry(const Game::Campaign::CampaignDefinition& campaign,
                          const QVariantList& missions_progress) -> QVariantMap {
  QVariantMap campaign_map;
  campaign_map.insert(QStringLiteral("id"), campaign.id);
  campaign_map.insert(
      QStringLiteral("title"),
      Game::Util::tr_asset(Game::Util::k_campaigns_context, campaign.title));
  campaign_map.insert(
      QStringLiteral("description"),
      Game::Util::tr_asset(Game::Util::k_campaigns_context, campaign.description));
  campaign_map.insert(QStringLiteral("unlocked"), true);

  bool all_completed = true;
  QVariantList missions_list;
  for (const auto& mission : campaign.missions) {
    QVariantMap mission_map;
    mission_map.insert(QStringLiteral("mission_id"), mission.mission_id);
    mission_map.insert(QStringLiteral("order_index"), mission.order_index);
    if (mission.intro_text.has_value()) {
      mission_map.insert(
          QStringLiteral("intro_text"),
          Game::Util::tr_asset(Game::Util::k_campaigns_context, *mission.intro_text));
    }
    if (mission.outro_text.has_value()) {
      mission_map.insert(
          QStringLiteral("outro_text"),
          Game::Util::tr_asset(Game::Util::k_campaigns_context, *mission.outro_text));
    }
    if (mission.difficulty_modifier.has_value()) {
      mission_map.insert(QStringLiteral("difficulty_modifier"),
                         *mission.difficulty_modifier);
    }

    if (mission.world_region_id.has_value()) {
      mission_map.insert(QStringLiteral("world_region_id"), *mission.world_region_id);
    }

    bool unlocked = mission.order_index == 0;
    bool completed = false;
    for (const QVariant& progress_var : missions_progress) {
      const QVariantMap progress = progress_var.toMap();
      if (progress[QStringLiteral("mission_id")].toString() == mission.mission_id) {
        unlocked = progress[QStringLiteral("unlocked")].toBool();
        completed = progress[QStringLiteral("completed")].toBool();
        break;
      }
    }

    mission_map.insert(QStringLiteral("unlocked"), unlocked);
    mission_map.insert(QStringLiteral("completed"), completed);
    missions_list.append(mission_map);

    if (!completed) {
      all_completed = false;
    }
  }

  campaign_map.insert(QStringLiteral("completed"), all_completed);
  campaign_map.insert(QStringLiteral("missions"), missions_list);
  return campaign_map;
}

} // namespace

SaveStorage::SaveStorage(QString database_path)
    : m_database_path(std::move(database_path))
    , m_connection_name(build_connection_name(this)) {
}

SaveStorage::~SaveStorage() {
  if (m_database.isValid()) {
    if (m_database.isOpen()) {
      m_database.close();
    }
    const QString connection_name = m_connection_name;
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection_name);
  }
}

auto SaveStorage::initialize(QString* out_error) const -> bool {
  if (m_initialized && m_database.isValid() && m_database.isOpen()) {
    return true;
  }

  QString error;
  bool ready = open(&error);

  if (!ready && !is_memory_database() && QFile::exists(m_database_path)) {
    QString quarantine_error;
    if (quarantine_and_recreate(error, &quarantine_error)) {
      m_health_error.clear();
      m_initialized = true;
      return true;
    }
    error = quarantine_error;
  }

  if (!ready || !ensure_schema(&error)) {
    m_health_error = error.isEmpty()
                         ? QCoreApplication::translate(
                               "SaveStorage", "The save database could not be opened.")
                         : error;
    if (out_error != nullptr) {
      *out_error = m_health_error;
    }
    return false;
  }

  m_health_error.clear();
  m_initialized = true;
  return true;
}

auto SaveStorage::is_memory_database() const -> bool {
  return m_database_path.startsWith(QStringLiteral(":memory:")) ||
         m_database_path.contains(QStringLiteral("mode=memory"));
}

void SaveStorage::close_connection() const {
  if (m_database.isValid()) {
    if (m_database.isOpen()) {
      m_database.close();
    }
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection_name);
  }
  m_initialized = false;
}

auto SaveStorage::open(QString* out_error) const -> bool {
  if (m_database.isValid() && m_database.isOpen()) {
    return true;
  }

  if (!m_database.isValid()) {
    m_database = QSqlDatabase::addDatabase(k_driver_name, m_connection_name);
    m_database.setDatabaseName(m_database_path);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=15000"));
  }

  if (!m_database.open()) {
    return fail(
        out_error,
        QCoreApplication::translate("SaveStorage", "Failed to open save database"),
        m_database.lastError());
  }

  QSqlQuery pragma(m_database);
  if (pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL")) && pragma.next()) {
    const QString mode = pragma.value(0).toString();
    if (mode.compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0 &&
        !is_memory_database()) {
      qWarning() << "SaveStorage: journal_mode is" << mode
                 << "not WAL; a crash mid-save may leave the database behind";
    }
  }
  pragma.finish();
  pragma.exec(QStringLiteral("PRAGMA synchronous=FULL"));
  pragma.finish();
  constexpr int k_synchronous_full = 2;
  if (pragma.exec(QStringLiteral("PRAGMA synchronous")) && pragma.next() &&
      pragma.value(0).toInt() != k_synchronous_full) {
    qWarning() << "SaveStorage: synchronous is" << pragma.value(0).toInt()
               << "not FULL; a commit may not have reached the disk when it "
                  "reports success";
  }
  pragma.finish();
  pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
  return true;
}

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

  return open(out_error) && create_fresh_schema(out_error);
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

auto SaveStorage::ensure_schema(QString* out_error) const -> bool {
  QString integrity_reason;
  if (!passes_integrity_check(&integrity_reason)) {
    return quarantine_and_recreate(
        QCoreApplication::translate("SaveStorage", "integrity check failed: %1")
            .arg(integrity_reason),
        out_error);
  }

  int version = 0;
  {
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
  }

  if (version == 0 && database_is_empty()) {
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
  const QStringList statements = {
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

auto SaveStorage::write_slot(const Save::Record& record, QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

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

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
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

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to persist save slot"),
         query.lastError());
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

    slot.insert(QStringLiteral("loadable"),
                snapshot_version == Save::k_snapshot_version);
    slot.insert(QStringLiteral("mission_id"), query.value(15).toString());
    slot.insert(QStringLiteral("created_at"), query.value(16).toString());

    result.append(slot);
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

auto SaveStorage::list_campaigns(QString* out_error) -> QVariantList {
  QVariantList result;
  if (!initialize(out_error)) {
    return result;
  }

  const auto campaigns = load_campaign_definitions();
  for (const auto& campaign : campaigns) {
    QString db_error;
    if (!ensure_campaign_missions_in_db(campaign, &db_error)) {
      qWarning() << "Failed to initialize campaign missions in DB for" << campaign.id
                 << ":" << db_error;
      continue;
    }
    result.append(
        build_campaign_entry(campaign, get_campaign_mission_progress(campaign.id)));
  }

  if (result.isEmpty()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("No campaigns found");
    }
    qWarning() << "No campaigns found in filesystem or Qt resources";
  } else {
    qInfo() << "Successfully loaded" << result.size() << "campaign(s)";
  }

  return result;
}

auto SaveStorage::get_campaign_progress(const QString& campaign_id,
                                        QString* out_error) const -> QVariantMap {
  QVariantMap result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("SELECT completed, unlocked, completed_at FROM campaign_progress "
                     "WHERE campaign_id = :campaign_id"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to get campaign progress"),
         query.lastError());
    return result;
  }

  if (query.next()) {
    result.insert(QStringLiteral("completed"), query.value(0).toInt() != 0);
    result.insert(QStringLiteral("unlocked"), query.value(1).toInt() != 0);
    result.insert(QStringLiteral("completedAt"), query.value(2).toString());
  }

  return result;
}

auto SaveStorage::mark_campaign_completed(const QString& campaign_id,
                                          QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("INSERT INTO campaign_progress (campaign_id, completed, "
                               "unlocked, completed_at) "
                               "VALUES (:campaign_id, 1, 1, :completed_at) "
                               "ON CONFLICT(campaign_id) DO UPDATE SET "
                               "completed = 1, unlocked = 1, "
                               "completed_at = excluded.completed_at"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  query.bindValue(QStringLiteral(":completed_at"), now_iso());

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to mark campaign as completed"),
         query.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::save_mission_result(const QString& mission_id,
                                      const QString& mode,
                                      const QString& campaign_id,
                                      bool completed,
                                      const QString& result,
                                      const QString& difficulty,
                                      float completion_time,
                                      QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  const QString timestamp = now_iso();

  QSqlQuery query(m_database);
  if (!query.prepare(QStringLiteral(
          "INSERT INTO mission_results (mission_id, mode, campaign_id, completed, "
          "completion_time, difficulty, result, completed_at, created_at, "
          "updated_at) "
          "VALUES (:mission_id, :mode, :campaign_id, :completed, :completion_time, "
          ":difficulty, :result, :completed_at, :created_at, :updated_at) "
          "ON CONFLICT(mission_id, mode, campaign_id) DO UPDATE SET "
          "completed = excluded.completed, "
          "completion_time = excluded.completion_time, "
          "difficulty = excluded.difficulty, "
          "result = excluded.result, "
          "completed_at = excluded.completed_at, "
          "updated_at = excluded.updated_at"))) {
    return fail(out_error,
                QCoreApplication::translate("SaveStorage",
                                            "Failed to prepare mission result insert"),
                query.lastError());
  }

  query.bindValue(QStringLiteral(":mission_id"), text(mission_id));
  query.bindValue(QStringLiteral(":mode"), text(mode));
  query.bindValue(QStringLiteral(":campaign_id"), text(campaign_id));
  query.bindValue(QStringLiteral(":completed"), completed ? 1 : 0);
  query.bindValue(QStringLiteral(":completion_time"), completion_time);
  query.bindValue(QStringLiteral(":difficulty"), difficulty);
  query.bindValue(QStringLiteral(":result"), result);
  query.bindValue(QStringLiteral(":completed_at"), completed ? timestamp : QString());
  query.bindValue(QStringLiteral(":created_at"), timestamp);
  query.bindValue(QStringLiteral(":updated_at"), timestamp);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to save mission result"),
         query.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::get_mission_progress(const QString& mission_id,
                                       QString* out_error) const -> QVariantMap {
  QVariantMap result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT mode, campaign_id, completed, completion_time, difficulty, "
      "result, completed_at FROM mission_results "
      "WHERE mission_id = :mission_id ORDER BY updated_at DESC LIMIT 1"));
  query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to get mission progress"),
         query.lastError());
    return result;
  }

  if (query.next()) {
    result.insert(QStringLiteral("mode"), query.value(0).toString());
    result.insert(QStringLiteral("campaign_id"), query.value(1).toString());
    result.insert(QStringLiteral("completed"), query.value(2).toInt() != 0);
    result.insert(QStringLiteral("completion_time"), query.value(3).toDouble());
    result.insert(QStringLiteral("difficulty"), query.value(4).toString());
    result.insert(QStringLiteral("result"), query.value(5).toString());
    result.insert(QStringLiteral("completed_at"), query.value(6).toString());
  }

  return result;
}

auto SaveStorage::get_campaign_mission_progress(
    const QString& campaign_id, QString* out_error) const -> QVariantList {
  QVariantList result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT mission_id, order_index, unlocked, completed, completed_at "
      "FROM campaign_missions "
      "WHERE campaign_id = :campaign_id ORDER BY order_index ASC"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to get campaign mission progress"),
         query.lastError());
    return result;
  }

  while (query.next()) {
    QVariantMap mission;
    mission.insert(QStringLiteral("mission_id"), query.value(0).toString());
    mission.insert(QStringLiteral("order_index"), query.value(1).toInt());
    mission.insert(QStringLiteral("unlocked"), query.value(2).toInt() != 0);
    mission.insert(QStringLiteral("completed"), query.value(3).toInt() != 0);
    mission.insert(QStringLiteral("completed_at"), query.value(4).toString());
    result.append(mission);
  }

  return result;
}

auto SaveStorage::ensure_campaign_missions_in_db(
    const Game::Campaign::CampaignDefinition& campaign, QString* out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  for (const auto& mission : campaign.missions) {
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO campaign_missions (campaign_id, mission_id, order_index, "
        "unlocked, completed) VALUES (:campaign_id, :mission_id, :order_index, "
        ":unlocked, 0) "
        "ON CONFLICT(campaign_id, mission_id) DO UPDATE SET "
        "order_index = excluded.order_index"));
    query.bindValue(QStringLiteral(":campaign_id"), campaign.id);
    query.bindValue(QStringLiteral(":mission_id"), mission.mission_id);
    query.bindValue(QStringLiteral(":order_index"), mission.order_index);
    query.bindValue(QStringLiteral(":unlocked"), mission.order_index == 0 ? 1 : 0);

    if (!query.exec()) {
      fail(out_error,
           QCoreApplication::translate("SaveStorage",
                                       "Failed to register campaign mission"),
           query.lastError());
      transaction.rollback();
      return false;
    }
  }

  QStringList placeholders;
  placeholders.reserve(static_cast<int>(campaign.missions.size()));
  for (int i = 0; i < static_cast<int>(campaign.missions.size()); ++i) {
    placeholders.append(QStringLiteral(":mission%1").arg(i));
  }

  QSqlQuery prune(m_database);
  const QString sql =
      campaign.missions.empty()
          ? QStringLiteral("DELETE FROM campaign_missions WHERE campaign_id = "
                           ":campaign_id AND completed = 0")
          : QStringLiteral("DELETE FROM campaign_missions WHERE campaign_id = "
                           ":campaign_id AND completed = 0 AND mission_id NOT IN (%1)")
                .arg(placeholders.join(QStringLiteral(", ")));
  prune.prepare(sql);
  prune.bindValue(QStringLiteral(":campaign_id"), campaign.id);
  for (int i = 0; i < static_cast<int>(campaign.missions.size()); ++i) {
    prune.bindValue(placeholders[i],
                    campaign.missions[static_cast<std::size_t>(i)].mission_id);
  }

  if (!prune.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to prune removed campaign missions"),
         prune.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::complete_campaign_mission(const QString& campaign_id,
                                            const QString& mission_id,
                                            QString* out_error)
    -> std::optional<CampaignAdvance> {
  if (!initialize(out_error)) {
    return std::nullopt;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return std::nullopt;
  }

  QSqlQuery order_query(m_database);
  order_query.prepare(
      QStringLiteral("SELECT order_index, completed FROM campaign_missions "
                     "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
  order_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  order_query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!order_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to look up the completed mission"),
         order_query.lastError());
    transaction.rollback();
    return std::nullopt;
  }

  if (!order_query.next()) {
    if (out_error != nullptr) {
      *out_error = QCoreApplication::translate("SaveStorage",
                                               "Mission %1 is not part of campaign %2")
                       .arg(mission_id, campaign_id);
    }
    transaction.rollback();
    return std::nullopt;
  }

  const int completed_order = order_query.value(0).toInt();
  const bool was_completed = order_query.value(1).toInt() != 0;

  QSqlQuery complete_query(m_database);
  complete_query.prepare(
      QStringLiteral("UPDATE campaign_missions SET completed = 1, unlocked = 1, "
                     "completed_at = :completed_at "
                     "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
  complete_query.bindValue(QStringLiteral(":completed_at"), now_iso());
  complete_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  complete_query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!complete_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to mark mission as completed"),
         complete_query.lastError());
    transaction.rollback();
    return std::nullopt;
  }

  CampaignAdvance advance;
  advance.newly_completed = !was_completed;

  QSqlQuery next_query(m_database);
  next_query.prepare(
      QStringLiteral("SELECT mission_id FROM campaign_missions "
                     "WHERE campaign_id = :campaign_id AND order_index > :order "
                     "ORDER BY order_index ASC LIMIT 1"));
  next_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  next_query.bindValue(QStringLiteral(":order"), completed_order);

  if (!next_query.exec()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to find the next mission"),
         next_query.lastError());
    transaction.rollback();
    return std::nullopt;
  }

  if (next_query.next()) {
    advance.unlocked_mission_id = next_query.value(0).toString();

    QSqlQuery unlock_query(m_database);
    unlock_query.prepare(QStringLiteral(
        "UPDATE campaign_missions SET unlocked = 1 "
        "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
    unlock_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
    unlock_query.bindValue(QStringLiteral(":mission_id"), advance.unlocked_mission_id);

    if (!unlock_query.exec()) {
      fail(out_error,
           QCoreApplication::translate("SaveStorage", "Failed to unlock next mission"),
           unlock_query.lastError());
      transaction.rollback();
      return std::nullopt;
    }
  }

  QSqlQuery remaining_query(m_database);
  remaining_query.prepare(
      QStringLiteral("SELECT COUNT(*) FROM campaign_missions "
                     "WHERE campaign_id = :campaign_id AND completed = 0"));
  remaining_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!remaining_query.exec() || !remaining_query.next()) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage",
                                     "Failed to count remaining missions"),
         remaining_query.lastError());
    transaction.rollback();
    return std::nullopt;
  }

  advance.campaign_completed = remaining_query.value(0).toInt() == 0;

  if (advance.campaign_completed) {
    QSqlQuery campaign_query(m_database);
    campaign_query.prepare(
        QStringLiteral("INSERT INTO campaign_progress (campaign_id, completed, "
                       "unlocked, completed_at) VALUES (:campaign_id, 1, 1, "
                       ":completed_at) "
                       "ON CONFLICT(campaign_id) DO UPDATE SET "
                       "completed = 1, unlocked = 1, "
                       "completed_at = COALESCE(campaign_progress.completed_at, "
                       "excluded.completed_at)"));
    campaign_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
    campaign_query.bindValue(QStringLiteral(":completed_at"), now_iso());

    if (!campaign_query.exec()) {
      fail(out_error,
           QCoreApplication::translate("SaveStorage",
                                       "Failed to mark campaign as completed"),
           campaign_query.lastError());
      transaction.rollback();
      return std::nullopt;
    }
  }

  if (!transaction.commit(out_error)) {
    return std::nullopt;
  }
  return advance;
}

} // namespace Game::Systems
