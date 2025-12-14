#include "save_storage.h"

#include <QDateTime>
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

namespace Game::Systems {

namespace {
constexpr const char *k_driver_name = "QSQLITE";
constexpr int k_current_schema_version = 2;

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

auto SaveStorage::initialize(QString *out_error) -> bool {
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
  if (!const_cast<SaveStorage *>(this)->initialize(out_error)) {
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

auto SaveStorage::list_campaigns(QString *out_error) const -> QVariantList {
  QVariantList result;
  if (!const_cast<SaveStorage *>(this)->initialize(out_error)) {
    return result;
  }

  QSqlQuery query(m_database);
  const QString sql = QStringLiteral(
      "SELECT c.id, c.title, c.description, c.map_path, c.order_index, "
      "COALESCE(p.completed, 0) as completed, COALESCE(p.unlocked, 0) as "
      "unlocked, "
      "p.completed_at "
      "FROM campaigns c "
      "LEFT JOIN campaign_progress p ON c.id = p.campaign_id "
      "ORDER BY c.order_index ASC");

  if (!query.exec(sql)) {
    if (out_error != nullptr) {
      *out_error = QStringLiteral("Failed to list campaigns: %1")
                       .arg(last_error_string(query.lastError()));
    }
    return result;
  }

  while (query.next()) {
    QVariantMap campaign;
    campaign.insert(QStringLiteral("id"), query.value(0).toString());
    campaign.insert(QStringLiteral("title"), query.value(1).toString());
    campaign.insert(QStringLiteral("description"), query.value(2).toString());
    campaign.insert(QStringLiteral("mapPath"), query.value(3).toString());
    campaign.insert(QStringLiteral("orderIndex"), query.value(4).toInt());
    campaign.insert(QStringLiteral("completed"), query.value(5).toInt() != 0);
    campaign.insert(QStringLiteral("unlocked"), query.value(6).toInt() != 0);
    campaign.insert(QStringLiteral("completedAt"), query.value(7).toString());
    result.append(campaign);
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

} // namespace Game::Systems
