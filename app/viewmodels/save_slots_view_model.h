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
  Q_PROPERTY(bool save_in_progress READ save_in_progress NOTIFY save_progress_changed)
  Q_PROPERTY(
      int save_progress_percent READ save_progress_percent NOTIFY save_progress_changed)
  Q_PROPERTY(
      QString save_progress_stage READ save_progress_stage NOTIFY save_progress_changed)
  Q_PROPERTY(
      QString save_progress_slot READ save_progress_slot NOTIFY save_progress_changed)

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

  Q_INVOKABLE void save_to_slot(const QString& slot_name);
  Q_INVOKABLE void quicksave();
  Q_INVOKABLE void autosave();
  Q_INVOKABLE void cancel_active_save();
  Q_INVOKABLE void load_from_slot(const QString& slot_name);

  [[nodiscard]] auto save_in_progress() const -> bool { return m_in_progress; }
  [[nodiscard]] auto save_progress_percent() const -> int { return m_percent; }
  [[nodiscard]] auto save_progress_stage() const -> QString { return m_stage; }
  [[nodiscard]] auto save_progress_slot() const -> QString { return m_slot; }
  void set_save_progress(bool in_progress,
                         int percent,
                         const QString& stage,
                         const QString& slot);

  [[nodiscard]] int autosave_slot_count() const;
  void set_autosave_slot_count(int count);
  [[nodiscard]] int autosave_interval_minutes() const;
  void set_autosave_interval_minutes(int minutes);

signals:
  void save_requested(const QString& slot_name);
  void quicksave_requested();
  void autosave_requested();
  void cancel_save_requested();
  void load_requested(const QString& slot_name);
  void save_progress_changed();
  void save_completed(QString slot_name, bool success, QString error);

  void save_slots_changed();
  void autosave_settings_changed();

  void error_occurred(const QString& message);

  void autosave_interval_changed();

private:
  Game::Systems::SaveLoadService* m_service = nullptr;
  bool m_in_progress = false;
  int m_percent = 0;
  QString m_stage;
  QString m_slot;
};

} // namespace App::ViewModels
