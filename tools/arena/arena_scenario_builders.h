#pragma once

#include <QString>
#include <QVector3D>

#include <initializer_list>
#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios::builders {

auto group(QString name,
           Game::Units::TroopType troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals = 0,
           QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup;

auto building(QString name,
              Game::Units::SpawnType type,
              Game::Systems::NationID nation,
              int owner,
              int count,
              QVector3D origin,
              QVector3D spacing = {4.5F, 0.0F, 0.0F},
              float facing = 0.0F) -> ArenaScenarioGroup;

void add_settlement_acceptance(ArenaScenarioDefinition& scenario,
                               std::initializer_list<QString> groups);

auto at(float time,
        ScenarioCommandKind command,
        QString source = {},
        QString target = {}) -> ArenaScenarioStep;

auto when_destroyed(QString destroyed,
                    ScenarioCommandKind command,
                    QString source,
                    QString target) -> ArenaScenarioStep;

auto when_near(QString lhs,
               QString rhs,
               float distance,
               ScenarioCommandKind command) -> ArenaScenarioStep;

auto expectation(ArenaExpectationKind kind,
                 QString source = {},
                 QString target = {},
                 float threshold = 0.0F,
                 float start = 0.0F,
                 float distance = 0.0F) -> ArenaExpectation;

void add_visual_stability(ArenaScenarioDefinition& scenario,
                          std::initializer_list<QString> groups);

void add_commander_control_metrics(ArenaScenarioDefinition& scenario,
                                   const QString& commander_group);

auto stop_moving(float time) -> ArenaScenarioStep;

auto nation_group(QString name,
                  Game::Units::TroopType troop,
                  Game::Systems::NationID nation,
                  int owner,
                  int count,
                  QVector3D origin,
                  int individuals = 0,
                  QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup;

auto street(QVector3D start,
            QVector3D end,
            float width,
            const char* style = "default") -> Game::Map::RoadSegment;

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing = {2.5F, 0.0F, 0.0F},
           float scale = 1.0F) -> ArenaScenarioResourcePatch;

auto residents(QString name,
               Game::Systems::NationID nation,
               int owner,
               int count,
               QVector3D origin,
               QVector3D spacing,
               float roam_radius) -> ArenaScenarioGroup;

auto undead_wave(QString trigger, std::vector<Game::Map::UndeadWaveUnitSpawn> units)
    -> Game::Map::UndeadWave;

auto undead_zone(QString id,
                 Game::Map::WorldProp::Type anchor_type,
                 QVector3D center,
                 float radius,
                 int owner_id,
                 std::vector<Game::Map::UndeadWave> waves) -> Game::Map::UndeadZone;

auto zone_expectation(ArenaExpectationKind kind,
                      QString zone_id,
                      float threshold = 0.0F,
                      float end = 0.0F) -> ArenaExpectation;

auto definition(QString id,
                QString label,
                QString description,
                float duration,
                ArenaCameraView camera = {}) -> ArenaScenarioDefinition;

} // namespace Arena::Scenarios::builders
