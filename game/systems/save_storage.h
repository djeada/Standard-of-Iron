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
  explicit SaveStorage(QString database_path);
  ~SaveStorage();

  auto initialize(QString *out_error = nullptr) -> bool;

  auto saveSlot(const QString &slotName, const QString &title,
                const QJsonObject &metadata, const QByteArray &worldState,
                const QByteArray &screenshot,
                QString *out_error = nullptr) -> bool;

  auto loadSlot(const QString &slotName, QByteArray &worldState,
                QJsonObject &metadata, QByteArray &screenshot, QString &title,
                QString *out_error = nullptr) -> bool;

  auto listSlots(QString *out_error = nullptr) const -> QVariantList;

  auto deleteSlot(const QString &slotName,
                  QString *out_error = nullptr) -> bool;

  auto list_campaigns(QString *out_error = nullptr) const -> QVariantList;
  auto get_campaign_progress(const QString &campaign_id,
                             QString *out_error = nullptr) const -> QVariantMap;
  auto mark_campaign_completed(const QString &campaign_id,
                               QString *out_error = nullptr) -> bool;

private:
  auto open(QString *out_error = nullptr) const -> bool;
  auto ensureSchema(QString *out_error = nullptr) const -> bool;
  auto createBaseSchema(QString *out_error = nullptr) const -> bool;
  auto migrateSchema(int fromVersion,
                     QString *out_error = nullptr) const -> bool;
  auto schemaVersion(QString *out_error = nullptr) const -> int;
  auto setSchemaVersion(int version,
                        QString *out_error = nullptr) const -> bool;
  auto migrate_to_2(QString *out_error = nullptr) const -> bool;

  QString m_database_path;
  QString m_connection_name;
  mutable bool m_initialized = false;
  mutable QSqlDatabase m_database;
};

} // namespace Game::Systems
