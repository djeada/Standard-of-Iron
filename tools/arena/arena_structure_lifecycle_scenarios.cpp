#include "arena_structure_lifecycle_scenarios.h"

#include <utility>

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using SpawnType = Game::Units::SpawnType;
using Troop = Game::Units::TroopType;

constexpr int k_owner = 1;
constexpr int k_structure_health = 1000;

auto structure(QString name,
               SpawnType type,
               QVector3D origin,
               int count = 1,
               QVector3D spacing = {2.0F, 0.0F, 0.0F},
               float facing = 0.0F) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = k_owner;
  result.count = count;
  result.origin = origin;
  result.spacing = spacing;
  result.facing_degrees = facing;
  result.health_override = k_structure_health;
  result.max_health_override = k_structure_health;
  return result;
}

auto crew(QString name, QVector3D origin) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = Troop::Builder;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = k_owner;
  result.count = 1;
  result.origin = origin;
  return result;
}

auto step(float time,
          Command command,
          QString group,
          QString target = {},
          int value = 0) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), group);
  result.trigger = {ScenarioTriggerKind::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(group);
  result.target_group = std::move(target);
  result.value = value;
  return result;
}

auto expect(Expect kind,
            QString group = {},
            float threshold = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(group);
  result.threshold = threshold;
  return result;
}

auto frame_budget() -> ArenaExpectation {
  auto result = expect(Expect::FrameBudget, {}, 33.34F);
  result.start_seconds = 0.25F;
  return result;
}

auto staged_scene(QString id,
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
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.select_spawned_units = false;
  result.suppress_terrain_scatter = true;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  return result;
}

// Healthy until 3 s, damaged until 7 s, critical until 11 s, then every
// structure is brought down at once and followed through its collapse, the
// rubble resting, and the rubble settling into the ground.
auto damage_stages() -> ArenaScenarioDefinition {
  auto s = staged_scene(QString::fromLatin1(k_structure_damage_stages_id),
                        QStringLiteral("Structure Damage Stages"),
                        QStringLiteral("A barracks, house, tower, market, and a gated "
                                       "wall step from healthy to damaged to critical, "
                                       "then collapse into rubble that settles away."),
                        31.0F,
                        {34.0F, 50.0F, 12.0F});
  s.groups = {
      structure(QStringLiteral("barracks"), SpawnType::Barracks, {-15.0F, 0.0F, 5.0F}),
      structure(QStringLiteral("home"), SpawnType::Home, {-5.0F, 0.0F, 5.0F}),
      structure(QStringLiteral("tower"), SpawnType::DefenseTower, {3.0F, 0.0F, 5.0F}),
      structure(QStringLiteral("market"), SpawnType::Marketplace, {12.0F, 0.0F, 5.0F}),
      structure(
          QStringLiteral("west_wall"), SpawnType::WallSegment, {-7.0F, 0.0F, -8.0F}, 4),
      structure(
          QStringLiteral("east_wall"), SpawnType::WallSegment, {7.0F, 0.0F, -8.0F}, 4),
      structure(QStringLiteral("gate"), SpawnType::WallGate, {0.0F, 0.0F, -8.0F}),
  };
  for (auto const& group : s.groups) {
    s.steps.push_back(step(3.0F, Command::SetHealth, group.name, {}, 600));
    s.steps.push_back(step(7.0F, Command::SetHealth, group.name, {}, 200));
    s.steps.push_back(step(11.0F, Command::ApplyDamage, group.name, {}, 100000));
    s.expectations.push_back(expect(Expect::StructureCollapseObserved, group.name));
    s.expectations.push_back(expect(Expect::GroupDestroyed, group.name));
  }
  s.expectations.push_back(frame_budget());
  return s;
}

auto repair() -> ArenaScenarioDefinition {
  auto s = staged_scene(QString::fromLatin1(k_structure_repair_id),
                        QStringLiteral("Structure Repair"),
                        QStringLiteral("A crew raises scaffolding round a critically "
                                       "damaged house and a damaged tower and builds "
                                       "them back up, each state swap under dust."),
                        26.0F,
                        {24.0F, 48.0F, 14.0F});
  s.groups = {
      structure(QStringLiteral("home"), SpawnType::Home, {-4.0F, 0.0F, 0.0F}),
      structure(QStringLiteral("tower"), SpawnType::DefenseTower, {5.0F, 0.0F, 0.0F}),
      crew(QStringLiteral("home_crew"), {-4.0F, 0.0F, -6.0F}),
      crew(QStringLiteral("tower_crew"), {5.0F, 0.0F, -6.0F}),
  };
  s.steps = {
      step(0.4F, Command::SetHealth, QStringLiteral("home"), {}, 150),
      step(0.4F, Command::SetHealth, QStringLiteral("tower"), {}, 450),
      step(2.0F,
           Command::RepairStructure,
           QStringLiteral("home_crew"),
           QStringLiteral("home")),
      step(2.0F,
           Command::RepairStructure,
           QStringLiteral("tower_crew"),
           QStringLiteral("tower")),
  };
  s.expectations = {
      expect(Expect::StructureRepairObserved, QStringLiteral("home")),
      expect(Expect::StructureRepairObserved, QStringLiteral("tower")),
      expect(Expect::GroupExists, QStringLiteral("home")),
      expect(Expect::GroupExists, QStringLiteral("tower")),
      frame_budget(),
  };
  return s;
}

auto dismantle() -> ArenaScenarioDefinition {
  auto s =
      staged_scene(QString::fromLatin1(k_structure_dismantle_id),
                   QStringLiteral("Structure Dismantle"),
                   QStringLiteral("A crew takes a healthy house down from the top "
                                  "and stacks its timber and stone beside it; it "
                                  "leaves without the collapse of a destroyed one."),
                   20.0F,
                   {22.0F, 48.0F, 14.0F});
  s.groups = {
      structure(QStringLiteral("home"), SpawnType::Home, {0.0F, 0.0F, 0.0F}),
      crew(QStringLiteral("crew"), {-2.0F, 0.0F, -6.0F}),
  };
  s.steps = {step(1.0F,
                  Command::DismantleStructure,
                  QStringLiteral("crew"),
                  QStringLiteral("home"))};
  s.expectations = {
      expect(Expect::StructureDismantleObserved, QStringLiteral("home")),
      expect(Expect::GroupDestroyed, QStringLiteral("home")),
      frame_budget(),
  };
  return s;
}

auto construction() -> ArenaScenarioDefinition {
  auto s = staged_scene(QString::fromLatin1(k_structure_construction_id),
                        QStringLiteral("Structure Construction"),
                        QStringLiteral("A crew lays a house's foundation curb, raises "
                                       "its walls inside scaffolding, and strikes the "
                                       "scaffolding as the building is finished."),
                        40.0F,
                        {22.0F, 48.0F, 14.0F});
  s.groups = {crew(QStringLiteral("crew"), {-4.0F, 0.0F, -5.0F})};
  auto order = step(1.0F, Command::StartConstruction, QStringLiteral("crew"));
  order.construction_type = QStringLiteral("home");
  order.destination = QVector3D(0.0F, 0.0F, 0.0F);
  order.construction_rotation_degrees = 0.0F;
  s.steps = {order};
  s.expectations = {
      expect(Expect::OwnerCompletesConstruction, QStringLiteral("crew"), 1.0F),
      frame_budget(),
  };
  return s;
}

} // namespace

auto build_structure_lifecycle_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(damage_stages());
  result.push_back(repair());
  result.push_back(dismantle());
  result.push_back(construction());
  return result;
}

} // namespace Arena::Scenarios
