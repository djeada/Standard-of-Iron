#include "save_storage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qnamespace.h>
#include <qsqldatabase.h>
#include <qsqlerror.h>
#include <qsqlquery.h>
#include <qstringliteral.h>
#include <qvariant.h>
#include <utility>

#include "../map/campaign_definition.h"
#include "../map/campaign_loader.h"
#include "../map/mission_loader.h"
#include "utils/resource_utils.h"

namespace Game::Systems {

namespace {
constexpr const char *k_driver_name = "QSQLITE";
constexpr int k_current_schema_version = 3;

auto build_connection_name(const SaveStorage *instance) -> QString {
  return QStringLiteral("SaveStorage_%1")
      .arg(reinterpret_cast<quintptr>(instance), 0, 16);
}

auto last_error_string(const QSqlError &error) -> QString {
  if (error.type() == QSqlError::NoError) {
    return {};
  }
  return error.text();
}

class TransactionGuard {
public:
  explicit TransactionGuard(QSqlDatabase &database) : m_database(database) {}

  auto begin(QString *out_error) -> bool {
    if (!m_database.transaction()) {
      if (out_error != nullptr) {
        *out_error = QStringLiteral("Failed to begin transaction: %1")
                         .arg(last_error_string(m_database.lastError()));
      }
      return false;
    }
    m_active = true;
    return true;
  }

