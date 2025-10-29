#include "map_catalog.h"
#include "json_keys.h"
#include "utils/resource_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <algorithm>
#include <qdir.h>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qlist.h>
#include <qobject.h>
#include <qset.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <qtimer.h>
#include <qtmetamacros.h>

namespace Game::Map {

using namespace JsonKeys;

MapCatalog::MapCatalog(QObject *parent) : QObject(parent) {}

auto MapCatalog::availableMaps() -> QVariantList {
  QVariantList list;
  const QString maps_root =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir const maps_dir(maps_root);
  if (!maps_dir.exists()) {
    return list;
  }

  QStringList const files =
      maps_dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
  for (const QString &f : files) {
    QString const path =
        Utils::Resources::resolveResourcePath(maps_dir.filePath(f));
    QFile file(path);
    QString name = f;
    QString desc;
    QSet<int> player_ids;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray const data = file.readAll();
      file.close();
      QJsonParseError err;
      QJsonDocument const doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains(NAME) && obj[NAME].isString()) {
          name = obj[NAME].toString();
        }
        if (obj.contains(DESCRIPTION) && obj[DESCRIPTION].isString()) {
          desc = obj[DESCRIPTION].toString();
        }

        if (obj.contains(SPAWNS) && obj[SPAWNS].isArray()) {
          QJsonArray const spawns = obj[SPAWNS].toArray();
          for (const QJsonValue &spawn_val : spawns) {
            if (spawn_val.isObject()) {
              QJsonObject spawn = spawn_val.toObject();
              if (spawn.contains(PLAYER_ID)) {
                int const player_id = spawn[PLAYER_ID].toInt();
                if (player_id > 0) {
                  player_ids.insert(player_id);
                }
              }
            }
          }
        }
      }
    }
    QVariantMap entry;
    entry[NAME] = name;
    entry[DESCRIPTION] = desc;
    entry["path"] = path;
    entry["playerCount"] = player_ids.size();
    QVariantList player_id_list;
    QList<int> sorted_ids = player_ids.values();
    std::sort(sorted_ids.begin(), sorted_ids.end());
    for (int const id : sorted_ids) {
      player_id_list.append(id);
    }
    entry["player_ids"] = player_id_list;

    QString thumbnail;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray const data = file.readAll();
      file.close();
      QJsonParseError err;
      QJsonDocument const doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains(THUMBNAIL) && obj[THUMBNAIL].isString()) {
          thumbnail = obj[THUMBNAIL].toString();
        }
      }
    }

    if (thumbnail.isEmpty()) {
      QString const base_name = QFileInfo(f).baseName();
      QString const thumb_candidate = Utils::Resources::resolveResourcePath(
          QString(":/assets/maps/%1_thumb.png").arg(base_name));

      if (QFileInfo::exists(thumb_candidate)) {
        thumbnail = thumb_candidate;
      } else {
        thumbnail = "";
      }
    }
    entry["thumbnail"] = thumbnail;

    list.append(entry);
  }
  return list;
}

void MapCatalog::loadMapsAsync() {
  if (m_loading) {
    return;
  }

  m_maps.clear();
  m_pendingFiles.clear();
  m_loading = true;
  emit loadingChanged(true);

  const QString maps_root =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir const maps_dir(maps_root);
  if (!maps_dir.exists()) {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
    return;
  }

  m_pendingFiles =
      maps_dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

  if (m_pendingFiles.isEmpty()) {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
    return;
  }

  QTimer::singleShot(0, this, &MapCatalog::loadNextMap);
}

void MapCatalog::loadNextMap() {
  if (m_pendingFiles.isEmpty()) {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
    return;
  }

  QString const file_name = m_pendingFiles.takeFirst();
  const QString maps_root =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir const maps_dir(maps_root);
  QString const path =
      Utils::Resources::resolveResourcePath(maps_dir.filePath(file_name));

  QVariantMap const entry = loadSingleMap(path);
  if (!entry.isEmpty()) {
    m_maps.append(entry);
    emit mapLoaded(entry);
  }

  if (!m_pendingFiles.isEmpty()) {
    QTimer::singleShot(10, this, &MapCatalog::loadNextMap);
  } else {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
  }
}

auto MapCatalog::loadSingleMap(const QString &path) -> QVariantMap {
  const QString resolved_path = Utils::Resources::resolveResourcePath(path);
  QFile file(resolved_path);
  QString name = QFileInfo(resolved_path).fileName();
  QString desc;
  QSet<int> player_ids;

  if (file.open(QIODevice::ReadOnly)) {
    QByteArray const data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument const doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains(NAME) && obj[NAME].isString()) {
        name = obj[NAME].toString();
      }
      if (obj.contains(DESCRIPTION) && obj[DESCRIPTION].isString()) {
        desc = obj[DESCRIPTION].toString();
      }

      if (obj.contains(SPAWNS) && obj[SPAWNS].isArray()) {
        QJsonArray const spawns = obj[SPAWNS].toArray();
        for (const QJsonValue &spawn_val : spawns) {
          if (spawn_val.isObject()) {
            QJsonObject spawn = spawn_val.toObject();
            if (spawn.contains(PLAYER_ID)) {
              int const player_id = spawn[PLAYER_ID].toInt();
              if (player_id > 0) {
                player_ids.insert(player_id);
              }
            }
          }
        }
      }
    }
  }

  QVariantMap entry;
  entry[NAME] = name;
  entry[DESCRIPTION] = desc;
  entry["path"] = resolved_path;
  entry["playerCount"] = player_ids.size();

  QVariantList player_id_list;
  QList<int> sorted_ids = player_ids.values();
  std::sort(sorted_ids.begin(), sorted_ids.end());
  for (int const id : sorted_ids) {
    player_id_list.append(id);
  }
  entry["player_ids"] = player_id_list;

  QString thumbnail;
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray const data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument const doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains(THUMBNAIL) && obj[THUMBNAIL].isString()) {
        thumbnail = obj[THUMBNAIL].toString();
      }
    }
  }
  if (thumbnail.isEmpty()) {
    QString const base_name = QFileInfo(resolved_path).baseName();
    QString const thumb_candidate = Utils::Resources::resolveResourcePath(
        QString(":/assets/maps/%1_thumb.png").arg(base_name));

    if (QFileInfo::exists(thumb_candidate)) {
      thumbnail = thumb_candidate;
    } else {
      thumbnail = "";
    }
  }
  entry["thumbnail"] = thumbnail;

  return entry;
}

} // namespace Game::Map
