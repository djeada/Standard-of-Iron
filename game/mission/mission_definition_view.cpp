#include "game/mission/mission_definition_view.h"

#include <QFileInfo>
#include <QStringList>
#include <QVariantList>

#include "game/map/mission_loader.h"
#include "game/mission/mission_commander_setup.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"
#include "utils/resource_utils.h"

namespace {

constexpr int k_local_owner_id = 1;
constexpr int k_first_ai_owner_id = 2;

auto resolve_mission_file_path(const QString& mission_id) -> QString {
  const QStringList search_paths = {QStringLiteral("assets/missions/%1.json"),
                                    QStringLiteral("../assets/missions/%1.json"),
                                    QStringLiteral("../../assets/missions/%1.json"),
                                    QStringLiteral(":/assets/missions/%1.json"),
                                    QStringLiteral("/assets/missions/%1.json"),
                                    QStringLiteral("/../assets/missions/%1.json")};

  for (const auto& pattern : search_paths) {
    QString candidate = pattern.arg(mission_id);
    candidate = Utils::Resources::resolve_resource_path(candidate);
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

auto titleize(const QString& value) -> QString {
  QStringList parts = value.split('_', Qt::SkipEmptyParts);
  for (QString& part : parts) {
    if (!part.isEmpty()) {
      part[0] = part[0].toUpper();
    }
  }
  return parts.join(' ');
}

auto build_condition_list(const std::vector<Game::Mission::Condition>& conditions)
    -> QVariantList {
  QVariantList list;
  for (const auto& condition : conditions) {
    QVariantMap cond;
    cond["type"] = condition.type;
    cond["description"] =
        Game::Util::tr_asset(Game::Util::k_missions_context, condition.description);
    if (condition.duration.has_value()) {
      cond["duration"] = condition.duration.value();
    }
    if (condition.structure_type.has_value()) {
      cond["structure_type"] = condition.structure_type.value();
    }
    if (!condition.structure_types.empty()) {
      QVariantList types;
      for (const auto& type : condition.structure_types) {
        types.append(type);
      }
      cond["structure_types"] = types;
    }
    if (condition.min_count.has_value()) {
      cond["min_count"] = condition.min_count.value();
    }
    if (condition.wave_count.has_value()) {
      cond["wave_count"] = condition.wave_count.value();
    }
    if (condition.resources.has_value()) {
      QVariantMap resources;
      for (Game::Systems::ResourceType const type :
           Game::Systems::k_all_resource_types) {
        int const amount = condition.resources->get(type);
        if (amount > 0) {
          resources[QLatin1String(Game::Systems::resource_type_key(type))] = amount;
        }
      }
      cond["resources"] = resources;
    }
    list.append(cond);
  }
  return list;
}

auto build_unit_setup_list(const std::vector<Game::Mission::UnitSetup>& units)
    -> QVariantList {
  QVariantList list;
  for (const auto& unit : units) {
    QVariantMap entry;
    entry["type"] = unit.type;
    entry["count"] = unit.count;
    entry["x"] = unit.position.x;
    entry["z"] = unit.position.z;
    switch (unit.behavior) {
    case Game::Mission::UnitBehavior::Guard:
      entry["behavior"] = QStringLiteral("guard");
      entry["guard_radius"] = unit.guard_radius;
      break;
    case Game::Mission::UnitBehavior::Hold:
      entry["behavior"] = QStringLiteral("hold");
      break;
    case Game::Mission::UnitBehavior::Patrol: {
      entry["behavior"] = QStringLiteral("patrol");
      QVariantList waypoints;
      for (const auto& waypoint : unit.patrol_waypoints) {
        QVariantMap point;
        point["x"] = waypoint.x;
        point["z"] = waypoint.z;
        waypoints.append(point);
      }
      entry["patrol_waypoints"] = waypoints;
      break;
    }
    case Game::Mission::UnitBehavior::Strategic:
      break;
    }
    list.append(entry);
  }
  return list;
}

auto build_building_setup_list(
    const std::vector<Game::Mission::BuildingSetup>& buildings) -> QVariantList {
  QVariantList list;
  for (const auto& building : buildings) {
    QVariantMap entry;
    entry["type"] = building.type;
    entry["max_population"] = building.max_population;
    entry["x"] = building.position.x;
    entry["z"] = building.position.z;
    list.append(entry);
  }
  return list;
}

auto build_wave_list(const std::vector<Game::Mission::Wave>& waves) -> QVariantList {
  QVariantList list;
  for (const auto& wave : waves) {
    QVariantMap entry;
    entry["timing"] = wave.timing;

    QVariantMap point;
    point["x"] = wave.entry_point.x;
    point["z"] = wave.entry_point.z;
    entry["entry_point"] = point;

    QVariantList composition;
    for (const auto& member : wave.composition) {
      QVariantMap component;
      component["type"] = member.type;
      component["count"] = member.count;
      composition.append(component);
    }
    entry["composition"] = composition;
    list.append(entry);
  }
  return list;
}

auto build_personality_map(const Game::Mission::AIPersonality& personality)
    -> QVariantMap {
  QVariantMap map;
  map["aggression"] = personality.aggression;
  map["defense"] = personality.defense;
  map["harassment"] = personality.harassment;
  return map;
}

auto build_commander_map(const QString& commander_troop) -> QVariantMap {
  QVariantMap map;
  map["troop"] = commander_troop;

  Game::Units::TroopType troop_type;
  if (!Game::Units::try_parse_troop_type(commander_troop, troop_type)) {
    map["display_name"] = titleize(commander_troop);
    return map;
  }

  if (const auto* definition = Game::Units::commander_definition(troop_type)) {
    using Game::Util::k_commanders_context;
    using Game::Util::tr_asset;
    map["id"] = QString::fromStdString(definition->id);
    map["display_name"] = tr_asset(k_commanders_context, definition->display_name);
    map["strategic_identity"] =
        tr_asset(k_commanders_context, definition->strategic_identity);
    map["recruitment_effect"] =
        tr_asset(k_commanders_context, definition->recruitment_effect);
    map["battlefield_role"] =
        tr_asset(k_commanders_context, definition->battlefield_role);
    map["strengths"] = tr_asset(k_commanders_context, definition->strengths);
    map["weaknesses"] = tr_asset(k_commanders_context, definition->weaknesses);
    map["passive_aura"] = tr_asset(k_commanders_context, definition->passive_aura);

    map["bonus_type"] = QString::fromStdString(definition->bonus_type);
    map["bonus_summary"] = tr_asset(k_commanders_context, definition->bonus_summary);
    map["rally_ability"] = tr_asset(k_commanders_context, definition->rally_ability);
    map["death_consequence"] =
        tr_asset(k_commanders_context, definition->death_consequence);

    map["visual_requirements"] =
        QString::fromStdString(definition->visual_requirements);
  } else {
    map["display_name"] = titleize(commander_troop);
  }

  return map;
}

void apply_map_commander(QVariantMap& map,
                         const std::map<int, QString>& map_commanders,
                         int owner_id) {
  const auto it = map_commanders.find(owner_id);
  if (it == map_commanders.end() || it->second.isEmpty()) {
    return;
  }
  map["commander_troop"] = it->second;
  map["commander"] = build_commander_map(it->second);
}

auto build_player_setup_map(const Game::Mission::PlayerSetup& setup,
                            const std::map<int, QString>& map_commanders)
    -> QVariantMap {
  QVariantMap map;
  map["nation"] = setup.nation;
  map["faction"] = setup.faction;
  map["color"] = setup.color;

  apply_map_commander(map, map_commanders, k_local_owner_id);

  map["starting_units"] = build_unit_setup_list(setup.starting_units);
  map["starting_buildings"] = build_building_setup_list(setup.starting_buildings);

  QVariantMap resources;
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    resources[QLatin1String(Game::Systems::resource_type_key(type))] =
        setup.starting_resources.get(type);
  }
  map["starting_resources"] = resources;

  return map;
}

auto build_ai_setup_map(const Game::Mission::AISetup& setup,
                        const std::map<int, QString>& map_commanders,
                        int owner_id) -> QVariantMap {
  QVariantMap map;
  map["id"] = setup.id;
  map["nation"] = setup.nation;
  map["faction"] = setup.faction;
  map["color"] = setup.color;
  map["difficulty"] = setup.difficulty;

  apply_map_commander(map, map_commanders, owner_id);
  if (setup.team_id.has_value()) {
    map["team_id"] = setup.team_id.value();
  }
  if (setup.strategy.has_value()) {
    map["strategy"] = setup.strategy.value();
  }

  map["personality"] = build_personality_map(setup.personality);
  map["starting_units"] = build_unit_setup_list(setup.starting_units);
  map["starting_buildings"] = build_building_setup_list(setup.starting_buildings);
  map["waves"] = build_wave_list(setup.waves);

  return map;
}

auto build_campaign_player_config(const QString& nation,
                                  int player_id,
                                  int color_index,
                                  int team_id,
                                  bool is_human) -> QVariantMap {
  QVariantMap player;
  player["player_id"] = player_id;
  player["playerName"] = nation;
  player["colorIndex"] = color_index;
  player["team_id"] = team_id;
  player["nationId"] = nation;
  player["isHuman"] = is_human;
  return player;
}

} // namespace

auto build_mission_definition_map(const Game::Mission::MissionDefinition& mission)
    -> QVariantMap {
  QVariantMap result;
  result["id"] = mission.id;
  result["title"] = Game::Util::tr_asset(Game::Util::k_missions_context, mission.title);
  result["summary"] =
      Game::Util::tr_asset(Game::Util::k_missions_context, mission.summary);
  result["map_path"] = mission.map_path;
  if (mission.teaching_goal.has_value()) {
    result["teaching_goal"] = Game::Util::tr_asset(Game::Util::k_missions_context,
                                                   mission.teaching_goal.value());
  }
  if (mission.narrative_intent.has_value()) {
    result["narrative_intent"] = Game::Util::tr_asset(Game::Util::k_missions_context,
                                                      mission.narrative_intent.value());
  }
  if (mission.historical_context.has_value()) {
    result["historical_context"] = Game::Util::tr_asset(
        Game::Util::k_missions_context, mission.historical_context.value());
  }
  if (mission.terrain_type.has_value()) {
    result["terrain_type"] = mission.terrain_type.value();
  }

  const auto map_commanders = Game::Mission::commander_troops_for_map(mission.map_path);
  result["player_setup"] = build_player_setup_map(mission.player_setup, map_commanders);

  QVariantList ai_setups;
  int owner_id = k_first_ai_owner_id;
  for (const auto& ai_setup : mission.ai_setups) {
    ai_setups.append(build_ai_setup_map(ai_setup, map_commanders, owner_id));
    ++owner_id;
  }
  result["ai_setups"] = ai_setups;

  result["victory_mode"] = mission.victory_mode;
  result["victory_conditions"] = build_condition_list(mission.victory_conditions);
  result["defeat_conditions"] = build_condition_list(mission.defeat_conditions);
  result["optional_objectives"] = build_condition_list(mission.optional_objectives);

  return result;
}

auto build_mission_objectives_map(const Game::Mission::MissionDefinition& mission)
    -> QVariantMap {
  QVariantMap result;
  result["title"] = Game::Util::tr_asset(Game::Util::k_missions_context, mission.title);
  result["summary"] =
      Game::Util::tr_asset(Game::Util::k_missions_context, mission.summary);
  result["victory_mode"] = mission.victory_mode;
  result["victory_conditions"] = build_condition_list(mission.victory_conditions);
  result["defeat_conditions"] = build_condition_list(mission.defeat_conditions);
  result["optional_objectives"] = build_condition_list(mission.optional_objectives);
  return result;
}

auto build_campaign_player_configs(const Game::Mission::MissionDefinition& mission)
    -> QVariantList {
  QVariantList player_configs;
  player_configs.append(build_campaign_player_config(
      mission.player_setup.nation, k_local_owner_id, 0, 0, true));

  int player_id = k_first_ai_owner_id;
  int default_team_id = 1;
  for (const auto& ai_setup : mission.ai_setups) {
    const int team_id =
        ai_setup.team_id.has_value() ? ai_setup.team_id.value() : default_team_id++;
    player_configs.append(build_campaign_player_config(
        ai_setup.nation, player_id, player_id - 1, team_id, false));
    ++player_id;
  }

  return player_configs;
}

auto load_mission_definition_map(const QString& mission_id) -> QVariantMap {
  QVariantMap result;
  if (mission_id.isEmpty()) {
    return result;
  }

  const QString mission_path = resolve_mission_file_path(mission_id);
  if (mission_path.isEmpty()) {
    qWarning() << "Mission definition not found for" << mission_id;
    return result;
  }

  Game::Mission::MissionDefinition mission;
  QString error;
  if (!Game::Mission::MissionLoader::load_from_json_file(
          mission_path, mission, &error)) {
    qWarning() << "Failed to load mission definition" << mission_id << error;
    return result;
  }

  return build_mission_definition_map(mission);
}
