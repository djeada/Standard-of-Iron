#pragma once

#include "mission_definition.h"
#include <QJsonObject>
#include <QString>

namespace Game::Mission {

class MissionLoader {
public:
  static auto load_from_json_file(const QString &file_path,
                                  MissionDefinition &out_mission,
                                  QString *error_msg = nullptr) -> bool;

private:
  static auto parse_position(const QJsonObject &obj) -> Position;
  static auto parse_unit_setup(const QJsonObject &obj) -> UnitSetup;
  static auto parse_building_setup(const QJsonObject &obj) -> BuildingSetup;
  static auto parse_resources(const QJsonObject &obj) -> Resources;
  static auto parse_player_setup(const QJsonObject &obj) -> PlayerSetup;
  static auto parse_ai_personality(const QJsonObject &obj) -> AIPersonality;
  static auto parse_wave_composition(const QJsonObject &obj) -> WaveComposition;
  static auto parse_wave(const QJsonObject &obj) -> Wave;
  static auto parse_ai_setup(const QJsonObject &obj) -> AISetup;
  static auto parse_condition(const QJsonObject &obj) -> Condition;
  static auto parse_event_trigger(const QJsonObject &obj) -> EventTrigger;
  static auto parse_event_action(const QJsonObject &obj) -> EventAction;
  static auto parse_game_event(const QJsonObject &obj) -> GameEvent;
};

} // namespace Game::Mission
