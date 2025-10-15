#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QVariantList>
#include <QString>

#include <memory>

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

namespace Game {
namespace Systems {

class SaveStorage;

class SaveLoadService {
public:
  SaveLoadService();
  ~SaveLoadService();

  // Save game to named slot with metadata
  bool saveGameToSlot(Engine::Core::World &world, const QString &slotName,
                      const QString &title, const QString &mapName,
                      const QJsonObject &metadata = {},
                      const QByteArray &screenshot = QByteArray());

  // Load game from named slot
  bool loadGameFromSlot(Engine::Core::World &world, const QString &slotName);

  // Get list of all save slots with metadata
  QVariantList getSaveSlots() const;

  // Delete a save slot
  bool deleteSaveSlot(const QString &slotName);

  // Get the last error message
  QString getLastError() const { return m_lastError; }

  // Clear the last error
  void clearError() { m_lastError.clear(); }

  QJsonObject getLastMetadata() const { return m_lastMetadata; }
  QString getLastTitle() const { return m_lastTitle; }
  QByteArray getLastScreenshot() const { return m_lastScreenshot; }

  // Settings-related functionality
  void openSettings();

  // Exit game functionality
  void exitGame();

private:
  QString getSavesDirectory() const;
  QString getDatabasePath() const;
  void ensureSavesDirectoryExists() const;

  mutable QString m_lastError;
  QJsonObject m_lastMetadata;
  QString m_lastTitle;
  QByteArray m_lastScreenshot;
  std::unique_ptr<SaveStorage> m_storage;
};

} // namespace Systems
} // namespace Game
