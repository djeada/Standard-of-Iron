#include "map_catalog.h"
#include "utils/resource_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <algorithm>

namespace Game {
namespace Map {

MapCatalog::MapCatalog(QObject *parent) : QObject(parent) {}

QVariantList MapCatalog::availableMaps() {
  QVariantList list;
  const QString mapsRoot =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir mapsDir(mapsRoot);
  if (!mapsDir.exists()) {
    return list;
  }

  QStringList files =
      mapsDir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
  for (const QString &f : files) {
    QString path = Utils::Resources::resolveResourcePath(mapsDir.filePath(f));
    QFile file(path);
    QString name = f;
    QString desc;
    QSet<int> playerIds;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      file.close();
      QJsonParseError err;
      QJsonDocument doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("name") && obj["name"].isString()) {
          name = obj["name"].toString();
        }
        if (obj.contains("description") && obj["description"].isString()) {
          desc = obj["description"].toString();
        }

        if (obj.contains("spawns") && obj["spawns"].isArray()) {
          QJsonArray spawns = obj["spawns"].toArray();
          for (const QJsonValue &spawnVal : spawns) {
            if (spawnVal.isObject()) {
              QJsonObject spawn = spawnVal.toObject();
              if (spawn.contains("playerId")) {
                int playerId = spawn["playerId"].toInt();
                if (playerId > 0) {
                  playerIds.insert(playerId);
                }
              }
            }
          }
        }
      }
    }
    QVariantMap entry;
    entry["name"] = name;
    entry["description"] = desc;
    entry["path"] = path;
    entry["playerCount"] = playerIds.size();
    QVariantList playerIdList;
    QList<int> sortedIds = playerIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int id : sortedIds) {
      playerIdList.append(id);
    }
    entry["playerIds"] = playerIdList;

    QString thumbnail;
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      file.close();
      QJsonParseError err;
      QJsonDocument doc = QJsonDocument::fromJson(data, &err);
      if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("thumbnail") && obj["thumbnail"].isString()) {
          thumbnail = obj["thumbnail"].toString();
        }
      }
    }

    if (thumbnail.isEmpty()) {
      QString baseName = QFileInfo(f).baseName();
      QString thumbCandidate = Utils::Resources::resolveResourcePath(
          QString(":/assets/maps/%1_thumb.png").arg(baseName));

      if (QFileInfo::exists(thumbCandidate)) {
        thumbnail = thumbCandidate;
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

  const QString mapsRoot =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir mapsDir(mapsRoot);
  if (!mapsDir.exists()) {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
    return;
  }

  m_pendingFiles =
      mapsDir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

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

  QString fileName = m_pendingFiles.takeFirst();
  const QString mapsRoot =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir mapsDir(mapsRoot);
  QString path =
      Utils::Resources::resolveResourcePath(mapsDir.filePath(fileName));

  QVariantMap entry = loadSingleMap(path);
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

QVariantMap MapCatalog::loadSingleMap(const QString &path) {
  const QString resolvedPath = Utils::Resources::resolveResourcePath(path);
  QFile file(resolvedPath);
  QString name = QFileInfo(resolvedPath).fileName();
  QString desc;
  QSet<int> playerIds;

  if (file.open(QIODevice::ReadOnly)) {
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("name") && obj["name"].isString()) {
        name = obj["name"].toString();
      }
      if (obj.contains("description") && obj["description"].isString()) {
        desc = obj["description"].toString();
      }

      if (obj.contains("spawns") && obj["spawns"].isArray()) {
        QJsonArray spawns = obj["spawns"].toArray();
        for (const QJsonValue &spawnVal : spawns) {
          if (spawnVal.isObject()) {
            QJsonObject spawn = spawnVal.toObject();
            if (spawn.contains("playerId")) {
              int playerId = spawn["playerId"].toInt();
              if (playerId > 0) {
                playerIds.insert(playerId);
              }
            }
          }
        }
      }
    }
  }

  QVariantMap entry;
  entry["name"] = name;
  entry["description"] = desc;
  entry["path"] = resolvedPath;
  entry["playerCount"] = playerIds.size();

  QVariantList playerIdList;
  QList<int> sortedIds = playerIds.values();
  std::sort(sortedIds.begin(), sortedIds.end());
  for (int id : sortedIds) {
    playerIdList.append(id);
  }
  entry["playerIds"] = playerIdList;

  QString thumbnail;
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("thumbnail") && obj["thumbnail"].isString()) {
        thumbnail = obj["thumbnail"].toString();
      }
    }
  }

  if (thumbnail.isEmpty()) {
    QString baseName = QFileInfo(resolvedPath).baseName();
    QString thumbCandidate = Utils::Resources::resolveResourcePath(
        QString(":/assets/maps/%1_thumb.png").arg(baseName));

    if (QFileInfo::exists(thumbCandidate)) {
      thumbnail = thumbCandidate;
    } else {
      thumbnail = "";
    }
  }
  entry["thumbnail"] = thumbnail;

  return entry;
}

} // namespace Map
} // namespace Game
