#include "app/viewmodels/save_slots_view_model.h"

#include <QDebug>
#include <QFileInfo>

#include "app/core/user_settings.h"
#include "game/systems/save_format.h"
#include "game/systems/save_load_service.h"

namespace App::ViewModels {

SaveSlotsViewModel::SaveSlotsViewModel(Game::Systems::SaveLoadService* service,
                                       QObject* parent)
    : QObject(parent)
    , m_service(service) {
}

auto SaveSlotsViewModel::get_save_slots() const -> QVariantList {
  if (m_service == nullptr) {
    qWarning() << "Cannot get save slots: service not initialized";
    return {};
  }
  return m_service->get_save_slots();
}

void SaveSlotsViewModel::refresh_save_slots() {
  emit save_slots_changed();
}

auto SaveSlotsViewModel::delete_save_slot(const QString& slot_name) -> bool {
  if (m_service == nullptr) {
    qWarning() << "Cannot delete save slot: service not initialized";
    return false;
  }

  const bool success = m_service->delete_save_slot(slot_name);
  if (!success) {
    const QString error = m_service->get_last_error();
    qWarning() << "Failed to delete save slot:" << error;
    emit error_occurred(error);
  }
  return success;
}

auto SaveSlotsViewModel::has_save_slot(const QString& slot_name) const -> bool {
  return m_service != nullptr && m_service->slot_exists(slot_name);
}

auto SaveSlotsViewModel::verify_save_slot(const QString& slot_name) -> bool {
  return check_save_slot(slot_name).value(QStringLiteral("ok"), false).toBool();
}

auto SaveSlotsViewModel::check_save_slot(const QString& slot_name) -> QVariantMap {
  QVariantMap result;
  if (m_service == nullptr) {
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("reason"), tr("Save storage unavailable"));
    return result;
  }

  QString error;
  const bool ok = m_service->verify_save_slot(slot_name, &error);
  result.insert(QStringLiteral("ok"), ok);
  result.insert(QStringLiteral("reason"), ok ? QString() : error);
  return result;
}

auto SaveSlotsViewModel::slot_name_rejection(const QString& slot_name) const
    -> QString {
  return Game::Systems::Save::slot_name_rejection(slot_name);
}

auto SaveSlotsViewModel::most_recent_loadable_slot() const -> QVariantMap {
  if (m_service == nullptr) {
    return {};
  }

  for (const QVariant& entry : m_service->get_save_slots()) {
    const QVariantMap slot = entry.toMap();
    if (slot.value(QStringLiteral("loadable"), true).toBool()) {
      return slot;
    }
  }
  return {};
}

auto SaveSlotsViewModel::storage_healthy() const -> bool {
  return m_service != nullptr && m_service->storage_healthy();
}

auto SaveSlotsViewModel::storage_error() const -> QString {
  return m_service == nullptr ? tr("Save storage unavailable")
                              : m_service->storage_error();
}

auto SaveSlotsViewModel::recovered_database_path() const -> QString {
  return m_service == nullptr ? QString() : m_service->quarantined_database_path();
}

auto SaveSlotsViewModel::export_save_slot(const QString& slot_name) -> QVariantMap {
  QVariantMap result;
  result.insert(QStringLiteral("ok"), false);
  result.insert(QStringLiteral("path"), QString());

  if (m_service == nullptr) {
    result.insert(QStringLiteral("reason"), tr("Save storage unavailable"));
    return result;
  }

  QString file_stem = Game::Systems::Save::sanitize_file_stem(slot_name);
  if (file_stem.isEmpty()) {
    result.insert(QStringLiteral("reason"),
                  tr("Cannot export a save with an empty name"));
    return result;
  }

  if (file_stem != slot_name) {
    file_stem += QStringLiteral("_") +
                 Game::Systems::Save::checksum_of(slot_name.toUtf8()).left(6);
  }

  const QString file_path =
      QStringLiteral("%1/%2.%3")
          .arg(Game::Systems::SaveLoadService::exports_directory(),
               file_stem,
               Game::Systems::Save::package_file_suffix());

  QString error;
  if (!m_service->export_slot(slot_name, file_path, &error)) {
    result.insert(QStringLiteral("reason"), error);
    return result;
  }

  result.insert(QStringLiteral("ok"), true);
  result.insert(QStringLiteral("path"), file_path);
  return result;
}

auto SaveSlotsViewModel::list_exported_saves() const -> QVariantList {
  QVariantList result;
  if (m_service == nullptr) {
    return result;
  }

  for (const QString& path : m_service->list_exported_packages()) {
    QVariantMap entry;
    entry.insert(QStringLiteral("path"), path);
    entry.insert(QStringLiteral("name"), QFileInfo(path).completeBaseName());
    result.append(entry);
  }
  return result;
}

auto SaveSlotsViewModel::import_save_file(const QString& file_path) -> QVariantMap {
  QVariantMap result;
  result.insert(QStringLiteral("ok"), false);
  result.insert(QStringLiteral("slot_name"), QString());

  if (m_service == nullptr) {
    result.insert(QStringLiteral("reason"), tr("Save storage unavailable"));
    return result;
  }

  QString slot_name;
  QString error;
  if (!m_service->import_package(file_path, slot_name, &error)) {
    result.insert(QStringLiteral("reason"), error);
    return result;
  }

  result.insert(QStringLiteral("ok"), true);
  result.insert(QStringLiteral("slot_name"), slot_name);
  return result;
}

auto SaveSlotsViewModel::autosave_slot_count() const -> int {
  return App::Core::UserSettings::load_autosave_slot_count();
}

void SaveSlotsViewModel::set_autosave_slot_count(int count) {
  if (count == autosave_slot_count()) {
    return;
  }
  App::Core::UserSettings::save_autosave_slot_count(count);
  if (m_service != nullptr) {
    m_service->prune_autosaves(autosave_slot_count());
  }
  emit autosave_settings_changed();
}

auto SaveSlotsViewModel::autosave_interval_minutes() const -> int {
  return App::Core::UserSettings::load_autosave_interval_minutes();
}

void SaveSlotsViewModel::set_autosave_interval_minutes(int minutes) {
  if (minutes == autosave_interval_minutes()) {
    return;
  }
  App::Core::UserSettings::save_autosave_interval_minutes(minutes);
  emit autosave_interval_changed();
  emit autosave_settings_changed();
}

void SaveSlotsViewModel::save_to_slot(const QString& slot_name) {
  emit save_requested(slot_name);
}

void SaveSlotsViewModel::quicksave() {
  emit quicksave_requested();
}

void SaveSlotsViewModel::autosave() {
  emit autosave_requested();
}

void SaveSlotsViewModel::cancel_active_save() {
  emit cancel_save_requested();
}

void SaveSlotsViewModel::load_from_slot(const QString& slot_name) {
  emit load_requested(slot_name);
}

void SaveSlotsViewModel::set_save_progress(bool in_progress,
                                           int percent,
                                           const QString& stage,
                                           const QString& slot) {
  m_in_progress = in_progress;
  m_percent = percent;
  m_stage = stage;
  m_slot = slot;
  emit save_progress_changed();
}

} // namespace App::ViewModels
