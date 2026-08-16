#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "game/map/mission_definition.h"

[[nodiscard]] auto build_mission_definition_map(
    const Game::Mission::MissionDefinition& mission) -> QVariantMap;
[[nodiscard]] auto build_mission_objectives_map(
    const Game::Mission::MissionDefinition& mission) -> QVariantMap;
[[nodiscard]] auto build_campaign_player_configs(
    const Game::Mission::MissionDefinition& mission) -> QVariantList;
[[nodiscard]] auto
load_mission_definition_map(const QString& mission_id) -> QVariantMap;
