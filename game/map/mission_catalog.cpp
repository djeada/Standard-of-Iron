#include "mission_catalog.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

#include "campaign_loader.h"
#include "game/map/campaign_definition.h"
#include "game/map/mission_definition.h"
#include "game/util/asset_text.h"
#include "json_keys.h"
#include "mission_loader.h"
#include "utils/resource_utils.h"

namespace Game::Map {

using namespace JsonKeys;

namespace {

constexpr const char* k_menu_order = "menu_order";

auto read_json_object(const QString& path) -> QJsonObject {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QByteArray data = file.readAll();
  file.close();
  QJsonParseError error{};
  const QJsonDocument document = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return {};
  }
  return document.object();
}

auto missions_dir() -> QDir {
  return QDir(
      Utils::Resources::resolve_resource_path(QStringLiteral(":/assets/missions")));
}

auto mission_files() -> QStringList {
  const QDir dir = missions_dir();
  if (!dir.exists()) {
    return {};
  }
  QStringList paths;
  for (const QString& entry :
       dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name)) {
    paths.append(Utils::Resources::resolve_resource_path(dir.filePath(entry)));
  }
  return paths;
}

auto load_mission(const QString& path,
                  Game::Mission::MissionDefinition& out_mission) -> bool {
  QString error;
  if (Game::Mission::MissionLoader::load_from_json_file(path, out_mission, &error)) {
    return true;
  }
  qWarning() << "Mission catalog: cannot read" << path << error;
  return false;
}

auto build_objective_list(const std::vector<Game::Mission::Condition>& conditions)
    -> QVariantList {
  QVariantList list;
  list.reserve(static_cast<int>(conditions.size()));
  for (const auto& condition : conditions) {
    QVariantMap entry;
    entry[QStringLiteral("type")] = condition.type;
    entry[QStringLiteral("description")] =
        Util::tr_asset(Util::k_missions_context, condition.description);
    list.append(entry);
  }
  return list;
}

auto build_starting_force(const std::vector<Game::Mission::UnitSetup>& units)
    -> QVariantList {
  QVariantList list;
  for (const auto& unit : units) {
    QVariantMap entry;
    entry[QStringLiteral("type")] = unit.type;
    entry[QStringLiteral("count")] = std::max(1, unit.count);
    list.append(entry);
  }
  return list;
}

auto build_map_summary(const QString& map_path) -> QVariantMap {
  QVariantMap summary;
  const QString resolved = Utils::Resources::resolve_resource_path(map_path);
  summary[QStringLiteral("map_path")] = resolved;

  const QJsonObject map_object = read_json_object(resolved);
  if (map_object.isEmpty()) {
    return summary;
  }

  summary[QStringLiteral("map_name")] =
      Util::tr_asset(Util::k_maps_context, map_object.value(NAME).toString());
  const QJsonObject grid = map_object.value(GRID).toObject();
  summary[QStringLiteral("map_width")] = grid.value(WIDTH).toInt();
  summary[QStringLiteral("map_height")] = grid.value(HEIGHT).toInt();

  QString thumbnail = map_object.value(THUMBNAIL).toString();
  if (thumbnail.isEmpty()) {
    const QString candidate = Utils::Resources::resolve_resource_path(
        QStringLiteral(":/assets/maps/%1_thumb.png")
            .arg(QFileInfo(resolved).baseName()));
    if (QFileInfo::exists(candidate)) {
      thumbnail = candidate;
    }
  }
  summary[QStringLiteral("thumbnail")] = thumbnail;
  return summary;
}

auto campaign_files() -> QStringList {
  const QStringList search_paths = {QStringLiteral("assets/campaigns"),
                                    QStringLiteral("../assets/campaigns"),
                                    QStringLiteral("../../assets/campaigns"),
                                    QStringLiteral(":/assets/campaigns"),
                                    QStringLiteral("/assets/campaigns"),
                                    QStringLiteral("/../assets/campaigns")};

  QStringList files;
  QSet<QString> seen;
  for (const QString& path : search_paths) {
    const QDir dir(Utils::Resources::resolve_resource_path(path));
    if (!dir.exists()) {
      continue;
    }
    for (const QString& entry :
         dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name)) {
      const QString campaign_path = dir.filePath(entry);
      if (!seen.contains(campaign_path)) {
        seen.insert(campaign_path);
        files.append(campaign_path);
      }
    }
  }
  return files;
}

} // namespace

