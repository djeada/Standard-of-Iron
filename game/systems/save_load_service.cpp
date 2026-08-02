#include "save_load_service.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <exception>

#include "game/core/serialization.h"
#include "game/core/world.h"
#include "save_storage.h"

namespace Game::Systems {

namespace {

constexpr const char* k_autosave_prefix = "autosave_";
constexpr int k_min_autosave_retention = 1;
constexpr int k_max_autosave_retention = 20;

auto clamp_retention(int retention) -> int {
  return std::clamp(retention, k_min_autosave_retention, k_max_autosave_retention);
}

auto now_iso() -> QString {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

SaveLoadService::SaveLoadService() {
  ensure_directories();
  m_storage = std::make_unique<SaveStorage>(database_path());
  QString init_error;
  if (!m_storage->initialize(&init_error)) {
    set_last_error(init_error);
    qWarning() << "SaveLoadService: failed to initialize storage" << init_error;
  }
}

SaveLoadService::~SaveLoadService() {
  shutdown();
}

auto SaveLoadService::instance() -> SaveLoadService* {
  static SaveLoadService service;
  return &service;
}

auto SaveLoadService::saves_directory() -> QString {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/saves";
}

auto SaveLoadService::exports_directory() -> QString {
  return saves_directory() + QStringLiteral("/exports");
}

auto SaveLoadService::database_path() -> QString {
  return saves_directory() + QStringLiteral("/saves.sqlite");
}

void SaveLoadService::ensure_directories() {
  QDir const dir;
  dir.mkpath(saves_directory());
  dir.mkpath(exports_directory());
}

auto SaveLoadService::get_last_error() const -> QString {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_last_error;
}

void SaveLoadService::clear_error() {
  set_last_error({});
}

void SaveLoadService::set_last_error(const QString& error) const {
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_last_error = error;
}

void SaveLoadService::ensure_worker_started() {
  if (m_worker_started) {
    return;
  }
  m_worker_started = true;
  m_worker = std::thread([this]() { worker_loop(); });
}

auto SaveLoadService::begin_save(const SaveRequest& request) -> quint64 {
  if (request.slot_name.isEmpty()) {
    set_last_error(tr("Cannot save: empty slot name"));
    return 0;
  }

  quint64 job_id = 0;
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) {
      m_last_error = tr("Save service is shutting down");
      return 0;
    }
    job_id = m_next_job_id++;
    m_queue.push_back(Job{job_id, JobKind::Write, request, QByteArray()});
    ensure_worker_started();
  }

  m_queue_cv.notify_one();
  emit save_progress(job_id, request.slot_name, 0, tr("Queued"));
  return job_id;
}

void SaveLoadService::attach_screenshot(const QString& slot_name,
                                        const QByteArray& screenshot) {
  if (slot_name.isEmpty() || screenshot.isEmpty()) {
    return;
  }

  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) {
      return;
    }
    Job job;
    job.id = m_next_job_id++;
    job.kind = JobKind::Screenshot;
    job.request.slot_name = slot_name;
    job.screenshot = screenshot;
    m_queue.push_back(std::move(job));
    ensure_worker_started();
  }

  m_queue_cv.notify_one();
}

void SaveLoadService::cancel_save(quint64 job_id) {
  if (job_id == 0) {
    return;
  }
  const std::lock_guard<std::mutex> lock(m_mutex);
  m_cancelled.insert(job_id);
}

auto SaveLoadService::is_cancelled(quint64 job_id) const -> bool {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return m_cancelled.contains(job_id);
}

auto SaveLoadService::pending_save_count() const -> int {
  const std::lock_guard<std::mutex> lock(m_mutex);
  return static_cast<int>(m_queue.size()) + (m_active_job != 0 ? 1 : 0);
}

auto SaveLoadService::wait_for_pending_saves(int timeout_ms) -> bool {
  std::unique_lock<std::mutex> lock(m_mutex);
  return m_idle_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() {
    return m_queue.empty() && m_active_job == 0;
  });
}

void SaveLoadService::shutdown() {
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stopping) {
      return;
    }
    m_stopping = true;
  }
  m_queue_cv.notify_all();
  if (m_worker.joinable()) {
    m_worker.join();
  }
}

