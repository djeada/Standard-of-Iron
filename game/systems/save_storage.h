#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

#include <memory>

namespace Game::Campaign {
struct CampaignDefinition;
}

namespace Game::Systems {

class SaveStorage {
public:
  explicit SaveStorage(QString database_path);
  ~SaveStorage();

  auto initialize(QString *out_error = nullptr) -> bool;

  auto save_slot(const QString &slot_name, const QString &title,
                 const QJsonObject &metadata, const QByteArray &world_state,
                 const QByteArray &screenshot,
                 QString *out_error = nullptr) -> bool;

  auto load_slot(const QString &slot_name, QByteArray &world_state,
                 QJsonObject &metadata, QByteArray &screenshot, QString &title,
                 QString *out_error = nullptr) -> bool;

  auto list_slots(QString *out_error = nullptr) const -> QVariantList;

  auto delete_slot(const QString &slot_name,
                   QString *out_error = nullptr) -> bool;

  auto list_campaigns(QString *out_error = nullptr) -> QVariantList;
  auto get_campaign_progress(const QString &campaign_id,
                             QString *out_error = nullptr) const -> QVariantMap;
  auto mark_campaign_completed(const QString &campaign_id,
                               QString *out_error = nullptr) -> bool;

  auto save_mission_result(const QString &mission_id, const QString &mode,
                           const QString &campaign_id, bool completed,
                           const QString &result, const QString &difficulty,
                           float completion_time,
                           QString *out_error = nullptr) -> bool;

  auto get_mission_progress(const QString &mission_id,
                            QString *out_error = nullptr) const -> QVariantMap;

  auto get_campaign_mission_progress(const QString &campaign_id,
                                     QString *out_error = nullptr) const
      -> QVariantList;

  auto unlock_next_mission(const QString &campaign_id,
                           const QString &completed_mission_id,
                           QString *out_error = nullptr) -> bool;

  auto ensure_campaign_missions_in_db(
      const Game::Campaign::CampaignDefinition &campaign,
      QString *out_error = nullptr) -> bool;

private:
  auto open(QString *out_error = nullptr) const -> bool;
  auto ensure_schema(QString *out_error = nullptr) const -> bool;
  auto create_base_schema(QString *out_error = nullptr) const -> bool;
  auto migrate_schema(int from_version,
                      QString *out_error = nullptr) const -> bool;
  auto schema_version(QString *out_error = nullptr) const -> int;
  auto set_schema_version(int version,
                          QString *out_error = nullptr) const -> bool;
  auto migrate_to_2(QString *out_error = nullptr) const -> bool;
  auto migrate_to_3(QString *out_error = nullptr) const -> bool;

  QString m_database_path;
  QString m_connection_name;
  mutable bool m_initialized = false;
  mutable QSqlDatabase m_database;
};

} // namespace Game::Systems
