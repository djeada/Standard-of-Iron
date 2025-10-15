#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

#include <memory>

namespace Game::Systems {

class SaveStorage {
public:
  explicit SaveStorage(QString databasePath);
  ~SaveStorage();

  bool initialize(QString *outError = nullptr);

  bool saveSlot(const QString &slotName, const QString &title,
                const QJsonObject &metadata, const QByteArray &worldState,
                const QByteArray &screenshot, QString *outError = nullptr);

  bool loadSlot(const QString &slotName, QByteArray &worldState,
                QJsonObject &metadata, QByteArray &screenshot, QString &title,
                QString *outError = nullptr);

  QVariantList listSlots(QString *outError = nullptr) const;

  bool deleteSlot(const QString &slotName, QString *outError = nullptr);

private:
  bool open(QString *outError = nullptr) const;
  bool ensureSchema(QString *outError = nullptr) const;
  bool createBaseSchema(QString *outError = nullptr) const;
  bool migrateSchema(int fromVersion, QString *outError = nullptr) const;
  int schemaVersion(QString *outError = nullptr) const;
  bool setSchemaVersion(int version, QString *outError = nullptr) const;

  QString m_databasePath;
  QString m_connectionName;
  mutable bool m_initialized = false;
  mutable QSqlDatabase m_database;
};

} // namespace Game::Systems
