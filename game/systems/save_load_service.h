#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_set>

#include "save_format.h"
#include "save_storage.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class SaveStorage;

struct SaveRequest {
  QString slot_name;
  QString title;
  QString map_name;
  QString map_path;
  QString mode;
  QString campaign_id;
  QString mission_id;
  QString difficulty;
  Save::SlotKind kind = Save::SlotKind::Manual;
  double play_time_seconds = 0.0;
  QJsonObject metadata;
  QByteArray screenshot;
  QJsonDocument world;
  int autosave_retention = 0;
};

class SaveLoadService : public QObject {
  Q_OBJECT

public:
  SaveLoadService();
  ~SaveLoadService() override;

  static auto instance() -> SaveLoadService*;

  auto begin_save(const SaveRequest& request) -> quint64;

  void attach_screenshot(const QString& slot_name, const QByteArray& screenshot);

  void cancel_save(quint64 job_id);
  [[nodiscard]] auto pending_save_count() const -> int;
  auto wait_for_pending_saves(int timeout_ms = 30000) -> bool;
  void shutdown();

  auto load_game_from_slot(Engine::Core::World& world,
                           const QString& slot_name) -> bool;

  [[nodiscard]] auto verify_save_slot(const QString& slot_name,
                                      QString* out_error = nullptr) const -> bool;

  [[nodiscard]] auto get_save_slots() const -> QVariantList;
  [[nodiscard]] auto slot_exists(const QString& slot_name) const -> bool;
  auto delete_save_slot(const QString& slot_name) -> bool;

  [[nodiscard]] auto next_autosave_slot(int retention) const -> QString;
  auto prune_autosaves(int retention) -> int;

  auto export_slot(const QString& slot_name,
                   const QString& file_path,
                   QString* out_error = nullptr) -> bool;
  auto import_package(const QString& file_path,
                      QString& out_slot_name,
                      QString* out_error = nullptr) -> bool;
  [[nodiscard]] auto list_exported_packages() const -> QStringList;

  static auto saves_directory() -> QString;
  static auto exports_directory() -> QString;
  static auto database_path() -> QString;

  [[nodiscard]] auto get_last_error() const -> QString;
  void clear_error();

  [[nodiscard]] auto get_last_record() const -> const Save::Record& {
    return m_last_record;
  }
  [[nodiscard]] auto get_last_metadata() const -> QJsonObject {
    return m_last_record.metadata;
  }
  [[nodiscard]] auto get_last_title() const -> QString { return m_last_record.title; }
  [[nodiscard]] auto get_last_screenshot() const -> QByteArray {
    return m_last_record.screenshot;
  }

  auto list_campaigns(QString* out_error = nullptr) -> QVariantList;
  auto get_campaign_progress(const QString& campaign_id,
                             QString* out_error = nullptr) const -> QVariantMap;
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

  static void open_settings();
  static void exit_game();

signals:
  void save_progress(quint64 job_id,
                     const QString& slot_name,
                     int percent,
                     const QString& stage);
  void save_finished(quint64 job_id,
                     const QString& slot_name,
                     bool success,
                     const QString& error);
  void save_slots_changed();

private:
  enum class JobKind {
    Write,
    Screenshot
  };

  struct Job {
    quint64 id = 0;
    JobKind kind = JobKind::Write;
    SaveRequest request;
    QByteArray screenshot;
  };

  void set_last_error(const QString& error) const;
  void ensure_worker_started();
  void worker_loop();
  void run_write_job(SaveStorage& storage, const Job& job);
  void run_screenshot_job(SaveStorage& storage, const Job& job);
  [[nodiscard]] auto is_cancelled(quint64 job_id) const -> bool;
  static void ensure_directories();

  mutable QString m_last_error;
  Save::Record m_last_record;
  std::unique_ptr<SaveStorage> m_storage;

  mutable std::mutex m_mutex;
  std::condition_variable m_queue_cv;
  std::condition_variable m_idle_cv;
  std::deque<Job> m_queue;
  std::unordered_set<quint64> m_cancelled;
  std::thread m_worker;
  quint64 m_next_job_id = 1;
  quint64 m_active_job = 0;
  bool m_stopping = false;
  bool m_worker_started = false;
};

} // namespace Game::Systems