void SaveLoadService::worker_loop() {

  SaveStorage storage(database_path());

  while (true) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_queue_cv.wait(lock, [this]() { return m_stopping || !m_queue.empty(); });
      if (m_stopping && m_queue.empty()) {
        return;
      }
      job = m_queue.front();
      m_queue.pop_front();
      m_active_job = job.id;
    }

    if (job.kind == JobKind::Screenshot) {
      run_screenshot_job(storage, job);
    } else {
      run_write_job(storage, job);
    }

    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_active_job = 0;
      m_cancelled.erase(job.id);
    }
    m_idle_cv.notify_all();
  }
}

void SaveLoadService::run_screenshot_job(SaveStorage& storage, const Job& job) {
  QString error;
  if (storage.update_screenshot(job.request.slot_name, job.screenshot, &error)) {
    emit save_slots_changed();
    return;
  }

  qWarning() << "SaveLoadService: could not attach preview to" << job.request.slot_name
             << ":" << error;
}

void SaveLoadService::run_write_job(SaveStorage& storage, const Job& job) {
  QString error;
  bool success = false;
  bool cancelled = false;

  const auto report = [&](int percent, const QString& stage) {
    emit save_progress(job.id, job.request.slot_name, percent, stage);
  };

  try {
    if (is_cancelled(job.id)) {
      cancelled = true;
    } else {
      report(15, tr("Serializing world"));
      const QByteArray world_bytes = job.request.world.toJson(QJsonDocument::Compact);

      if (is_cancelled(job.id)) {
        cancelled = true;
      } else {
        report(45, tr("Compressing"));
        Save::Record record;
        record.slot_name = job.request.slot_name;
        record.title = job.request.title;
        record.map_name = job.request.map_name;
        record.map_path = job.request.map_path;
        record.mode = job.request.mode;
        record.campaign_id = job.request.campaign_id;
        record.mission_id = job.request.mission_id;
        record.difficulty = job.request.difficulty;
        record.kind = job.request.kind;
        record.play_time_seconds = job.request.play_time_seconds;
        record.created_at = now_iso();
        record.updated_at = record.created_at;
        record.metadata = job.request.metadata;
        record.screenshot = job.request.screenshot;
        record.world = Save::pack(world_bytes);

        if (is_cancelled(job.id)) {
          cancelled = true;
        } else {
          report(75, tr("Writing"));
          success = storage.write_slot(record, &error);
          if (success && job.request.autosave_retention > 0) {
            const int retention = clamp_retention(job.request.autosave_retention);
            QString prune_error;
            const QStringList autosaves =
                storage.slot_names_by_kind(Save::SlotKind::Autosave, &prune_error);
            for (int i = 0; i < autosaves.size() - retention; ++i) {
              storage.delete_slot(autosaves.at(i), &prune_error);
            }
          }
        }
      }
    }
  } catch (const std::exception& exception) {
    error = tr("Exception while saving: %1").arg(QString::fromUtf8(exception.what()));
    success = false;
  }

  if (cancelled) {
    error = tr("Save cancelled");
    success = false;
  }

  if (success) {
    report(100, tr("Done"));
    emit save_finished(job.id, job.request.slot_name, true, QString());
    emit save_slots_changed();
  } else {
    qWarning() << "SaveLoadService: save to" << job.request.slot_name
               << "failed:" << error;
    emit save_finished(job.id, job.request.slot_name, false, error);
    set_last_error(error);
  }
}

