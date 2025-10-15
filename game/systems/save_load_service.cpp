#include "save_load_service.h"
#include "game/core/serialization.h"
#include "game/core/world.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Game {
namespace Systems {

SaveLoadService::SaveLoadService() = default;
SaveLoadService::~SaveLoadService() = default;

QString SaveLoadService::getSavesDirectory() const {
  QString savesPath = QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation);
  return savesPath + "/saves";
}

void SaveLoadService::ensureSavesDirectoryExists() const {
  QString savesDir = getSavesDirectory();
  QDir dir;
  if (!dir.exists(savesDir)) {
    dir.mkpath(savesDir);
  }
}

QString SaveLoadService::getSlotFilePath(const QString &slotName) const {
  QString sanitized = slotName;
  QRegularExpression regex("[^a-zA-Z0-9_-]");
  sanitized.replace(regex, "_");
  return getSavesDirectory() + "/" + sanitized + ".json";
}

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
      m_lastError =
          "Failed to load game from file or file is empty: " + filename;
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

bool SaveLoadService::saveGameToSlot(Engine::Core::World &world,
                                     const QString &slotName,
                                     const QString &mapName) {
  qInfo() << "Saving game to slot:" << slotName;

  try {
    ensureSavesDirectoryExists();

    // Serialize the world
    QJsonDocument worldDoc =
        Engine::Core::Serialization::serializeWorld(&world);
    QJsonObject worldObj = worldDoc.object();

    // Add metadata
    QJsonObject metadata;
    metadata["slotName"] = slotName;
    metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["mapName"] = mapName.isEmpty() ? "Unknown Map" : mapName;
    metadata["version"] = "1.0";

    worldObj["metadata"] = metadata;

    QJsonDocument finalDoc(worldObj);

    // Save to slot file
    QString filePath = getSlotFilePath(slotName);
    bool success = Engine::Core::Serialization::saveToFile(filePath, finalDoc);

    if (success) {
      qInfo() << "Game saved successfully to slot:" << slotName << "at"
              << filePath;
      m_lastError.clear();
      return true;
    } else {
      m_lastError = "Failed to save game to slot: " + slotName;
      qWarning() << m_lastError;
      return false;
    }
  } catch (const std::exception &e) {
    m_lastError = QString("Exception while saving game to slot: %1").arg(e.what());
    qWarning() << m_lastError;
    return false;
  }
}

bool SaveLoadService::loadGameFromSlot(Engine::Core::World &world,
                                       const QString &slotName) {
  qInfo() << "Loading game from slot:" << slotName;

  try {
    QString filePath = getSlotFilePath(slotName);

    // Check if file exists
    if (!QFile::exists(filePath)) {
      m_lastError = "Save slot not found: " + slotName;
      qWarning() << m_lastError;
      return false;
    }

    // Load JSON document
    QJsonDocument doc = Engine::Core::Serialization::loadFromFile(filePath);

    if (doc.isNull() || doc.isEmpty()) {
      m_lastError = "Failed to load game from slot or file is empty: " + slotName;
      qWarning() << m_lastError;
      return false;
    }

    // Clear current world state before loading
    world.clear();

    // Deserialize the world (metadata will be ignored during deserialization)
    Engine::Core::Serialization::deserializeWorld(&world, doc);

    qInfo() << "Game loaded successfully from slot:" << slotName;
    m_lastError.clear();
    return true;
  } catch (const std::exception &e) {
    m_lastError =
        QString("Exception while loading game from slot: %1").arg(e.what());
    qWarning() << m_lastError;
    return false;
  }
}

QVariantList SaveLoadService::getSaveSlots() const {
  QVariantList slots;

  QString savesDir = getSavesDirectory();
  QDir dir(savesDir);

  if (!dir.exists()) {
    return slots;
  }

  QStringList filters;
  filters << "*.json";
  QFileInfoList files =
      dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);

  for (const QFileInfo &fileInfo : files) {
    try {
      QJsonDocument doc =
          Engine::Core::Serialization::loadFromFile(fileInfo.absoluteFilePath());

      if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        QJsonObject metadata = obj["metadata"].toObject();

        QVariantMap slot;
        slot["name"] = metadata["slotName"].toString(fileInfo.baseName());
        slot["timestamp"] = metadata["timestamp"].toString(
            fileInfo.lastModified().toString(Qt::ISODate));
        slot["mapName"] = metadata["mapName"].toString("Unknown Map");
        slot["filePath"] = fileInfo.absoluteFilePath();

        slots.append(slot);
      }
    } catch (...) {
      // Skip corrupted save files
      continue;
    }
  }

  return slots;
}

bool SaveLoadService::deleteSaveSlot(const QString &slotName) {
  qInfo() << "Deleting save slot:" << slotName;

  QString filePath = getSlotFilePath(slotName);

  if (!QFile::exists(filePath)) {
    m_lastError = "Save slot not found: " + slotName;
    qWarning() << m_lastError;
    return false;
  }

  if (QFile::remove(filePath)) {
    qInfo() << "Save slot deleted successfully:" << slotName;
    m_lastError.clear();
    return true;
  } else {
    m_lastError = "Failed to delete save slot: " + slotName;
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
