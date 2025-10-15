#include "save_load_service.h"
#include "game/core/serialization.h"
#include "game/core/world.h"
#include <QCoreApplication>
#include <QDebug>

namespace Game {
namespace Systems {

SaveLoadService::SaveLoadService() = default;
SaveLoadService::~SaveLoadService() = default;

bool SaveLoadService::saveGame(Engine::Core::World &world,
                                const QString &filename) {
  qInfo() << "Saving game to:" << filename;

  try {
    // Serialize the world to a JSON document
    QJsonDocument doc = Engine::Core::Serialization::serializeWorld(&world);

    // Save to file
    bool success = Engine::Core::Serialization::saveToFile(filename, doc);

    if (success) {
      qInfo() << "Game saved successfully to:" << filename;
      m_lastError.clear();
      return true;
    } else {
      m_lastError = "Failed to save game to file: " + filename;
      qWarning() << m_lastError;
      return false;
    }
  } catch (const std::exception &e) {
    m_lastError = QString("Exception while saving game: %1").arg(e.what());
    qWarning() << m_lastError;
    return false;
  }
}

bool SaveLoadService::loadGame(Engine::Core::World &world,
                                const QString &filename) {
  qInfo() << "Loading game from:" << filename;

  try {
    // Load JSON document from file
    QJsonDocument doc = Engine::Core::Serialization::loadFromFile(filename);

    if (doc.isNull() || doc.isEmpty()) {
      m_lastError = "Failed to load game from file or file is empty: " + filename;
      qWarning() << m_lastError;
      return false;
    }

    // Clear current world state before loading
    world.clear();

    // Deserialize the world from the JSON document
    Engine::Core::Serialization::deserializeWorld(&world, doc);

    qInfo() << "Game loaded successfully from:" << filename;
    m_lastError.clear();
    return true;
  } catch (const std::exception &e) {
    m_lastError = QString("Exception while loading game: %1").arg(e.what());
    qWarning() << m_lastError;
    return false;
  }
}

void SaveLoadService::openSettings() {
  qInfo() << "Open settings requested";
  // TODO: Implement settings dialog/menu integration
  // This could trigger a signal or callback to open the settings UI
}

void SaveLoadService::exitGame() {
  qInfo() << "Exit game requested";
  // Perform any cleanup before exit if needed
  // Then quit the application
  QCoreApplication::quit();
}

} // namespace Systems
} // namespace Game