auto MissionCatalog::resolve_mission_file(const QString& mission_id) -> QString {
  if (mission_id.isEmpty()) {
    return {};
  }
  const QStringList patterns = {QStringLiteral("assets/missions/%1.json"),
                                QStringLiteral("../assets/missions/%1.json"),
                                QStringLiteral("../../assets/missions/%1.json"),
                                QStringLiteral(":/assets/missions/%1.json"),
                                QStringLiteral("/assets/missions/%1.json"),
                                QStringLiteral("/../assets/missions/%1.json")};

  for (const QString& pattern : patterns) {
    const QString candidate =
        Utils::Resources::resolve_resource_path(pattern.arg(mission_id));
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

auto MissionCatalog::campaign_mission_ids() -> QSet<QString> {
  QSet<QString> ids;
  for (const QString& campaign_path : campaign_files()) {
    Game::Campaign::CampaignDefinition campaign;
    QString error;
    if (!Game::Campaign::CampaignLoader::load_from_json_file(
            campaign_path, campaign, &error)) {
      qWarning() << "Mission catalog: cannot read campaign" << campaign_path << error;
      continue;
    }
    for (const auto& mission : campaign.missions) {
      ids.insert(mission.mission_id);
    }
  }
  return ids;
}

auto MissionCatalog::standalone_missions() -> QVariantList {
  const QSet<QString> owned = campaign_mission_ids();

  QVariantList entries;
  for (const QString& path : mission_files()) {
    Game::Mission::MissionDefinition mission;
    if (!load_mission(path, mission)) {
      continue;
    }
    if (mission.tutorial || owned.contains(mission.id)) {
      continue;
    }

    QVariantMap entry = build_map_summary(mission.map_path);
    entry[QStringLiteral("mission_id")] = mission.id;
    entry[QStringLiteral("file_path")] = path;
    entry[QStringLiteral("title")] =
        Util::tr_asset(Util::k_missions_context, mission.title);
    entry[QStringLiteral("summary")] =
        Util::tr_asset(Util::k_missions_context, mission.summary);
    entry[QStringLiteral("victory_mode")] = mission.victory_mode;
    entry[QStringLiteral("starting_force")] =
        build_starting_force(mission.player_setup.starting_units);
    entry[QStringLiteral("objectives")] =
        build_objective_list(mission.victory_conditions);
    entry[QStringLiteral("optional_objectives")] =
        build_objective_list(mission.optional_objectives);
    entry[QStringLiteral("defeat_conditions")] =
        build_objective_list(mission.defeat_conditions);
    entry[QStringLiteral("menu_order")] =
        read_json_object(path).value(QLatin1String(k_menu_order)).toInt(1000);
    entries.append(entry);
  }

  std::sort(entries.begin(), entries.end(), [](const QVariant& a, const QVariant& b) {
    const QVariantMap lhs = a.toMap();
    const QVariantMap rhs = b.toMap();
    const int lhs_order = lhs.value(QStringLiteral("menu_order")).toInt();
    const int rhs_order = rhs.value(QStringLiteral("menu_order")).toInt();
    if (lhs_order != rhs_order) {
      return lhs_order < rhs_order;
    }
    return lhs.value(QStringLiteral("title"))
               .toString()
               .localeAwareCompare(rhs.value(QStringLiteral("title")).toString()) < 0;
  });

  return entries;
}

auto MissionCatalog::mission_map_paths() -> QSet<QString> {
  QSet<QString> map_paths;
  for (const QString& path : mission_files()) {
    Game::Mission::MissionDefinition mission;
    if (!load_mission(path, mission)) {
      continue;
    }
    const QString map_path = Utils::Resources::resolve_resource_path(mission.map_path);
    if (!map_path.isEmpty()) {
      map_paths.insert(map_path);
    }
  }
  return map_paths;
}

} // namespace Game::Map
