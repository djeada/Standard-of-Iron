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

  auto loadGameFromSlot(Engine::Core::World &world,
                        const QString &slotName) -> bool;

  auto getSaveSlots() const -> QVariantList;

  auto deleteSaveSlot(const QString &slotName) -> bool;

  auto getLastError() const -> QString { return m_last_error; }

  void clearError() { m_last_error.clear(); }

  auto getLastMetadata() const -> QJsonObject { return m_lastMetadata; }
  auto getLastTitle() const -> QString { return m_lastTitle; }
  auto getLastScreenshot() const -> QByteArray { return m_lastScreenshot; }

  static void openSettings();

  static void exitGame();

private:
  static auto getSavesDirectory() -> QString;
  auto get_database_path() const -> QString;
  void ensureSavesDirectoryExists() const;

  mutable QString m_last_error;
  QJsonObject m_lastMetadata;
  QString m_lastTitle;
  QByteArray m_lastScreenshot;
  std::unique_ptr<SaveStorage> m_storage;
};

} // namespace Game::Systems
