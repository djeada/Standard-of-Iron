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
#include <algorithm>

namespace Game {
namespace Map {

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

}
}
