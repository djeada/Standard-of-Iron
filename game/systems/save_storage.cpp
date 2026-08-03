#include "save_storage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

#include "../map/campaign_definition.h"
#include "../map/campaign_loader.h"
#include "../util/asset_text.h"

namespace Game::Systems {

namespace {
constexpr const char* k_driver_name = "QSQLITE";

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
    // The war table places its objective marker and frames its camera from this.
    // Without it every mission reported an empty region, so the campaign map
    // never marked where the next battle was.
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
  if (!open(out_error)) {
    return false;
  }
  if (!ensure_schema(out_error)) {
    return false;
  }
  m_initialized = true;
  return true;
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
  pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
  pragma.exec(QStringLiteral("PRAGMA synchronous=FULL"));
  return true;
}

auto SaveStorage::ensure_schema(QString* out_error) const -> bool {
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

  if (version == Save::k_schema_version) {
    return true;
  }

  if (version != 0) {
    qWarning() << "Save database schema version" << version
               << "is not supported; rebuilding a clean database (previous saves are "
                  "discarded)";
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  if (!drop_schema(out_error) || !create_schema(out_error)) {
    transaction.rollback();
    return false;
  }

  QSqlQuery set_version(m_database);
  if (!set_version.exec(
          QStringLiteral("PRAGMA user_version = %1").arg(Save::k_schema_version))) {
    fail(out_error,
         QCoreApplication::translate("SaveStorage", "Failed to record schema version"),
         set_version.lastError());
    transaction.rollback();
    return false;
  }

  return transaction.commit(out_error);
}

auto SaveStorage::drop_schema(QString* out_error) const -> bool {
  const QStringList tables = {QStringLiteral("saves"),
                              QStringLiteral("campaigns"),
                              QStringLiteral("campaign_progress"),
                              QStringLiteral("campaign_missions"),
                              QStringLiteral("mission_progress"),
                              QStringLiteral("mission_results")};

  for (const QString& table : tables) {
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("DROP TABLE IF EXISTS %1").arg(table))) {
      return fail(out_error,
                  QCoreApplication::translate("SaveStorage", "Failed to drop table %1")
                      .arg(table),
                  query.lastError());
    }
  }
  return true;
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

  if (record.slot_name.isEmpty()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Refusing to write a save with an empty slot name");
    }
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  if (!query.prepare(QStringLiteral(
          "INSERT INTO saves (slot_name, title, map_name, map_path, mode, "
          "campaign_id, mission_id, difficulty, kind, play_time_seconds, "
          "created_at, updated_at, format_version, compression, world_raw_size, "
          "world_raw_checksum, world_blob_checksum, metadata, world_state, "
          "screenshot) "
          "VALUES (:slot_name, :title, :map_name, :map_path, :mode, :campaign_id, "
          ":mission_id, :difficulty, :kind, :play_time_seconds, :created_at, "
          ":updated_at, :format_version, :compression, :world_raw_size, "
          ":world_raw_checksum, :world_blob_checksum, :metadata, :world_state, "
          ":screenshot) "
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

  return transaction.commit(out_error);
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
      "metadata, world_state, screenshot, format_version "
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

  Save::Record record;
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
  return Save::verify_blob(record.world, out_error);
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
          "length(world_state), metadata, screenshot "
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

  // Drop rows for missions the campaign no longer contains. Without this an
  // update that removes a mission leaves an orphan row behind, and because the
  // campaign is finished when no mission is left uncompleted, that orphan makes
  // the campaign permanently unfinishable for anyone upgrading.
  QStringList placeholders;
  placeholders.reserve(static_cast<int>(campaign.missions.size()));
  for (int i = 0; i < static_cast<int>(campaign.missions.size()); ++i) {
    placeholders.append(QStringLiteral(":mission%1").arg(i));
  }

  QSqlQuery prune(m_database);
  const QString sql =
      campaign.missions.empty()
          ? QStringLiteral("DELETE FROM campaign_missions WHERE campaign_id = "
                           ":campaign_id")
          : QStringLiteral("DELETE FROM campaign_missions WHERE campaign_id = "
                           ":campaign_id AND mission_id NOT IN (%1)")
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

  // A mission the campaign does not contain is a stale request -- a menu built
  // from an older campaign file, or a save naming a mission that has since been
  // removed. Refuse it rather than writing progress nothing can read back.
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

  // The next mission by order rather than order + 1: a campaign edited down to
  // fewer missions leaves gaps, and a gap must not strand the player.
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

  // Completion is "every mission done", not "the last mission done", so a
  // player who unlocks the finale early still has to clear what they skipped.
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
