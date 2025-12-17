#pragma once

#include "mission_definition.h"
#include <QJsonObject>
#include <QString>

namespace Game::Mission {

class MissionLoader {
public:
  static auto loadFromJsonFile(const QString &file_path,
                               MissionDefinition &out_mission,
                               QString *error_msg = nullptr) -> bool;

private:
  static auto parsePosition(const QJsonObject &obj) -> Position;
  static auto parseUnitSetup(const QJsonObject &obj) -> UnitSetup;
  static auto parseBuildingSetup(const QJsonObject &obj) -> BuildingSetup;
  static auto parseResources(const QJsonObject &obj) -> Resources;
  static auto parsePlayerSetup(const QJsonObject &obj) -> PlayerSetup;
  static auto parseAIPersonality(const QJsonObject &obj) -> AIPersonality;
  static auto parseWaveComposition(const QJsonObject &obj) -> WaveComposition;
  static auto parseWave(const QJsonObject &obj) -> Wave;
  static auto parseAISetup(const QJsonObject &obj) -> AISetup;
  static auto parseCondition(const QJsonObject &obj) -> Condition;
  static auto parseEventTrigger(const QJsonObject &obj) -> EventTrigger;
  static auto parseEventAction(const QJsonObject &obj) -> EventAction;
  static auto parseGameEvent(const QJsonObject &obj) -> GameEvent;
};

} // namespace Game::Mission
