#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace Game::Systems {
class SaveLoadService;
}

namespace App::ViewModels {

class SaveSlotsViewModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(int autosave_slot_count READ autosave_slot_count WRITE
                 set_autosave_slot_count NOTIFY autosave_settings_changed)
  Q_PROPERTY(int autosave_interval_minutes READ autosave_interval_minutes WRITE
                 set_autosave_interval_minutes NOTIFY autosave_settings_changed)

public:
  explicit SaveSlotsViewModel(Game::Systems::SaveLoadService* service,
                              QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] QVariantList get_save_slots() const;
  Q_INVOKABLE void refresh_save_slots();
  Q_INVOKABLE bool delete_save_slot(const QString& slot_name);
  Q_INVOKABLE [[nodiscard]] bool has_save_slot(const QString& slot_name) const;
  Q_INVOKABLE [[nodiscard]] bool verify_save_slot(const QString& slot_name);
  Q_INVOKABLE [[nodiscard]] QString export_save_slot(const QString& slot_name);
  Q_INVOKABLE [[nodiscard]] QVariantList list_exported_saves() const;
  Q_INVOKABLE [[nodiscard]] QString import_save_file(const QString& file_path);

  [[nodiscard]] int autosave_slot_count() const;
  void set_autosave_slot_count(int count);
  [[nodiscard]] int autosave_interval_minutes() const;
  void set_autosave_interval_minutes(int minutes);

signals:
  void save_slots_changed();
  void autosave_settings_changed();

  void error_occurred(const QString& message);

  void autosave_interval_changed();

private:
  Game::Systems::SaveLoadService* m_service = nullptr;
};

} // namespace App::ViewModels
