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

  auto slot_kind(const QString& slot_name,
                 Save::SlotKind& out_kind,
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

  [[nodiscard]] auto health_error() const -> QString { return m_health_error; }

  [[nodiscard]] auto quarantined_path() const -> QString { return m_quarantined_path; }

private:
  auto open(QString* out_error) const -> bool;
  auto ensure_schema(QString* out_error) const -> bool;
  auto create_schema(QString* out_error) const -> bool;
  auto create_fresh_schema(QString* out_error) const -> bool;
  auto stamp_schema_version(QString* out_error) const -> bool;
  auto migrate_schema(int from_version, QString* out_error) const -> bool;
  [[nodiscard]] auto schema_shape_is_current() const -> bool;
  [[nodiscard]] auto database_is_empty() const -> bool;
  [[nodiscard]] auto passes_integrity_check(QString* out_reason) const -> bool;
  [[nodiscard]] auto is_memory_database() const -> bool;
  auto quarantine_and_recreate(const QString& reason, QString* out_error) const -> bool;
  void backup_before_migration(int from_version) const;
  auto read_back_matches(const Save::Record& record, QString* out_error) const -> bool;
  void close_connection() const;

  QString m_database_path;
  QString m_connection_name;
  mutable bool m_initialized = false;
  mutable QSqlDatabase m_database;
  mutable QString m_health_error;
  mutable QString m_quarantined_path;
};

} // namespace Game::Systems
