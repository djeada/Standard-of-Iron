#include "map_catalog.h"

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
#include <qdir.h>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qlist.h>
#include <qobject.h>
#include <qset.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <qtimer.h>
#include <qtmetamacros.h>

#include <algorithm>

#include "game/util/asset_text.h"
#include "json_keys.h"
#include "mission_catalog.h"
#include "utils/resource_utils.h"

namespace Game::Map {

using namespace JsonKeys;

namespace {

auto is_hidden_from_skirmish(const QJsonObject& map_object) -> bool {
  return map_object.value(SKIRMISH_HIDDEN).toBool(false);
}

auto has_scripted_opposition(const QJsonObject& map_object) -> bool {
  return map_object.contains(UNDEAD_ZONES) &&
         map_object.value(UNDEAD_ZONES).isArray() &&
         !map_object.value(UNDEAD_ZONES).toArray().isEmpty();
}

auto maps_dir() -> QDir {
  return QDir(Utils::Resources::resolve_resource_path(QStringLiteral(":/assets/maps")));
}

} // namespace

MapCatalog::MapCatalog(QObject* parent)
    : QObject(parent) {
}

auto MapCatalog::available_maps() -> QVariantList {
  QVariantList list;
  const QSet<QString> mission_map_paths = MissionCatalog::mission_map_paths();
  QDir const dir = maps_dir();
  if (!dir.exists()) {
    return list;
  }

  for (const QString& file_name :
       dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name)) {
    QString const path =
        Utils::Resources::resolve_resource_path(dir.filePath(file_name));
    if (mission_map_paths.contains(path)) {
      continue;
    }
    QVariantMap const entry = load_single_map(path);
    if (!entry.isEmpty()) {
      list.append(entry);
    }
  }
  return list;
}

void MapCatalog::load_maps_async() {
  if (m_loading) {
    return;
  }

  m_maps.clear();
  m_pending_files.clear();
  ensure_mission_map_paths_loaded();
  m_loading = true;
  emit loading_changed(true);

  QDir const dir = maps_dir();
  if (!dir.exists()) {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
    return;
  }

  m_pending_files = dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

  if (m_pending_files.isEmpty()) {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
    return;
  }

  QTimer::singleShot(0, this, &MapCatalog::load_next_map);
}

void MapCatalog::load_next_map() {
  if (m_pending_files.isEmpty()) {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
    return;
  }

  QString const file_name = m_pending_files.takeFirst();
  QString const path =
      Utils::Resources::resolve_resource_path(maps_dir().filePath(file_name));

  if (!m_mission_map_paths.contains(path)) {
    QVariantMap const entry = load_single_map(path);
    if (!entry.isEmpty()) {
      m_maps.append(entry);
      emit map_loaded(entry);
    }
  }

  if (!m_pending_files.isEmpty()) {
    QTimer::singleShot(10, this, &MapCatalog::load_next_map);
  } else {
    m_loading = false;
    emit loading_changed(false);
    emit all_maps_loaded();
  }
}

auto MapCatalog::load_single_map(const QString& path) -> QVariantMap {
  const QString resolved_path = Utils::Resources::resolve_resource_path(path);
  QFile file(resolved_path);
  QString name = QFileInfo(resolved_path).fileName();
  QString desc;
  QSet<int> player_ids;
  bool solo_playable = false;
  QString thumbnail;

  if (file.open(QIODevice::ReadOnly)) {
    QByteArray const data = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument const doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (is_hidden_from_skirmish(obj)) {
        return {};
      }
      if (obj.contains(NAME) && obj[NAME].isString()) {
        name = obj[NAME].toString();
      }
      if (obj.contains(DESCRIPTION) && obj[DESCRIPTION].isString()) {
        desc = obj[DESCRIPTION].toString();
      }
      if (obj.contains(THUMBNAIL) && obj[THUMBNAIL].isString()) {
        thumbnail = obj[THUMBNAIL].toString();
      }
      solo_playable = has_scripted_opposition(obj);

      for (const char* collection : {SPAWNS, STRUCTURES}) {
        if (obj.contains(collection) && obj[collection].isArray()) {
          QJsonArray const entries = obj[collection].toArray();
          for (const QJsonValue entry_val : entries) {
            if (entry_val.isObject()) {
              QJsonObject entry = entry_val.toObject();
              if (entry.contains(PLAYER_ID)) {
                int const player_id = entry[PLAYER_ID].toInt();
                if (player_id > 0) {
                  player_ids.insert(player_id);
                }
              }
            }
          }
        }
      }
    }
  }

  QVariantMap entry;
  entry[NAME] = Util::tr_asset(Util::k_maps_context, name);
  entry[DESCRIPTION] = Util::tr_asset(Util::k_maps_context, desc);
  entry["path"] = resolved_path;
  entry["playerCount"] = player_ids.size();
  entry["soloPlayable"] = solo_playable;

  QVariantList player_id_list;
  QList<int> sorted_ids = player_ids.values();
  std::sort(sorted_ids.begin(), sorted_ids.end());
  for (int const id : sorted_ids) {
    player_id_list.append(id);
  }
  entry["player_ids"] = player_id_list;

  if (thumbnail.isEmpty()) {
    QString const base_name = QFileInfo(resolved_path).baseName();
    QString const thumb_candidate = Utils::Resources::resolve_resource_path(
        QString(":/assets/maps/%1_thumb.png").arg(base_name));
    if (QFileInfo::exists(thumb_candidate)) {
      thumbnail = thumb_candidate;
    }
  }
  entry["thumbnail"] = thumbnail;

  return entry;
}

void MapCatalog::ensure_mission_map_paths_loaded() {
  if (m_mission_map_paths_loaded) {
    return;
  }
  m_mission_map_paths = MissionCatalog::mission_map_paths();
  m_mission_map_paths_loaded = true;
}

} // namespace Game::Map
