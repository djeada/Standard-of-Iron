#pragma once

#include "../systems/nation_id.h"
#include <QString>
#include <optional>
#include <vector>

namespace Game::Mission {

struct Position {
  float x = 0.0F;
  float z = 0.0F;
};

struct UnitSetup {
  QString type;
  int count = 1;
  Position position;
};

struct BuildingSetup {
  QString type;
  Position position;
  int max_population = 100;
};

struct Resources {
  int gold = 0;
  int food = 0;
};

struct PlayerSetup {
  QString nation;
  QString faction;
  QString color;
  std::vector<UnitSetup> starting_units;
  std::vector<BuildingSetup> starting_buildings;
  Resources starting_resources;
};

struct AIPersonality {
  float aggression = 0.5F;
  float defense = 0.5F;
  float harassment = 0.5F;
};

struct WaveComposition {
  QString type;
  int count = 1;
};

struct Wave {
  float timing = 0.0F;
  std::vector<WaveComposition> composition;
  Position entry_point;
};

struct AISetup {
  QString id;
  QString nation;
  QString faction;
  QString color;
  QString difficulty;
  AIPersonality personality;
  std::vector<UnitSetup> starting_units;
  std::vector<BuildingSetup> starting_buildings;
  std::vector<Wave> waves;
};

struct Condition {
  QString type;
  QString description;
  std::optional<float> duration;
  std::optional<QString> structure_type;
};

struct EventTrigger {
  QString type;
  std::optional<float> time;
};

struct EventAction {
  QString type;
  std::optional<QString> text;
};

struct GameEvent {
  EventTrigger trigger;
  std::vector<EventAction> actions;
};

struct MissionDefinition {
  QString id;
  QString title;
  QString summary;
  QString map_path;
  PlayerSetup player_setup;
  std::vector<AISetup> ai_setups;
  std::vector<Condition> victory_conditions;
  std::vector<Condition> defeat_conditions;
  std::vector<GameEvent> events;
};

} 
