#pragma once

#include <QString>
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

  // Get the last error message
  QString getLastError() const { return m_lastError; }

  // Clear the last error
  void clearError() { m_lastError.clear(); }

  // Settings-related functionality
  void openSettings();

  // Exit game functionality
  void exitGame();

private:
  QString m_lastError;
};

} // namespace Systems
} // namespace Game