auto SaveLoadService::load_game_from_slot(Engine::Core::World& world,
                                          const QString& slot_name) -> bool {
  qInfo() << "Loading game from slot:" << slot_name;

  if (!m_storage) {
    set_last_error(tr("Save storage unavailable"));
    qWarning() << "SaveLoadService: save storage unavailable";
    return false;
  }

  try {
    Save::Record record;
    QString error;
    if (!m_storage->read_slot(slot_name, record, &error)) {
      set_last_error(error);
      qWarning() << "SaveLoadService: failed to read slot" << error;
      return false;
    }

    QByteArray world_bytes;
    if (!Save::unpack(record.world, world_bytes, &error)) {
      const QString message =
          tr("Save slot '%1' is corrupted: %2").arg(slot_name, error);
      set_last_error(message);
      qWarning() << message;
      return false;
    }

    QJsonParseError parse_error{};
    const QJsonDocument doc = QJsonDocument::fromJson(world_bytes, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
      const QString message = tr("Save slot '%1' is corrupted: %2")
                                  .arg(slot_name, parse_error.errorString());
      set_last_error(message);
      qWarning() << message;
      return false;
    }

    world.clear();
    Engine::Core::Serialization::deserialize_world(&world, doc);

    m_last_record = record;
    set_last_error({});
    return true;
  } catch (const std::exception& exception) {
    const QString message = tr("Exception while loading save '%1': %2")
                                .arg(slot_name, QString::fromUtf8(exception.what()));
    set_last_error(message);
    qWarning() << message;
    return false;
  }
}

auto SaveLoadService::verify_save_slot(const QString& slot_name,
                                       QString* out_error) const -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return false;
  }
  return m_storage->verify_slot(slot_name, out_error);
}

auto SaveLoadService::get_save_slots() const -> QVariantList {
  if (!m_storage) {
    return {};
  }

  QString list_error;
  QVariantList slot_list = m_storage->list_slots(&list_error);
  if (!list_error.isEmpty()) {
    set_last_error(list_error);
    qWarning() << "SaveLoadService: failed to enumerate slots" << list_error;
  }
  return slot_list;
}

auto SaveLoadService::slot_exists(const QString& slot_name) const -> bool {
  return m_storage && m_storage->slot_exists(slot_name);
}

auto SaveLoadService::delete_save_slot(const QString& slot_name) -> bool {
  if (!m_storage) {
    set_last_error(tr("Save storage unavailable"));
    return false;
  }

  QString error;
  if (!m_storage->delete_slot(slot_name, &error)) {
    set_last_error(error);
    qWarning() << "SaveLoadService: failed to delete slot" << error;
    return false;
  }

  set_last_error({});
  emit save_slots_changed();
  return true;
}

auto SaveLoadService::next_autosave_slot(int retention) const -> QString {
  const int slot_count = clamp_retention(retention);
  if (!m_storage) {
    return QStringLiteral("%1%2").arg(QString::fromLatin1(k_autosave_prefix)).arg(1);
  }

  const QStringList existing = m_storage->slot_names_by_kind(Save::SlotKind::Autosave);
  for (int index = 1; index <= slot_count; ++index) {
    const QString candidate =
        QStringLiteral("%1%2").arg(QString::fromLatin1(k_autosave_prefix)).arg(index);
    if (!existing.contains(candidate)) {
      return candidate;
    }
  }

  for (const QString& candidate : existing) {
    if (candidate.startsWith(QString::fromLatin1(k_autosave_prefix))) {
      return candidate;
    }
  }

  return QStringLiteral("%1%2").arg(QString::fromLatin1(k_autosave_prefix)).arg(1);
}

auto SaveLoadService::prune_autosaves(int retention) -> int {
  if (!m_storage) {
    return 0;
  }

  const int slot_count = clamp_retention(retention);
  const QStringList existing = m_storage->slot_names_by_kind(Save::SlotKind::Autosave);
  int removed = 0;
  for (int i = 0; i < existing.size() - slot_count; ++i) {
    QString error;
    if (m_storage->delete_slot(existing.at(i), &error)) {
      ++removed;
    } else {
      qWarning() << "SaveLoadService: failed to prune autosave" << existing.at(i) << ":"
                 << error;
    }
  }

  if (removed > 0) {
    emit save_slots_changed();
  }
  return removed;
}

auto SaveLoadService::export_slot(const QString& slot_name,
                                  const QString& file_path,
                                  QString* out_error) -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return false;
  }

  Save::Record record;
  if (!m_storage->read_slot(slot_name, record, out_error)) {
    return false;
  }

  if (!Save::verify_blob(record.world, out_error)) {
    return false;
  }

  ensure_directories();
  QDir().mkpath(QFileInfo(file_path).absolutePath());

  QSaveFile file(file_path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (out_error != nullptr) {
      *out_error = tr("Cannot write '%1': %2").arg(file_path, file.errorString());
    }
    return false;
  }

  const QByteArray package = Save::encode_package(record);
  if (file.write(package) != package.size() || !file.commit()) {
    if (out_error != nullptr) {
      *out_error = tr("Failed to write '%1': %2").arg(file_path, file.errorString());
    }
    return false;
  }

  return true;
}

