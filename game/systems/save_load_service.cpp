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
#include <QJsonParseError>
#include <QStandardPaths>

#include <exception>
#include <memory>
#include <qcoreapplication.h>
#include <qdir.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qnamespace.h>
#include <qstandardpaths.h>
#include <qstringliteral.h>

namespace Game::Systems {

SaveLoadService::SaveLoadService() {
  ensureSavesDirectoryExists();
  m_storage = std::make_unique<SaveStorage>(get_database_path());
  QString init_error;
  if (!m_storage->initialize(&init_error)) {
    m_last_error = init_error;
    qWarning() << "SaveLoadService: failed to initialize storage" << init_error;
  }
}

SaveLoadService::~SaveLoadService() = default;

QString SaveLoadService::getSavesDirectory() {
  QString const savesPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return savesPath + "/saves";
}

auto SaveLoadService::get_database_path() const -> QString {
  return getSavesDirectory() + QStringLiteral("/saves.sqlite");
}

void SaveLoadService::ensureSavesDirectoryExists() const {
  QString const savesDir = getSavesDirectory();
  QDir const dir;
  if (!dir.exists(savesDir)) {
    dir.mkpath(savesDir);
  }
}

auto SaveLoadService::saveGameToSlot(Engine::Core::World &world,
                                     const QString &slotName,
                                     const QString &title,
                                     const QString &map_name,
                                     const QJsonObject &metadata,
                                     const QByteArray &screenshot) -> bool {
  qInfo() << "Saving game to slot:" << slotName;

  try {
    if (!m_storage) {
      m_last_error = QStringLiteral("Save storage unavailable");
      qWarning() << m_last_error;
      return false;
    }

    QJsonDocument const worldDoc =
        Engine::Core::Serialization::serializeWorld(&world);
    const QByteArray worldBytes = worldDoc.toJson(QJsonDocument::Compact);

    QJsonObject combinedMetadata = metadata;
    combinedMetadata["slotName"] = slotName;
    combinedMetadata["title"] = title;
    combinedMetadata["timestamp"] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!combinedMetadata.contains("map_name")) {
      combinedMetadata["map_name"] =
          map_name.isEmpty() ? QStringLiteral("Unknown Map") : map_name;
    }
    combinedMetadata["version"] = QStringLiteral("1.0");

    QString storage_error;
    if (!m_storage->saveSlot(slotName, title, combinedMetadata, worldBytes,
                             screenshot, &storage_error)) {
      m_last_error = storage_error;
      qWarning() << "SaveLoadService: failed to persist slot" << storage_error;
      return false;
    }

    m_lastMetadata = combinedMetadata;
    m_lastTitle = title;
    m_lastScreenshot = screenshot;
    m_last_error.clear();
    return true;
  } catch (const std::exception &e) {
    m_last_error =
        QString("Exception while saving game to slot: %1").arg(e.what());
    qWarning() << m_last_error;
    return false;
  }
}

auto SaveLoadService::loadGameFromSlot(Engine::Core::World &world,
                                       const QString &slotName) -> bool {
  qInfo() << "Loading game from slot:" << slotName;

  try {
    if (!m_storage) {
      m_last_error = QStringLiteral("Save storage unavailable");
      qWarning() << m_last_error;
      return false;
    }

    QByteArray worldBytes;
    QJsonObject metadata;
    QByteArray screenshot;
    QString title;

    QString load_error;
    if (!m_storage->loadSlot(slotName, worldBytes, metadata, screenshot, title,
                             &load_error)) {
      m_last_error = load_error;
      qWarning() << "SaveLoadService: failed to load slot" << load_error;
      return false;
    }

    QJsonParseError parse_error{};
    QJsonDocument const doc = QJsonDocument::fromJson(worldBytes, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || doc.isNull()) {
      m_last_error = QStringLiteral("Corrupted save data for slot '%1': %2")
                         .arg(slotName, parse_error.errorString());
      qWarning() << m_last_error;
      return false;
    }

    world.clear();
    Engine::Core::Serialization::deserializeWorld(&world, doc);

    m_lastMetadata = metadata;
    m_lastTitle = title;
    m_lastScreenshot = screenshot;
    m_last_error.clear();
    return true;
  } catch (const std::exception &e) {
    m_last_error =
        QString("Exception while loading game from slot: %1").arg(e.what());
    qWarning() << m_last_error;
    return false;
  }
}

auto SaveLoadService::getSaveSlots() const -> QVariantList {
  if (!m_storage) {
    return {};
  }

  QString list_error;
  QVariantList slotList = m_storage->listSlots(&list_error);
  if (!list_error.isEmpty()) {
    m_last_error = list_error;
    qWarning() << "SaveLoadService: failed to enumerate slots" << list_error;
  } else {
    m_last_error.clear();
  }
  return slotList;
}

auto SaveLoadService::deleteSaveSlot(const QString &slotName) -> bool {
  qInfo() << "Deleting save slot:" << slotName;

  if (!m_storage) {
    m_last_error = QStringLiteral("Save storage unavailable");
    qWarning() << m_last_error;
    return false;
  }

  QString delete_error;
  if (!m_storage->deleteSlot(slotName, &delete_error)) {
    m_last_error = delete_error;
    qWarning() << "SaveLoadService: failed to delete slot" << delete_error;
    return false;
  }

  m_last_error.clear();
  return true;
}

void SaveLoadService::openSettings() { qInfo() << "Open settings requested"; }

void SaveLoadService::exitGame() {
  qInfo() << "Exit game requested";

  QCoreApplication::quit();
}

} // namespace Game::Systems
