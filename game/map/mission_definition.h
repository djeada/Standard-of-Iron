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
  std::optional<int> team_id;
  std::optional<QString> strategy;
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
  std::vector<QString> structure_types;
  std::optional<int> min_count;
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
  std::optional<QString> teaching_goal;
  std::optional<QString> narrative_intent;
  std::optional<QString> historical_context;
  std::optional<QString> terrain_type;
  PlayerSetup player_setup;
  std::vector<AISetup> ai_setups;
  std::vector<Condition> victory_conditions;
  std::vector<Condition> defeat_conditions;
  std::vector<Condition> optional_objectives;
  std::vector<GameEvent> events;
};

} // namespace Game::Mission
