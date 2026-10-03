#include "arena_scenario_builders.h"

#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

} // namespace

namespace builders {

auto group(QString name,
           Troop troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals,
           QVector3D spacing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = owner == 1 ? Nation::RomanRepublic : Nation::Carthage;
  result.owner_id = owner;
  result.count = count;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = owner == 1 ? 0.0F : 180.0F;
  return result;
}

auto building(QString name,
              Game::Units::SpawnType type,
              Nation nation,
              int owner,
              int count,
              QVector3D origin,
              QVector3D spacing,
              float facing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = nation;
  result.owner_id = owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = facing;
  return result;
}

void add_settlement_acceptance(ArenaScenarioDefinition& scenario,
                               std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    ArenaExpectation exists;
    exists.kind = Expect::GroupExists;
    exists.group = name;
    scenario.expectations.push_back(std::move(exists));
  }
  ArenaExpectation frame_budget;
  frame_budget.kind = Expect::FrameBudget;
  frame_budget.threshold = 33.34F;
  frame_budget.start_seconds = 0.25F;
  scenario.expectations.push_back(std::move(frame_budget));
}

auto at(float time,
        Command command,
        QString source,
        QString target) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto when_destroyed(QString destroyed,
                    Command command,
                    QString source,
                    QString target) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("after_%1_destroyed").arg(destroyed);
  result.trigger = {Trigger::GroupDestroyed, 0.0F, std::move(destroyed), {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto when_near(QString lhs,
               QString rhs,
               float distance,
               Command command) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("when_%1_nears_%2").arg(lhs, rhs);
  result.trigger = {Trigger::GroupsWithinDistance, 0.0F, lhs, rhs, distance};
  result.command = command;
  result.group = std::move(lhs);
  result.target_group = std::move(rhs);
  return result;
}

auto expectation(Expect kind,
                 QString source,
                 QString target,
                 float threshold,
                 float start,
                 float distance) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.target_group = std::move(target);
  result.threshold = threshold;
  result.start_seconds = start;
  result.distance = distance;
  return result;
}

void add_visual_stability(ArenaScenarioDefinition& scenario,
                          std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    scenario.expectations.push_back(expectation(Expect::NoPoseOscillation, name));
    scenario.expectations.push_back(expectation(Expect::NoRootTeleport, name));
    scenario.expectations.push_back(expectation(Expect::NoUnexpectedFallPose, name));
    scenario.expectations.push_back(expectation(Expect::MovementIsContinuous, name));
    scenario.expectations.push_back(expectation(Expect::GroupIsRendered, name));
  }
  scenario.expectations.push_back(
      expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
}

void add_commander_control_metrics(ArenaScenarioDefinition& scenario,
                                   const QString& commander_group) {
  scenario.expectations.push_back(
      expectation(Expect::CommanderInputEdgesAllConsumed, commander_group));
  scenario.expectations.push_back(
      expectation(Expect::CommanderBoomIsContinuous, commander_group, {}, 0.35F));
  scenario.expectations.push_back(
      expectation(Expect::CommanderMotorCorrectionWithin, commander_group, {}, 0.08F));
  scenario.expectations.push_back(
      expectation(Expect::CommanderCameraClearanceAtLeast, commander_group, {}, 0.10F));
  scenario.expectations.push_back(
      expectation(Expect::CommanderPresentedPoseAgrees, commander_group, {}, 0.01F));
}

auto stop_moving(float time) -> ArenaScenarioStep {
  auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
  step.destination = {};
  step.value = 0;
  return step;
}

auto nation_group(QString name,
                  Troop troop,
                  Nation nation,
                  int owner,
                  int count,
                  QVector3D origin,
                  int individuals,
                  QVector3D spacing) -> ArenaScenarioGroup {
  auto result =
      group(std::move(name), troop, owner, count, origin, individuals, spacing);
  result.nation_id = nation;
  return result;
}

auto street(QVector3D start,
            QVector3D end,
            float width,
            const char* style) -> Game::Map::RoadSegment {
  return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
}

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing,
           float scale) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type), count, origin, spacing, scale};
}

auto residents(QString name,
               Nation nation,
               int owner,
               int count,
               QVector3D origin,
               QVector3D spacing,
               float roam_radius) -> ArenaScenarioGroup {
  auto result = nation_group(
      std::move(name), Troop::Civilian, nation, owner, count, origin, 1, spacing);
  result.settlement_resident = true;
  result.settlement_roam_radius = roam_radius;
  return result;
}

auto undead_wave(QString trigger, std::vector<Game::Map::UndeadWaveUnitSpawn> units)
    -> Game::Map::UndeadWave {
  Game::Map::UndeadWave wave;
  wave.trigger = std::move(trigger);
  wave.units = std::move(units);
  return wave;
}

auto undead_zone(QString id,
                 Game::Map::WorldProp::Type anchor_type,
                 QVector3D center,
                 float radius,
                 int owner_id,
                 std::vector<Game::Map::UndeadWave> waves) -> Game::Map::UndeadZone {
  Game::Map::UndeadZone zone;
  zone.id = std::move(id);
  zone.anchor_type = anchor_type;
  zone.x = center.x();
  zone.z = center.z();
  zone.radius = radius;
  zone.leash_radius = radius * 1.8F;
  zone.owner_id = owner_id;
  zone.team_id = owner_id;
  zone.awaken_on = {QStringLiteral("unit_enters_radius")};
  zone.waves = std::move(waves);
  return zone;
}

auto zone_expectation(Expect kind,
                      QString zone_id,
                      float threshold,
                      float end) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.zone_id = std::move(zone_id);
  result.threshold = threshold;
  result.end_seconds = end;
  return result;
}

auto definition(QString id,
                QString label,
                QString description,
                float duration,
                ArenaCameraView camera) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = std::move(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = camera;
  return result;
}

} // namespace builders

} // namespace Arena::Scenarios
