#pragma once

#include <QString>

#include <algorithm>
#include <optional>
#include <vector>

#include "../systems/nation_id.h"
#include "../systems/resource_types.h"

namespace Game::Mission {

struct Position {
  float x = 0.0F;
  float z = 0.0F;
};

enum class UnitBehavior {
  Strategic,
  Guard,
  Hold,
  Patrol
};

struct UnitSetup {
  QString type;
  int count = 1;
  Position position;
  UnitBehavior behavior = UnitBehavior::Strategic;
  float guard_radius = 10.0F;
  std::vector<Position> patrol_waypoints;
};

struct BuildingSetup {
  QString type;
  Position position;
  int max_population = 60;
};

using Resources = Game::Systems::ResourceAmounts;

struct PlayerSetup {
  QString nation;
  QString faction;
  QString color;
  std::vector<UnitSetup> starting_units;
  std::vector<BuildingSetup> starting_buildings;
  Game::Systems::ResourceOverlay starting_resources;
};

struct AIPersonality {
  float aggression = 0.5F;
  float defense = 0.5F;
  float harassment = 0.5F;
};

struct WaveComposition {
  QString type;
  int count = 1;
  bool elite = false;
  QString title;
};

enum class WaveTriggerMode {
  Time,
  AfterPreviousCleared
};

inline constexpr float k_default_wave_grace_seconds = 25.0F;
inline constexpr float k_default_wave_warning_seconds = 15.0F;

struct Wave {
  float timing = 0.0F;
  std::vector<WaveComposition> composition;
  Position entry_point;
  std::vector<Position> entry_points;
  WaveTriggerMode trigger = WaveTriggerMode::Time;
  float grace_seconds = k_default_wave_grace_seconds;
  float warning_seconds = k_default_wave_warning_seconds;
  std::optional<int> phase;
  QString archetype;
  float strength = 1.0F;
  QString label;
  Resources clear_reward;

  [[nodiscard]] auto resolved_entry_points() const -> std::vector<Position> {
    if (!entry_points.empty()) {
      return entry_points;
    }
    return {entry_point};
  }
};

struct AISetup {
  QString id;
  QString nation;
  QString faction;
  QString color;
  QString difficulty;
  std::optional<int> team_id;
  std::optional<QString> strategy;
  std::optional<QString> posture;
  AIPersonality personality;
  float wave_escalation = 0.0F;
  std::vector<UnitSetup> starting_units;
  std::vector<BuildingSetup> starting_buildings;
  std::vector<Wave> waves;
};

struct Condition {
  QString type;
  QString description;
  std::optional<float> duration;
  std::optional<QString> structure_type;
  std::optional<QString> zone_id;
  std::vector<QString> structure_types;
  std::optional<int> min_count;
  std::optional<int> wave_count;
  std::optional<Resources> resources;
};

struct MissionStage {
  QString id;
  QString title;
  QString description;
  QString hint;
  QString type;
  std::vector<QString> structure_types;
  int required_count = 1;
  std::optional<float> duration;
  std::optional<int> wave_count;
  std::optional<Resources> resources;
  std::optional<Position> target;
  std::optional<float> target_radius;
  std::vector<Position> route;
};

enum class CommanderMessageTrigger {
  MissionStart,
  MissionVictory,
  MissionDefeat,
  StructureCaptured,
  CommanderDefeated
};

struct CommanderMessageCondition {

  std::optional<int> owner_id;
  bool owner_is_local = false;

  std::optional<int> by_owner_id;
  bool by_owner_is_local = false;

  std::optional<QString> subject_type;

  std::optional<QString> nation;

  std::optional<Position> at;
  std::optional<float> radius;
};

inline constexpr float k_default_commander_message_seconds = 9.0F;

inline constexpr float k_commander_message_type_seconds_per_char = 0.022F;

inline constexpr float k_commander_message_read_seconds_per_char = 0.055F;

inline constexpr float k_commander_message_max_seconds = 20.0F;

[[nodiscard]] inline auto
legible_commander_message_seconds(int character_count,
                                  float authored_seconds) -> float {
  const float needed = static_cast<float>(std::max(0, character_count)) *
                       (k_commander_message_type_seconds_per_char +
                        k_commander_message_read_seconds_per_char);
  return std::max(authored_seconds, std::min(needed, k_commander_message_max_seconds));
}

struct CommanderMessage {
  QString id;

  QString speaker;

  QString pose;
  QString text;
  QString voice_cue;

  CommanderMessageTrigger trigger = CommanderMessageTrigger::MissionStart;
  CommanderMessageCondition condition;

  float delay = 0.0F;
  float duration = k_default_commander_message_seconds;
  int priority = 0;
  bool once = true;
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
  QString victory_mode = QStringLiteral("any");
  std::vector<Condition> victory_conditions;
  std::vector<Condition> defeat_conditions;
  std::vector<Condition> optional_objectives;
  std::vector<MissionStage> stages;
  std::vector<GameEvent> events;
  std::vector<CommanderMessage> commander_messages;
  bool include_ambient_undead = false;
  bool tutorial = false;
};

} // namespace Game::Mission
