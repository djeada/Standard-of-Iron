#include "map_catalog.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QTimer>
#include <algorithm>

namespace Game {
namespace Map {

MapCatalog::MapCatalog(QObject *parent) : QObject(parent) {}

QVariantList MapCatalog::availableMaps() {
  QVariantList list;
  QDir mapsDir(QStringLiteral("assets/maps"));
  if (!mapsDir.exists())
    return list;

  QStringList files =
      mapsDir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
  for (const QString &f : files) {
    QString path = mapsDir.filePath(f);
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
        if (obj.contains("name") && obj["name"].isString())
          name = obj["name"].toString();
        if (obj.contains("description") && obj["description"].isString())
          desc = obj["description"].toString();

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
      thumbnail = QString("assets/maps/%1_thumb.png").arg(baseName);

      if (!QFileInfo::exists(thumbnail)) {
        thumbnail = "";
      }
    }
    entry["thumbnail"] = thumbnail;

    list.append(entry);
  }
  return list;
}

void MapCatalog::loadMapsAsync() {
  if (m_loading) return;
  
  m_maps.clear();
  m_pendingFiles.clear();
  m_loading = true;
  emit loadingChanged(true);
  
  QDir mapsDir(QStringLiteral("assets/maps"));
  if (!mapsDir.exists()) {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
    return;
  }
  
  m_pendingFiles = mapsDir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);
  
  if (m_pendingFiles.isEmpty()) {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
    return;
  }
  
  // Start loading the first map immediately
  // Subsequent maps are loaded with a small delay to keep UI responsive
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
  QDir mapsDir(QStringLiteral("assets/maps"));
  QString path = mapsDir.filePath(fileName);
  
  QVariantMap entry = loadSingleMap(path);
  if (!entry.isEmpty()) {
    m_maps.append(entry);
    emit mapLoaded(entry);  // Notify that a new map is available
  }
  // Note: Failed/invalid maps are silently skipped
  
  // Schedule next map load with a small delay to keep UI responsive
  // This allows the event loop to process UI updates between map loads
  if (!m_pendingFiles.isEmpty()) {
    QTimer::singleShot(10, this, &MapCatalog::loadNextMap);
  } else {
    m_loading = false;
    emit loadingChanged(false);
    emit allMapsLoaded();
  }
}

QVariantMap MapCatalog::loadSingleMap(const QString &path) {
  QFile file(path);
  QString name = QFileInfo(path).fileName();
  QString desc;
  QSet<int> playerIds;
  
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("name") && obj["name"].isString())
        name = obj["name"].toString();
      if (obj.contains("description") && obj["description"].isString())
        desc = obj["description"].toString();

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

  // Load thumbnail
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
    QString baseName = QFileInfo(path).baseName();
    thumbnail = QString("assets/maps/%1_thumb.png").arg(baseName);

    if (!QFileInfo::exists(thumbnail)) {
      thumbnail = "";
    }
  }
  entry["thumbnail"] = thumbnail;

  return entry;
}

}
}