  auto commit(QString *out_error) -> bool {
    if (!m_active) {
      return true;
    }

    if (!m_database.commit()) {
      if (out_error != nullptr) {
        *out_error = QStringLiteral("Failed to commit transaction: %1")
                         .arg(last_error_string(m_database.lastError()));
      }
      rollback();
      return false;
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
  QSqlDatabase &m_database;
  bool m_active = false;
};
} // namespace

SaveStorage::SaveStorage(QString database_path)
    : m_database_path(std::move(database_path)),
      m_connection_name(build_connection_name(this)) {}

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

auto SaveStorage::initialize(QString *out_error) const -> bool {
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

auto SaveStorage::save_slot(const QString &slot_name, const QString &title,
                            const QJsonObject &metadata,
                            const QByteArray &world_state,
                            const QByteArray &screenshot,
                            QString *out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  const QString insert_sql = QStringLiteral(
      "INSERT INTO saves (slot_name, title, map_name, timestamp, "
      "metadata, world_state, screenshot, created_at, updated_at) "
      "VALUES (:slot_name, :title, :map_name, :timestamp, :metadata, "
      ":world_state, :screenshot, :created_at, :updated_at) "
      "ON CONFLICT(slot_name) DO UPDATE SET "
      "title = excluded.title, "
      "map_name = excluded.map_name, "
      "timestamp = excluded.timestamp, "
      "metadata = excluded.metadata, "
      "world_state = excluded.world_state, "
      "screenshot = excluded.screenshot, "
      "updated_at = excluded.updated_at");

  if (!query.prepare(insert_sql)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to prepare save query: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return false;
  }

  const QString now_iso =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  QString map_name = metadata.value("map_name").toString();
  if (map_name.isEmpty()) {
    map_name = QStringLiteral("Unknown Map");
  }
  const QByteArray metadata_bytes =
      QJsonDocument(metadata).toJson(QJsonDocument::Compact);

  query.bindValue(QStringLiteral(":slot_name"), slot_name);
  query.bindValue(QStringLiteral(":title"), title);
  query.bindValue(QStringLiteral(":map_name"), map_name);
  query.bindValue(QStringLiteral(":timestamp"), now_iso);
  query.bindValue(QStringLiteral(":metadata"), metadata_bytes);
  query.bindValue(QStringLiteral(":world_state"), world_state);
  if (screenshot.isEmpty()) {
    query.bindValue(QStringLiteral(":screenshot"),
                    QVariant(QMetaType::fromType<QByteArray>()));
  } else {
    query.bindValue(QStringLiteral(":screenshot"), screenshot);
  }
  query.bindValue(QStringLiteral(":created_at"), now_iso);
  query.bindValue(QStringLiteral(":updated_at"), now_iso);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to persist save slot: %1")
                       .arg(last_error_string(query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::load_slot(const QString &slot_name, QByteArray &world_state,
                            QJsonObject &metadata, QByteArray &screenshot,
                            QString &title, QString *out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT title, metadata, world_state, screenshot FROM saves "
      "WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to read save slot: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return false;
  }

  if (!query.next()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Save slot '%1' not found").arg(slot_name);
    }
    return false;
  }

  title = query.value(0).toString();
  const QByteArray metadata_bytes = query.value(1).toByteArray();
  metadata = QJsonDocument::fromJson(metadata_bytes).object();
  world_state = query.value(2).toByteArray();
  screenshot = query.value(3).toByteArray();
  return true;
}

auto SaveStorage::list_slots(QString *out_error) const -> QVariantList {
  QVariantList result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  if (!query.exec(QStringLiteral(
          "SELECT slot_name, title, map_name, timestamp, metadata, screenshot "
          "FROM saves ORDER BY datetime(timestamp) DESC"))) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to enumerate save slots: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return result;
  }

  while (query.next()) {
    QVariantMap slot;
    slot.insert(QStringLiteral("slotName"), query.value(0).toString());
    slot.insert(QStringLiteral("title"), query.value(1).toString());
    slot.insert(QStringLiteral("map_name"), query.value(2).toString());
    slot.insert(QStringLiteral("timestamp"), query.value(3).toString());

    const QByteArray metadata_bytes = query.value(4).toByteArray();
    const QJsonObject metadata_obj =
        QJsonDocument::fromJson(metadata_bytes).object();
    slot.insert(QStringLiteral("metadata"), metadata_obj.toVariantMap());

    const QByteArray screenshot_bytes = query.value(5).toByteArray();
    if (!screenshot_bytes.isEmpty()) {
      slot.insert(QStringLiteral("thumbnail"),
                  QString::fromLatin1(screenshot_bytes.toBase64()));
    } else {
      slot.insert(QStringLiteral("thumbnail"), QString());
    }

    if (metadata_obj.contains("playTime")) {
      slot.insert(QStringLiteral("playTime"),
                  metadata_obj.value("playTime").toString());
    }

    result.append(slot);
  }

  return result;
}

auto SaveStorage::list_campaigns(QString *out_error) -> QVariantList {
  QVariantList result;

  if (!initialize(out_error)) {
    return result;
  }

  QStringList campaign_files;

  QStringList search_paths = {QStringLiteral("assets/campaigns"),
                              QStringLiteral("../assets/campaigns"),
                              QStringLiteral("../../assets/campaigns"),
                              QCoreApplication::applicationDirPath() +
                                  QStringLiteral("/assets/campaigns"),
                              QCoreApplication::applicationDirPath() +
                                  QStringLiteral("/../assets/campaigns")};

  bool found_filesystem = false;
  for (const QString &campaigns_path : search_paths) {
    QDir campaigns_dir(campaigns_path);
    if (campaigns_dir.exists()) {
      campaign_files = campaigns_dir.entryList(
          QStringList() << QStringLiteral("*.json"), QDir::Files);

      if (!campaign_files.isEmpty()) {
        qInfo() << "Loading campaigns from filesystem:"
                << campaigns_dir.absolutePath();

        for (const auto &campaign_file : campaign_files) {
          const QString campaign_path = campaigns_dir.filePath(campaign_file);

          Game::Campaign::CampaignDefinition campaign;
          QString error;
          if (!Game::Campaign::CampaignLoader::loadFromJsonFile(
                  campaign_path, campaign, &error)) {
            qWarning() << "Failed to load campaign" << campaign_file << ":"
                       << error;
            continue;
          }

          // Ensure campaign missions are in the database
          QString db_error;
          if (!ensure_campaign_missions_in_db(campaign, &db_error)) {
            qWarning() << "Failed to initialize campaign missions in DB for"
                       << campaign.id << ":" << db_error;
            // Continue with next campaign rather than failing completely
            continue;
          }

          // Load progress from database
          QVariantList missions_progress =
              get_campaign_mission_progress(campaign.id);

          QVariantMap campaign_map;
          campaign_map.insert(QStringLiteral("id"), campaign.id);
          campaign_map.insert(QStringLiteral("title"), campaign.title);
          campaign_map.insert(QStringLiteral("description"),
                              campaign.description);
          campaign_map.insert(QStringLiteral("unlocked"), true);

          // Check if campaign is completed
          bool all_completed = true;
          QVariantList missions_list;
          for (const auto &mission : campaign.missions) {
            QVariantMap mission_map;
            mission_map.insert(QStringLiteral("mission_id"),
                               mission.mission_id);
            mission_map.insert(QStringLiteral("order_index"),
                               mission.order_index);
            if (mission.intro_text.has_value()) {
              mission_map.insert(QStringLiteral("intro_text"),
                                 *mission.intro_text);
            }
            if (mission.outro_text.has_value()) {
              mission_map.insert(QStringLiteral("outro_text"),
                                 *mission.outro_text);
            }
            if (mission.difficulty_modifier.has_value()) {
              mission_map.insert(QStringLiteral("difficulty_modifier"),
                                 *mission.difficulty_modifier);
            }

            // Find progress for this mission
            bool unlocked = mission.order_index == 0;
            bool completed = false;
            for (const QVariant &progress_var : missions_progress) {
              QVariantMap progress = progress_var.toMap();
              if (progress["mission_id"].toString() == mission.mission_id) {
                unlocked = progress["unlocked"].toBool();
                completed = progress["completed"].toBool();
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

          result.append(campaign_map);
        }

        found_filesystem = true;
        break;
      }
    }
  }

  if (!found_filesystem) {

    qInfo() << "Loading campaigns from Qt resources";

    QStringList known_campaigns = {QStringLiteral("tutorial_campaign"),
                                   QStringLiteral("second_punic_war")};

    for (const auto &campaign_name : known_campaigns) {
      const QString campaign_path =
          QString(":/assets/campaigns/%1.json").arg(campaign_name);

      QFile test_file(campaign_path);
      if (!test_file.exists()) {
        qWarning() << "Campaign resource does not exist:" << campaign_path;
        continue;
      }

      Game::Campaign::CampaignDefinition campaign;
      QString error;
      if (!Game::Campaign::CampaignLoader::loadFromJsonFile(campaign_path,
                                                            campaign, &error)) {
        qWarning() << "Failed to load campaign from resources" << campaign_name
                   << ":" << error;
        continue;
      }

      // Ensure campaign missions are in the database
      QString db_error;
      if (!ensure_campaign_missions_in_db(campaign, &db_error)) {
        qWarning() << "Failed to initialize campaign missions in DB for"
                   << campaign.id << ":" << db_error;
        // Continue with next campaign rather than failing completely
        continue;
      }

      // Load progress from database
      QVariantList missions_progress =
          get_campaign_mission_progress(campaign.id);

      QVariantMap campaign_map;
      campaign_map.insert(QStringLiteral("id"), campaign.id);
      campaign_map.insert(QStringLiteral("title"), campaign.title);
      campaign_map.insert(QStringLiteral("description"), campaign.description);
      campaign_map.insert(QStringLiteral("unlocked"), true);

      // Check if campaign is completed
      bool all_completed = true;
      QVariantList missions_list;
      for (const auto &mission : campaign.missions) {
        QVariantMap mission_map;
        mission_map.insert(QStringLiteral("mission_id"), mission.mission_id);
        mission_map.insert(QStringLiteral("order_index"), mission.order_index);
        if (mission.intro_text.has_value()) {
          mission_map.insert(QStringLiteral("intro_text"), *mission.intro_text);
        }
        if (mission.outro_text.has_value()) {
          mission_map.insert(QStringLiteral("outro_text"), *mission.outro_text);
        }
        if (mission.difficulty_modifier.has_value()) {
          mission_map.insert(QStringLiteral("difficulty_modifier"),
                             *mission.difficulty_modifier);
        }

        // Find progress for this mission
        bool unlocked = mission.order_index == 0;
        bool completed = false;
        for (const QVariant &progress_var : missions_progress) {
          QVariantMap progress = progress_var.toMap();
          if (progress["mission_id"].toString() == mission.mission_id) {
            unlocked = progress["unlocked"].toBool();
            completed = progress["completed"].toBool();
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

      result.append(campaign_map);
    }
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

auto SaveStorage::get_campaign_progress(
    const QString &campaign_id, QString *out_error) const -> QVariantMap {
  QVariantMap result;
  if (!const_cast<SaveStorage *>(this)->initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT completed, unlocked, completed_at FROM campaign_progress "
      "WHERE campaign_id = :campaign_id"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to get campaign progress: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return result;
  }

  if (query.next()) {
    result.insert(QStringLiteral("completed"), query.value(0).toInt() != 0);
    result.insert(QStringLiteral("unlocked"), query.value(1).toInt() != 0);
    result.insert(QStringLiteral("completedAt"), query.value(2).toString());
  }

  return result;
}

auto SaveStorage::mark_campaign_completed(const QString &campaign_id,
                                          QString *out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  const QString now_iso =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("INSERT INTO campaign_progress (campaign_id, completed, "
                     "unlocked, completed_at) "
                     "VALUES (:campaign_id, 1, 1, :completed_at) "
                     "ON CONFLICT(campaign_id) DO UPDATE SET "
                     "completed = 1, completed_at = excluded.completed_at"));
  query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  query.bindValue(QStringLiteral(":completed_at"), now_iso);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to mark campaign as completed: %1")
                       .arg(last_error_string(query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::delete_slot(const QString &slot_name,
                              QString *out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("DELETE FROM saves WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), slot_name);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to delete save slot: %1")
                       .arg(last_error_string(query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (query.numRowsAffected() == 0) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Save slot '%1' not found").arg(slot_name);
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::open(QString *out_error) const -> bool {
  if (m_database.isValid() && m_database.isOpen()) {
    return true;
  }

  if (!m_database.isValid()) {
    m_database = QSqlDatabase::addDatabase(k_driver_name, m_connection_name);
    m_database.setDatabaseName(m_database_path);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
  }

  if (!m_database.open()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to open save database: %1")
                       .arg(last_error_string(m_database.lastError()));
    }
    return false;
  }

  QSqlQuery foreign_keys_query(m_database);
  foreign_keys_query.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

  QSqlQuery journal_mode_query(m_database);
  journal_mode_query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));

  return true;
}

auto SaveStorage::ensure_schema(QString *out_error) const -> bool {
  const int current_version = schema_version(out_error);
  if (current_version < 0) {
    return false;
  }

  if (current_version > k_current_schema_version) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral(
              "Save database schema version %1 is newer than supported %2")
              .arg(current_version)
              .arg(k_current_schema_version);
    }
    return false;
  }

  if (current_version == k_current_schema_version) {
    return true;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  if (!migrate_schema(current_version, out_error)) {
    transaction.rollback();
    return false;
  }

  if (!set_schema_version(k_current_schema_version, out_error)) {
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::schema_version(QString *out_error) const -> int {
  QSqlQuery pragma_query(m_database);
  if (!pragma_query.exec(QStringLiteral("PRAGMA user_version"))) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to read schema version: %1")
                       .arg(last_error_string(pragma_query.lastError()));
    }
    return -1;
  }

  if (pragma_query.next()) {
    return pragma_query.value(0).toInt();
  }

  return 0;
}

auto SaveStorage::set_schema_version(int version,
                                     QString *out_error) const -> bool {
  QSqlQuery pragma_query(m_database);
  if (!pragma_query.exec(
          QStringLiteral("PRAGMA user_version = %1").arg(version))) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to update schema version: %1")
                       .arg(last_error_string(pragma_query.lastError()));
    }
    return false;
  }
  return true;
}

auto SaveStorage::create_base_schema(QString *out_error) const -> bool {
  QSqlQuery query(m_database);
  const QString create_sql =
      QStringLiteral("CREATE TABLE IF NOT EXISTS saves ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                     "slot_name TEXT UNIQUE NOT NULL, "
                     "title TEXT NOT NULL, "
                     "map_name TEXT, "
                     "timestamp TEXT NOT NULL, "
                     "metadata BLOB NOT NULL, "
                     "world_state BLOB NOT NULL, "
                     "screenshot BLOB, "
                     "created_at TEXT NOT NULL, "
                     "updated_at TEXT NOT NULL"
                     ")");

  if (!query.exec(create_sql)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to create save schema: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return false;
  }

  QSqlQuery index_query(m_database);
  if (!index_query.exec(QStringLiteral(
          "CREATE INDEX IF NOT EXISTS idx_saves_updated_at ON saves "
          "(updated_at DESC)"))) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to build save index: %1")
                       .arg(last_error_string(index_query.lastError()));
    }
    return false;
  }

  return true;
}

auto SaveStorage::migrate_schema(int fromVersion,
                                 QString *out_error) const -> bool {
  int version = fromVersion;

  while (version < k_current_schema_version) {
    switch (version) {
    case 0:
      if (!create_base_schema(out_error)) {
        return false;
      }
      version = 1;
      break;
    case 1:
      if (!migrate_to_2(out_error)) {
        return false;
      }
      version = 2;
      break;
    case 2:
      if (!migrate_to_3(out_error)) {
        return false;
      }
      version = 3;
      break;
    default:
      if (out_error != nullptr) {
        *out_error =
            QStringLiteral("Unsupported migration path from %1").arg(version);
      }
      return false;
    }
  }

  return true;
}

auto SaveStorage::migrate_to_2(QString *out_error) const -> bool {

  QSqlQuery query(m_database);
  const QString create_campaigns_sql =
      QStringLiteral("CREATE TABLE IF NOT EXISTS campaigns ("
                     "id TEXT PRIMARY KEY NOT NULL, "
                     "title TEXT NOT NULL, "
                     "description TEXT NOT NULL, "
                     "map_path TEXT NOT NULL, "
                     "order_index INTEGER NOT NULL DEFAULT 0"
                     ")");

  if (!query.exec(create_campaigns_sql)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to create campaigns table: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return false;
  }

  QSqlQuery progress_query(m_database);
  const QString create_progress_sql = QStringLiteral(
      "CREATE TABLE IF NOT EXISTS campaign_progress ("
      "campaign_id TEXT PRIMARY KEY NOT NULL, "
      "completed INTEGER NOT NULL DEFAULT 0, "
      "unlocked INTEGER NOT NULL DEFAULT 0, "
      "completed_at TEXT, "
      "FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE"
      ")");

  if (!progress_query.exec(create_progress_sql)) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to create campaign_progress table: %1")
              .arg(last_error_string(progress_query.lastError()));
    }
    return false;
  }

  QSqlQuery insert_query(m_database);
  const QString insert_campaign_sql = QStringLiteral(
      "INSERT INTO campaigns (id, title, description, map_path, order_index) "
      "VALUES ('carthage_vs_rome', 'Carthage vs Rome', "
      "'Historic battle between Carthage and the Roman Republic. "
      "Command Carthaginian forces to defeat the Roman barracks.', "
      "':/assets/maps/map_rivers.json', 0)");

  if (!insert_query.exec(insert_campaign_sql)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to insert initial campaign: %1")
                       .arg(last_error_string(insert_query.lastError()));
    }
    return false;
  }

  QSqlQuery progress_insert_query(m_database);
  const QString insert_progress_sql = QStringLiteral(
      "INSERT INTO campaign_progress (campaign_id, completed, unlocked) "
      "VALUES ('carthage_vs_rome', 0, 1)");

  if (!progress_insert_query.exec(insert_progress_sql)) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to initialize campaign progress: %1")
              .arg(last_error_string(progress_insert_query.lastError()));
    }
    return false;
  }

  return true;
}

auto SaveStorage::migrate_to_3(QString *out_error) const -> bool {
  // Create mission_progress table
  QSqlQuery query(m_database);
  const QString create_mission_progress_sql = QStringLiteral(
      "CREATE TABLE IF NOT EXISTS mission_progress ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "mission_id TEXT NOT NULL, "
      "mode TEXT NOT NULL, "
      "campaign_id TEXT, "
      "completed INTEGER NOT NULL DEFAULT 0, "
      "completion_time REAL, "
      "difficulty TEXT, "
      "result TEXT, "
      "completed_at TEXT, "
      "created_at TEXT NOT NULL, "
      "updated_at TEXT NOT NULL, "
      "UNIQUE(mission_id, mode, campaign_id)"
      ")");

  if (!query.exec(create_mission_progress_sql)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to create mission_progress table: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return false;
  }

  // Create campaign_missions table
  QSqlQuery missions_query(m_database);
  const QString create_campaign_missions_sql = QStringLiteral(
      "CREATE TABLE IF NOT EXISTS campaign_missions ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "campaign_id TEXT NOT NULL, "
      "mission_id TEXT NOT NULL, "
      "order_index INTEGER NOT NULL, "
      "unlocked INTEGER NOT NULL DEFAULT 0, "
      "completed INTEGER NOT NULL DEFAULT 0, "
      "completed_at TEXT, "
      "UNIQUE(campaign_id, mission_id)"
      ")");

  if (!missions_query.exec(create_campaign_missions_sql)) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to create campaign_missions table: %1")
              .arg(last_error_string(missions_query.lastError()));
    }
    return false;
  }

