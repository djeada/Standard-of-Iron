#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class SaveStorage;

class SaveLoadService {
public:
  SaveLoadService();
  ~SaveLoadService();

  auto save_game_to_slot(Engine::Core::World &world, const QString &slot_name,
                         const QString &title, const QString &map_name,
                         const QJsonObject &metadata = {},
                         const QByteArray &screenshot = QByteArray()) -> bool;

  auto load_game_from_slot(Engine::Core::World &world,
                           const QString &slot_name) -> bool;

  auto get_save_slots() const -> QVariantList;

  auto delete_save_slot(const QString &slot_name) -> bool;

  auto get_last_error() const -> QString { return m_last_error; }

  void clear_error() { m_last_error.clear(); }

  auto get_last_metadata() const -> QJsonObject { return m_last_metadata; }
  auto get_last_title() const -> QString { return m_last_title; }
  auto get_last_screenshot() const -> QByteArray { return m_last_screenshot; }

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

  auto unlock_next_campaign_mission(const QString &campaign_id,
                                    const QString &completed_mission_id,
                                    QString *out_error = nullptr) -> bool;

  static SaveLoadService *instance();

  static void open_settings();

  static void exit_game();

private:
  static auto get_saves_directory() -> QString;
  static auto get_database_path() -> QString;
  static void ensure_saves_directory_exists();

  mutable QString m_last_error;
  QJsonObject m_last_metadata;
  QString m_last_title;
  QByteArray m_last_screenshot;
  std::unique_ptr<SaveStorage> m_storage;
};

} // namespace Game::Systems
