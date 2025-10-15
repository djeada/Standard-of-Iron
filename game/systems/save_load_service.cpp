#include "save_load_service.h"

#include "game/core/serialization.h"
#include "game/core/world.h"
#include "save_storage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <utility>

namespace Game {
namespace Systems {

SaveLoadService::SaveLoadService() {
  ensureSavesDirectoryExists();
  m_storage = std::make_unique<SaveStorage>(getDatabasePath());
  QString initError;
  if (!m_storage->initialize(&initError)) {
    m_lastError = initError;
    qWarning() << "SaveLoadService: failed to initialize storage" << initError;
  }
}

SaveLoadService::~SaveLoadService() = default;

QString SaveLoadService::getSavesDirectory() const {
  QString savesPath = QStandardPaths::writableLocation(
      QStandardPaths::AppDataLocation);
  return savesPath + "/saves";
}

QString SaveLoadService::getDatabasePath() const {
  return getSavesDirectory() + QStringLiteral("/saves.sqlite");
}

void SaveLoadService::ensureSavesDirectoryExists() const {
  QString savesDir = getSavesDirectory();
  QDir dir;
  if (!dir.exists(savesDir)) {
    dir.mkpath(savesDir);
  }
}

bool SaveLoadService::saveGameToSlot(Engine::Core::World &world,
                                     const QString &slotName,
                                     const QString &title,
                                     const QString &mapName,
                                     const QJsonObject &metadata,
                                     const QByteArray &screenshot) {
  qInfo() << "Saving game to slot:" << slotName;

  try {
    if (!m_storage) {
      m_lastError = QStringLiteral("Save storage unavailable");
      qWarning() << m_lastError;
      return false;
    }

    QJsonDocument worldDoc =
        Engine::Core::Serialization::serializeWorld(&world);
    const QByteArray worldBytes =
        worldDoc.toJson(QJsonDocument::Compact);

    QJsonObject combinedMetadata = metadata;
    combinedMetadata["slotName"] = slotName;
    combinedMetadata["title"] = title;
    combinedMetadata["timestamp"] = QDateTime::currentDateTimeUtc().toString(
        Qt::ISODateWithMs);
    if (!combinedMetadata.contains("mapName")) {
      combinedMetadata["mapName"] =
          mapName.isEmpty() ? QStringLiteral("Unknown Map") : mapName;
    }
    combinedMetadata["version"] = QStringLiteral("1.0");

    QString storageError;
    if (!m_storage->saveSlot(slotName, title, combinedMetadata, worldBytes,
                             screenshot, &storageError)) {
      m_lastError = storageError;
      qWarning() << "SaveLoadService: failed to persist slot" << storageError;
      return false;
    }

    m_lastMetadata = combinedMetadata;
    m_lastTitle = title;
    m_lastScreenshot = screenshot;
    m_lastError.clear();
    return true;
  } catch (const std::exception &e) {
    m_lastError =
        QString("Exception while saving game to slot: %1").arg(e.what());
    qWarning() << m_lastError;
    return false;
  }
}

bool SaveLoadService::loadGameFromSlot(Engine::Core::World &world,
                                       const QString &slotName) {
  qInfo() << "Loading game from slot:" << slotName;

  try {
    if (!m_storage) {
      m_lastError = QStringLiteral("Save storage unavailable");
      qWarning() << m_lastError;
      return false;
    }

    QByteArray worldBytes;
    QJsonObject metadata;
    QByteArray screenshot;
    QString title;

    QString loadError;
    if (!m_storage->loadSlot(slotName, worldBytes, metadata, screenshot, title,
                             &loadError)) {
      m_lastError = loadError;
      qWarning() << "SaveLoadService: failed to load slot" << loadError;
      return false;
    }

    QJsonParseError parseError{};
    QJsonDocument doc =
        QJsonDocument::fromJson(worldBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || doc.isNull()) {
      m_lastError = QStringLiteral("Corrupted save data for slot '%1': %2")
                        .arg(slotName, parseError.errorString());
      qWarning() << m_lastError;
      return false;
    }

    world.clear();
    Engine::Core::Serialization::deserializeWorld(&world, doc);

    m_lastMetadata = metadata;
    m_lastTitle = title;
    m_lastScreenshot = screenshot;
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
  if (!m_storage) {
    return {};
  }

  QString listError;
  QVariantList slotList = m_storage->listSlots(&listError);
  if (!listError.isEmpty()) {
    m_lastError = listError;
    qWarning() << "SaveLoadService: failed to enumerate slots" << listError;
  } else {
    m_lastError.clear();
  }
  return slotList;
}

bool SaveLoadService::deleteSaveSlot(const QString &slotName) {
  qInfo() << "Deleting save slot:" << slotName;

  if (!m_storage) {
    m_lastError = QStringLiteral("Save storage unavailable");
    qWarning() << m_lastError;
    return false;
  }

  QString deleteError;
  if (!m_storage->deleteSlot(slotName, &deleteError)) {
    m_lastError = deleteError;
    qWarning() << "SaveLoadService: failed to delete slot" << deleteError;
    return false;
  }

  m_lastError.clear();
  return true;
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
