#include "save_slots_view_model.h"

#include <QDebug>
#include <QFileInfo>

#include "../core/user_settings.h"
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
  if (m_service == nullptr) {
    return false;
  }

  QString error;
  if (!m_service->verify_save_slot(slot_name, &error)) {
    emit error_occurred(error);
    return false;
  }
  return true;
}

auto SaveSlotsViewModel::export_save_slot(const QString& slot_name) -> QString {
  if (m_service == nullptr) {
    return {};
  }

  const QString file_stem = Game::Systems::Save::sanitize_file_stem(slot_name);
  if (file_stem.isEmpty()) {
    emit error_occurred(tr("Cannot export a save with an empty name"));
    return {};
  }

  const QString file_path =
      QStringLiteral("%1/%2.%3")
          .arg(Game::Systems::SaveLoadService::exports_directory(),
               file_stem,
               Game::Systems::Save::package_file_suffix());

  QString error;
  if (!m_service->export_slot(slot_name, file_path, &error)) {
    emit error_occurred(error);
    return {};
  }
  return file_path;
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

auto SaveSlotsViewModel::import_save_file(const QString& file_path) -> QString {
  if (m_service == nullptr) {
    return {};
  }

  QString slot_name;
  QString error;
  if (!m_service->import_package(file_path, slot_name, &error)) {
    emit error_occurred(error);
    return {};
  }
  return slot_name;
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

} // namespace App::ViewModels
