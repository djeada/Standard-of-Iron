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

  auto saveGameToSlot(Engine::Core::World &world, const QString &slotName,
                      const QString &title, const QString &map_name,
                      const QJsonObject &metadata = {},
                      const QByteArray &screenshot = QByteArray()) -> bool;

  auto load_game_from_slot(Engine::Core::World &world,
                           const QString &slotName) -> bool;

  auto get_save_slots() const -> QVariantList;

  auto deleteSaveSlot(const QString &slotName) -> bool;

  auto getLastError() const -> QString { return m_last_error; }

  void clearError() { m_last_error.clear(); }

  auto getLastMetadata() const -> QJsonObject { return m_last_metadata; }
  auto getLastTitle() const -> QString { return m_lastTitle; }
  auto getLastScreenshot() const -> QByteArray { return m_last_screenshot; }

  auto list_campaigns(QString *out_error = nullptr) const -> QVariantList;
  auto get_campaign_progress(const QString &campaign_id,
                             QString *out_error = nullptr) const -> QVariantMap;
  auto mark_campaign_completed(const QString &campaign_id,
                               QString *out_error = nullptr) -> bool;

  static void openSettings();

  static void exitGame();

private:
  static auto getSavesDirectory() -> QString;
  static auto get_database_path() -> QString;
  static void ensureSavesDirectoryExists();

  mutable QString m_last_error;
  QJsonObject m_last_metadata;
  QString m_lastTitle;
  QByteArray m_last_screenshot;
  std::unique_ptr<SaveStorage> m_storage;
};

} // namespace Game::Systems