  // Create indexes for performance
  QSqlQuery index_query(m_database);
  if (!index_query.exec(QStringLiteral(
          "CREATE INDEX IF NOT EXISTS idx_mission_progress_mission_id ON "
          "mission_progress (mission_id)"))) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to create mission_progress index: %1")
                       .arg(last_error_string(index_query.lastError()));
    }
    return false;
  }

  QSqlQuery campaign_index_query(m_database);
  if (!campaign_index_query.exec(QStringLiteral(
          "CREATE INDEX IF NOT EXISTS idx_campaign_missions_campaign_id ON "
          "campaign_missions (campaign_id)"))) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to create campaign_missions index: %1")
              .arg(last_error_string(campaign_index_query.lastError()));
    }
    return false;
  }

  return true;
}

auto SaveStorage::save_mission_result(
    const QString &mission_id, const QString &mode, const QString &campaign_id,
    bool completed, const QString &result, const QString &difficulty,
    float completion_time, QString *out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  const QString now_iso =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

  QSqlQuery query(m_database);
  const QString insert_sql = QStringLiteral(
      "INSERT INTO mission_progress (mission_id, mode, campaign_id, completed, "
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
      "updated_at = excluded.updated_at");

  if (!query.prepare(insert_sql)) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to prepare mission_progress insert: %1")
              .arg(last_error_string(query.lastError()));
    }
    return false;
  }

  query.bindValue(QStringLiteral(":mission_id"), mission_id);
  query.bindValue(QStringLiteral(":mode"), mode);
  if (campaign_id.isEmpty()) {
    query.bindValue(QStringLiteral(":campaign_id"), QVariant());
  } else {
    query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  }
  query.bindValue(QStringLiteral(":completed"), completed ? 1 : 0);
  query.bindValue(QStringLiteral(":completion_time"), completion_time);
  query.bindValue(QStringLiteral(":difficulty"), difficulty);
  query.bindValue(QStringLiteral(":result"), result);
  query.bindValue(QStringLiteral(":completed_at"),
                  completed ? now_iso : QVariant());
  query.bindValue(QStringLiteral(":created_at"), now_iso);
  query.bindValue(QStringLiteral(":updated_at"), now_iso);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to save mission result: %1")
                       .arg(last_error_string(query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::get_mission_progress(const QString &mission_id,
                                       QString *out_error) const
    -> QVariantMap {
  QVariantMap result;
  if (!initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT mode, campaign_id, completed, completion_time, difficulty, "
      "result, completed_at FROM mission_progress "
      "WHERE mission_id = :mission_id ORDER BY updated_at DESC LIMIT 1"));
  query.bindValue(QStringLiteral(":mission_id"), mission_id);

  if (!query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to get mission progress: %1")
                       .arg(last_error_string(query.lastError()));
    }
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

auto SaveStorage::get_campaign_mission_progress(const QString &campaign_id,
                                                QString *out_error) const
    -> QVariantList {
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
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to get campaign mission progress: %1")
              .arg(last_error_string(query.lastError()));
    }
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
    const Game::Campaign::CampaignDefinition &campaign, QString *out_error)
    -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  for (const auto &mission : campaign.missions) {
    QSqlQuery check_query(m_database);
    check_query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM campaign_missions "
        "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
    check_query.bindValue(QStringLiteral(":campaign_id"), campaign.id);
    check_query.bindValue(QStringLiteral(":mission_id"), mission.mission_id);

    if (!check_query.exec() || !check_query.next()) {
      if (out_error != nullptr) {
        *out_error =
            QStringLiteral("Failed to check campaign mission existence: %1")
                .arg(last_error_string(check_query.lastError()));
      }
      transaction.rollback();
      return false;
    }

    int count = check_query.value(0).toInt();
    if (count == 0) {
      // Insert the mission
      QSqlQuery insert_query(m_database);
      insert_query.prepare(QStringLiteral(
          "INSERT INTO campaign_missions (campaign_id, mission_id, "
          "order_index, unlocked, completed) "
          "VALUES (:campaign_id, :mission_id, :order_index, :unlocked, 0)"));
      insert_query.bindValue(QStringLiteral(":campaign_id"), campaign.id);
      insert_query.bindValue(QStringLiteral(":mission_id"),
                             mission.mission_id);
      insert_query.bindValue(QStringLiteral(":order_index"),
                             mission.order_index);
      // First mission is unlocked by default
      insert_query.bindValue(QStringLiteral(":unlocked"),
                             mission.order_index == 0 ? 1 : 0);

      if (!insert_query.exec()) {
        if (out_error != nullptr) {
          *out_error =
              QStringLiteral("Failed to insert campaign mission: %1")
                  .arg(last_error_string(insert_query.lastError()));
        }
        transaction.rollback();
        return false;
      }
    }
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

auto SaveStorage::unlock_next_mission(const QString &campaign_id,
                                      const QString &completed_mission_id,
                                      QString *out_error) -> bool {
  if (!initialize(out_error)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(out_error)) {
    return false;
  }

  const QString now_iso =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

  // Mark the completed mission as completed
  QSqlQuery update_query(m_database);
  update_query.prepare(QStringLiteral(
      "UPDATE campaign_missions SET completed = 1, completed_at = :completed_at "
      "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
  update_query.bindValue(QStringLiteral(":completed_at"), now_iso);
  update_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  update_query.bindValue(QStringLiteral(":mission_id"), completed_mission_id);

  if (!update_query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to mark mission as completed: %1")
                       .arg(last_error_string(update_query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  // Get the order_index of the completed mission
  QSqlQuery order_query(m_database);
  order_query.prepare(QStringLiteral(
      "SELECT order_index FROM campaign_missions "
      "WHERE campaign_id = :campaign_id AND mission_id = :mission_id"));
  order_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  order_query.bindValue(QStringLiteral(":mission_id"), completed_mission_id);

  if (!order_query.exec() || !order_query.next()) {
    if (out_error != nullptr) {
      *out_error =
          QStringLiteral("Failed to find completed mission order: %1")
              .arg(last_error_string(order_query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  int completed_order = order_query.value(0).toInt();

  // Unlock the next mission
  QSqlQuery unlock_query(m_database);
  unlock_query.prepare(
      QStringLiteral("UPDATE campaign_missions SET unlocked = 1 "
                     "WHERE campaign_id = :campaign_id AND order_index = "
                     ":next_order_index"));
  unlock_query.bindValue(QStringLiteral(":campaign_id"), campaign_id);
  unlock_query.bindValue(QStringLiteral(":next_order_index"),
                         completed_order + 1);

  if (!unlock_query.exec()) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to unlock next mission: %1")
                       .arg(last_error_string(unlock_query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(out_error)) {
    return false;
  }

  return true;
}

} // namespace Game::Systems
