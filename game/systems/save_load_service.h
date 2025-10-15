#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

namespace Game {
namespace Systems {

class SaveLoadService {
public:
  SaveLoadService();
  ~SaveLoadService();

  // Save current game state to file
  bool saveGame(Engine::Core::World &world, const QString &filename);

  // Load game state from file
  bool loadGame(Engine::Core::World &world, const QString &filename);

  // Save game to named slot with metadata
  bool saveGameToSlot(Engine::Core::World &world, const QString &slotName,
                      const QString &mapName = QString());

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

  // Settings-related functionality
  void openSettings();

  // Exit game functionality
  void exitGame();

private:
  QString getSlotFilePath(const QString &slotName) const;
  QString getSavesDirectory() const;
  void ensureSavesDirectoryExists() const;

  QString m_lastError;
};

} // namespace Systems
} // namespace Game
