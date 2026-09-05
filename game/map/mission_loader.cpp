#include "mission_loader.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "../systems/resource_json.h"
#include "commander_message_grammar.h"

namespace Game::Mission {

auto MissionLoader::parse_position(const QJsonObject& obj) -> Position {
  Position pos;
  pos.x = static_cast<float>(obj["x"].toDouble(0.0));
  pos.z = static_cast<float>(obj["z"].toDouble(0.0));
  return pos;
}

auto parse_unit_behavior(const QString& value) -> UnitBehavior {
  const QString lowered = value.trimmed().toLower();
  if (lowered == "guard" || lowered == "defend" || lowered == "defensive") {
    return UnitBehavior::Guard;
  }
  if (lowered == "hold" || lowered == "hold_position") {
    return UnitBehavior::Hold;
  }
  if (lowered == "patrol") {
    return UnitBehavior::Patrol;
  }
  return UnitBehavior::Strategic;
}

auto MissionLoader::parse_unit_setup(const QJsonObject& obj) -> UnitSetup {
  UnitSetup unit;
  unit.type = obj["type"].toString();
  unit.count = obj["count"].toInt(1);
  unit.position = parse_position(obj["position"].toObject());
  unit.behavior = parse_unit_behavior(obj["behavior"].toString());
  unit.guard_radius = static_cast<float>(obj["guard_radius"].toDouble(10.0));

  const QJsonArray patrol_waypoints = obj["patrol_waypoints"].toArray();
  for (const auto waypoint_val : patrol_waypoints) {
    unit.patrol_waypoints.push_back(parse_position(waypoint_val.toObject()));
  }
  return unit;
}

auto MissionLoader::parse_building_setup(const QJsonObject& obj) -> BuildingSetup {
  BuildingSetup building;
  building.type = obj["type"].toString();
  building.position = parse_position(obj["position"].toObject());
  building.max_population = obj["max_population"].toInt(100);
  return building;
}

auto MissionLoader::parse_resources(const QJsonObject& obj) -> Resources {
  Resources res;
  Game::Systems::read_resource_overlay(obj).apply_to(res);
  return res;
}

namespace {

void warn_on_authored_commander(const QJsonObject& obj, const QString& force_label) {
  if (obj.contains("commander_troop")) {
    qWarning() << "MissionLoader: ignoring 'commander_troop' on" << force_label
               << "- commanders are authored in the map's spawns[]";
  }
}

} // namespace

auto MissionLoader::parse_player_setup(const QJsonObject& obj) -> PlayerSetup {
  PlayerSetup setup;
  setup.nation = obj["nation"].toString();
  setup.faction = obj["faction"].toString();
  setup.color = obj["color"].toString();
  warn_on_authored_commander(obj, QStringLiteral("player_setup"));

  const QJsonArray units = obj["starting_units"].toArray();
  for (const auto unit_val : units) {
    setup.starting_units.push_back(parse_unit_setup(unit_val.toObject()));
  }

  const QJsonArray buildings = obj["starting_buildings"].toArray();
  for (const auto building_val : buildings) {
    setup.starting_buildings.push_back(parse_building_setup(building_val.toObject()));
  }

  setup.starting_resources =
      Game::Systems::read_resource_overlay(obj["starting_resources"].toObject());

  return setup;
}

auto MissionLoader::parse_ai_personality(const QJsonObject& obj) -> AIPersonality {
  AIPersonality personality;
  personality.aggression = static_cast<float>(obj["aggression"].toDouble(0.5));
  personality.defense = static_cast<float>(obj["defense"].toDouble(0.5));
  personality.harassment = static_cast<float>(obj["harassment"].toDouble(0.5));
  return personality;
}

auto MissionLoader::parse_wave_composition(const QJsonObject& obj) -> WaveComposition {
  WaveComposition comp;
  comp.type = obj["type"].toString();
  comp.count = obj["count"].toInt(1);
  comp.elite = obj["elite"].toBool(false);
  comp.title = obj["title"].toString();
  return comp;
}

auto parse_wave_trigger(const QString& value) -> WaveTriggerMode {
  const QString lowered = value.trimmed().toLower();
  if (lowered == "after_previous_cleared" || lowered == "after_previous" ||
      lowered == "on_cleared" || lowered == "cleared") {
    return WaveTriggerMode::AfterPreviousCleared;
  }
  return WaveTriggerMode::Time;
}

auto MissionLoader::parse_wave(const QJsonObject& obj) -> Wave {
  Wave wave;
  wave.timing = static_cast<float>(obj["timing"].toDouble(0.0));
  wave.entry_point = parse_position(obj["entry_point"].toObject());

  const QJsonArray entry_points = obj["entry_points"].toArray();
  for (const auto point_val : entry_points) {
    wave.entry_points.push_back(parse_position(point_val.toObject()));
  }

  wave.trigger = parse_wave_trigger(obj["trigger"].toString());
  wave.grace_seconds =
      static_cast<float>(obj["grace_seconds"].toDouble(k_default_wave_grace_seconds));
  wave.warning_seconds = static_cast<float>(
      obj["warning_seconds"].toDouble(k_default_wave_warning_seconds));
  if (obj.contains("phase")) {
    wave.phase = obj["phase"].toInt();
  }
  wave.archetype = obj["archetype"].toString().trimmed().toLower();
  wave.strength = static_cast<float>(obj["strength"].toDouble(1.0));
  wave.label = obj["label"].toString();
  if (obj.contains("clear_reward")) {
    wave.clear_reward = parse_resources(obj["clear_reward"].toObject());
  }

  const QJsonArray composition = obj["composition"].toArray();
  for (const auto comp_val : composition) {
    wave.composition.push_back(parse_wave_composition(comp_val.toObject()));
  }

  return wave;
}

auto MissionLoader::parse_ai_setup(const QJsonObject& obj) -> AISetup {
  AISetup setup;
  setup.id = obj["id"].toString();
  setup.nation = obj["nation"].toString();
  setup.faction = obj["faction"].toString();
  setup.color = obj["color"].toString();
  setup.difficulty = obj["difficulty"].toString();
  warn_on_authored_commander(obj, setup.id);

  if (obj.contains("team_id")) {
    setup.team_id = obj["team_id"].toInt();
  }

  if (obj.contains("strategy")) {
    setup.strategy = obj["strategy"].toString();
  }

  if (obj.contains("posture")) {
    setup.posture = obj["posture"].toString();
  }

  if (obj.contains("personality")) {
    setup.personality = parse_ai_personality(obj["personality"].toObject());
  }

  setup.wave_escalation = static_cast<float>(obj["wave_escalation"].toDouble(0.0));

  if (obj.contains("starting_units")) {
    const QJsonArray units = obj["starting_units"].toArray();
    for (const auto unit_val : units) {
      setup.starting_units.push_back(parse_unit_setup(unit_val.toObject()));
    }
  }

  if (obj.contains("starting_buildings")) {
    const QJsonArray buildings = obj["starting_buildings"].toArray();
    for (const auto building_val : buildings) {
      setup.starting_buildings.push_back(parse_building_setup(building_val.toObject()));
    }
  }

  if (obj.contains("waves")) {
    const QJsonArray waves = obj["waves"].toArray();
    for (const auto wave_val : waves) {
      setup.waves.push_back(parse_wave(wave_val.toObject()));
    }
  }

  return setup;
}

auto MissionLoader::parse_condition(const QJsonObject& obj) -> Condition {
  Condition cond;
  cond.type = obj["type"].toString();
  cond.description = obj["description"].toString();

  if (obj.contains("duration")) {
    cond.duration = static_cast<float>(obj["duration"].toDouble());
  }

  if (obj.contains("structure_type")) {
    cond.structure_type = obj["structure_type"].toString();
  }

  if (obj.contains("zone_id")) {
    cond.zone_id = obj["zone_id"].toString();
  }

  if (obj.contains("structure_types") && obj["structure_types"].isArray()) {
    const QJsonArray types = obj["structure_types"].toArray();
    for (const auto type_val : types) {
      cond.structure_types.push_back(type_val.toString());
    }
  }

  if (obj.contains("min_count")) {
    cond.min_count = obj["min_count"].toInt();
  }

  if (obj.contains("wave_count")) {
    cond.wave_count = obj["wave_count"].toInt();
  }

  if (obj.contains("resources")) {
    cond.resources = parse_resources(obj["resources"].toObject());
  }

  return cond;
}

auto MissionLoader::parse_stage(const QJsonObject& obj) -> MissionStage {
  MissionStage stage;
  stage.id = obj["id"].toString();
  stage.title = obj["title"].toString();
  stage.description = obj["description"].toString();
  stage.hint = obj["hint"].toString();
  stage.type = obj["type"].toString();

  if (obj.contains("structure_types") && obj["structure_types"].isArray()) {
    const QJsonArray types = obj["structure_types"].toArray();
    for (const auto type_val : types) {
      stage.structure_types.push_back(type_val.toString());
    }
  } else if (obj.contains("structure_type")) {
    stage.structure_types.push_back(obj["structure_type"].toString());
  }

  stage.required_count = obj["required_count"].toInt(stage.required_count);

  if (obj.contains("duration")) {
    stage.duration = static_cast<float>(obj["duration"].toDouble());
  }

  if (obj.contains("wave_count")) {
    stage.wave_count = obj["wave_count"].toInt();
  }

  if (obj.contains("resources")) {
    stage.resources = parse_resources(obj["resources"].toObject());
  }

  if (obj.contains("target")) {
    stage.target = parse_position(obj["target"].toObject());
  }

  if (obj.contains("target_radius")) {
    stage.target_radius = static_cast<float>(obj["target_radius"].toDouble());
  }

  if (obj.contains("route") && obj["route"].isArray()) {
    const QJsonArray route = obj["route"].toArray();
    for (const auto point_val : route) {
      stage.route.push_back(parse_position(point_val.toObject()));
    }
  }

  return stage;
}

auto MissionLoader::parse_event_trigger(const QJsonObject& obj) -> EventTrigger {
  EventTrigger trigger;
  trigger.type = obj["type"].toString();

  if (obj.contains("time")) {
    trigger.time = static_cast<float>(obj["time"].toDouble());
  }

  return trigger;
}

auto MissionLoader::parse_event_action(const QJsonObject& obj) -> EventAction {
  EventAction action;
  action.type = obj["type"].toString();

  if (obj.contains("text")) {
    action.text = obj["text"].toString();
  }

  return action;
}

auto MissionLoader::parse_game_event(const QJsonObject& obj) -> GameEvent {
  GameEvent event;
  event.trigger = parse_event_trigger(obj["trigger"].toObject());

  const QJsonArray actions = obj["actions"].toArray();
  for (const auto action_val : actions) {
    event.actions.push_back(parse_event_action(action_val.toObject()));
  }

  return event;
}

auto MissionLoader::parse_commander_message(const QJsonObject& obj)
    -> CommanderMessage {
  return Game::Mission::parse_commander_message(obj);
}

auto MissionLoader::load_from_json_file(const QString& file_path,
                                        MissionDefinition& out_mission,
                                        QString* error_msg) -> bool {
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error_msg != nullptr) {
      *error_msg = QString("Failed to open file: %1").arg(file_path);
    }
    return false;
  }

  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  file.close();

  if (parse_error.error != QJsonParseError::NoError) {
    if (error_msg != nullptr) {
      *error_msg = QString("JSON parse error: %1").arg(parse_error.errorString());
    }
    return false;
  }

  if (!doc.isObject()) {
    if (error_msg != nullptr) {
      *error_msg = "JSON root is not an object";
    }
    return false;
  }

  const QJsonObject root = doc.object();

  out_mission.id = root["id"].toString();
  out_mission.title = root["title"].toString();
  out_mission.summary = root["summary"].toString();
  out_mission.map_path = root["map_path"].toString();

  if (root.contains("teaching_goal")) {
    out_mission.teaching_goal = root["teaching_goal"].toString();
  }

  if (root.contains("narrative_intent")) {
    out_mission.narrative_intent = root["narrative_intent"].toString();
  }

  if (root.contains("historical_context")) {
    out_mission.historical_context = root["historical_context"].toString();
  }

  if (root.contains("terrain_type")) {
    out_mission.terrain_type = root["terrain_type"].toString();
  }

  if (root.contains("player_setup")) {
    out_mission.player_setup = parse_player_setup(root["player_setup"].toObject());
  }

  if (root.contains("ai_setups")) {
    const QJsonArray ai_setups = root["ai_setups"].toArray();
    for (const auto ai_val : ai_setups) {
      out_mission.ai_setups.push_back(parse_ai_setup(ai_val.toObject()));
    }
  }

  if (root.contains("victory_mode")) {
    out_mission.victory_mode = root["victory_mode"].toString().trimmed().toLower();
  }

  if (root.contains("victory_conditions")) {
    const QJsonArray victory = root["victory_conditions"].toArray();
    for (const auto cond_val : victory) {
      out_mission.victory_conditions.push_back(parse_condition(cond_val.toObject()));
    }
  }

  if (root.contains("defeat_conditions")) {
    const QJsonArray defeat = root["defeat_conditions"].toArray();
    for (const auto cond_val : defeat) {
      out_mission.defeat_conditions.push_back(parse_condition(cond_val.toObject()));
    }
  }

  if (root.contains("optional_objectives")) {
    const QJsonArray optional = root["optional_objectives"].toArray();
    for (const auto cond_val : optional) {
      out_mission.optional_objectives.push_back(parse_condition(cond_val.toObject()));
    }
  }

  if (root.contains("stages")) {
    const QJsonArray stages = root["stages"].toArray();
    for (const auto stage_val : stages) {
      out_mission.stages.push_back(parse_stage(stage_val.toObject()));
    }
  }

  if (root.contains("events")) {
    const QJsonArray events = root["events"].toArray();
    for (const auto event_val : events) {
      out_mission.events.push_back(parse_game_event(event_val.toObject()));
    }
  }

  if (root.contains("commander_messages")) {
    const QJsonArray messages = root["commander_messages"].toArray();
    for (const auto message_val : messages) {
      out_mission.commander_messages.push_back(
          parse_commander_message(message_val.toObject()));
    }
  }

  if (root.contains("commander_voices")) {
    out_mission.commander_voices =
        parse_commander_voices_policy(root["commander_voices"].toObject());
  }

  out_mission.include_ambient_undead =
      root["include_ambient_undead"].toBool(out_mission.include_ambient_undead);
  out_mission.tutorial = root["tutorial"].toBool(out_mission.tutorial);

  return true;
}

} // namespace Game::Mission
