#include "save_storage.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace Game::Systems {

namespace {
constexpr const char *kDriverName = "QSQLITE";
constexpr int kCurrentSchemaVersion = 1;

QString buildConnectionName(const SaveStorage *instance) {
  return QStringLiteral("SaveStorage_%1").arg(
      reinterpret_cast<quintptr>(instance), /*fieldWidth=*/0, /*base=*/16);
}

QString lastErrorString(const QSqlError &error) {
  if (error.type() == QSqlError::NoError) {
    return {};
  }
  return error.text();
}

class TransactionGuard {
public:
  explicit TransactionGuard(QSqlDatabase &database) : m_database(database) {}

  bool begin(QString *outError) {
    if (!m_database.transaction()) {
      if (outError) {
        *outError = QStringLiteral("Failed to begin transaction: %1")
                        .arg(lastErrorString(m_database.lastError()));
      }
      return false;
    }
    m_active = true;
    return true;
  }

  bool commit(QString *outError) {
    if (!m_active) {
      return true;
    }

    if (!m_database.commit()) {
      if (outError) {
        *outError = QStringLiteral("Failed to commit transaction: %1")
                        .arg(lastErrorString(m_database.lastError()));
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

SaveStorage::SaveStorage(QString databasePath)
    : m_databasePath(std::move(databasePath)),
      m_connectionName(buildConnectionName(this)) {}

SaveStorage::~SaveStorage() {
  if (m_database.isValid()) {
    if (m_database.isOpen()) {
      m_database.close();
    }
    const QString connectionName = m_connectionName;
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
  }
}

bool SaveStorage::initialize(QString *outError) {
  if (m_initialized && m_database.isValid() && m_database.isOpen()) {
    return true;
  }
  if (!open(outError)) {
    return false;
  }
  if (!ensureSchema(outError)) {
    return false;
  }
  m_initialized = true;
  return true;
}

bool SaveStorage::saveSlot(const QString &slotName, const QString &title,
                           const QJsonObject &metadata,
                           const QByteArray &worldState,
                           const QByteArray &screenshot,
                           QString *outError) {
  if (!initialize(outError)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(outError)) {
    return false;
  }

  QSqlQuery query(m_database);
  const QString insertSql =
      QStringLiteral(
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

  if (!query.prepare(insertSql)) {
    if (outError) {
      *outError = QStringLiteral("Failed to prepare save query: %1")
                       .arg(lastErrorString(query.lastError()));
    }
    return false;
  }

  const QString nowIso =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  QString mapName = metadata.value("mapName").toString();
  if (mapName.isEmpty()) {
    mapName = QStringLiteral("Unknown Map");
  }
  const QByteArray metadataBytes =
      QJsonDocument(metadata).toJson(QJsonDocument::Compact);

  query.bindValue(QStringLiteral(":slot_name"), slotName);
  query.bindValue(QStringLiteral(":title"), title);
  query.bindValue(QStringLiteral(":map_name"), mapName);
  query.bindValue(QStringLiteral(":timestamp"), nowIso);
  query.bindValue(QStringLiteral(":metadata"), metadataBytes);
  query.bindValue(QStringLiteral(":world_state"), worldState);
  if (screenshot.isEmpty()) {
    query.bindValue(QStringLiteral(":screenshot"),
                    QVariant(QMetaType::fromType<QByteArray>()));
  } else {
    query.bindValue(QStringLiteral(":screenshot"), screenshot);
  }
  query.bindValue(QStringLiteral(":created_at"), nowIso);
  query.bindValue(QStringLiteral(":updated_at"), nowIso);

  if (!query.exec()) {
    if (outError) {
      *outError = QStringLiteral("Failed to persist save slot: %1")
                       .arg(lastErrorString(query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(outError)) {
    return false;
  }

  return true;
}

bool SaveStorage::loadSlot(const QString &slotName, QByteArray &worldState,
                           QJsonObject &metadata, QByteArray &screenshot,
                           QString &title, QString *outError) {
  if (!initialize(outError)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(QStringLiteral(
      "SELECT title, metadata, world_state, screenshot FROM saves "
      "WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), slotName);

  if (!query.exec()) {
    if (outError) {
      *outError = QStringLiteral("Failed to read save slot: %1")
                       .arg(lastErrorString(query.lastError()));
    }
    return false;
  }

  if (!query.next()) {
    if (outError) {
      *outError = QStringLiteral("Save slot '%1' not found").arg(slotName);
    }
    return false;
  }

  title = query.value(0).toString();
  const QByteArray metadataBytes = query.value(1).toByteArray();
  metadata = QJsonDocument::fromJson(metadataBytes).object();
  worldState = query.value(2).toByteArray();
  screenshot = query.value(3).toByteArray();
  return true;
}

QVariantList SaveStorage::listSlots(QString *outError) const {
  QVariantList result;
  if (!const_cast<SaveStorage *>(this)->initialize(outError)) {
    return result;
  }

  QSqlQuery query(m_database);
  if (!query.exec(QStringLiteral(
          "SELECT slot_name, title, map_name, timestamp, metadata, screenshot "
          "FROM saves ORDER BY datetime(timestamp) DESC"))) {
    if (outError) {
      *outError = QStringLiteral("Failed to enumerate save slots: %1")
                       .arg(lastErrorString(query.lastError()));
    }
    return result;
  }

  while (query.next()) {
    QVariantMap slot;
    slot.insert(QStringLiteral("slotName"), query.value(0).toString());
    slot.insert(QStringLiteral("title"), query.value(1).toString());
    slot.insert(QStringLiteral("mapName"), query.value(2).toString());
    slot.insert(QStringLiteral("timestamp"), query.value(3).toString());

    const QByteArray metadataBytes = query.value(4).toByteArray();
    const QJsonObject metadataObj =
        QJsonDocument::fromJson(metadataBytes).object();
    slot.insert(QStringLiteral("metadata"), metadataObj.toVariantMap());

    const QByteArray screenshotBytes = query.value(5).toByteArray();
    if (!screenshotBytes.isEmpty()) {
      slot.insert(QStringLiteral("thumbnail"),
                  QString::fromLatin1(screenshotBytes.toBase64()));
    } else {
      slot.insert(QStringLiteral("thumbnail"), QString());
    }

    // Optional helpers for QML convenience
    if (metadataObj.contains("playTime")) {
      slot.insert(QStringLiteral("playTime"),
                  metadataObj.value("playTime").toString());
    }

    result.append(slot);
  }

  return result;
}

bool SaveStorage::deleteSlot(const QString &slotName, QString *outError) {
  if (!initialize(outError)) {
    return false;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(outError)) {
    return false;
  }

  QSqlQuery query(m_database);
  query.prepare(
      QStringLiteral("DELETE FROM saves WHERE slot_name = :slot_name"));
  query.bindValue(QStringLiteral(":slot_name"), slotName);

  if (!query.exec()) {
    if (outError) {
      *outError = QStringLiteral("Failed to delete save slot: %1")
                       .arg(lastErrorString(query.lastError()));
    }
    transaction.rollback();
    return false;
  }

  if (query.numRowsAffected() == 0) {
    if (outError) {
      *outError = QStringLiteral("Save slot '%1' not found").arg(slotName);
    }
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(outError)) {
    return false;
  }

  return true;
}

bool SaveStorage::open(QString *outError) const {
  if (m_database.isValid() && m_database.isOpen()) {
    return true;
  }

  if (!m_database.isValid()) {
    m_database = QSqlDatabase::addDatabase(kDriverName, m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
  }

  if (!m_database.open()) {
    if (outError) {
      *outError = QStringLiteral("Failed to open save database: %1")
                       .arg(lastErrorString(m_database.lastError()));
    }
    return false;
  }

  QSqlQuery foreignKeysQuery(m_database);
  foreignKeysQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

  QSqlQuery journalModeQuery(m_database);
  journalModeQuery.exec(QStringLiteral("PRAGMA journal_mode=WAL"));

  return true;
}

bool SaveStorage::ensureSchema(QString *outError) const {
  const int currentVersion = schemaVersion(outError);
  if (currentVersion < 0) {
    return false;
  }

  if (currentVersion > kCurrentSchemaVersion) {
    if (outError) {
      *outError = QStringLiteral(
          "Save database schema version %1 is newer than supported %2")
                       .arg(currentVersion)
                       .arg(kCurrentSchemaVersion);
    }
    return false;
  }

  if (currentVersion == kCurrentSchemaVersion) {
    return true;
  }

  TransactionGuard transaction(m_database);
  if (!transaction.begin(outError)) {
    return false;
  }

  if (!migrateSchema(currentVersion, outError)) {
    transaction.rollback();
    return false;
  }

  if (!setSchemaVersion(kCurrentSchemaVersion, outError)) {
    transaction.rollback();
    return false;
  }

  if (!transaction.commit(outError)) {
    return false;
  }

  return true;
}

int SaveStorage::schemaVersion(QString *outError) const {
  QSqlQuery pragmaQuery(m_database);
  if (!pragmaQuery.exec(QStringLiteral("PRAGMA user_version"))) {
    if (outError) {
      *outError = QStringLiteral("Failed to read schema version: %1")
                       .arg(lastErrorString(pragmaQuery.lastError()));
    }
    return -1;
  }

  if (pragmaQuery.next()) {
    return pragmaQuery.value(0).toInt();
  }

  return 0;
}

bool SaveStorage::setSchemaVersion(int version, QString *outError) const {
  QSqlQuery pragmaQuery(m_database);
  if (!pragmaQuery.exec(
          QStringLiteral("PRAGMA user_version = %1").arg(version))) {
    if (outError) {
      *outError = QStringLiteral("Failed to update schema version: %1")
                       .arg(lastErrorString(pragmaQuery.lastError()));
    }
    return false;
  }
  return true;
}

bool SaveStorage::createBaseSchema(QString *outError) const {
  QSqlQuery query(m_database);
  const QString createSql = QStringLiteral(
      "CREATE TABLE IF NOT EXISTS saves ("
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

  if (!query.exec(createSql)) {
    if (outError) {
      *outError = QStringLiteral("Failed to create save schema: %1")
                       .arg(lastErrorString(query.lastError()));
    }
    return false;
  }

  QSqlQuery indexQuery(m_database);
  if (!indexQuery.exec(QStringLiteral(
          "CREATE INDEX IF NOT EXISTS idx_saves_updated_at ON saves "
          "(updated_at DESC)"))) {
    if (outError) {
      *outError = QStringLiteral("Failed to build save index: %1")
                       .arg(lastErrorString(indexQuery.lastError()));
    }
    return false;
  }

  return true;
}

bool SaveStorage::migrateSchema(int fromVersion, QString *outError) const {
  int version = fromVersion;

  while (version < kCurrentSchemaVersion) {
    switch (version) {
    case 0:
      if (!createBaseSchema(outError)) {
        return false;
      }
      version = 1;
      break;
    default:
      if (outError) {
        *outError = QStringLiteral("Unsupported migration path from %1")
                         .arg(version);
      }
      return false;
    }
  }

  return true;
}

} // namespace Game::Systems