auto SaveLoadService::import_package(const QString& file_path,
                                     QString& out_slot_name,
                                     QString* out_error) -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return false;
  }

  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (out_error != nullptr) {
      *out_error = tr("Cannot read '%1': %2").arg(file_path, file.errorString());
    }
    return false;
  }

  Save::Record record;
  if (!Save::decode_package(file.readAll(), record, out_error)) {
    return false;
  }

  QString base = Save::sanitize_file_stem(record.slot_name);
  if (base.isEmpty()) {
    base = Save::sanitize_file_stem(QFileInfo(file_path).completeBaseName());
  }
  if (base.isEmpty()) {
    base = QStringLiteral("imported_save");
  }

  QString slot_name = base;
  int suffix = 2;
  while (m_storage->slot_exists(slot_name)) {
    slot_name = QStringLiteral("%1_%2").arg(base).arg(suffix++);
  }

  record.slot_name = slot_name;
  record.kind = Save::SlotKind::Manual;
  record.updated_at = now_iso();
  if (record.created_at.isEmpty()) {
    record.created_at = record.updated_at;
  }

  if (!m_storage->write_slot(record, out_error)) {
    return false;
  }

  out_slot_name = slot_name;
  emit save_slots_changed();
  return true;
}

auto SaveLoadService::list_exported_packages() const -> QStringList {
  QDir const dir(exports_directory());
  const QStringList filter = {QStringLiteral("*.%1").arg(Save::package_file_suffix())};
  QStringList result;
  for (const QString& name :
       dir.entryList(filter, QDir::Files, QDir::Time | QDir::Reversed)) {
    result.append(dir.filePath(name));
  }
  return result;
}

auto SaveLoadService::list_campaigns(QString* out_error) -> QVariantList {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return {};
  }
  return m_storage->list_campaigns(out_error);
}

auto SaveLoadService::get_campaign_progress(const QString& campaign_id,
                                            QString* out_error) const -> QVariantMap {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return {};
  }
  return m_storage->get_campaign_progress(campaign_id, out_error);
}

auto SaveLoadService::mark_campaign_completed(const QString& campaign_id,
                                              QString* out_error) -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return false;
  }
  return m_storage->mark_campaign_completed(campaign_id, out_error);
}

auto SaveLoadService::save_mission_result(const QString& mission_id,
                                          const QString& mode,
                                          const QString& campaign_id,
                                          bool completed,
                                          const QString& result,
                                          const QString& difficulty,
                                          float completion_time,
                                          QString* out_error) -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return false;
  }
  return m_storage->save_mission_result(mission_id,
                                        mode,
                                        campaign_id,
                                        completed,
                                        result,
                                        difficulty,
                                        completion_time,
                                        out_error);
}

auto SaveLoadService::get_mission_progress(const QString& mission_id,
                                           QString* out_error) const -> QVariantMap {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return {};
  }
  return m_storage->get_mission_progress(mission_id, out_error);
}

auto SaveLoadService::get_campaign_mission_progress(
    const QString& campaign_id, QString* out_error) const -> QVariantList {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return {};
  }
  return m_storage->get_campaign_mission_progress(campaign_id, out_error);
}

auto SaveLoadService::unlock_next_campaign_mission(const QString& campaign_id,
                                                   const QString& completed_mission_id,
                                                   QString* out_error) -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = tr("Save storage unavailable");
    }
    return false;
  }
  return m_storage->unlock_next_mission(campaign_id, completed_mission_id, out_error);
}

void SaveLoadService::open_settings() {
  qInfo() << "Open settings requested";
}

void SaveLoadService::exit_game() {
  qInfo() << "Exit game requested";
  QCoreApplication::quit();
}

} // namespace Game::Systems
