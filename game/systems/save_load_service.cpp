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

auto SaveLoadService::getSavesDirectory() -> QString {
  QString const saves_path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return saves_path + "/saves";
}

auto SaveLoadService::get_database_path() -> QString {
  return getSavesDirectory() + QStringLiteral("/saves.sqlite");
}

void SaveLoadService::ensureSavesDirectoryExists() {
  QString const saves_dir = getSavesDirectory();
  QDir const dir;
  if (!dir.exists(saves_dir)) {
    dir.mkpath(saves_dir);
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

    QJsonDocument const world_doc =
        Engine::Core::Serialization::serializeWorld(&world);
    const QByteArray world_bytes = world_doc.toJson(QJsonDocument::Compact);

    QJsonObject combined_metadata = metadata;
    combined_metadata["slotName"] = slotName;
    combined_metadata["title"] = title;
    combined_metadata["timestamp"] =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!combined_metadata.contains("map_name")) {
      combined_metadata["map_name"] =
          map_name.isEmpty() ? QStringLiteral("Unknown Map") : map_name;
    }
    combined_metadata["version"] = QStringLiteral("1.0");

    QString storage_error;
    if (!m_storage->saveSlot(slotName, title, combined_metadata, world_bytes,
                             screenshot, &storage_error)) {
      m_last_error = storage_error;
      qWarning() << "SaveLoadService: failed to persist slot" << storage_error;
      return false;
    }

    m_lastMetadata = combined_metadata;
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

    QByteArray world_bytes;
    QJsonObject metadata;
    QByteArray screenshot;
    QString title;

    QString load_error;
    if (!m_storage->loadSlot(slotName, world_bytes, metadata, screenshot, title,
                             &load_error)) {
      m_last_error = load_error;
      qWarning() << "SaveLoadService: failed to load slot" << load_error;
      return false;
    }

    QJsonParseError parse_error{};
    QJsonDocument const doc =
        QJsonDocument::fromJson(world_bytes, &parse_error);
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
  QVariantList slot_list = m_storage->listSlots(&list_error);
  if (!list_error.isEmpty()) {
    m_last_error = list_error;
    qWarning() << "SaveLoadService: failed to enumerate slots" << list_error;
  } else {
    m_last_error.clear();
  }
  return slot_list;
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

auto SaveLoadService::list_campaigns(QString *out_error) const -> QVariantList {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = "Storage not initialized";
    }
    return {};
  }
  return m_storage->list_campaigns(out_error);
}

auto SaveLoadService::get_campaign_progress(
    const QString &campaign_id, QString *out_error) const -> QVariantMap {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = "Storage not initialized";
    }
    return {};
  }
  return m_storage->get_campaign_progress(campaign_id, out_error);
}

auto SaveLoadService::mark_campaign_completed(const QString &campaign_id,
                                              QString *out_error) -> bool {
  if (!m_storage) {
    if (out_error != nullptr) {
      *out_error = "Storage not initialized";
    }
    return false;
  }
  return m_storage->mark_campaign_completed(campaign_id, out_error);
}

void SaveLoadService::openSettings() { qInfo() << "Open settings requested"; }

void SaveLoadService::exitGame() {
  qInfo() << "Exit game requested";

  QCoreApplication::quit();
}

} // namespace Game::Systems
