#include "mission_loader.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Game::Mission {

auto MissionLoader::parsePosition(const QJsonObject &obj) -> Position {
  Position pos;
  pos.x = static_cast<float>(obj["x"].toDouble(0.0));
  pos.z = static_cast<float>(obj["z"].toDouble(0.0));
  return pos;
}

auto MissionLoader::parseUnitSetup(const QJsonObject &obj) -> UnitSetup {
  UnitSetup unit;
  unit.type = obj["type"].toString();
  unit.count = obj["count"].toInt(1);
  unit.position = parsePosition(obj["position"].toObject());
  return unit;
}

auto MissionLoader::parseBuildingSetup(const QJsonObject &obj)
    -> BuildingSetup {
  BuildingSetup building;
  building.type = obj["type"].toString();
  building.position = parsePosition(obj["position"].toObject());
  building.max_population = obj["max_population"].toInt(100);
  return building;
}

auto MissionLoader::parseResources(const QJsonObject &obj) -> Resources {
  Resources res;
  res.gold = obj["gold"].toInt(0);
  res.food = obj["food"].toInt(0);
  return res;
}

auto MissionLoader::parsePlayerSetup(const QJsonObject &obj) -> PlayerSetup {
  PlayerSetup setup;
  setup.nation = obj["nation"].toString();
  setup.faction = obj["faction"].toString();
  setup.color = obj["color"].toString();

  const QJsonArray units = obj["starting_units"].toArray();
  for (const auto &unit_val : units) {
    setup.starting_units.push_back(parseUnitSetup(unit_val.toObject()));
  }

  const QJsonArray buildings = obj["starting_buildings"].toArray();
  for (const auto &building_val : buildings) {
    setup.starting_buildings.push_back(
        parseBuildingSetup(building_val.toObject()));
  }

  if (obj.contains("starting_resources")) {
    setup.starting_resources =
        parseResources(obj["starting_resources"].toObject());
  }

  return setup;
}

auto MissionLoader::parseAIPersonality(const QJsonObject &obj)
    -> AIPersonality {
  AIPersonality personality;
  personality.aggression = static_cast<float>(obj["aggression"].toDouble(0.5));
  personality.defense = static_cast<float>(obj["defense"].toDouble(0.5));
  personality.harassment = static_cast<float>(obj["harassment"].toDouble(0.5));
  return personality;
}

auto MissionLoader::parseWaveComposition(const QJsonObject &obj)
    -> WaveComposition {
  WaveComposition comp;
  comp.type = obj["type"].toString();
  comp.count = obj["count"].toInt(1);
  return comp;
}

auto MissionLoader::parseWave(const QJsonObject &obj) -> Wave {
  Wave wave;
  wave.timing = static_cast<float>(obj["timing"].toDouble(0.0));
  wave.entry_point = parsePosition(obj["entry_point"].toObject());

  const QJsonArray composition = obj["composition"].toArray();
  for (const auto &comp_val : composition) {
    wave.composition.push_back(parseWaveComposition(comp_val.toObject()));
  }

  return wave;
}

auto MissionLoader::parseAISetup(const QJsonObject &obj) -> AISetup {
  AISetup setup;
  setup.id = obj["id"].toString();
  setup.nation = obj["nation"].toString();
  setup.faction = obj["faction"].toString();
  setup.color = obj["color"].toString();
  setup.difficulty = obj["difficulty"].toString();

  if (obj.contains("personality")) {
    setup.personality = parseAIPersonality(obj["personality"].toObject());
  }

  if (obj.contains("starting_units")) {
    const QJsonArray units = obj["starting_units"].toArray();
    for (const auto &unit_val : units) {
      setup.starting_units.push_back(parseUnitSetup(unit_val.toObject()));
    }
  }

  if (obj.contains("starting_buildings")) {
    const QJsonArray buildings = obj["starting_buildings"].toArray();
    for (const auto &building_val : buildings) {
      setup.starting_buildings.push_back(
          parseBuildingSetup(building_val.toObject()));
    }
  }

  if (obj.contains("waves")) {
    const QJsonArray waves = obj["waves"].toArray();
    for (const auto &wave_val : waves) {
      setup.waves.push_back(parseWave(wave_val.toObject()));
    }
  }

  return setup;
}

auto MissionLoader::parseCondition(const QJsonObject &obj) -> Condition {
  Condition cond;
  cond.type = obj["type"].toString();
  cond.description = obj["description"].toString();

  if (obj.contains("duration")) {
    cond.duration = static_cast<float>(obj["duration"].toDouble());
  }

  if (obj.contains("structure_type")) {
    cond.structure_type = obj["structure_type"].toString();
  }

  return cond;
}

auto MissionLoader::parseEventTrigger(const QJsonObject &obj) -> EventTrigger {
  EventTrigger trigger;
  trigger.type = obj["type"].toString();

  if (obj.contains("time")) {
    trigger.time = static_cast<float>(obj["time"].toDouble());
  }

  return trigger;
}

auto MissionLoader::parseEventAction(const QJsonObject &obj) -> EventAction {
  EventAction action;
  action.type = obj["type"].toString();

  if (obj.contains("text")) {
    action.text = obj["text"].toString();
  }

  return action;
}

auto MissionLoader::parseGameEvent(const QJsonObject &obj) -> GameEvent {
  GameEvent event;
  event.trigger = parseEventTrigger(obj["trigger"].toObject());

  const QJsonArray actions = obj["actions"].toArray();
  for (const auto &action_val : actions) {
    event.actions.push_back(parseEventAction(action_val.toObject()));
  }

  return event;
}

auto MissionLoader::loadFromJsonFile(const QString &file_path,
                                     MissionDefinition &out_mission,
                                     QString *error_msg) -> bool {
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error_msg != nullptr) {
      *error_msg = QString("Failed to open file: %1").arg(file_path);
    }
    return false;
  }

  QJsonParseError parse_error;
  const QJsonDocument doc =
      QJsonDocument::fromJson(file.readAll(), &parse_error);
  file.close();

  if (parse_error.error != QJsonParseError::NoError) {
    if (error_msg != nullptr) {
      *error_msg =
          QString("JSON parse error: %1").arg(parse_error.errorString());
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

  if (root.contains("player_setup")) {
    out_mission.player_setup =
        parsePlayerSetup(root["player_setup"].toObject());
  }

  if (root.contains("ai_setups")) {
    const QJsonArray ai_setups = root["ai_setups"].toArray();
    for (const auto &ai_val : ai_setups) {
      out_mission.ai_setups.push_back(parseAISetup(ai_val.toObject()));
    }
  }

  if (root.contains("victory_conditions")) {
    const QJsonArray victory = root["victory_conditions"].toArray();
    for (const auto &cond_val : victory) {
      out_mission.victory_conditions.push_back(
          parseCondition(cond_val.toObject()));
    }
  }

  if (root.contains("defeat_conditions")) {
    const QJsonArray defeat = root["defeat_conditions"].toArray();
    for (const auto &cond_val : defeat) {
      out_mission.defeat_conditions.push_back(
          parseCondition(cond_val.toObject()));
    }
  }

  if (root.contains("events")) {
    const QJsonArray events = root["events"].toArray();
    for (const auto &event_val : events) {
      out_mission.events.push_back(parseGameEvent(event_val.toObject()));
    }
  }

  return true;
}

} // namespace Game::Mission
