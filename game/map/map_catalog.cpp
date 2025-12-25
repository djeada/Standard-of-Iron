#include "map_catalog.h"
#include "campaign_loader.h"
#include "json_keys.h"
#include "mission_loader.h"
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

namespace {

auto resolve_mission_file_path(const QString &mission_id) -> QString {
  const QStringList search_paths = {
      QStringLiteral("assets/missions/%1.json"),
      QStringLiteral("../assets/missions/%1.json"),
      QStringLiteral("../../assets/missions/%1.json"),
      QStringLiteral(":/assets/missions/%1.json"),
      QStringLiteral("/assets/missions/%1.json"),
      QStringLiteral("/../assets/missions/%1.json")};

  for (const auto &pattern : search_paths) {
    QString candidate = pattern.arg(mission_id);
    candidate = Utils::Resources::resolveResourcePath(candidate);
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

auto collect_campaign_files() -> QStringList {
  const QStringList search_paths = {QStringLiteral("assets/campaigns"),
                                    QStringLiteral("../assets/campaigns"),
                                    QStringLiteral("../../assets/campaigns"),
                                    QStringLiteral(":/assets/campaigns"),
                                    QStringLiteral("/assets/campaigns"),
                                    QStringLiteral("/../assets/campaigns")};

  QStringList files;
  QSet<QString> seen;
  for (const auto &path : search_paths) {
    const QString resolved = Utils::Resources::resolveResourcePath(path);
    QDir campaigns_dir(resolved);
    if (!campaigns_dir.exists()) {
      continue;
    }
    const QStringList entries = campaigns_dir.entryList(
        QStringList() << "*.json", QDir::Files, QDir::Name);
    for (const auto &entry : entries) {
      const QString campaign_path = campaigns_dir.filePath(entry);
      if (!seen.contains(campaign_path)) {
        seen.insert(campaign_path);
        files.append(campaign_path);
      }
    }
  }
  return files;
}

auto load_campaign_map_paths() -> QSet<QString> {
  QSet<QString> map_paths;
  const QStringList campaign_files = collect_campaign_files();
  for (const auto &campaign_path : campaign_files) {
    Game::Campaign::CampaignDefinition campaign;
    QString error;
    if (!Game::Campaign::CampaignLoader::loadFromJsonFile(campaign_path,
                                                          campaign, &error)) {
      qWarning() << "Failed to load campaign for map filtering:"
                 << campaign_path << error;
      continue;
    }

    for (const auto &mission : campaign.missions) {
      const QString mission_file =
          resolve_mission_file_path(mission.mission_id);
      if (mission_file.isEmpty()) {
        qWarning() << "Missing mission file for campaign map filtering:"
                   << mission.mission_id;
        continue;
      }

      Game::Mission::MissionDefinition mission_def;
      if (!Game::Mission::MissionLoader::loadFromJsonFile(
              mission_file, mission_def, &error)) {
        qWarning() << "Failed to load mission for map filtering:"
                   << mission_file << error;
        continue;
      }

      const QString map_path =
          Utils::Resources::resolveResourcePath(mission_def.map_path);
      if (!map_path.isEmpty()) {
        map_paths.insert(map_path);
      }
    }
  }

  return map_paths;
}

} // namespace

MapCatalog::MapCatalog(QObject *parent) : QObject(parent) {}

auto MapCatalog::availableMaps() -> QVariantList {
  QVariantList list;
  const QSet<QString> campaign_map_paths = load_campaign_map_paths();
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
    if (campaign_map_paths.contains(path)) {
      continue;
    }
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

void MapCatalog::load_maps_async() {
  if (m_loading) {
    return;
  }

  m_maps.clear();
  m_pendingFiles.clear();
  ensure_campaign_map_paths_loaded();
  m_loading = true;
  emit loading_changed(true);

  const QString maps_root =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir const maps_dir(maps_root);
  if (!maps_dir.exists()) {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
    return;
  }

  m_pendingFiles =
      maps_dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

  if (m_pendingFiles.isEmpty()) {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
    return;
  }

  QTimer::singleShot(0, this, &MapCatalog::load_next_map);
}

void MapCatalog::load_next_map() {
  if (m_pendingFiles.isEmpty()) {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
    return;
  }

  QString const file_name = m_pendingFiles.takeFirst();
  const QString maps_root =
      Utils::Resources::resolveResourcePath(QStringLiteral(":/assets/maps"));
  QDir const maps_dir(maps_root);
  QString const path =
      Utils::Resources::resolveResourcePath(maps_dir.filePath(file_name));

  if (!m_campaign_map_paths.contains(path)) {
    QVariantMap const entry = loadSingleMap(path);
    if (!entry.isEmpty()) {
      m_maps.append(entry);
      emit map_loaded(entry);
    }
  }

  if (!m_pendingFiles.isEmpty()) {
    QTimer::singleShot(10, this, &MapCatalog::load_next_map);
  } else {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
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

void MapCatalog::ensure_campaign_map_paths_loaded() {
  if (m_campaign_map_paths_loaded) {
    return;
  }

  m_campaign_map_paths = load_campaign_map_paths();
  m_campaign_map_paths_loaded = true;
}

} // namespace Game::Map
