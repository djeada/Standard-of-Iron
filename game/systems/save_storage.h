#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

#include "save_format.h"

namespace Game::Campaign {
struct CampaignDefinition;
}

namespace Game::Systems {

struct CampaignAdvance {

  QString unlocked_mission_id;

  bool campaign_completed = false;

  bool newly_completed = false;
};

class SaveStorage {
public:
  explicit SaveStorage(QString database_path);
  ~SaveStorage();

  SaveStorage(const SaveStorage&) = delete;
  auto operator=(const SaveStorage&) -> SaveStorage& = delete;

  auto initialize(QString* out_error = nullptr) const -> bool;

  auto write_slot(const Save::Record& record, QString* out_error = nullptr) -> bool;

  auto read_slot(const QString& slot_name,
                 Save::Record& out_record,
                 QString* out_error = nullptr) const -> bool;

  auto verify_slot(const QString& slot_name,
                   QString* out_error = nullptr) const -> bool;

  auto list_slots(QString* out_error = nullptr) const -> QVariantList;

  auto slot_names_by_kind(Save::SlotKind kind,
                          QString* out_error = nullptr) const -> QStringList;

  auto slot_exists(const QString& slot_name,
                   QString* out_error = nullptr) const -> bool;

  auto update_screenshot(const QString& slot_name,
                         const QByteArray& screenshot,
                         QString* out_error = nullptr) -> bool;

  auto delete_slot(const QString& slot_name, QString* out_error = nullptr) -> bool;

  auto list_campaigns(QString* out_error = nullptr) -> QVariantList;
  auto get_campaign_progress(const QString& campaign_id,
                             QString* out_error = nullptr) const -> QVariantMap;

  auto mark_campaign_completed(const QString& campaign_id,
                               QString* out_error = nullptr) -> bool;

  auto save_mission_result(const QString& mission_id,
                           const QString& mode,
                           const QString& campaign_id,
                           bool completed,
                           const QString& result,
                           const QString& difficulty,
                           float completion_time,
                           QString* out_error = nullptr) -> bool;

  auto get_mission_progress(const QString& mission_id,
                            QString* out_error = nullptr) const -> QVariantMap;

  auto
  get_campaign_mission_progress(const QString& campaign_id,
                                QString* out_error = nullptr) const -> QVariantList;

  auto complete_campaign_mission(const QString& campaign_id,
                                 const QString& mission_id,
                                 QString* out_error = nullptr)
      -> std::optional<CampaignAdvance>;

  auto
  ensure_campaign_missions_in_db(const Game::Campaign::CampaignDefinition& campaign,
                                 QString* out_error = nullptr) -> bool;

private:
  auto open(QString* out_error) const -> bool;
  auto ensure_schema(QString* out_error) const -> bool;
  auto create_schema(QString* out_error) const -> bool;
  auto drop_schema(QString* out_error) const -> bool;

  QString m_database_path;
  QString m_connection_name;
  mutable bool m_initialized = false;
  mutable QSqlDatabase m_database;
};

} // namespace Game::Systems
