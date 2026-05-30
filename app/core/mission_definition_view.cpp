#include "mission_definition_view.h"

#include <QFileInfo>
#include <QStringList>
#include <QVariantList>

#include "game/map/mission_loader.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_type.h"
#include "mission_commander_setup.h"
#include "utils/resource_utils.h"

namespace {

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
    cond["description"] = condition.description;
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
    map["id"] = QString::fromStdString(definition->id);
    map["display_name"] = QString::fromStdString(definition->display_name);
    map["strategic_identity"] = QString::fromStdString(definition->strategic_identity);
    map["recruitment_effect"] = QString::fromStdString(definition->recruitment_effect);
    map["battlefield_role"] = QString::fromStdString(definition->battlefield_role);
    map["strengths"] = QString::fromStdString(definition->strengths);
    map["weaknesses"] = QString::fromStdString(definition->weaknesses);
    map["passive_aura"] = QString::fromStdString(definition->passive_aura);
    map["bonus_type"] = QString::fromStdString(definition->bonus_type);
    map["bonus_summary"] = QString::fromStdString(definition->bonus_summary);
    map["rally_ability"] = QString::fromStdString(definition->rally_ability);
    map["death_consequence"] = QString::fromStdString(definition->death_consequence);
    map["visual_requirements"] =
        QString::fromStdString(definition->visual_requirements);
  } else {
    map["display_name"] = titleize(commander_troop);
  }

  return map;
}

auto build_player_setup_map(const Game::Mission::PlayerSetup& setup) -> QVariantMap {
  QVariantMap map;
  map["nation"] = setup.nation;
  map["faction"] = setup.faction;
  map["color"] = setup.color;

  const QString commander_troop =
      App::Core::resolve_commander_troop(setup.nation, setup.commander_troop);
  if (!commander_troop.isEmpty()) {
    map["commander_troop"] = commander_troop;
    map["commander"] = build_commander_map(commander_troop);
  }

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

auto build_ai_setup_map(const Game::Mission::AISetup& setup) -> QVariantMap {
  QVariantMap map;
  map["id"] = setup.id;
  map["nation"] = setup.nation;
  map["faction"] = setup.faction;
  map["color"] = setup.color;
  map["difficulty"] = setup.difficulty;

  const QString commander_troop =
      App::Core::resolve_commander_troop(setup.nation, setup.commander_troop);
  if (!commander_troop.isEmpty()) {
    map["commander_troop"] = commander_troop;
    map["commander"] = build_commander_map(commander_troop);
  }
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
                                  const std::optional<QString>& commander_troop,
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
  player["commanderTroop"] =
      App::Core::resolve_commander_troop(nation, commander_troop);
  player["isHuman"] = is_human;
  return player;
}

} // namespace

auto build_mission_definition_map(const Game::Mission::MissionDefinition& mission)
    -> QVariantMap {
  QVariantMap result;
  result["id"] = mission.id;
  result["title"] = mission.title;
  result["summary"] = mission.summary;
  result["map_path"] = mission.map_path;
  if (mission.teaching_goal.has_value()) {
    result["teaching_goal"] = mission.teaching_goal.value();
  }
  if (mission.narrative_intent.has_value()) {
    result["narrative_intent"] = mission.narrative_intent.value();
  }
  if (mission.historical_context.has_value()) {
    result["historical_context"] = mission.historical_context.value();
  }
  if (mission.terrain_type.has_value()) {
    result["terrain_type"] = mission.terrain_type.value();
  }

  result["player_setup"] = build_player_setup_map(mission.player_setup);

  QVariantList ai_setups;
  for (const auto& ai_setup : mission.ai_setups) {
    ai_setups.append(build_ai_setup_map(ai_setup));
  }
  result["ai_setups"] = ai_setups;

  result["victory_conditions"] = build_condition_list(mission.victory_conditions);
  result["defeat_conditions"] = build_condition_list(mission.defeat_conditions);
  result["optional_objectives"] = build_condition_list(mission.optional_objectives);

  return result;
}

auto build_mission_objectives_map(const Game::Mission::MissionDefinition& mission)
    -> QVariantMap {
  QVariantMap result;
  result["title"] = mission.title;
  result["summary"] = mission.summary;
  result["victory_conditions"] = build_condition_list(mission.victory_conditions);
  result["defeat_conditions"] = build_condition_list(mission.defeat_conditions);
  result["optional_objectives"] = build_condition_list(mission.optional_objectives);
  return result;
}

auto build_campaign_player_configs(const Game::Mission::MissionDefinition& mission)
    -> QVariantList {
  QVariantList player_configs;
  player_configs.append(
      build_campaign_player_config(mission.player_setup.nation,
                                   mission.player_setup.commander_troop,
                                   1,
                                   0,
                                   0,
                                   true));

  int player_id = 2;
  int default_team_id = 1;
  for (const auto& ai_setup : mission.ai_setups) {
    const int team_id =
        ai_setup.team_id.has_value() ? ai_setup.team_id.value() : default_team_id++;
    player_configs.append(build_campaign_player_config(ai_setup.nation,
                                                       ai_setup.commander_troop,
                                                       player_id,
                                                       player_id - 1,
                                                       team_id,
                                                       false));
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
