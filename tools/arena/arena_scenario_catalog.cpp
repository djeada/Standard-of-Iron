#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <iterator>
#include <numbers>
#include <tuple>
#include <utility>

#include "arena_ambience_scenarios.h"
#include "arena_city_scenarios.h"
#include "arena_formation_scenarios.h"
#include "arena_scenarios.h"
#include "arena_showcase_scenarios.h"
#include "arena_trailer_scenarios.h"
#include "arena_wildlife_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto group(QString name,
           Troop troop,
           int owner,
           int count,
           QVector3D origin,
           int individuals = 0,
           QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
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
              QVector3D spacing = {4.5F, 0.0F, 0.0F},
              float facing = 0.0F) -> ArenaScenarioGroup {
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
        QString source = {},
        QString target = {}) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(source);
  result.target_group = std::move(target);
  return result;
}

auto harvest_at(float time,
                QString source,
                QString resource_kind) -> ArenaScenarioStep {
  auto result = at(time, Command::HarvestResource, std::move(source));
  result.resource_kind = std::move(resource_kind);
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
                 QString source = {},
                 QString target = {},
                 float threshold = 0.0F,
                 float start = 0.0F,
                 float distance = 0.0F) -> ArenaExpectation {
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

void add_animation_quality(ArenaScenarioDefinition& scenario,
                           std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    scenario.expectations.push_back(expectation(Expect::NoPlantedFootSliding, name));
    scenario.expectations.push_back(expectation(Expect::NoWeaponTeleport, name));
    scenario.expectations.push_back(expectation(Expect::NoPelvisSnap, name));
  }
}

auto nation_group(QString name,
                  Troop troop,
                  Nation nation,
                  int owner,
                  int count,
                  QVector3D origin,
                  int individuals = 0,
                  QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  auto result =
      group(std::move(name), troop, owner, count, origin, individuals, spacing);
  result.nation_id = nation;
  return result;
}

auto street(QVector3D start,
            QVector3D end,
            float width,
            const char* style = "default") -> Game::Map::RoadSegment {
  return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
}

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing = {2.5F, 0.0F, 0.0F},
           float scale = 1.0F) -> ArenaScenarioResourcePatch {
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

struct RampartPlan {
  float extent_x{18.0F};
  float extent_z{18.0F};
  float gate_offset_x{0.0F};
  float gate_offset_z{0.0F};
};

void add_rampart(ArenaScenarioDefinition& scenario,
                 const QString& prefix,
                 Nation nation,
                 int owner,
                 const RampartPlan& plan) {
  auto const name = [&](const char* suffix) {
    return prefix + QString::fromLatin1(suffix);
  };
  auto const run =
      [&](const char* suffix, float first, float last, bool along_x, float fixed) {
        int const count = static_cast<int>((last - first) / 2.0F) + 1;
        float const center = (first + last) * 0.5F;
        scenario.groups.push_back(building(
            name(suffix),
            Game::Units::SpawnType::WallSegment,
            nation,
            owner,
            count,
            along_x ? QVector3D(center, 0.0F, fixed) : QVector3D(fixed, 0.0F, center),
            along_x ? QVector3D(2.0F, 0.0F, 0.0F) : QVector3D(0.0F, 0.0F, 2.0F),
            along_x ? 0.0F : 90.0F));
      };

  constexpr float k_gate_clearance = 4.0F;

  for (float const sign : {-1.0F, 1.0F}) {
    bool const north = sign < 0.0F;
    run(north ? "_wall_nw" : "_wall_ne",
        north ? -plan.extent_x + 2.0F : plan.gate_offset_x + k_gate_clearance,
        north ? plan.gate_offset_x - k_gate_clearance : plan.extent_x - 2.0F,
        true,
        -plan.extent_z);
    run(north ? "_wall_sw" : "_wall_se",
        north ? -plan.extent_x + 2.0F : plan.gate_offset_x + k_gate_clearance,
        north ? plan.gate_offset_x - k_gate_clearance : plan.extent_x - 2.0F,
        true,
        plan.extent_z);
    run(north ? "_wall_wn" : "_wall_en",
        -plan.extent_z + 2.0F,
        plan.gate_offset_z - k_gate_clearance,
        false,
        sign * plan.extent_x);
    run(north ? "_wall_ws" : "_wall_es",
        plan.gate_offset_z + k_gate_clearance,
        plan.extent_z - 2.0F,
        false,
        sign * plan.extent_x);
  }

  scenario.groups.push_back(building(name("_gate_north"),
                                     Game::Units::SpawnType::WallGate,
                                     nation,
                                     owner,
                                     1,
                                     {plan.gate_offset_x, 0.0F, -plan.extent_z}));
  scenario.groups.push_back(building(name("_gate_south"),
                                     Game::Units::SpawnType::WallGate,
                                     nation,
                                     owner,
                                     1,
                                     {plan.gate_offset_x, 0.0F, plan.extent_z}));
  scenario.groups.push_back(building(name("_gate_west"),
                                     Game::Units::SpawnType::WallGate,
                                     nation,
                                     owner,
                                     1,
                                     {-plan.extent_x, 0.0F, plan.gate_offset_z},
                                     {},
                                     90.0F));
  scenario.groups.push_back(building(name("_gate_east"),
                                     Game::Units::SpawnType::WallGate,
                                     nation,
                                     owner,
                                     1,
                                     {plan.extent_x, 0.0F, plan.gate_offset_z},
                                     {},
                                     90.0F));

  struct Corner {
    const char* suffix;
    float x;
    float z;
  };
  for (auto const& corner : {Corner{"_tower_nw", -plan.extent_x, -plan.extent_z},
                             Corner{"_tower_ne", plan.extent_x, -plan.extent_z},
                             Corner{"_tower_sw", -plan.extent_x, plan.extent_z},
                             Corner{"_tower_se", plan.extent_x, plan.extent_z}}) {
    scenario.groups.push_back(building(name(corner.suffix),
                                       Game::Units::SpawnType::DefenseTower,
                                       nation,
                                       owner,
                                       1,
                                       {corner.x, 0.0F, corner.z}));
  }
}

auto rampart_groups(const QString& prefix) -> std::vector<QString> {
  std::vector<QString> names;
  for (auto const* suffix : {"_wall_nw",
                             "_wall_ne",
                             "_wall_sw",
                             "_wall_se",
                             "_wall_wn",
                             "_wall_en",
                             "_wall_ws",
                             "_wall_es",
                             "_gate_north",
                             "_gate_south",
                             "_gate_west",
                             "_gate_east",
                             "_tower_nw",
                             "_tower_ne",
                             "_tower_sw",
                             "_tower_se"}) {
    names.push_back(prefix + QString::fromLatin1(suffix));
  }
  return names;
}

void require_groups_exist(ArenaScenarioDefinition& scenario,
                          const std::vector<QString>& groups) {
  for (auto const& name : groups) {
    ArenaExpectation exists;
    exists.kind = Expect::GroupExists;
    exists.group = name;
    scenario.expectations.push_back(std::move(exists));
  }
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
                      float threshold = 0.0F,
                      float end = 0.0F) -> ArenaExpectation {
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
                ArenaCameraView camera = {}) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = std::move(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = camera;
  return result;
}

auto performance_battle_definition(QString id,
                                   QString label,
                                   int units_per_side) -> ArenaScenarioDefinition {
  auto s = definition(std::move(id),
                      std::move(label),
                      QStringLiteral("Mixed full-LOD battle with %1 units per side and "
                                     "a strict over-100-FPS p95 contract.")
                          .arg(units_per_side),
                      8.0F,
                      {units_per_side <= 20 ? 58.0F : 68.0F, 56.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.require_rigged_instancing = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  int const swords = units_per_side * 2 / 5;
  int const spears = units_per_side * 3 / 10;
  int const archers = units_per_side / 5;
  int const cavalry = units_per_side - swords - spears - archers;
  s.groups = {
      group(QStringLiteral("blue_swords"),
            Troop::Swordsman,
            1,
            swords,
            {-16.0F, 0.0F, -12.0F},
            1),
      group(QStringLiteral("blue_spears"),
            Troop::Spearman,
            1,
            spears,
            {-12.0F, 0.0F, -18.0F},
            1),
      group(QStringLiteral("blue_archers"),
            Troop::Archer,
            1,
            archers,
            {-8.0F, 0.0F, -25.0F},
            1),
      group(QStringLiteral("blue_cavalry"),
            Troop::MountedKnight,
            1,
            cavalry,
            {-23.0F, 0.0F, -20.0F},
            1),
      group(QStringLiteral("red_swords"),
            Troop::Swordsman,
            2,
            swords,
            {-16.0F, 0.0F, 12.0F},
            1),
      group(QStringLiteral("red_spears"),
            Troop::Spearman,
            2,
            spears,
            {-12.0F, 0.0F, 18.0F},
            1),
      group(QStringLiteral("red_archers"),
            Troop::Archer,
            2,
            archers,
            {-8.0F, 0.0F, 25.0F},
            1),
      group(QStringLiteral("red_cavalry"),
            Troop::MountedKnight,
            2,
            cavalry,
            {-23.0F, 0.0F, 20.0F},
            1),
  };
  s.steps = {
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("blue_swords"),
         QStringLiteral("red_spears")),
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("blue_spears"),
         QStringLiteral("red_swords")),
      at(0.25F,
         Command::Attack,
         QStringLiteral("blue_archers"),
         QStringLiteral("red_spears")),
      at(0.25F,
         Command::Charge,
         QStringLiteral("blue_cavalry"),
         QStringLiteral("red_archers")),
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("red_swords"),
         QStringLiteral("blue_spears")),
      at(0.25F,
         Command::AttackMove,
         QStringLiteral("red_spears"),
         QStringLiteral("blue_swords")),
      at(0.25F,
         Command::Attack,
         QStringLiteral("red_archers"),
         QStringLiteral("blue_spears")),
      at(0.25F,
         Command::Charge,
         QStringLiteral("red_cavalry"),
         QStringLiteral("blue_archers")),
  };
  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("blue_swords")),
      expectation(Expect::GroupExists, QStringLiteral("red_swords")),
      expectation(Expect::FrameBudget, {}, {}, 9.99F, 2.0F),
  };
  return s;
}

auto massed_battle_definition(QString id,
                              QString label,
                              int units_per_side,
                              int individuals_per_unit = 20)
    -> ArenaScenarioDefinition {
  int const soldiers_per_side = units_per_side * individuals_per_unit;

  auto s = definition(std::move(id),
                      std::move(label),
                      QStringLiteral("Full-field line battle: %1 squads and %2 "
                                     "rendered soldiers per side at forced full "
                                     "creature LOD, Ultra shadows, batching, combat, "
                                     "archery and cavalry.")
                          .arg(units_per_side)
                          .arg(soldiers_per_side),
                      16.0F,
                      {units_per_side >= 40 ? 132.0F : 96.0F, 58.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.arena_floor_half_extent = 45.0F;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.require_rigged_instancing = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  int const swords = units_per_side * 32 / 100;
  int const spears = units_per_side * 28 / 100;
  int const archers = units_per_side * 20 / 100;
  int const horse_archers = std::max(1, units_per_side * 4 / 100);
  int const healers = std::max(1, units_per_side * 2 / 100);
  int const cavalry =
      units_per_side - swords - spears - archers - horse_archers - healers;

  auto line = [individuals_per_unit](const QString& name,
                                     Troop troop,
                                     int owner,
                                     int count,
                                     float sign,
                                     float depth,
                                     float x_step) {
    return group(name,
                 troop,
                 owner,
                 count,
                 {0.0F, 0.0F, sign * depth},
                 individuals_per_unit,
                 {x_step, 0.0F, 0.0F});
  };
  auto column = [individuals_per_unit](const QString& name,
                                       Troop troop,
                                       int owner,
                                       int count,
                                       float sign,
                                       float x_offset,
                                       float depth) {
    return group(name,
                 troop,
                 owner,
                 count,
                 {x_offset, 0.0F, sign * depth},
                 individuals_per_unit,
                 {0.0F, 0.0F, sign * 6.5F});
  };

  auto add_side = [&](const QString& prefix, int owner, float sign) {
    s.groups.push_back(line(prefix + QStringLiteral("_swords_a"),
                            Troop::Swordsman,
                            owner,
                            (swords + 1) / 2,
                            sign,
                            11.0F,
                            6.0F));
    s.groups.push_back(line(prefix + QStringLiteral("_swords_b"),
                            Troop::Swordsman,
                            owner,
                            swords / 2,
                            sign,
                            17.0F,
                            6.0F));
    s.groups.push_back(line(prefix + QStringLiteral("_spears_a"),
                            Troop::Spearman,
                            owner,
                            (spears + 1) / 2,
                            sign,
                            23.0F,
                            6.5F));
    s.groups.push_back(line(prefix + QStringLiteral("_spears_b"),
                            Troop::Spearman,
                            owner,
                            spears / 2,
                            sign,
                            29.0F,
                            6.5F));
    s.groups.push_back(line(prefix + QStringLiteral("_archers_a"),
                            Troop::Archer,
                            owner,
                            (archers + 1) / 2,
                            sign,
                            35.0F,
                            8.0F));
    s.groups.push_back(line(prefix + QStringLiteral("_archers_b"),
                            Troop::Archer,
                            owner,
                            archers / 2,
                            sign,
                            41.0F,
                            8.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_cavalry_left"),
                              Troop::MountedKnight,
                              owner,
                              (cavalry + 1) / 2,
                              sign,
                              -34.0F,
                              18.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_cavalry_right"),
                              Troop::MountedKnight,
                              owner,
                              cavalry / 2,
                              sign,
                              34.0F,
                              18.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_horse_archers"),
                              Troop::HorseArcher,
                              owner,
                              horse_archers,
                              sign,
                              -41.0F,
                              33.0F));
    s.groups.push_back(column(prefix + QStringLiteral("_healers"),
                              Troop::Healer,
                              owner,
                              healers,
                              sign,
                              41.0F,
                              38.0F));
  };
  add_side(QStringLiteral("blue"), 1, -1.0F);
  add_side(QStringLiteral("red"), 2, 1.0F);

  auto add_orders = [&](const QString& prefix, const QString& enemy) {
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_swords_a"),
                         enemy + QStringLiteral("_spears_a")));
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_swords_b"),
                         enemy + QStringLiteral("_spears_b")));
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_spears_a"),
                         enemy + QStringLiteral("_swords_a")));
    s.steps.push_back(at(0.5F,
                         Command::AttackMove,
                         prefix + QStringLiteral("_spears_b"),
                         enemy + QStringLiteral("_swords_b")));
    s.steps.push_back(at(0.5F,
                         Command::Attack,
                         prefix + QStringLiteral("_archers_a"),
                         enemy + QStringLiteral("_spears_a")));
    s.steps.push_back(at(0.5F,
                         Command::Attack,
                         prefix + QStringLiteral("_archers_b"),
                         enemy + QStringLiteral("_swords_a")));
    s.steps.push_back(at(0.5F,
                         Command::Attack,
                         prefix + QStringLiteral("_horse_archers"),
                         enemy + QStringLiteral("_swords_a")));
    s.steps.push_back(at(0.5F,
                         Command::Charge,
                         prefix + QStringLiteral("_cavalry_left"),
                         enemy + QStringLiteral("_archers_a")));
    s.steps.push_back(at(0.5F,
                         Command::Charge,
                         prefix + QStringLiteral("_cavalry_right"),
                         enemy + QStringLiteral("_archers_b")));
  };
  add_orders(QStringLiteral("blue"), QStringLiteral("red"));
  add_orders(QStringLiteral("red"), QStringLiteral("blue"));

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("blue_swords_a")),
      expectation(Expect::GroupExists, QStringLiteral("red_swords_a")),
      expectation(Expect::FrameBudget, {}, {}, 16.67F, 4.0F),
  };
  return s;
}

auto seven_ai_scale_definition() -> ArenaScenarioDefinition {
  constexpr int k_ai_count = 7;
  constexpr float k_circle_radius = 72.0F;
  constexpr float k_unit_radius = 49.0F;
  constexpr int k_individuals = 10;

  auto s = definition(
      QString::fromLatin1(k_seven_ai_scale_id),
      QStringLiteral("Performance: Seven Full Economies"),
      QStringLiteral("Seven active AI economies with 140 initial buildings, 252 "
                     "simulation units, and roughly 2,000 full-detail rendered "
                     "soldiers. Exercises AI snapshots, construction, resources, "
                     "combat, building rendering, and maximum-player scheduling."),
      30.0F,
      {188.0F, 62.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.arena_floor_half_extent = 120.0F;
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.force_full_creature_lod = true;
  s.require_rigged_instancing = true;
  s.collect_animation_diagnostics = false;
  s.graphics_quality = Render::GraphicsQuality::Ultra;

  auto add_ai_group = [&](ArenaScenarioGroup value) {
    value.ai_controlled = true;
    s.groups.push_back(std::move(value));
  };

  for (int index = 0; index < k_ai_count; ++index) {
    const int owner = index + 2;
    const auto nation = (index % 2) == 0 ? Nation::RomanRepublic : Nation::Carthage;
    const float angle = -std::numbers::pi_v<float> * 0.5F +
                        2.0F * std::numbers::pi_v<float> * static_cast<float>(index) /
                            static_cast<float>(k_ai_count);
    const QVector3D radial(std::cos(angle), 0.0F, std::sin(angle));
    const QVector3D tangent(-radial.z(), 0.0F, radial.x());
    const QVector3D center = radial * k_circle_radius;
    const QVector3D unit_center = radial * k_unit_radius;
    const QString prefix = QStringLiteral("ai_%1_").arg(owner);

    auto add_building_row = [&](const QString& suffix,
                                Game::Units::SpawnType type,
                                int count,
                                float radial_offset,
                                float tangent_offset,
                                float spacing) {
      auto row = building(prefix + suffix,
                          type,
                          nation,
                          owner,
                          count,
                          center + radial * radial_offset + tangent * tangent_offset,
                          tangent * spacing,
                          -angle * 180.0F / std::numbers::pi_v<float>);
      add_ai_group(std::move(row));
    };

    add_building_row(
        QStringLiteral("homes"), Game::Units::SpawnType::Home, 8, 7.0F, -17.5F, 5.0F);
    add_building_row(QStringLiteral("barracks"),
                     Game::Units::SpawnType::Barracks,
                     3,
                     0.0F,
                     -8.0F,
                     8.0F);
    add_building_row(QStringLiteral("markets"),
                     Game::Units::SpawnType::Marketplace,
                     3,
                     -8.0F,
                     -8.0F,
                     8.0F);
    add_building_row(QStringLiteral("temples"),
                     Game::Units::SpawnType::Temple,
                     2,
                     15.0F,
                     -5.0F,
                     10.0F);
    add_building_row(QStringLiteral("towers"),
                     Game::Units::SpawnType::DefenseTower,
                     4,
                     -16.0F,
                     -15.0F,
                     10.0F);

    auto add_troops = [&](const QString& suffix,
                          Troop troop,
                          int count,
                          float radial_offset,
                          int individuals) {
      auto troops = nation_group(prefix + suffix,
                                 troop,
                                 nation,
                                 owner,
                                 count,
                                 unit_center + radial * radial_offset - tangent * 9.0F,
                                 individuals,
                                 tangent * 2.8F);
      troops.facing_degrees = -angle * 180.0F / std::numbers::pi_v<float> - 90.0F;
      add_ai_group(std::move(troops));
    };
    add_troops(QStringLiteral("builders"), Troop::Builder, 8, 12.0F, 1);
    add_troops(QStringLiteral("swords"), Troop::Swordsman, 8, 6.0F, k_individuals);
    add_troops(QStringLiteral("spears"), Troop::Spearman, 8, 0.0F, k_individuals);
    add_troops(QStringLiteral("archers"), Troop::Archer, 8, -6.0F, k_individuals);
    add_troops(
        QStringLiteral("cavalry"), Troop::MountedKnight, 4, -12.0F, k_individuals);

    const QVector3D resources = radial * 94.0F - tangent * 8.0F;
    s.resource_patches.push_back(
        patch("olive_tree", 8, resources, tangent * 2.6F, 1.05F));
    s.resource_patches.push_back(
        patch("boulder", 6, resources + radial * 7.0F, tangent * 2.8F, 1.0F));
    s.resource_patches.push_back(
        patch("iron_ore", 4, resources - radial * 7.0F, tangent * 3.0F, 1.0F));
  }

  s.expectations.push_back(
      expectation(Expect::GroupExists, QStringLiteral("ai_2_builders")));
  s.expectations.push_back(
      expectation(Expect::GroupExists, QStringLiteral("ai_8_builders")));
  s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 1000.0F, 2.0F));
  return s;
}

auto build_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_commander_aura_pulse_id),
        QStringLiteral("Commander Aura Pulse"),
        QStringLiteral(
            "Explicitly activates a short command aura, verifies nearby "
            "troops receive its visible timed bonus, verifies distant troops "
            "do not, and waits for cooldown."),
        5.0F,
        {26.0F, 55.0F, 25.0F});
    auto commander = group(QStringLiteral("commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.nation_id = Nation::RomanRepublic;
    auto near_troops = group(QStringLiteral("near_troops"),
                             Troop::Swordsman,
                             1,
                             2,
                             {5.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 3.0F});
    auto distant_troops = group(QStringLiteral("distant_troops"),
                                Troop::Swordsman,
                                1,
                                1,
                                {21.0F, 0.0F, 0.0F},
                                4);
    s.groups = {commander, near_troops, distant_troops};
    auto activate =
        at(0.5F, Command::TriggerCommanderAura, QStringLiteral("commander"));
    activate.value = 2;
    s.steps = {activate};
    s.expectations = {
        expectation(Expect::CommanderAuraActivated, QStringLiteral("commander")),
        expectation(Expect::CommanderAuraBuffObserved, QStringLiteral("near_troops")),
        expectation(Expect::NoCommanderAuraBuffObserved,
                    QStringLiteral("distant_troops")),
        expectation(Expect::CommanderAuraExpired, QStringLiteral("commander")),
        expectation(Expect::GroupIsRendered, QStringLiteral("commander")),
        expectation(Expect::GroupIsRendered, QStringLiteral("near_troops")),
        expectation(Expect::GroupIsRendered, QStringLiteral("distant_troops")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_melee_contact_id),
        QStringLiteral("RPG Exact Melee Contact"),
        QStringLiteral(
            "Behind-head commander combat against a six-soldier formation. "
            "Validates exact in-range soldier highlighting, authored blade "
            "contact, exact hit reaction, and visible incoming weapon damage."),
        5.4F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, -1.8F},
                           1);
    commander.facing_degrees = 0.0F;
    auto enemy = group(QStringLiteral("enemy_formation"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 0.5F},
                       6);
    enemy.health_override = enemy.max_health_override = 500;
    s.groups = {commander, enemy};
    s.steps = {
        at(0.15F,
           Command::Attack,
           QStringLiteral("enemy_formation"),
           QStringLiteral("rpg_commander")),
        at(0.55F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(3.05F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_formation")});
    add_animation_quality(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_formation")});
    s.expectations.push_back(
        expectation(Expect::AttackHasTorsoRotation, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::ExactRpgTargetObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::HitReactionObserved, QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::RpgDamageContactObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgStrikeAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgHealthReduced, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_defense_contact_id),
        QStringLiteral("RPG Block and Dodge Contact"),
        QStringLiteral(
            "Behind-head defensive sequence with a frontal sword block followed "
            "by a timed dodge against a newly joining attacker. Health must stay "
            "unchanged while the block contact and dodge window remain visible."),
        3.3F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto guard_attacker = group(QStringLiteral("guard_attacker"),
                                Troop::Swordsman,
                                2,
                                1,
                                {0.0F, 0.0F, 1.45F},
                                1);
    auto dodge_attacker = group(QStringLiteral("dodge_attacker"),
                                Troop::Swordsman,
                                2,
                                1,
                                {0.45F, 0.0F, 1.45F},
                                1);
    dodge_attacker.spawn_at_start = false;
    s.groups = {commander, guard_attacker, dodge_attacker};
    auto enable_guard = at(0.05F, Command::RpgGuard, QStringLiteral("rpg_commander"));
    enable_guard.enabled = true;
    auto remove_guard_attacker =
        at(1.55F, Command::SetHealth, QStringLiteral("guard_attacker"));
    remove_guard_attacker.value = 0;
    auto disable_guard = at(1.62F, Command::RpgGuard, QStringLiteral("rpg_commander"));
    disable_guard.enabled = false;
    s.steps = {
        enable_guard,
        at(0.15F,
           Command::Attack,
           QStringLiteral("guard_attacker"),
           QStringLiteral("rpg_commander")),
        remove_guard_attacker,
        disable_guard,
        at(1.72F,
           Command::SpawnAmbush,
           QStringLiteral("dodge_attacker"),
           QStringLiteral("rpg_commander")),
        [] {
          auto dodge = at(2.12F, Command::RpgDodge, QStringLiteral("rpg_commander"));
          dodge.destination = {0.0F, 0.0F, -1.0F};
          return dodge;
        }(),
    };
    add_visual_stability(s,
                         {QStringLiteral("rpg_commander"),
                          QStringLiteral("guard_attacker"),
                          QStringLiteral("dodge_attacker")});
    s.expectations.push_back(
        expectation(Expect::NoWeaponTeleport, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::NoPelvisSnap, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::ExactRpgTargetObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgBlockContactObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgDodgeWindowObserved, QStringLiteral("rpg_commander")));
    {

      auto health_unchanged =
          expectation(Expect::RpgHealthUnchanged, QStringLiteral("rpg_commander"));
      health_unchanged.end_seconds = 2.60F;
      s.expectations.push_back(health_unchanged);
    }
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_projectile_block_id),
        QStringLiteral("RPG Projectile Block"),
        QStringLiteral(
            "Behind-head commander faces an authored enemy arrow flight. The "
            "projectile must visibly arrive at the guard, publish a block contact, "
            "and leave RPG health unchanged."),
        4.2F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto archer = group(
        QStringLiteral("enemy_archer"), Troop::Archer, 2, 1, {0.0F, 0.0F, 6.0F}, 1);
    s.groups = {commander, archer};
    auto enable_guard = at(0.05F, Command::RpgGuard, QStringLiteral("rpg_commander"));
    enable_guard.enabled = true;
    s.steps = {
        enable_guard,
        at(0.15F,
           Command::Attack,
           QStringLiteral("enemy_archer"),
           QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_archer")});
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("enemy_archer"),
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("enemy_archer"),
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgBlockContactObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgHealthUnchanged, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_escort_crowd_id),
        QStringLiteral("RPG Escort Crowd"),
        QStringLiteral(
            "Behind-head commander standing inside his own escort, with a rank of "
            "friendly spearmen between him and the lens. The chase camera must stay "
            "readable: bodies that crowd the gap in front of the lens are dropped "
            "rather than filling the frame or shoving the camera into first person."),
        3.6F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;

    auto escort_rear = group(
        QStringLiteral("escort_rear"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -1.7F}, 4);
    escort_rear.facing_degrees = 0.0F;

    auto escort_flank = group(
        QStringLiteral("escort_flank"), Troop::Swordsman, 1, 1, {2.4F, 0.0F, 0.2F}, 2);
    escort_flank.facing_degrees = 0.0F;
    auto enemy = group(
        QStringLiteral("enemy_line"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 3.2F}, 3);
    enemy.health_override = enemy.max_health_override = 500;
    s.groups = {commander, escort_rear, escort_flank, enemy};
    s.steps = {
        at(0.20F,
           Command::Attack,
           QStringLiteral("enemy_line"),
           QStringLiteral("rpg_commander")),
        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };

    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_line")});
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("escort_flank")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_locomotion_id),
        QStringLiteral("RPG Locomotion"),
        QStringLiteral(
            "Behind-head commander walks forward, breaks into a run, backs up, "
            "strafes, then halts on open ground without ever turning. The rendered "
            "locomotion has to follow the simulated one frame for frame, with no "
            "root teleporting and no idle pose while the body is still travelling."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, -6.0F},
                           1);
    commander.facing_degrees = 0.0F;
    s.groups = {commander};

    auto move = [](float time, QVector3D axes, bool run) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = run ? 1 : 0;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, false),
        move(2.20F, {0.0F, 0.0F, 1.0F}, true),

        move(4.40F, {0.0F, 0.0F, -1.0F}, false),
        move(6.20F, {1.0F, 0.0F, 0.0F}, false),
        move(8.00F, {0.0F, 0.0F, 0.0F}, false),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgRunObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));

    auto hitched = s;
    hitched.id = QString::fromLatin1(k_rpg_locomotion_hitch_id);
    hitched.label = QStringLiteral("RPG Locomotion Under Hitches");
    hitched.description = QStringLiteral(
        "The locomotion script again, with four deliberately long presented "
        "frames injected while the commander is mid-stride. A planted foot that "
        "keeps travelling while the body reports as idle only appears when a "
        "frame runs long, so the hitches are authored rather than waited for: "
        "the defect reproduces on every run instead of once in five on a busy "
        "machine.");
    hitched.presentation_hitches = {
        {1.20F, 33.0F}, {3.00F, 50.0F}, {5.00F, 100.0F}, {6.80F, 100.0F}};
    result.push_back(std::move(s));
    result.push_back(std::move(hitched));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_motor_start_stop_id),
        QStringLiteral("RPG Motor Start And Stop"),
        QStringLiteral(
            "Walk, run, halt, and reverse on open ground. The motor contract is "
            "that a held direction settles at the configured speed, a release "
            "decelerates rather than snapping, and a reversal does not teleport "
            "the body through the turn."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, -8.0F},
                           1);
    commander.facing_degrees = 0.0F;
    s.groups = {commander};

    auto move = [](float time, QVector3D axes, bool run) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = run ? 1 : 0;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, false),
        move(1.80F, {0.0F, 0.0F, 0.0F}, false),
        move(3.00F, {0.0F, 0.0F, 1.0F}, true),
        move(5.00F, {0.0F, 0.0F, -1.0F}, true),
        move(7.00F, {0.0F, 0.0F, 0.0F}, false),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(
        expectation(Expect::RpgRunObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_motor_diagonal_id),
        QStringLiteral("RPG Motor Diagonal"),
        QStringLiteral(
            "All eight input directions in turn, each held long enough to settle. "
            "A diagonal is two axes at once and must not travel faster than a "
            "single axis; the same script also walks every backpedal and strafe "
            "scale past the locomotion contract."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    s.groups = {commander};

    auto move = [](float time, QVector3D axes) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}),
        move(1.30F, {1.0F, 0.0F, 1.0F}),
        move(2.30F, {1.0F, 0.0F, 0.0F}),
        move(3.30F, {1.0F, 0.0F, -1.0F}),
        move(4.30F, {0.0F, 0.0F, -1.0F}),
        move(5.30F, {-1.0F, 0.0F, -1.0F}),
        move(6.30F, {-1.0F, 0.0F, 0.0F}),
        move(7.30F, {-1.0F, 0.0F, 1.0F}),
        move(8.30F, {0.0F, 0.0F, 0.0F}),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_motor_figure_eight_id),
        QStringLiteral("RPG Motor Figure Eight"),
        QStringLiteral(
            "Continuous camera-relative direction changes: the view yaw sweeps "
            "while a forward input is held, so the movement basis rotates under "
            "the motor every tick. The body must trace a smooth path rather than "
            "stutter as the basis turns."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;
    s.groups = {commander};

    auto move = [](float time, QVector3D axes, float yaw) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      step.rpg_view_yaw_degrees = yaw;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, 0.0F),
        move(1.10F, {0.0F, 0.0F, 1.0F}, 45.0F),
        move(1.90F, {0.0F, 0.0F, 1.0F}, 90.0F),
        move(2.70F, {0.0F, 0.0F, 1.0F}, 135.0F),
        move(3.50F, {0.0F, 0.0F, 1.0F}, 180.0F),
        move(4.30F, {0.0F, 0.0F, 1.0F}, 225.0F),
        move(5.10F, {0.0F, 0.0F, 1.0F}, 270.0F),
        move(5.90F, {0.0F, 0.0F, 1.0F}, 315.0F),
        move(6.70F, {0.0F, 0.0F, 1.0F}, 0.0F),
        move(8.00F, {0.0F, 0.0F, 0.0F}, 0.0F),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    add_animation_quality(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::RpgWalkObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_close_quarters_id),
        QStringLiteral("RPG Close Quarters"),
        QStringLiteral(
            "Behind-head commander walks straight into a house wall, backs off, and "
            "closes on it again from the flank. The RTS navigation grid keeps whole "
            "formations a pad away from every structure; a person-sized commander "
            "has to reach the facade itself, so this is the close-quarters "
            "clearance contract."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, -5.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto home = building(QStringLiteral("close_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {0.0F, 0.0F, 0.0F});
    home.health_override = home.max_health_override = 4000;
    s.groups = {commander, home};

    auto move =
        [](float time, QVector3D axes, bool run, std::optional<float> yaw = {}) {
          auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
          step.destination = axes;
          step.value = run ? 1 : 0;
          step.rpg_view_yaw_degrees = yaw;
          return step;
        };
    s.steps = {

        move(0.30F, {0.0F, 0.0F, 1.0F}, false, 0.0F),
        move(2.40F, {0.0F, 0.0F, 0.0F}, false),

        move(2.70F, {0.0F, 0.0F, 1.0F}, false, 180.0F),
        move(4.20F, {0.0F, 0.0F, 0.0F}, false),

        move(4.50F, {0.0F, 0.0F, 1.0F}, false, 90.0F),
        move(6.30F, {0.0F, 0.0F, 0.0F}, false),

        move(6.60F, {0.0F, 0.0F, 1.0F}, false, 0.0F),
        move(8.60F, {0.0F, 0.0F, 0.0F}, false),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    s.expectations.push_back(expectation(Expect::RpgApproachWithin,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("close_home"),
                                         0.0F,
                                         0.0F,
                                         1.90F));
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_obstacle_slide_id),
        QStringLiteral("RPG Obstacle Slide"),
        QStringLiteral(
            "Behind-head commander walks diagonally into a house facade and keeps "
            "going. Direct control has to behave like a body against a wall: the "
            "blocked axis is dropped and the commander slides along the facade "
            "instead of stopping dead in front of it with the stick still pushed."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {-2.0F, 0.0F, -6.0F},
                           1);
    commander.facing_degrees = 0.0F;
    auto home = building(QStringLiteral("slide_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {0.0F, 0.0F, 0.0F});
    home.health_override = home.max_health_override = 4000;
    s.groups = {commander, home};

    auto move = [](float time, QVector3D axes, std::optional<float> yaw = {}) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      step.rpg_view_yaw_degrees = yaw;
      return step;
    };
    s.steps = {
        move(0.30F, {0.0F, 0.0F, 1.0F}, 30.0F),
        move(8.20F, {0.0F, 0.0F, 0.0F}),
    };
    add_visual_stability(s, {QStringLiteral("rpg_commander")});

    s.expectations.push_back(expectation(Expect::RpgApproachWithin,
                                         QStringLiteral("rpg_commander"),
                                         QStringLiteral("slide_home"),
                                         0.0F,
                                         0.0F,
                                         1.90F));
    auto slide = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 1.50F, 2.30F);
    slide.end_seconds = 4.00F;
    s.expectations.push_back(slide);
    s.expectations.push_back(expectation(Expect::RpgLocomotionAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_combo_cadence_id),
        QStringLiteral("RPG Combo Cadence"),
        QStringLiteral(
            "Behind-head commander holds the attack input against a six-soldier "
            "formation. Every swing has to chain out of the previous one inside "
            "its cancel window, so a held attack reads as a combo rather than as "
            "one committed animation the player waits out."),
        7.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, -1.8F},
                           1);
    commander.facing_degrees = 0.0F;
    auto enemy = group(QStringLiteral("enemy_formation"),
                       Troop::Swordsman,
                       2,
                       1,
                       {0.0F, 0.0F, 0.5F},
                       6);
    enemy.health_override = enemy.max_health_override = 4000;
    s.groups = {commander, enemy};

    auto hold_attack = [](float time, bool held) {
      auto step = at(time, Command::RpgAttackHold, QStringLiteral("rpg_commander"));
      step.enabled = held;
      return step;
    };
    s.steps = {
        hold_attack(0.40F, true),
        hold_attack(5.60F, false),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_formation")});

    s.expectations.push_back(expectation(Expect::RpgSwingCadenceWithin,
                                         QStringLiteral("rpg_commander"),
                                         {},
                                         1.10F,
                                         0.0F,
                                         5.0F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgStrikeAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::RpgDamageContactObserved,
                                         QStringLiteral("enemy_formation")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_formation")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_bow_volley_id),
        QStringLiteral("RPG Bow Volley"),
        QStringLiteral(
            "Behind-head bow commander against nine charging swordsmen. Every shot "
            "is drawn, held at full draw, and loosed along the aim ray with the "
            "crosshair solved from the live chase camera, so each arrow has to "
            "leave the bow where the reticle was sitting and drop one attacker "
            "before the line reaches him."),
        14.6F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.arena_floor_half_extent = 30.0F;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 16.2F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanFieldCommander,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;

    auto charger = [](QString name, QVector3D origin) {
      auto enemy = group(std::move(name), Troop::Swordsman, 2, 1, origin, 1);
      enemy.health_override = enemy.max_health_override = 70;
      return enemy;
    };
    std::array<QString, 9> const charger_names{QStringLiteral("charger_centre"),
                                               QStringLiteral("charger_left"),
                                               QStringLiteral("charger_right"),
                                               QStringLiteral("charger_far_left"),
                                               QStringLiteral("charger_far_right"),
                                               QStringLiteral("charger_deep_left"),
                                               QStringLiteral("charger_deep_right"),
                                               QStringLiteral("charger_flank_left"),
                                               QStringLiteral("charger_flank_right")};

    std::array<QVector3D, 9> const charger_origins{QVector3D{0.4F, 0.0F, 12.0F},
                                                   QVector3D{-4.2F, 0.0F, 12.0F},
                                                   QVector3D{4.4F, 0.0F, 12.0F},
                                                   QVector3D{-7.6F, 0.0F, 11.0F},
                                                   QVector3D{7.8F, 0.0F, 11.0F},
                                                   QVector3D{-2.6F, 0.0F, 14.5F},
                                                   QVector3D{2.8F, 0.0F, 14.5F},
                                                   QVector3D{-9.5F, 0.0F, 13.0F},
                                                   QVector3D{9.6F, 0.0F, 13.0F}};
    s.groups = {commander};
    for (std::size_t i = 0; i < charger_names.size(); ++i) {
      s.groups.push_back(charger(charger_names[i], charger_origins[i]));
    }

    auto draw = [](float time, bool held) {
      auto step = at(time, Command::RpgAttackHold, QStringLiteral("rpg_commander"));
      step.enabled = held;
      return step;
    };
    auto aim_at = [](float time, const QString& target) {
      return at(time, Command::RpgAim, QStringLiteral("rpg_commander"), target);
    };

    constexpr float k_first_shot = 0.55F;
    constexpr float k_shot_interval = 1.45F;
    for (std::size_t i = 0; i < charger_names.size(); ++i) {
      float const shot = k_first_shot + (k_shot_interval * static_cast<float>(i));
      s.steps.push_back(at(std::max(0.10F, shot - 3.20F),
                           Command::Attack,
                           charger_names[i],
                           QStringLiteral("rpg_commander")));

      s.steps.push_back(aim_at(shot, charger_names[i]));
      s.steps.push_back(draw(shot + 0.16F, true));
      s.steps.push_back(draw(shot + 0.86F, false));
    }

    add_visual_stability(s, {QStringLiteral("rpg_commander")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("rpg_commander"),
                                         charger_names[0]));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("rpg_commander"),
                                         charger_names[0]));
    for (auto const& name : charger_names) {
      s.expectations.push_back(expectation(Expect::GroupDestroyed, name));
    }
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_strike_lunge_id),
        QStringLiteral("RPG Strike Lunge"),
        QStringLiteral(
            "Behind-head commander attacks an enemy standing just outside his "
            "planted reach, with no movement input at all. A swing has to carry "
            "the body into the target: from a planted stance the strike whiffs "
            "at the edge of reach and the fight reads as two puppets waving at "
            "each other."),
        6.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 0.0F;

    auto enemy = group(
        QStringLiteral("enemy_target"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.6F}, 1);
    enemy.facing_degrees = 180.0F;
    enemy.health_override = enemy.max_health_override = 4000;
    s.groups = {commander, enemy};

    s.steps = {
        at(0.60F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(2.20F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
        at(3.80F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("enemy_target")});

    auto lunge = expectation(
        Expect::RpgTravelObserved, QStringLiteral("rpg_commander"), {}, 0.30F, 0.50F);
    lunge.end_seconds = 5.50F;
    s.expectations.push_back(lunge);
    s.expectations.push_back(
        expectation(Expect::RpgDamageContactObserved, QStringLiteral("enemy_target")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_target")));
    s.expectations.push_back(expectation(Expect::RpgStrikeAnimationMatched,
                                         QStringLiteral("rpg_commander")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rpg_pass_ranks_id),
        QStringLiteral("RPG Pass Ranks"),
        QStringLiteral(
            "Behind-head commander walks the length of a friendly line and then "
            "back through it. Only the individual bodies that stand between the "
            "lens and the commander may drop out: a rank the player walks past "
            "has to stay on screen instead of the whole unit blinking away as "
            "soon as its anchor enters the gap."),
        9.0F);
    s.rpg_mode = true;
    s.rpg_commander_group = QStringLiteral("rpg_commander");
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto commander = group(QStringLiteral("rpg_commander"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {-6.0F, 0.0F, 0.0F},
                           1);
    commander.facing_degrees = 90.0F;

    auto line = group(QStringLiteral("marching_line"),
                      Troop::Spearman,
                      1,
                      3,
                      {-3.0F, 0.0F, 1.2F},
                      8,
                      {3.4F, 0.0F, 0.0F});
    line.facing_degrees = 180.0F;
    s.groups = {commander, line};

    auto move = [](float time, QVector3D axes, std::optional<float> yaw = {}) {
      auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
      step.destination = axes;
      step.value = 0;
      step.rpg_view_yaw_degrees = yaw;
      return step;
    };
    s.steps = {

        move(0.30F, {0.0F, 0.0F, 1.0F}, 90.0F),

        move(4.20F, {0.0F, 0.0F, 1.0F}, 0.0F),

        move(6.40F, {0.0F, 0.0F, 1.0F}, 270.0F),
        move(8.40F, {0.0F, 0.0F, 0.0F}),
    };
    add_visual_stability(
        s, {QStringLiteral("rpg_commander"), QStringLiteral("marching_line")});
    s.expectations.push_back(expectation(Expect::RpgFormationSurvivesLensGap,
                                         QStringLiteral("marching_line"),
                                         {},
                                         0.5F));
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("marching_line")));
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_commander_identity_lineup_id),
        QStringLiteral("Commander Identity Lineup"),
        QStringLiteral("Displays all six commanders without bodyguards or supporting "
                       "units for direct silhouette, weapon, scale, color, and "
                       "ancient-dark-fantasy identity review."),
        12.0F,
        {11.2F, 21.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.camera_focus = QVector3D(0.75F, 1.15F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    struct CommanderLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      QVector3D position;
      float facing{};
    };
    const CommanderLineupEntry entries[] = {
        {"scipio",
         Troop::RomanVeteranConsul,
         Nation::RomanRepublic,
         1,
         {-3.0F, 0.0F, 1.7F},
         180.0F},
        {"fabius",
         Troop::RomanLegionOrganizer,
         Nation::RomanRepublic,
         2,
         {0.0F, 0.0F, 1.7F},
         180.0F},
        {"marcellus",
         Troop::RomanFieldCommander,
         Nation::RomanRepublic,
         3,
         {3.0F, 0.0F, 1.7F},
         180.0F},
        {"hannibal",
         Troop::CarthageSwordCommander,
         Nation::Carthage,
         4,
         {-1.5F, 0.0F, -1.7F},
         180.0F},
        {"hanno",
         Troop::CarthageSpearCommander,
         Nation::Carthage,
         5,
         {1.5F, 0.0F, -1.7F},
         180.0F},
        {"hasdrubal",
         Troop::CarthageBowCommander,
         Nation::Carthage,
         6,
         {4.5F, 0.0F, -1.7F},
         180.0F},
    };
    for (auto const& entry : entries) {
      auto commander = group(QString::fromLatin1(entry.group_name),
                             entry.troop,
                             entry.owner,
                             1,
                             entry.position,
                             1);
      commander.nation_id = entry.nation;
      commander.facing_degrees = entry.facing;
      s.groups.push_back(std::move(commander));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_healer_identity_lineup_id),
        QStringLiteral("Healer Identity Lineup"),
        QStringLiteral("Displays the Roman senator-physician, the Carthaginian dark "
                       "mage, and the Iron Sepulcher grave priest from the front and "
                       "from behind for direct robe, silhouette, and faction-identity "
                       "review."),
        12.0F,
        {13.0F, 12.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.5F);
    struct HealerLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float facing{};
    };
    const HealerLineupEntry entries[] = {
        {"roman_front", Troop::Healer, Nation::RomanRepublic, 1, -7.5F, 180.0F},
        {"roman_back", Troop::Healer, Nation::RomanRepublic, 1, -4.5F, 0.0F},
        {"carthage_front", Troop::Healer, Nation::Carthage, 2, -1.5F, 180.0F},
        {"carthage_back", Troop::Healer, Nation::Carthage, 2, 1.5F, 0.0F},
        {"priest_front", Troop::GravePriest, Nation::IronSepulcher, 3, 4.5F, 180.0F},
        {"priest_back", Troop::GravePriest, Nation::IronSepulcher, 3, 7.5F, 0.0F},
    };
    for (auto const& entry : entries) {
      auto healer = group(QString::fromLatin1(entry.group_name),
                          entry.troop,
                          entry.owner,
                          1,
                          {entry.x, 0.0F, 0.0F},
                          1);
      healer.nation_id = entry.nation;
      healer.facing_degrees = entry.facing;
      s.groups.push_back(std::move(healer));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_troop_identity_lineup_id),
        QStringLiteral("Troop Identity Lineup"),
        QStringLiteral("Both nations' infantry, support and worker roles side by "
                       "side so silhouettes, equipment and palettes can be compared "
                       "directly. Rome occupies the near row, Carthage the far one."),
        12.0F,
        {15.0F, 17.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    struct TroopLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float z{};
    };
    const TroopLineupEntry entries[] = {
        {"rome_archer", Troop::Archer, Nation::RomanRepublic, 1, -7.5F, -3.0F},
        {"rome_spearman", Troop::Spearman, Nation::RomanRepublic, 1, -4.5F, -3.0F},
        {"rome_swordsman", Troop::Swordsman, Nation::RomanRepublic, 1, -1.5F, -3.0F},
        {"rome_healer", Troop::Healer, Nation::RomanRepublic, 1, 1.5F, -3.0F},
        {"rome_builder", Troop::Builder, Nation::RomanRepublic, 1, 4.5F, -3.0F},
        {"rome_civilian", Troop::Civilian, Nation::RomanRepublic, 1, 7.5F, -3.0F},
        {"carthage_archer", Troop::Archer, Nation::Carthage, 2, -7.5F, 3.0F},
        {"carthage_spearman", Troop::Spearman, Nation::Carthage, 2, -4.5F, 3.0F},
        {"carthage_swordsman", Troop::Swordsman, Nation::Carthage, 2, -1.5F, 3.0F},
        {"carthage_healer", Troop::Healer, Nation::Carthage, 2, 1.5F, 3.0F},
        {"carthage_builder", Troop::Builder, Nation::Carthage, 2, 4.5F, 3.0F},
        {"carthage_civilian", Troop::Civilian, Nation::Carthage, 2, 7.5F, 3.0F},
    };
    for (auto const& entry : entries) {
      auto troop = group(QString::fromLatin1(entry.group_name),
                         entry.troop,
                         entry.owner,
                         1,
                         {entry.x, 0.0F, entry.z},
                         1);
      troop.nation_id = entry.nation;
      troop.facing_degrees = 180.0F;
      s.groups.push_back(std::move(troop));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_commander_helmet_review_id),
                   QStringLiteral("Commander Helmet Review"),
                   QStringLiteral("Head-height close-up of all six commander helmets, "
                                  "three per rank, so crest, shell profile, cheek "
                                  "pieces and neck guard can be judged at the size a "
                                  "player actually reads them."),
                   6.0F,
                   {5.0F, 8.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.45F, 1.10F, 0.0F);
    struct CommanderHelmetEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      QVector3D position;
      float facing{};
    };
    const CommanderHelmetEntry entries[] = {
        {"fabius",
         Troop::RomanLegionOrganizer,
         Nation::RomanRepublic,
         1,
         {-1.9F, 0.0F, 1.3F},
         180.0F},
        {"scipio",
         Troop::RomanVeteranConsul,
         Nation::RomanRepublic,
         2,
         {0.0F, 0.0F, 1.3F},
         180.0F},
        {"marcellus",
         Troop::RomanFieldCommander,
         Nation::RomanRepublic,
         3,
         {1.9F, 0.0F, 1.3F},
         180.0F},
        {"hanno",
         Troop::CarthageSpearCommander,
         Nation::Carthage,
         4,
         {-0.95F, 0.0F, -1.3F},
         180.0F},
        {"hasdrubal",
         Troop::CarthageBowCommander,
         Nation::Carthage,
         5,
         {0.95F, 0.0F, -1.3F},
         180.0F},
        {"hannibal",
         Troop::CarthageSwordCommander,
         Nation::Carthage,
         6,
         {2.85F, 0.0F, -1.3F},
         180.0F},
    };
    for (auto const& entry : entries) {
      auto commander = group(QString::fromLatin1(entry.group_name),
                             entry.troop,
                             entry.owner,
                             1,
                             entry.position,
                             1);
      commander.nation_id = entry.nation;
      commander.facing_degrees = entry.facing;
      s.groups.push_back(std::move(commander));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_helmet_identity_review_id),
        QStringLiteral("Helmet Identity Review"),
        QStringLiteral("Head-height close-up of the helmets both nations issue, from "
                       "the front and in profile, so bowl shape, brow, cheek guards, "
                       "neck guard and crest can be judged at the size a player "
                       "actually reads them."),
        6.0F,
        {3.2F, 6.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 1.15F, 0.0F);
    struct HelmetReviewEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float z{};
      float facing{};
    };
    const HelmetReviewEntry entries[] = {
        {"line_front", Troop::Archer, Nation::RomanRepublic, 1, -1.8F, 0.0F, 180.0F},
        {"line_profile", Troop::Archer, Nation::RomanRepublic, 1, -0.6F, 0.0F, 270.0F},
        {"work_front", Troop::Builder, Nation::RomanRepublic, 1, 0.6F, 0.0F, 180.0F},
        {"work_profile", Troop::Builder, Nation::RomanRepublic, 1, 1.8F, 0.0F, 270.0F},
    };
    for (auto const& entry : entries) {
      auto unit = group(QString::fromLatin1(entry.group_name),
                        entry.troop,
                        entry.owner,
                        1,
                        {entry.x, 0.0F, entry.z},
                        1);
      unit.nation_id = entry.nation;
      unit.facing_degrees = entry.facing;
      s.groups.push_back(std::move(unit));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_worker_identity_lineup_id),
        QStringLiteral("Worker Identity Lineup"),
        QStringLiteral("Builders and civilians of both nations from the front and "
                       "from behind. Builders carry tools, aprons and arm guards; "
                       "civilians carry household goods and no work gear."),
        12.0F,
        {12.0F, 14.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.5F);
    struct WorkerLineupEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      int owner{};
      float x{};
      float z{};
      float facing{};
    };
    const WorkerLineupEntry entries[] = {
        {"rome_builder_front",
         Troop::Builder,
         Nation::RomanRepublic,
         1,
         -4.5F,
         -3.0F,
         180.0F},
        {"rome_builder_back",
         Troop::Builder,
         Nation::RomanRepublic,
         1,
         -1.5F,
         -3.0F,
         0.0F},
        {"rome_settler_front",
         Troop::Civilian,
         Nation::RomanRepublic,
         1,
         1.5F,
         -3.0F,
         180.0F},
        {"rome_settler_back",
         Troop::Civilian,
         Nation::RomanRepublic,
         1,
         4.5F,
         -3.0F,
         0.0F},
        {"punic_builder_front",
         Troop::Builder,
         Nation::Carthage,
         2,
         -4.5F,
         3.0F,
         180.0F},
        {"punic_builder_back", Troop::Builder, Nation::Carthage, 2, -1.5F, 3.0F, 0.0F},
        {"punic_settler_front",
         Troop::Civilian,
         Nation::Carthage,
         2,
         1.5F,
         3.0F,
         180.0F},
        {"punic_settler_back", Troop::Civilian, Nation::Carthage, 2, 4.5F, 3.0F, 0.0F},
    };
    for (auto const& entry : entries) {
      auto worker = group(QString::fromLatin1(entry.group_name),
                          entry.troop,
                          entry.owner,
                          1,
                          {entry.x, 0.0F, entry.z},
                          1);
      worker.nation_id = entry.nation;
      worker.facing_degrees = entry.facing;
      s.groups.push_back(std::move(worker));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    struct SettlementWorksSpec {
      const char* id;
      const char* label;
      const char* description;
      Nation nation;
      const char* builder_group;
      const char* civilian_group;
      const char* home_group;
      const char* market_group;
      const char* barracks_group;
      const char* tree_prop;
    };
    const SettlementWorksSpec specs[] = {
        {k_roman_settlement_works_id,
         "Roman Settlement Works",
         "Roman builders raise a home and a marketplace and work the tree line "
         "while civilians carry their household goods in to settle. Shows the "
         "worker roles apart: tools and aprons on the builders, bedroll and "
         "pannier on the civilians.",
         Nation::RomanRepublic,
         "roman_builders",
         "roman_settlers",
         "roman_works_home",
         "roman_works_market",
         "roman_works_barracks",
         "olive_tree"},
        {k_carthage_settlement_works_id,
         "Carthaginian Settlement Works",
         "Carthaginian builders raise a home and a marketplace and work the tree "
         "line while civilians carry their household goods in to settle. Shows "
         "the worker roles apart: tools and aprons on the builders, amphora and "
         "satchel on the civilians.",
         Nation::Carthage,
         "punic_builders",
         "punic_settlers",
         "punic_works_home",
         "punic_works_market",
         "punic_works_barracks",
         "olive_tree"},
    };
    for (auto const& spec : specs) {
      auto s = definition(QString::fromLatin1(spec.id),
                          QString::fromLatin1(spec.label),
                          QString::fromLatin1(spec.description),
                          60.0F,
                          {24.0F, 32.0F, 20.0F});
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

      auto builders = group(QString::fromLatin1(spec.builder_group),
                            Troop::Builder,
                            2,
                            3,
                            {-6.0F, 0.0F, 2.0F},
                            1,
                            {3.0F, 0.0F, 0.0F});
      builders.nation_id = spec.nation;
      builders.ai_controlled = true;

      auto settlers = group(QString::fromLatin1(spec.civilian_group),
                            Troop::Civilian,
                            2,
                            3,
                            {-6.0F, 0.0F, 8.0F},
                            1,
                            {3.0F, 0.0F, 0.0F});
      settlers.nation_id = spec.nation;
      settlers.facing_degrees = 180.0F;

      s.groups = {building(QString::fromLatin1(spec.home_group),
                           Game::Units::SpawnType::Home,
                           spec.nation,
                           2,
                           1,
                           {-9.0F, 0.0F, -6.0F}),
                  building(QString::fromLatin1(spec.market_group),
                           Game::Units::SpawnType::Marketplace,
                           spec.nation,
                           2,
                           1,
                           {2.0F, 0.0F, -7.0F}),
                  building(QString::fromLatin1(spec.barracks_group),
                           Game::Units::SpawnType::Barracks,
                           spec.nation,
                           2,
                           1,
                           {12.0F, 0.0F, -8.0F}),
                  std::move(builders),
                  std::move(settlers)};
      for (auto& works_group : s.groups) {
        works_group.ai_controlled = true;
      }

      s.resource_patches = {
          {QString::fromLatin1(spec.tree_prop),
           6,
           {12.0F, 0.0F, -2.0F},
           {0.0F, 0.0F, 2.4F},
           1.15F},
          {QStringLiteral("boulder"), 4, {10.0F, 0.0F, 9.0F}, {2.2F, 0.0F, 0.0F}, 1.1F},
      };

      s.expectations.push_back(expectation(Expect::GroupIsRendered,
                                           QString::fromLatin1(spec.builder_group)));
      s.expectations.push_back(expectation(Expect::GroupIsRendered,
                                           QString::fromLatin1(spec.civilian_group)));
      s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                           QString::fromLatin1(spec.builder_group),
                                           {},
                                           1.0F));
      s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                           QString::fromLatin1(spec.builder_group)));
      s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
      result.push_back(std::move(s));
    }
  }

  {
    auto s = definition(
        QString::fromLatin1(k_healer_lod_probe_id),
        QStringLiteral("Healer LOD Probe"),
        QStringLiteral("Healers and a swordsman at gameplay camera distance with "
                       "production level-of-detail selection, so support units are "
                       "checked for the same reduced-detail coverage as line troops."),
        6.0F,
        {45.0F, 40.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = false;
    s.camera_focus = QVector3D(0.5F, 0.0F, 0.5F);
    struct LodProbeEntry {
      const char* group_name{};
      Troop troop;
      Nation nation;
      QVector3D position;
    };
    const LodProbeEntry entries[] = {
        {"roman_healer", Troop::Healer, Nation::RomanRepublic, {-4.0F, 0.0F, 0.0F}},
        {"carthage_healer", Troop::Healer, Nation::Carthage, {0.0F, 0.0F, 0.0F}},
        {"roman_swordsman",
         Troop::Swordsman,
         Nation::RomanRepublic,
         {4.0F, 0.0F, 0.0F}},
    };
    for (auto const& entry : entries) {
      auto probe = group(
          QString::fromLatin1(entry.group_name), entry.troop, 1, 1, entry.position, 1);
      probe.nation_id = entry.nation;
      s.groups.push_back(std::move(probe));
      s.steps.push_back(
          at(0.05F, Command::Hold, QString::fromLatin1(entry.group_name)));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, QString::fromLatin1(entry.group_name)));
    }
    result.push_back(std::move(s));
  }

  {
    struct DuelSpec {
      const char* id;
      const char* label;
      Troop roman;
      Troop carthaginian;
    };
    constexpr DuelSpec duels[] = {
        {k_commander_sword_duel_id,
         "Sword Commanders: Scipio vs Hannibal",
         Troop::RomanVeteranConsul,
         Troop::CarthageSwordCommander},
        {k_commander_spear_duel_id,
         "Spear Commanders: Fabius vs Hanno",
         Troop::RomanLegionOrganizer,
         Troop::CarthageSpearCommander},
        {k_commander_bow_duel_id,
         "Bow Commanders: Marcellus vs Hasdrubal",
         Troop::RomanFieldCommander,
         Troop::CarthageBowCommander},
    };
    for (auto const& duel : duels) {
      auto s = definition(
          QString::fromLatin1(duel.id),
          QString::fromLatin1(duel.label),
          QStringLiteral("A durable one-on-one commander duel validating distinct "
                         "weapons, authored contact, reactions, attack cadence, and "
                         "readable silhouettes without bodyguards."),
          11.0F,
          {10.5F, 52.0F, 90.0F});
      s.suppress_terrain_scatter = true;
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      auto roman = group(
          QStringLiteral("roman_commander"), duel.roman, 1, 1, {0.0F, 0.0F, -4.5F}, 1);
      auto carthaginian = group(QStringLiteral("carthage_commander"),
                                duel.carthaginian,
                                2,
                                1,
                                {0.0F, 0.0F, 4.5F},
                                1);
      roman.health_override = roman.max_health_override = 1200;
      carthaginian.health_override = carthaginian.max_health_override = 1200;
      s.groups = {roman, carthaginian};
      s.steps = {
          at(0.4F,
             Command::Attack,
             QStringLiteral("roman_commander"),
             QStringLiteral("carthage_commander")),
          at(0.4F,
             Command::Attack,
             QStringLiteral("carthage_commander"),
             QStringLiteral("roman_commander")),
      };
      add_visual_stability(
          s, {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")});
      for (auto const& name :
           {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")}) {
        QString const opponent = name == QStringLiteral("roman_commander")
                                     ? QStringLiteral("carthage_commander")
                                     : QStringLiteral("roman_commander");
        s.expectations.push_back(expectation(Expect::AttackAnimationObserved, name));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, name, {}, 2.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, name, opponent));
        s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      }
      result.push_back(std::move(s));
    }
  }

  {
    struct SignatureDuel {
      const char* id;
      const char* label;
      const char* description;
      Troop roman;
      Troop carthaginian;
    };
    constexpr std::array<SignatureDuel, 3> signature_duels{{
        {k_commander_signature_spear_vs_sword_id,
         "Commander Signatures: Fabius vs Hannibal",
         "Spear against sword. Fabius answers with the braced thrust that rocks "
         "his man back; Hannibal replies with the encircling cut. Long enough for "
         "both signatures to come round more than once.",
         Troop::RomanLegionOrganizer,
         Troop::CarthageSwordCommander},
        {k_commander_signature_sword_vs_bow_id,
         "Commander Signatures: Scipio vs Hasdrubal",
         "Sword against bow at arm's length. Scipio's consular riposte lands "
         "heavy; Hasdrubal keeps loosing hunting shots into the clinch instead "
         "of backing away.",
         Troop::RomanVeteranConsul,
         Troop::CarthageBowCommander},
        {k_commander_signature_bow_vs_spear_id,
         "Commander Signatures: Marcellus vs Hanno",
         "Bow against spear. Marcellus fires point-blank between sword strokes "
         "while Hanno sweeps his spear through the clinch.",
         Troop::RomanFieldCommander,
         Troop::CarthageSpearCommander},
    }};

    for (auto const& duel : signature_duels) {
      auto s = definition(QString::fromLatin1(duel.id),
                          QString::fromLatin1(duel.label),
                          QString::fromLatin1(duel.description),
                          24.0F,
                          {9.0F, 46.0F, 90.0F});
      s.suppress_terrain_scatter = true;
      s.select_spawned_units = false;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.camera_focus = QVector3D(0.0F, 0.9F, 0.0F);

      auto roman = group(
          QStringLiteral("roman_commander"), duel.roman, 1, 1, {0.0F, 0.0F, -4.0F}, 1);
      auto carthaginian = group(QStringLiteral("carthage_commander"),
                                duel.carthaginian,
                                2,
                                1,
                                {0.0F, 0.0F, 4.0F},
                                1);

      roman.health_override = roman.max_health_override = 9000;
      carthaginian.health_override = carthaginian.max_health_override = 9000;
      s.groups = {roman, carthaginian};
      s.steps = {
          at(0.4F,
             Command::Attack,
             QStringLiteral("roman_commander"),
             QStringLiteral("carthage_commander")),
          at(0.4F,
             Command::Attack,
             QStringLiteral("carthage_commander"),
             QStringLiteral("roman_commander")),
      };
      add_visual_stability(
          s, {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")});
      for (auto const& name :
           {QStringLiteral("roman_commander"), QStringLiteral("carthage_commander")}) {
        QString const opponent = name == QStringLiteral("roman_commander")
                                     ? QStringLiteral("carthage_commander")
                                     : QStringLiteral("roman_commander");
        s.expectations.push_back(expectation(Expect::GroupExists, name));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, name, {}, 3.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, name, opponent));
        s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      }
      result.push_back(std::move(s));
    }
  }

  {
    auto s = definition(QString::fromLatin1(k_sword_duel_id),
                        QStringLiteral("Sword Duel"),
                        QStringLiteral("Baseline reciprocal sword attack flow."),
                        8.0F,
                        {9.0F, 42.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Swordsman, 1, 1, {-1.4F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Swordsman, 2, 1, {1.4F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::Attack, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::Attack, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue")));
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("red")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue"),
                                         QStringLiteral("red")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_spear_duel_id),
                        QStringLiteral("Spear Duel"),
                        QStringLiteral("Spear thrust, hit reaction, and recovery."),
                        8.0F,
                        {9.5F, 42.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Spearman, 1, 1, {-1.8F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Spearman, 2, 1, {1.8F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::Attack, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::Attack, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue"),
                                         QStringLiteral("red")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_bow_exchange_id),
                   QStringLiteral("Bow Exchange"),
                   QStringLiteral("Bow draw, release, reload, and pose stability."),
                   10.0F,
                   {18.0F, 45.0F, 35.0F});
    s.groups = {
        group(QStringLiteral("blue_archers"),
              Troop::Archer,
              1,
              1,
              {-8.0F, 0.0F, 0.0F},
              6),
        group(
            QStringLiteral("red_archers"), Troop::Archer, 2, 1, {8.0F, 0.0F, 0.0F}, 6)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_archers"),
                  QStringLiteral("red_archers")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_archers"),
                  QStringLiteral("blue_archers"))};

    s.steps[0].chase = true;
    s.steps[1].chase = true;
    add_visual_stability(
        s, {QStringLiteral("blue_archers"), QStringLiteral("red_archers")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("blue_archers")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("blue_archers"),
                                         QStringLiteral("red_archers")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("red_archers"),
                                         QStringLiteral("blue_archers")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_held_weapon_stances_id),
        QStringLiteral("Held Spear and Bow Stances"),
        QStringLiteral("Spearmen and archers stay fully kneeling with raised weapons "
                       "while combat remains active."),
        7.0F,
        {10.0F, 30.0F, 0.0F});
    s.groups = {
        group(QStringLiteral("held_spear"),
              Troop::Spearman,
              1,
              1,
              {-1.6F, 0.0F, -1.0F},
              1),
        group(QStringLiteral("spear_target"),
              Troop::Civilian,
              2,
              1,
              {-1.6F, 0.0F, 1.1F},
              1),
        group(
            QStringLiteral("held_archer"), Troop::Archer, 1, 1, {1.6F, 0.0F, -2.0F}, 1),
        group(QStringLiteral("archer_target"),
              Troop::Civilian,
              2,
              1,
              {1.6F, 0.0F, 2.0F},
              1),
    };
    s.groups[1].health_override = 5000;
    s.groups[1].max_health_override = 5000;
    s.groups[3].health_override = 5000;
    s.groups[3].max_health_override = 5000;
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("held_spear")),
        at(0.0F, Command::Hold, QStringLiteral("held_archer")),
    };
    add_visual_stability(s,
                         {QStringLiteral("held_spear"), QStringLiteral("held_archer")});
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("held_spear"), {}, 0.0F, 1.5F));
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("held_archer"), {}, 0.0F, 1.5F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("held_spear")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("held_archer")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mounted_charge_id),
        QStringLiteral("Mounted Charge"),
        QStringLiteral("Mounted approach and legitimate impact displacement."),
        12.0F,
        {22.0F, 48.0F, 20.0F});
    s.groups = {group(QStringLiteral("blue_cavalry"),
                      Troop::MountedKnight,
                      1,
                      2,
                      {0.0F, 0.0F, -10.0F},
                      4),
                group(QStringLiteral("red_infantry"),
                      Troop::Swordsman,
                      2,
                      2,
                      {0.0F, 0.0F, 10.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Charge,
                  QStringLiteral("blue_cavalry"),
                  QStringLiteral("red_infantry")),
               when_near(QStringLiteral("blue_cavalry"),
                         QStringLiteral("red_infantry"),
                         4.5F,
                         Command::SetCamera)};
    s.steps.back().camera_distance = 14.0F;
    s.steps.back().camera_angle = 48.0F;
    s.steps.back().camera_yaw = 20.0F;
    add_visual_stability(
        s, {QStringLiteral("blue_cavalry"), QStringLiteral("red_infantry")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("blue_cavalry"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("red_infantry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("red_infantry")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_cavalry"),
                                         QStringLiteral("red_infantry")));
    s.expectations.push_back(expectation(Expect::ChargeImpactPrecedesMeleeLock,
                                         QStringLiteral("blue_cavalry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_braced_spear_charge_id),
        QStringLiteral("Charge Into Braced Spears"),
        QStringLiteral("Held spears inflict catastrophic losses before melee lock."),
        12.0F,
        {22.0F, 48.0F, 20.0F});
    s.groups = {group(QStringLiteral("cavalry"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {0.0F, 0.0F, -10.0F},
                      4),
                group(QStringLiteral("braced_spears"),
                      Troop::Spearman,
                      2,
                      1,
                      {0.0F, 0.0F, 10.0F},
                      4)};
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("braced_spears")),
               at(0.0F,
                  Command::Charge,
                  QStringLiteral("cavalry"),
                  QStringLiteral("braced_spears")),
               when_near(QStringLiteral("cavalry"),
                         QStringLiteral("braced_spears"),
                         4.5F,
                         Command::SetCamera)};
    s.steps.back().camera_distance = 14.0F;
    s.steps.back().camera_angle = 48.0F;
    s.steps.back().camera_yaw = 20.0F;
    add_visual_stability(s,
                         {QStringLiteral("cavalry"), QStringLiteral("braced_spears")});
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("cavalry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("cavalry")));
    s.expectations.push_back(expectation(Expect::NoLaunchedCasualtyObserved,
                                         QStringLiteral("braced_spears")));
    s.expectations.push_back(
        expectation(Expect::ChargeImpactPrecedesMeleeLock, QStringLiteral("cavalry")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_elephant_trample_id),
                   QStringLiteral("Elephant Trample Impact"),
                   QStringLiteral("A war elephant tramples infantry and launches fresh "
                                  "casualties away from the impact."),
                   10.0F,
                   {18.0F, 47.0F, 18.0F});
    s.groups = {
        group(
            QStringLiteral("elephant"), Troop::Elephant, 1, 1, {0.0F, 0.0F, -6.0F}, 1),
        group(
            QStringLiteral("infantry"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.0F}, 8)};

    s.groups[1].health_override = 150;
    s.groups[1].max_health_override = 150;
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("elephant"),
                  QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("infantry")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("elephant")));
    s.expectations.push_back(expectation(
        Expect::FormationBodyOverlapObserved, QStringLiteral("elephant"), {}, 1.10F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("elephant"),
                                         QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::NoActiveCombatAtEnd, QStringLiteral("elephant")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_catapult_impact_id),
        QStringLiteral("Catapult Infantry Impact"),
        QStringLiteral("A catapult stone hits an infantry formation and launches "
                       "fresh casualties from the impact."),
        12.0F,
        {22.0F, 50.0F, 12.0F});
    s.groups = {
        group(
            QStringLiteral("catapult"), Troop::Catapult, 1, 1, {0.0F, 0.0F, -9.0F}, 1),
        group(
            QStringLiteral("infantry"), Troop::Spearman, 2, 1, {0.0F, 0.0F, 4.0F}, 8)};
    s.groups[1].health_override = 240;
    s.groups[1].max_health_override = 240;
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("infantry")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("catapult"),
                  QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("infantry")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("catapult")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::LaunchedCasualtyObserved, QStringLiteral("infantry")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("catapult"),
                                         QStringLiteral("infantry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_ballista_impact_id),
        QStringLiteral("Ballista Bolt Impact"),
        QStringLiteral("A ballista completes its loading stroke, releases a visible "
                       "heavy bolt, and damages infantry only when the bolt arrives."),
        7.0F,
        {14.0F, 32.0F, 8.0F});
    s.groups = {
        group(
            QStringLiteral("ballista"), Troop::Ballista, 1, 1, {0.0F, 0.0F, -7.0F}, 1),
        group(
            QStringLiteral("infantry"), Troop::Spearman, 2, 1, {0.0F, 0.0F, 3.0F}, 6)};
    s.groups[1].health_override = 900;
    s.groups[1].max_health_override = 900;
    s.select_spawned_units = false;
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("infantry")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("ballista"),
                  QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("infantry")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("ballista")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("ballista"),
                                         QStringLiteral("infantry")));
    s.expectations.push_back(
        expectation(Expect::HitReactionObserved, QStringLiteral("infantry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_melee_assault_id),
        QStringLiteral("Structure Melee Assault"),
        QStringLiteral("Sword, spear, and elephant lanes close all the way to "
                       "visible facades. Infantry chips structures slowly while "
                       "the elephant produces heavy localized impacts."),
        13.0F,
        {23.0F, 50.0F, 4.0F});
    auto swords = group(QStringLiteral("structure_swords"),
                        Troop::Swordsman,
                        1,
                        1,
                        {-8.0F, 0.0F, -6.0F},
                        6);
    auto spears = group(QStringLiteral("structure_spears"),
                        Troop::Spearman,
                        1,
                        1,
                        {0.0F, 0.0F, -6.0F},
                        6);
    auto elephant = group(QStringLiteral("structure_elephant"),
                          Troop::Elephant,
                          1,
                          1,
                          {8.0F, 0.0F, -6.0F},
                          1);
    auto sword_wall = building(QStringLiteral("sword_wall"),
                               Game::Units::SpawnType::WallSegment,
                               Nation::Carthage,
                               2,
                               1,
                               {-8.0F, 0.0F, 2.0F});
    auto spear_wall = building(QStringLiteral("spear_wall"),
                               Game::Units::SpawnType::WallSegment,
                               Nation::Carthage,
                               2,
                               1,
                               {0.0F, 0.0F, 2.0F});
    auto elephant_home = building(QStringLiteral("elephant_home"),
                                  Game::Units::SpawnType::Home,
                                  Nation::Carthage,
                                  2,
                                  1,
                                  {8.0F, 0.0F, 2.0F});
    swords.health_override = swords.max_health_override = 1400;
    spears.health_override = spears.max_health_override = 1400;
    elephant.health_override = elephant.max_health_override = 1800;
    sword_wall.health_override = sword_wall.max_health_override = 1600;
    spear_wall.health_override = spear_wall.max_health_override = 1600;
    elephant_home.health_override = elephant_home.max_health_override = 3200;
    s.groups = {swords, spears, elephant, sword_wall, spear_wall, elephant_home};
    s.steps = {
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_swords"),
           QStringLiteral("sword_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_spears"),
           QStringLiteral("spear_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_elephant"),
           QStringLiteral("elephant_home")),
    };
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    add_visual_stability(
        s, {QStringLiteral("structure_swords"), QStringLiteral("structure_spears")});
    for (auto const& name : {QStringLiteral("sword_wall"),
                             QStringLiteral("spear_wall"),
                             QStringLiteral("elephant_home")}) {
      s.expectations.push_back(expectation(Expect::GroupExists, name));
      s.expectations.push_back(expectation(Expect::GroupHealthReduced, name, {}, 1.0F));
      s.expectations.push_back(expectation(Expect::StructureDamageCueObserved, name));
    }
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("structure_swords")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("structure_spears")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("structure_elephant")));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("structure_swords"),
                                         QStringLiteral("sword_wall")));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("structure_spears"),
                                         QStringLiteral("spear_wall")));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("structure_elephant"),
                                         QStringLiteral("elephant_home")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_projectile_assault_id),
        QStringLiteral("Structure Projectile Assault"),
        QStringLiteral("Arrow, ballista, and catapult lanes strike visible facades. "
                       "Arrows remain cosmetic while siege projectiles cause "
                       "localized damage cues."),
        12.0F,
        {24.0F, 50.0F, 4.0F});
    auto archers = group(QStringLiteral("structure_archers"),
                         Troop::Archer,
                         1,
                         1,
                         {-8.0F, 0.0F, -8.0F},
                         8);
    auto ballista = group(QStringLiteral("structure_ballista"),
                          Troop::Ballista,
                          1,
                          1,
                          {0.0F, 0.0F, -8.0F},
                          1);
    auto catapult = group(QStringLiteral("structure_catapult"),
                          Troop::Catapult,
                          1,
                          1,
                          {8.0F, 0.0F, -8.0F},
                          1);
    auto arrow_wall = building(QStringLiteral("arrow_wall"),
                               Game::Units::SpawnType::WallSegment,
                               Nation::Carthage,
                               2,
                               1,
                               {-8.0F, 0.0F, 4.0F});
    auto bolt_wall = building(QStringLiteral("bolt_wall"),
                              Game::Units::SpawnType::WallSegment,
                              Nation::Carthage,
                              2,
                              1,
                              {0.0F, 0.0F, 4.0F});
    auto stone_home = building(QStringLiteral("stone_home"),
                               Game::Units::SpawnType::Home,
                               Nation::Carthage,
                               2,
                               1,
                               {8.0F, 0.0F, 4.0F});
    arrow_wall.health_override = arrow_wall.max_health_override = 1400;
    bolt_wall.health_override = bolt_wall.max_health_override = 1800;
    stone_home.health_override = stone_home.max_health_override = 3200;
    s.groups = {archers, ballista, catapult, arrow_wall, bolt_wall, stone_home};
    s.steps = {
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_archers"),
           QStringLiteral("arrow_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_ballista"),
           QStringLiteral("bolt_wall")),
        at(0.2F,
           Command::Attack,
           QStringLiteral("structure_catapult"),
           QStringLiteral("stone_home")),
    };
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("structure_archers"),
                                         QStringLiteral("arrow_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthUnchanged, QStringLiteral("arrow_wall")));
    for (auto const& pair :
         {std::pair{QStringLiteral("structure_ballista"), QStringLiteral("bolt_wall")},
          std::pair{QStringLiteral("structure_catapult"),
                    QStringLiteral("stone_home")}}) {
      s.expectations.push_back(
          expectation(Expect::ProjectileImpactSynchronized, pair.first, pair.second));
      s.expectations.push_back(
          expectation(Expect::GroupHealthReduced, pair.second, {}, 1.0F));
      s.expectations.push_back(
          expectation(Expect::StructureDamageCueObserved, pair.second));
    }
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("arrow_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("bolt_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("stone_home")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_flaming_siege_id),
        QStringLiteral("Structure Flaming Siege"),
        QStringLiteral("A catapult lobs flaming stones into a home while swordsmen "
                       "hack at a wall beside it. Only the structure taking "
                       "incendiary shot catches fire; the melee lane produces dust "
                       "and rubble alone."),
        16.0F,
        {24.0F, 50.0F, 6.0F});
    auto catapult = group(QStringLiteral("fire_catapult"),
                          Troop::Catapult,
                          1,
                          1,
                          {6.0F, 0.0F, -10.0F},
                          1);
    auto swords = group(
        QStringLiteral("wall_swords"), Troop::Swordsman, 1, 1, {-8.0F, 0.0F, -4.0F}, 6);
    auto burning_home = building(QStringLiteral("burning_home"),
                                 Game::Units::SpawnType::Home,
                                 Nation::Carthage,
                                 2,
                                 1,
                                 {6.0F, 0.0F, 4.0F});
    auto chipped_wall = building(QStringLiteral("chipped_wall"),
                                 Game::Units::SpawnType::WallSegment,
                                 Nation::Carthage,
                                 2,
                                 1,
                                 {-8.0F, 0.0F, 2.0F});
    swords.health_override = swords.max_health_override = 1400;
    burning_home.health_override = burning_home.max_health_override = 3200;
    chipped_wall.health_override = chipped_wall.max_health_override = 1600;
    s.groups = {catapult, swords, burning_home, chipped_wall};
    s.steps = {at(0.2F,
                  Command::Attack,
                  QStringLiteral("fire_catapult"),
                  QStringLiteral("burning_home")),
               at(0.2F,
                  Command::Attack,
                  QStringLiteral("wall_swords"),
                  QStringLiteral("chipped_wall"))};
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::FlamingProjectileObserved,
                                         QStringLiteral("fire_catapult"),
                                         QStringLiteral("burning_home")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("fire_catapult"),
                                         QStringLiteral("burning_home")));
    s.expectations.push_back(
        expectation(Expect::StructureFireObserved, QStringLiteral("burning_home")));
    s.expectations.push_back(expectation(Expect::StructureDamageCueObserved,
                                         QStringLiteral("burning_home")));
    s.expectations.push_back(expectation(
        Expect::GroupHealthReduced, QStringLiteral("burning_home"), {}, 1.0F));
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("wall_swords"),
                                         QStringLiteral("chipped_wall")));
    s.expectations.push_back(expectation(Expect::StructureDamageCueObserved,
                                         QStringLiteral("chipped_wall")));
    s.expectations.push_back(
        expectation(Expect::NoStructureFireObserved, QStringLiteral("chipped_wall")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_structure_melee_destruction_id),
        QStringLiteral("Structure Melee Destruction"),
        QStringLiteral("Swordsmen dismantle a weakened wall segment by hand. The "
                       "collapse is dust and debris from first blow to last, with "
                       "no flame anywhere in the sequence."),
        18.0F,
        {20.0F, 48.0F, 8.0F});
    auto swords = group(QStringLiteral("demolition_swords"),
                        Troop::Swordsman,
                        1,
                        2,
                        {-2.6F, 0.0F, -4.0F},
                        8);
    auto doomed_wall = building(QStringLiteral("doomed_wall"),
                                Game::Units::SpawnType::WallSegment,
                                Nation::Carthage,
                                2,
                                1,
                                {0.0F, 0.0F, 2.0F});
    swords.health_override = swords.max_health_override = 1400;
    doomed_wall.health_override = doomed_wall.max_health_override = 120;
    s.groups = {swords, doomed_wall};
    s.steps = {at(0.2F,
                  Command::Attack,
                  QStringLiteral("demolition_swords"),
                  QStringLiteral("doomed_wall"))};
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::StructureFacadeContactObserved,
                                         QStringLiteral("demolition_swords"),
                                         QStringLiteral("doomed_wall")));
    s.expectations.push_back(
        expectation(Expect::StructureDamageCueObserved, QStringLiteral("doomed_wall")));
    s.expectations.push_back(
        expectation(Expect::NoStructureFireObserved, QStringLiteral("doomed_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("doomed_wall")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_catapult_ammunition_retarget_id),
        QStringLiteral("Catapult Ammunition Retarget"),
        QStringLiteral("A catapult opens on a barracks with flaming shot, then is "
                       "swung onto infantry. The loaded round changes with the "
                       "target: plain stone for troops, fire for the structure."),
        22.0F,
        {24.0F, 50.0F, 10.0F});
    auto catapult = group(QStringLiteral("swing_catapult"),
                          Troop::Catapult,
                          1,
                          1,
                          {0.0F, 0.0F, -11.0F},
                          1);
    auto infantry = group(
        QStringLiteral("swing_infantry"), Troop::Spearman, 2, 1, {8.0F, 0.0F, 2.0F}, 8);
    auto barracks = building(QStringLiteral("swing_barracks"),
                             Game::Units::SpawnType::Barracks,
                             Nation::Carthage,
                             2,
                             1,
                             {-4.0F, 0.0F, 3.0F});
    infantry.health_override = infantry.max_health_override = 1600;
    barracks.health_override = barracks.max_health_override = 3200;
    s.groups = {catapult, infantry, barracks};
    s.steps = {at(0.2F, Command::Hold, QStringLiteral("swing_infantry")),
               at(0.2F,
                  Command::Attack,
                  QStringLiteral("swing_catapult"),
                  QStringLiteral("swing_barracks")),
               at(9.0F,
                  Command::Attack,
                  QStringLiteral("swing_catapult"),
                  QStringLiteral("swing_infantry"))};
    s.select_spawned_units = false;
    s.suppress_ui_overlays = true;
    s.suppress_spawn_anchor = true;
    s.expectations.push_back(expectation(Expect::FlamingProjectileObserved,
                                         QStringLiteral("swing_catapult"),
                                         QStringLiteral("swing_barracks")));
    s.expectations.push_back(expectation(Expect::NoFlamingProjectileObserved,
                                         QStringLiteral("swing_catapult"),
                                         QStringLiteral("swing_infantry")));
    s.expectations.push_back(
        expectation(Expect::StructureFireObserved, QStringLiteral("swing_barracks")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("swing_catapult"),
                                         QStringLiteral("swing_infantry")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_mounted_sword_duel_id),
                   QStringLiteral("Mounted Sword Duel"),
                   QStringLiteral("Mounted sword chamber, full swing, and recovery."),
                   10.0F,
                   {13.0F, 43.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {-3.0F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("red_knights"),
                      Troop::MountedKnight,
                      2,
                      1,
                      {3.0F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_knights"),
                  QStringLiteral("red_knights")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_knights"),
                  QStringLiteral("blue_knights"))};
    add_visual_stability(
        s, {QStringLiteral("blue_knights"), QStringLiteral("red_knights")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_knights")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_knights"),
                                         QStringLiteral("red_knights")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_spear_duel_id),
                        QStringLiteral("Mounted Spear Duel"),
                        QStringLiteral("Mounted couch, downward thrust, and recovery."),
                        10.0F,
                        {14.0F, 44.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_spears"),
                      Troop::HorseSpearman,
                      1,
                      1,
                      {-3.8F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("red_spears"),
                      Troop::HorseSpearman,
                      2,
                      1,
                      {3.8F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_spears"),
                  QStringLiteral("red_spears")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_spears"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_spears"), QStringLiteral("red_spears")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_spears")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("blue_spears"),
                                         QStringLiteral("red_spears")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mounted_bow_exchange_id),
        QStringLiteral("Mounted Bow Exchange"),
        QStringLiteral("Mounted bow raise, draw, release, and riding stability."),
        12.0F,
        {20.0F, 46.0F, 30.0F});
    s.groups = {group(QStringLiteral("blue_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {-9.0F, 0.0F, 0.0F},
                      6),
                group(QStringLiteral("red_archers"),
                      Troop::HorseArcher,
                      2,
                      1,
                      {9.0F, 0.0F, 0.0F},
                      6)};
    s.steps = {at(0.0F,
                  Command::Attack,
                  QStringLiteral("blue_archers"),
                  QStringLiteral("red_archers")),
               at(0.0F,
                  Command::Attack,
                  QStringLiteral("red_archers"),
                  QStringLiteral("blue_archers"))};
    add_visual_stability(
        s, {QStringLiteral("blue_archers"), QStringLiteral("red_archers")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("blue_archers")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("blue_archers")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_infantry_locomotion_matrix_id),
        QStringLiteral("Infantry Locomotion Matrix"),
        QStringLiteral(
            "Archers, swordsmen, and spearmen start, march, stop, and reverse."),
        13.0F,
        {26.0F, 48.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-6.0F, 0.0F, -9.0F}, 8),
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -9.0F}, 8),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {6.0F, 0.0F, -9.0F}, 8)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("archers")),
               at(0.5F, Command::FormationMove, QStringLiteral("swords")),
               at(0.5F, Command::FormationMove, QStringLiteral("spears")),
               at(6.0F, Command::Stop, QStringLiteral("archers")),
               at(6.0F, Command::Stop, QStringLiteral("swords")),
               at(6.0F, Command::Stop, QStringLiteral("spears")),
               at(8.0F, Command::FormationMove, QStringLiteral("archers")),
               at(8.0F, Command::FormationMove, QStringLiteral("swords")),
               at(8.0F, Command::FormationMove, QStringLiteral("spears"))};
    for (int lane = 0; lane < 3; ++lane) {
      s.steps[static_cast<std::size_t>(lane)].destination = {
          -6.0F + 6.0F * static_cast<float>(lane), 0.0F, 8.0F};
      s.steps[static_cast<std::size_t>(lane + 6)].destination = {
          -6.0F + 6.0F * static_cast<float>(lane), 0.0F, -16.0F};
    }
    add_visual_stability(s,
                         {QStringLiteral("archers"),
                          QStringLiteral("swords"),
                          QStringLiteral("spears")});
    for (auto const& name : {QStringLiteral("archers"),
                             QStringLiteral("swords"),
                             QStringLiteral("spears")}) {
      s.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, {}, 0.45F));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(
          expectation(Expect::FormationOrderPreserved, name, {}, 0.8F));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_locomotion_matrix_id),
                        QStringLiteral("Mounted Locomotion Matrix"),
                        QStringLiteral("Horse archers, knights, and horse spearmen "
                                       "start, ride, stop, and reverse."),
                        13.0F,
                        {30.0F, 50.0F, 30.0F});
    s.groups = {group(QStringLiteral("horse_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {-7.0F, 0.0F, -10.0F},
                      6),
                group(QStringLiteral("knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {0.0F, 0.0F, -10.0F},
                      6),
                group(QStringLiteral("horse_spears"),
                      Troop::HorseSpearman,
                      1,
                      1,
                      {7.0F, 0.0F, -10.0F},
                      6)};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("horse_archers")),
               at(0.5F, Command::FormationMove, QStringLiteral("knights")),
               at(0.5F, Command::FormationMove, QStringLiteral("horse_spears")),
               at(6.0F, Command::Stop, QStringLiteral("horse_archers")),
               at(6.0F, Command::Stop, QStringLiteral("knights")),
               at(6.0F, Command::Stop, QStringLiteral("horse_spears")),
               at(8.0F, Command::FormationMove, QStringLiteral("horse_archers")),
               at(8.0F, Command::FormationMove, QStringLiteral("knights")),
               at(8.0F, Command::FormationMove, QStringLiteral("horse_spears"))};
    for (int lane = 0; lane < 3; ++lane) {
      s.steps[static_cast<std::size_t>(lane)].destination = {
          -7.0F + 7.0F * static_cast<float>(lane), 0.0F, 9.0F};
      s.steps[static_cast<std::size_t>(lane + 6)].destination = {
          -7.0F + 7.0F * static_cast<float>(lane), 0.0F, -20.0F};
    }
    add_visual_stability(s,
                         {QStringLiteral("horse_archers"),
                          QStringLiteral("knights"),
                          QStringLiteral("horse_spears")});
    for (auto const& name : {QStringLiteral("horse_archers"),
                             QStringLiteral("knights"),
                             QStringLiteral("horse_spears")}) {
      s.expectations.push_back(
          expectation(Expect::AllGroupsRespondWithin, name, {}, 0.45F));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(
          expectation(Expect::FormationOrderPreserved, name, {}, 0.8F));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_elephant_locomotion_matrix_id),
                        QStringLiteral("Elephant Locomotion Matrix"),
                        QStringLiteral("War elephants start, travel, stop, and reverse "
                                       "using the production authored skin."),
                        13.0F,
                        {24.0F, 48.0F, 28.0F});
    s.groups = {group(QStringLiteral("elephants"),
                      Troop::Elephant,
                      2,
                      3,
                      {0.0F, 0.0F, -10.0F},
                      1,
                      {5.5F, 0.0F, 0.0F})};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("elephants")),
               at(6.0F, Command::Stop, QStringLiteral("elephants")),
               at(8.0F, Command::FormationMove, QStringLiteral("elephants"))};
    s.steps[0].destination = {0.0F, 0.0F, 9.0F};
    s.steps[2].destination = {0.0F, 0.0F, -20.0F};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("elephants")));
    s.expectations.push_back(
        expectation(Expect::NoRootTeleport, QStringLiteral("elephants")));
    s.expectations.push_back(
        expectation(Expect::MovementIsContinuous, QStringLiteral("elephants")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("elephants"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("elephants"), {}, 0.8F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_infantry_damage_matrix_id),
                        QStringLiteral("Infantry Damage Matrix"),
                        QStringLiteral("Archers, swordsmen, and spearmen absorb a hit "
                                       "and enter stable death sequences."),
                        10.0F,
                        {19.0F, 46.0F, 30.0F});
    auto archers =
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-5.0F, 0.0F, 0.0F}, 1);
    auto swords =
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    auto spears =
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {5.0F, 0.0F, 0.0F}, 1);
    for (auto* troop : {&archers, &swords, &spears}) {
      troop->nation_id = Nation::Carthage;
      troop->owner_id = 2;
      troop->facing_degrees = 180.0F;
      troop->health_override = 30;
      troop->max_health_override = 30;
    }
    archers.origin.setZ(1.8F);
    swords.origin.setZ(1.8F);
    spears.origin.setZ(1.8F);
    s.groups = {archers,
                swords,
                spears,
                group(QStringLiteral("attack_archers"),
                      Troop::Swordsman,
                      1,
                      1,
                      {-5.0F, 0.0F, -1.8F},
                      1),
                group(QStringLiteral("attack_swords"),
                      Troop::Swordsman,
                      1,
                      1,
                      {0.0F, 0.0F, -1.8F},
                      1),
                group(QStringLiteral("attack_spears"),
                      Troop::Swordsman,
                      1,
                      1,
                      {5.0F, 0.0F, -1.8F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_archers"),
                  QStringLiteral("archers")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_swords"),
                  QStringLiteral("swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_spears"),
                  QStringLiteral("spears"))};
    add_visual_stability(s,
                         {QStringLiteral("archers"),
                          QStringLiteral("swords"),
                          QStringLiteral("spears")});
    for (auto const& name : {QStringLiteral("archers"),
                             QStringLiteral("swords"),
                             QStringLiteral("spears")}) {
      s.expectations.push_back(expectation(Expect::DeathAnimationObserved, name));
      s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
    }
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_mounted_damage_matrix_id),
                   QStringLiteral("Mounted Damage Matrix"),
                   QStringLiteral("Horse archers, knights, and horse spearmen absorb a "
                                  "hit and dismount through stable death sequences."),
                   10.0F,
                   {23.0F, 48.0F, 30.0F});
    auto archers = group(QStringLiteral("horse_archers"),
                         Troop::HorseArcher,
                         1,
                         1,
                         {-6.0F, 0.0F, 0.0F},
                         1);
    auto knights = group(
        QStringLiteral("knights"), Troop::MountedKnight, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    auto spears = group(QStringLiteral("horse_spears"),
                        Troop::HorseSpearman,
                        1,
                        1,
                        {6.0F, 0.0F, 0.0F},
                        1);
    for (auto* troop : {&archers, &knights, &spears}) {
      troop->nation_id = Nation::Carthage;
      troop->owner_id = 2;
      troop->facing_degrees = 180.0F;
      troop->health_override = 40;
      troop->max_health_override = 40;
    }
    archers.origin.setZ(2.4F);
    knights.origin.setZ(2.4F);
    spears.origin.setZ(2.4F);
    s.groups = {archers,
                knights,
                spears,
                group(QStringLiteral("attack_archers"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {-6.0F, 0.0F, -2.4F},
                      1),
                group(QStringLiteral("attack_knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {0.0F, 0.0F, -2.4F},
                      1),
                group(QStringLiteral("attack_spears"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {6.0F, 0.0F, -2.4F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_archers"),
                  QStringLiteral("horse_archers")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_knights"),
                  QStringLiteral("knights")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("attack_spears"),
                  QStringLiteral("horse_spears"))};
    add_visual_stability(s,
                         {QStringLiteral("horse_archers"),
                          QStringLiteral("knights"),
                          QStringLiteral("horse_spears")});
    for (auto const& name : {QStringLiteral("horse_archers"),
                             QStringLiteral("knights"),
                             QStringLiteral("horse_spears")}) {
      s.expectations.push_back(expectation(Expect::DeathAnimationObserved, name));
      s.expectations.push_back(expectation(Expect::HitReactionObserved, name));
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
    }
    result.push_back(std::move(s));
  }

  struct ActionTransitionSpec {
    const char* id;
    const char* label;
    Troop troop;
    bool mounted;
  };
  for (auto const& spec : {ActionTransitionSpec{k_archer_action_transition_id,
                                                "Archer Action Transition",
                                                Troop::Archer,
                                                false},
                           ActionTransitionSpec{k_swordsman_action_transition_id,
                                                "Swordsman Action Transition",
                                                Troop::Swordsman,
                                                false},
                           ActionTransitionSpec{k_spearman_action_transition_id,
                                                "Spearman Action Transition",
                                                Troop::Spearman,
                                                false},
                           ActionTransitionSpec{k_horse_archer_action_transition_id,
                                                "Horse Archer Action Transition",
                                                Troop::HorseArcher,
                                                true},
                           ActionTransitionSpec{k_mounted_knight_action_transition_id,
                                                "Mounted Knight Action Transition",
                                                Troop::MountedKnight,
                                                true},
                           ActionTransitionSpec{k_horse_spearman_action_transition_id,
                                                "Horse Spearman Action Transition",
                                                Troop::HorseSpearman,
                                                true}}) {
    auto s = definition(
        QString::fromLatin1(spec.id),
        QString::fromLatin1(spec.label),
        QStringLiteral("Move, acquire one target, complete the authored attack and "
                       "recovery, then return to player-controlled locomotion."),
        10.0F,
        {spec.mounted ? 15.0F : 12.0F, 46.0F, 28.0F});
    auto target =
        group(QStringLiteral("target"), Troop::Healer, 2, 1, {0.0F, 0.0F, 2.0F}, 1);
    target.health_override = 8;
    target.max_health_override = 8;
    s.groups = {group(QStringLiteral("actor"),
                      spec.troop,
                      1,
                      1,
                      {0.0F, 0.0F, spec.mounted ? -9.0F : -8.0F},
                      1),
                target};
    s.steps = {at(0.0F, Command::Move, QStringLiteral("actor")),
               at(2.0F,
                  Command::AttackMove,
                  QStringLiteral("actor"),
                  QStringLiteral("target")),
               when_destroyed(QStringLiteral("target"),
                              Command::FormationMove,
                              QStringLiteral("actor"),
                              {})};
    s.steps[0].destination = {0.0F, 0.0F, -3.0F};
    s.steps[2].destination = {0.0F, 0.0F, -8.0F};
    add_visual_stability(s, {QStringLiteral("actor")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::AttackRecoveryObserved, QStringLiteral("actor")));
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("actor")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_melee_lock_id),
        QStringLiteral("Melee Lock"),
        QStringLiteral("Reciprocal melee-lock ownership and animation flow."),
        7.0F,
        {8.0F, 40.0F, 28.0F});
    s.groups = {
        group(QStringLiteral("blue"), Troop::Swordsman, 1, 1, {-0.8F, 0.0F, 0.0F}, 1),
        group(QStringLiteral("red"), Troop::Swordsman, 2, 1, {0.8F, 0.0F, 0.0F}, 1)};
    s.steps = {
        at(0.0F, Command::MeleeLock, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::MeleeLock, QStringLiteral("red"), QStringLiteral("blue"))};
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_chase_to_attack_id),
        QStringLiteral("Chase To Attack"),
        QStringLiteral("Out-of-range attacker chases into a visible strike."),
        12.0F,
        {17.0F, 46.0F, 20.0F});
    s.groups = {
        group(QStringLiteral("attacker"),
              Troop::Swordsman,
              1,
              1,
              {0.0F, 0.0F, -10.0F},
              6),
        group(
            QStringLiteral("defender"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 4.0F}, 6)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("defender"))};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("defender")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("attacker"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("defender")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_attack_to_chase_id),
                   QStringLiteral("Attack To Chase"),
                   QStringLiteral("Target disengages and attacker returns to chase."),
                   10.0F,
                   {14.0F, 42.0F, 28.0F});
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 4),
        group(QStringLiteral("runner"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 1.6F}, 4)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("runner")),
               at(0.85F, Command::Move, QStringLiteral("runner"))};
    s.steps.back().destination = {0.0F, 0.0F, 10.0F};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("runner")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("runner"), {}, 0.45F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_target_death_id),
                   QStringLiteral("Target Death"),
                   QStringLiteral("Normal damage path into a stable death sequence."),
                   8.0F,
                   {10.0F, 42.0F, 28.0F});
    auto target =
        group(QStringLiteral("target"), Troop::Healer, 2, 1, {0.0F, 0.0F, 1.8F}, 1);
    target.health_override = 6;
    target.max_health_override = 6;
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 1),
        target};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("target"))};
    add_visual_stability(s, {QStringLiteral("attacker"), QStringLiteral("target")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("target")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_combat_feedback_capture_id),
        QStringLiteral("Combat Feedback Capture"),
        QStringLiteral("Every kind of hit the player has to read at once: a melee "
                       "lane, an archer volley that connects, a volley loosed at "
                       "riders who ride out of it and miss, a ballista chipping a "
                       "wall, and a killing blow. Overlays are kept in the capture "
                       "so the target lock rings, incoming-attacker chevrons and "
                       "projectile relation tints can be reviewed."),
        12.0F,
        {30.0F, 50.0F, 8.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    auto swords = group(
        QStringLiteral("swords"), Troop::Swordsman, 1, 1, {-10.0F, 0.0F, -4.0F}, 3);
    auto sword_targets = group(QStringLiteral("sword_targets"),
                               Troop::Swordsman,
                               2,
                               1,
                               {-10.0F, 0.0F, 3.0F},
                               3);
    auto archers =
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {-2.0F, 0.0F, -9.0F}, 4);
    auto archer_targets = group(QStringLiteral("archer_targets"),
                                Troop::Spearman,
                                2,
                                1,
                                {-2.0F, 0.0F, 4.0F},
                                3);
    auto enemy_archers = group(
        QStringLiteral("enemy_archers"), Troop::Archer, 2, 1, {5.0F, 0.0F, 6.0F}, 3);
    auto riders = group(
        QStringLiteral("riders"), Troop::HorseArcher, 1, 1, {5.0F, 0.0F, -3.0F}, 2);
    auto ballista = group(
        QStringLiteral("ballista"), Troop::Ballista, 1, 1, {11.0F, 0.0F, -9.0F}, 1);
    auto wall = building(QStringLiteral("wall"),
                         Game::Units::SpawnType::WallSegment,
                         Nation::Carthage,
                         2,
                         1,
                         {12.0F, 0.0F, 4.0F});
    auto killer = group(
        QStringLiteral("killer"), Troop::Swordsman, 1, 1, {-15.0F, 0.0F, -2.0F}, 1);
    auto victim =
        group(QStringLiteral("victim"), Troop::Healer, 2, 1, {-15.0F, 0.0F, 1.6F}, 1);
    swords.health_override = swords.max_health_override = 900;
    sword_targets.health_override = sword_targets.max_health_override = 900;
    archer_targets.health_override = archer_targets.max_health_override = 900;
    riders.health_override = riders.max_health_override = 900;
    wall.health_override = wall.max_health_override = 1400;
    victim.health_override = victim.max_health_override = 6;
    s.groups = {swords,
                sword_targets,
                archers,
                archer_targets,
                enemy_archers,
                riders,
                ballista,
                wall,
                killer,
                victim};
    s.steps = {
        at(0.1F,
           Command::Attack,
           QStringLiteral("swords"),
           QStringLiteral("sword_targets")),
        at(0.1F,
           Command::Attack,
           QStringLiteral("archers"),
           QStringLiteral("archer_targets")),
        at(0.1F,
           Command::Attack,
           QStringLiteral("enemy_archers"),
           QStringLiteral("riders")),
        at(0.1F, Command::Attack, QStringLiteral("ballista"), QStringLiteral("wall")),
        at(0.1F,
           Command::AttackMove,
           QStringLiteral("killer"),
           QStringLiteral("victim")),
        at(0.9F, Command::Run, QStringLiteral("riders")),
    };
    s.steps.back().destination = {16.0F, 0.0F, -14.0F};
    s.capture_ui_overlays = true;
    add_visual_stability(s,
                         {QStringLiteral("swords"), QStringLiteral("sword_targets")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("swords")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("sword_targets")));
    s.expectations.push_back(expectation(
        Expect::GroupHealthReduced, QStringLiteral("sword_targets"), {}, 1.0F));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("archers"),
                                         QStringLiteral("archer_targets")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("archers"),
                                         QStringLiteral("archer_targets")));
    s.expectations.push_back(expectation(
        Expect::GroupHealthReduced, QStringLiteral("archer_targets"), {}, 1.0F));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("enemy_archers"),
                                         QStringLiteral("riders")));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("riders"), {}, 1.5F));
    s.expectations.push_back(expectation(Expect::GroupExists, QStringLiteral("wall")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("wall"), {}, 1.0F));
    s.expectations.push_back(
        expectation(Expect::StructureDamageCueObserved, QStringLiteral("wall")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("victim")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("victim")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_retargeting_id),
                   QStringLiteral("Retargeting"),
                   QStringLiteral("Attacker kills one target and reacquires another."),
                   12.0F,
                   {13.0F, 42.0F, 28.0F});
    auto primary =
        group(QStringLiteral("primary"), Troop::Healer, 2, 1, {-0.9F, 0.0F, 1.8F}, 1);
    primary.health_override = 6;
    primary.max_health_override = 6;
    s.groups = {
        group(
            QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -2.0F}, 1),
        primary,
        group(QStringLiteral("secondary"),
              Troop::Swordsman,
              2,
              1,
              {2.4F, 0.0F, 2.8F},
              1)};
    s.steps = {at(0.0F,
                  Command::AttackMove,
                  QStringLiteral("attacker"),
                  QStringLiteral("primary")),
               when_destroyed(QStringLiteral("primary"),
                              Command::AttackMove,
                              QStringLiteral("attacker"),
                              QStringLiteral("secondary"))};
    add_visual_stability(s,
                         {QStringLiteral("attacker"),
                          QStringLiteral("primary"),
                          QStringLiteral("secondary")});
    s.expectations.push_back(expectation(Expect::TargetRetakenAfterDeath,
                                         QStringLiteral("attacker"),
                                         QStringLiteral("secondary")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_hold_guard_exit_id),
                   QStringLiteral("Hold / Guard Exit"),
                   QStringLiteral("Hold and guard units exit stance under pressure."),
                   10.0F,
                   {14.0F, 44.0F, 32.0F});
    s.groups = {
        group(QStringLiteral("hold"), Troop::Spearman, 1, 1, {-1.8F, 0.0F, 0.0F}, 4),
        group(QStringLiteral("guard"), Troop::Swordsman, 1, 1, {1.8F, 0.0F, 0.0F}, 4),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 7.0F}, 6)};
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("hold")),
        at(0.0F, Command::Guard, QStringLiteral("guard"), QStringLiteral("hold")),
        at(0.0F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("hold")),
        at(1.0F, Command::Hold, QStringLiteral("hold")),
        at(1.0F, Command::Guard, QStringLiteral("guard"), QStringLiteral("hold")),
        at(1.0F,
           Command::AttackMove,
           QStringLiteral("guard"),
           QStringLiteral("enemy"))};
    s.steps[3].enabled = false;
    s.steps[4].enabled = false;
    add_visual_stability(
        s, {QStringLiteral("hold"), QStringLiteral("guard"), QStringLiteral("enemy")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_testudo_missile_defense_id),
        QStringLiteral("Testudo Missile Defense"),
        QStringLiteral("Roman swordsmen answer a guard order by forming the testudo "
                       "while Carthaginian archers shoot into their shield face."),
        16.0F,
        {13.0F, 28.0F, 55.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.groups = {group(QStringLiteral("legion"),
                      Troop::Swordsman,
                      1,
                      6,
                      {-3.0F, 0.0F, -3.0F},
                      6,
                      {2.4F, 0.0F, 0.0F}),
                group(QStringLiteral("archers"),
                      Troop::Archer,
                      2,
                      4,
                      {-2.4F, 0.0F, 3.0F},
                      6,
                      {2.4F, 0.0F, 0.0F})};
    s.steps = {
        at(0.5F, Command::Guard, QStringLiteral("legion"), QStringLiteral("archers")),
        at(3.5F, Command::Attack, QStringLiteral("archers"), QStringLiteral("legion"))};
    s.steps[1].chase = false;
    add_visual_stability(s, {QStringLiteral("legion"), QStringLiteral("archers")});
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("legion")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("legion")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_shield_wall_cavalry_impact_id),
        QStringLiteral("Shield Wall Cavalry Impact"),
        QStringLiteral("Carthaginian citizen infantry hold a shield wall on a guard "
                       "order and receive a Roman cavalry charge on the shield face."),
        16.0F,
        {12.0F, 26.0F, 55.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.groups = {group(QStringLiteral("wall"),
                      Troop::Swordsman,
                      2,
                      5,
                      {-4.8F, 0.0F, -2.0F},
                      6,
                      {2.4F, 0.0F, 0.0F}),
                group(QStringLiteral("cavalry"),
                      Troop::MountedKnight,
                      1,
                      3,
                      {-2.4F, 0.0F, 12.0F},
                      4,
                      {2.6F, 0.0F, 0.0F})};
    s.steps = {
        at(0.5F, Command::Guard, QStringLiteral("wall"), QStringLiteral("cavalry")),
        at(3.5F, Command::Charge, QStringLiteral("cavalry"), QStringLiteral("wall"))};
    add_visual_stability(s, {QStringLiteral("wall"), QStringLiteral("cavalry")});
    s.expectations.push_back(expectation(Expect::GroupExists, QStringLiteral("wall")));
    s.expectations.push_back(
        expectation(Expect::AttackHasVisibleContact, QStringLiteral("cavalry")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_hold_stance_review_id),
        QStringLiteral("Hold Stance Review"),
        QStringLiteral("Side-on close review of the kneeling hold stance. A spearman "
                       "braces its point down the approach lane, an archer keeps the "
                       "bow ready, and a swordsman proves the stance stays limited to "
                       "archers and spearmen."),
        9.0F,
        {6.4F, 18.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.4F);
    s.groups = {
        group(QStringLiteral("spear"), Troop::Spearman, 1, 1, {-2.2F, 0.0F, -1.2F}, 1),
        group(QStringLiteral("archer"), Troop::Archer, 1, 1, {0.0F, 0.0F, -1.2F}, 1),
        group(QStringLiteral("sword"), Troop::Swordsman, 1, 1, {2.2F, 0.0F, -1.2F}, 1),
        group(QStringLiteral("threat"),
              Troop::Civilian,
              2,
              3,
              {-2.2F, 0.0F, 2.0F},
              1,
              {2.2F, 0.0F, 0.0F})};
    s.groups[3].health_override = 5000;
    s.groups[3].max_health_override = 5000;
    s.steps = {at(2.0F, Command::Hold, QStringLiteral("spear")),
               at(2.0F, Command::Hold, QStringLiteral("archer")),
               at(2.0F, Command::Hold, QStringLiteral("sword"))};
    add_visual_stability(
        s,
        {QStringLiteral("spear"), QStringLiteral("archer"), QStringLiteral("sword")});
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("threat")));
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("spear"), {}, 0.0F, 4.5F));
    s.expectations.push_back(expectation(
        Expect::HoldPoseMaintained, QStringLiteral("archer"), {}, 0.0F, 4.5F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_hold_toggle_cycle_id),
        QStringLiteral("Hold Toggle Cycle"),
        QStringLiteral("Archers and spearmen enter and leave the hold stance three "
                       "times in a row, including re-entry while they are still "
                       "standing up, so the kneel blend can be reviewed for pops."),
        11.0F,
        {7.2F, 26.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.groups = {
        group(QStringLiteral("spear"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -1.6F}, 1),
        group(QStringLiteral("archer"), Troop::Archer, 1, 1, {0.0F, 0.0F, 1.6F}, 1)};
    const float toggle_times[] = {0.4F, 2.4F, 5.0F, 7.0F, 7.4F, 9.4F};
    bool enable = true;
    for (float time : toggle_times) {
      for (auto const& name : {QStringLiteral("spear"), QStringLiteral("archer")}) {
        auto step = at(time, Command::Hold, name);
        step.enabled = enable;
        s.steps.push_back(std::move(step));
      }
      enable = !enable;
    }
    add_visual_stability(s, {QStringLiteral("spear"), QStringLiteral("archer")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_hold_transition_interrupts_id),
        QStringLiteral("Hold Transition Interrupts"),
        QStringLiteral("Every interruption of the hold stance in one run: a move "
                       "order during the kneel, an attack order during the kneel, a "
                       "melee push against the kneeling line, and a death while the "
                       "stance is still held."),
        14.0F,
        {12.0F, 32.0F, 60.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 1.0F);
    s.groups = {
        group(QStringLiteral("spear"), Troop::Spearman, 1, 1, {-2.4F, 0.0F, -1.0F}, 1),
        group(QStringLiteral("archer"), Troop::Archer, 1, 1, {2.4F, 0.0F, -1.0F}, 1),
        group(QStringLiteral("doomed"), Troop::Archer, 1, 1, {0.0F, 0.0F, -1.0F}, 1),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 8.0F}, 4)};
    s.groups[2].health_override = 30;

    s.steps = {
        at(0.3F, Command::Hold, QStringLiteral("spear")),
        at(0.3F, Command::Hold, QStringLiteral("archer")),
        at(0.3F, Command::Hold, QStringLiteral("doomed")),

        at(1.0F, Command::Move, QStringLiteral("spear")),

        at(3.0F, Command::Hold, QStringLiteral("spear")),
        at(3.8F,
           Command::AttackMove,
           QStringLiteral("archer"),
           QStringLiteral("enemy")),

        at(5.0F, Command::Hold, QStringLiteral("archer")),
        at(5.4F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("spear")),
        at(8.0F,
           Command::ApplyDamage,
           QStringLiteral("doomed"),
           QStringLiteral("enemy"))};
    s.steps[3].destination = QVector3D(-2.4F, 0.0F, -5.0F);
    s.steps.back().value = 400;

    add_visual_stability(
        s,
        {QStringLiteral("spear"), QStringLiteral("archer"), QStringLiteral("enemy")});
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("doomed")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_lod_switch_id),
                   QStringLiteral("LOD Switch"),
                   QStringLiteral("Dense battle with near/far camera LOD transitions."),
                   8.0F,
                   {34.0F, 52.0F, 24.0F});
    s.groups = {group(QStringLiteral("blue"),
                      Troop::Swordsman,
                      1,
                      5,
                      {0.0F, 0.0F, -10.0F},
                      20,
                      {5.0F, 0.0F, 0.0F}),
                group(QStringLiteral("red"),
                      Troop::Swordsman,
                      2,
                      5,
                      {0.0F, 0.0F, 10.0F},
                      20,
                      {5.0F, 0.0F, 0.0F})};
    s.steps = {
        at(0.0F, Command::SetFullCreatureLod),
        at(0.0F, Command::AttackMove, QStringLiteral("blue"), QStringLiteral("red")),
        at(0.0F, Command::AttackMove, QStringLiteral("red"), QStringLiteral("blue")),
        at(1.5F, Command::SetCamera),
        at(3.0F, Command::SetCamera)};
    s.steps[0].enabled = false;
    s.steps[3].camera_distance = 12.0F;
    s.steps[3].camera_angle = 44.0F;
    s.steps[3].camera_yaw = 28.0F;
    s.steps[4].camera_distance = 34.0F;
    s.steps[4].camera_angle = 52.0F;
    s.steps[4].camera_yaw = 24.0F;
    add_visual_stability(s, {QStringLiteral("blue"), QStringLiteral("red")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_three_swords_vs_two_spears_id),
        QStringLiteral("3 Swords vs 2 Spears"),
        QStringLiteral("Unequal groups meet; no eligible unit may stay idle."),
        14.0F,
        {22.0F, 48.0F, 28.0F});
    s.groups = {
        group(
            QStringLiteral("blue_swords"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, -9.0F}),
        group(QStringLiteral("red_spears"), Troop::Spearman, 2, 2, {0.0F, 0.0F, 9.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_swords"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_swords"), QStringLiteral("red_spears")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("blue_swords"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("red_spears"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("blue_swords"),
                                         QStringLiteral("red_spears"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("red_spears"),
                                         QStringLiteral("blue_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_multi_front_melee_id),
        QStringLiteral("Multi-front Melee"),
        QStringLiteral("A durable infantry unit holds its original front while a "
                       "second formation joins from the flank; soldier assignments "
                       "must split without resetting the existing fight."),
        14.0F,
        {18.0F, 50.0F, 28.0F});
    auto defender = group(
        QStringLiteral("defender"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.0F}, 12);
    auto front =
        group(QStringLiteral("front"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -7.0F}, 12);
    auto flank =
        group(QStringLiteral("flank"), Troop::Spearman, 1, 1, {-8.0F, 0.0F, 2.0F}, 12);
    defender.health_override = defender.max_health_override = 2400;
    front.health_override = front.max_health_override = 1800;
    flank.health_override = flank.max_health_override = 1800;
    s.groups = {defender, front, flank};
    s.steps = {
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("front"),
           QStringLiteral("defender")),
        at(0.25F, Command::Attack, QStringLiteral("defender"), QStringLiteral("front")),
        at(3.0F,
           Command::AttackMove,
           QStringLiteral("flank"),
           QStringLiteral("defender")),
    };
    add_visual_stability(
        s,
        {QStringLiteral("defender"), QStringLiteral("front"), QStringLiteral("flank")});
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("defender")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("front")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("flank")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("front"),
                                         QStringLiteral("defender")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("flank"),
                                         QStringLiteral("defender")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_survivor_compaction_id),
        QStringLiteral("Melee Survivor Compaction"),
        QStringLiteral("A formation takes a controlled casualty burst during melee; "
                       "the final two living soldiers must step into a compact pair "
                       "while corpses remain at their impact anchors."),
        10.0F,
        {12.0F, 44.0F, 20.0F});
    auto survivors = group(
        QStringLiteral("survivors"), Troop::Swordsman, 2, 1, {0.0F, 0.0F, 2.0F}, 12);
    auto attacker = group(
        QStringLiteral("attacker"), Troop::Swordsman, 1, 1, {0.0F, 0.0F, -5.0F}, 12);
    survivors.health_override = survivors.max_health_override = 12000;
    attacker.health_override = attacker.max_health_override = 12000;
    s.groups = {survivors, attacker};
    s.steps = {
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("attacker"),
           QStringLiteral("survivors")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("survivors"),
           QStringLiteral("attacker")),
        at(3.0F,
           Command::ApplyDamage,
           QStringLiteral("survivors"),
           QStringLiteral("attacker")),
    };
    s.steps.back().value = 10000;
    add_visual_stability(s, {QStringLiteral("survivors"), QStringLiteral("attacker")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("survivors"),
                                         QStringLiteral("attacker")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("survivors")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_spear_walk_contact_id),
        QStringLiteral("Spear Walk Contact"),
        QStringLiteral("Spearmen walk into swords without root jets or falls."),
        12.0F,
        {19.0F, 46.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("spears"), Troop::Spearman, 1, 2, {0.0F, 0.0F, -8.0F}),
        group(QStringLiteral("swords"), Troop::Swordsman, 2, 2, {0.0F, 0.0F, 4.0F})};
    s.steps = {
        at(0.25F, Command::FormationMove, QStringLiteral("spears")),
        at(2.0F,
           Command::AttackMove,
           QStringLiteral("spears"),
           QStringLiteral("swords")),
        at(2.0F, Command::Attack, QStringLiteral("swords"), QStringLiteral("spears"))};
    s.steps[0].destination = {0.0F, 0.0F, 0.0F};
    add_visual_stability(s, {QStringLiteral("spears"), QStringLiteral("swords")});
    s.expectations.push_back(
        expectation(Expect::NoLimbOverextension, QStringLiteral("spears")));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("spears"), {}, 0.8F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("spears"),
                                         QStringLiteral("swords")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::CombatIndicatorIsContinuous, QStringLiteral("spears")));
    s.expectations.push_back(
        expectation(Expect::CombatIndicatorIsContinuous, QStringLiteral("swords")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_archer_stability_id),
                   QStringLiteral("Archer Stability"),
                   QStringLiteral("Three standing archers fire without pose epilepsy."),
                   12.0F,
                   {23.0F, 48.0F, 35.0F});
    s.groups = {
        group(QStringLiteral("archers"), Troop::Archer, 1, 3, {0.0F, 0.0F, -7.0F}, 10),
        group(QStringLiteral("infantry"),
              Troop::Swordsman,
              2,
              2,
              {0.0F, 0.0F, 7.0F},
              10)};
    for (auto& infantry : s.groups) {
      if (infantry.name == QStringLiteral("infantry")) {
        infantry.health_override = 2000;
        infantry.max_health_override = 2000;
      }
    }
    s.steps = {at(
        0.5F, Command::Attack, QStringLiteral("archers"), QStringLiteral("infantry"))};
    add_visual_stability(s, {QStringLiteral("archers"), QStringLiteral("infantry")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("archers"),
                                         QStringLiteral("infantry")));
    s.expectations.push_back(expectation(
        Expect::RepeatedAttackAnimationObserved, QStringLiteral("archers"), {}, 2.0F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("archers"), {}, 0.9F));
    result.push_back(std::move(s));
  }

  {

    auto s = definition(QString::fromLatin1(k_infantry_idle_ambient_id),
                        QStringLiteral("Infantry Idle Ambient"),
                        QStringLiteral("Swordsmen, spearmen and archers stand at ease "
                                       "long enough to breathe and to play ambient "
                                       "idles, easing in and out of the idle cycle."),
                        70.0F,
                        {7.5F, 18.0F, 18.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {-3.0F, 0.0F, 0.0F}, 3),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {0.0F, 0.0F, 0.0F}, 3),
        group(QStringLiteral("archers"), Troop::Archer, 1, 1, {3.0F, 0.0F, 0.0F}, 3)};
    s.steps = {at(0.25F, Command::Stand, QStringLiteral("swords")),
               at(0.25F, Command::Stand, QStringLiteral("spears")),
               at(0.25F, Command::Stand, QStringLiteral("archers"))};
    add_visual_stability(s,
                         {QStringLiteral("swords"),
                          QStringLiteral("spears"),
                          QStringLiteral("archers")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_mounted_idle_ambient_id),
                        QStringLiteral("Mounted Idle Ambient"),
                        QStringLiteral("Riders sit a long halt: the saddle stays "
                                       "aligned while mounted ambient idles play, and "
                                       "no rider tries to squat on the ground."),
                        70.0F,
                        {17.0F, 26.0F, 20.0F});
    s.groups = {group(QStringLiteral("knights"),
                      Troop::MountedKnight,
                      1,
                      1,
                      {-5.0F, 0.0F, 0.0F},
                      4),
                group(QStringLiteral("horse_archers"),
                      Troop::HorseArcher,
                      1,
                      1,
                      {5.0F, 0.0F, 0.0F},
                      4)};
    s.steps = {at(0.25F, Command::Stand, QStringLiteral("knights")),
               at(0.25F, Command::Stand, QStringLiteral("horse_archers"))};
    add_visual_stability(s,
                         {QStringLiteral("knights"), QStringLiteral("horse_archers")});
    result.push_back(std::move(s));
  }

  {

    auto s = definition(QString::fromLatin1(k_idle_ambient_interrupt_id),
                        QStringLiteral("Idle Ambient Interrupt"),
                        QStringLiteral("Repeated move orders arrive while ambient "
                                       "idles are running; every interruption has to "
                                       "blend out instead of snapping."),
                        60.0F,
                        {20.0F, 32.0F, 20.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 1, {-4.0F, 0.0F, 0.0F}, 6),
        group(QStringLiteral("spears"), Troop::Spearman, 1, 1, {4.0F, 0.0F, 0.0F}, 6)};
    s.steps = {at(0.25F, Command::Stand, QStringLiteral("swords")),
               at(0.25F, Command::Stand, QStringLiteral("spears")),
               at(14.0F, Command::Move, QStringLiteral("swords")),
               at(24.0F, Command::Move, QStringLiteral("swords")),
               at(34.0F, Command::Move, QStringLiteral("spears")),
               at(44.0F, Command::Move, QStringLiteral("spears")),
               at(52.0F, Command::Move, QStringLiteral("swords"))};
    s.steps[2].destination = {-4.0F, 0.0F, -7.0F};
    s.steps[3].destination = {-4.0F, 0.0F, 3.0F};
    s.steps[4].destination = {4.0F, 0.0F, -7.0F};
    s.steps[5].destination = {4.0F, 0.0F, 3.0F};
    s.steps[6].destination = {-4.0F, 0.0F, -5.0F};
    add_visual_stability(s, {QStringLiteral("swords"), QStringLiteral("spears")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_archer_melee_lock_id),
        QStringLiteral("Archers Forced Into Melee"),
        QStringLiteral("Swordsmen close through bow fire; locked archers stop "
                       "shooting and fight with a distinct two-handed bow strike."),
        12.0F,
        {18.0F, 48.0F, 24.0F});
    s.groups = {
        group(QStringLiteral("swords"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -7.0F}, 6),
        group(QStringLiteral("archers"), Troop::Archer, 2, 2, {0.0F, 0.0F, 4.0F}, 6)};
    for (auto& formation : s.groups) {
      formation.health_override = 1200;
      formation.max_health_override = 1200;
    }
    s.steps = {at(0.0F, Command::Hold, QStringLiteral("archers")),
               at(0.35F,
                  Command::Attack,
                  QStringLiteral("swords"),
                  QStringLiteral("archers"))};
    add_visual_stability(s, {QStringLiteral("swords"), QStringLiteral("archers")});
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("swords"),
                                         QStringLiteral("archers")));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("archers"),
                                         QStringLiteral("swords")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("archers")));
    s.expectations.push_back(expectation(
        Expect::RepeatedAttackAnimationObserved, QStringLiteral("archers"), {}, 2.0F));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("archers")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_infantry_charge_id),
                   QStringLiteral("Infantry Charge"),
                   QStringLiteral("Three infantry charge two stationary defenders."),
                   12.0F,
                   {22.0F, 48.0F, 25.0F});
    s.groups = {
        group(QStringLiteral("chargers"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, -11.0F}),
        group(QStringLiteral("defenders"), Troop::Spearman, 2, 2, {0.0F, 0.0F, 6.0F})};
    s.steps = {at(0.5F,
                  Command::Charge,
                  QStringLiteral("chargers"),
                  QStringLiteral("defenders")),
               at(0.5F, Command::Hold, QStringLiteral("defenders")),
               at(4.0F, Command::Hold, QStringLiteral("defenders")),
               at(4.0F,
                  Command::Attack,
                  QStringLiteral("defenders"),
                  QStringLiteral("chargers"))};
    s.steps[2].enabled = false;
    add_visual_stability(s, {QStringLiteral("chargers"), QStringLiteral("defenders")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("chargers"), {}, 0.4F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_flank_ambush_id),
                   QStringLiteral("Flank Ambush"),
                   QStringLiteral("Marching infantry react to a delayed flank ambush."),
                   14.0F,
                   {23.0F, 50.0F, 32.0F});
    auto ambushers = group(QStringLiteral("ambushers"),
                           Troop::Spearman,
                           2,
                           2,
                           {8.0F, 0.0F, 0.0F},
                           10,
                           {0.0F, 0.0F, 2.6F});
    ambushers.spawn_at_start = false;
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      3,
                      {0.0F, 0.0F, -7.0F},
                      10,
                      {0.0F, 0.0F, 2.6F}),
                ambushers};
    s.steps = {at(0.25F, Command::FormationMove, QStringLiteral("column")),
               at(2.5F,
                  Command::SpawnAmbush,
                  QStringLiteral("ambushers"),
                  QStringLiteral("column")),
               at(3.0F,
                  Command::AttackMove,
                  QStringLiteral("column"),
                  QStringLiteral("ambushers"))};
    s.steps[0].destination = {0.0F, 0.0F, 7.0F};
    add_visual_stability(s, {QStringLiteral("column"), QStringLiteral("ambushers")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("column"), {}, 0.55F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(QString::fromLatin1(k_reserve_release_id),
                        QStringLiteral("Reserve Release"),
                        QStringLiteral("Held reserve joins after front-line contact."),
                        15.0F,
                        {25.0F, 50.0F, 28.0F});
    s.groups = {
        group(QStringLiteral("front"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -5.0F}),
        group(QStringLiteral("reserve"), Troop::Spearman, 1, 1, {0.0F, 0.0F, -11.0F}),
        group(QStringLiteral("enemy"), Troop::Swordsman, 2, 2, {0.0F, 0.0F, 7.0F})};
    s.steps = {
        at(0.0F, Command::Hold, QStringLiteral("reserve")),
        at(0.5F, Command::AttackMove, QStringLiteral("front"), QStringLiteral("enemy")),
        at(0.5F, Command::AttackMove, QStringLiteral("enemy"), QStringLiteral("front")),
        at(3.5F, Command::Hold, QStringLiteral("reserve")),
        at(3.5F,
           Command::ReleaseReserve,
           QStringLiteral("reserve"),
           QStringLiteral("enemy"))};
    s.steps[3].enabled = false;
    add_visual_stability(
        s,
        {QStringLiteral("front"), QStringLiteral("reserve"), QStringLiteral("enemy")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("reserve"), {}, 0.55F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("reserve")));
    s.expectations.push_back(
        expectation(Expect::AllLivingSoldiersFight, QStringLiteral("reserve")));
    s.expectations.push_back(
        expectation(Expect::FormationEngagementIsStable, QStringLiteral("reserve")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_mixed_roles_id),
        QStringLiteral("Mixed Roles"),
        QStringLiteral("Two melee groups and archers fight three melee groups."),
        15.0F,
        {27.0F, 52.0F, 32.0F});
    s.groups = {
        group(
            QStringLiteral("blue_melee"), Troop::Swordsman, 1, 2, {-3.0F, 0.0F, -8.0F}),
        group(QStringLiteral("blue_archer"), Troop::Archer, 1, 1, {4.0F, 0.0F, -10.0F}),
        group(QStringLiteral("red_melee"), Troop::Spearman, 2, 3, {0.0F, 0.0F, 8.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_melee"),
                  QStringLiteral("red_melee")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_archer"),
                  QStringLiteral("red_melee")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_melee"),
                  QStringLiteral("blue_melee"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_melee"),
                          QStringLiteral("blue_archer"),
                          QStringLiteral("red_melee")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_bot_skirmish_id),
        QStringLiteral("Bot Skirmish"),
        QStringLiteral("AI-controlled group must make a useful battlefield move."),
        18.0F,
        {24.0F, 50.0F, 28.0F});
    auto bot = group(QStringLiteral("bot"), Troop::Spearman, 2, 3, {0.0F, 0.0F, 8.0F});
    bot.ai_controlled = true;
    s.groups = {
        group(QStringLiteral("player"), Troop::Swordsman, 1, 2, {0.0F, 0.0F, -8.0F}),
        bot};
    s.steps = {
        at(0.5F, Command::AttackMove, QStringLiteral("player"), QStringLiteral("bot"))};
    add_visual_stability(s, {QStringLiteral("player"), QStringLiteral("bot")});
    s.expectations.push_back(
        expectation(Expect::BotIssuesUsefulCommand, QStringLiteral("bot")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_bridge_crossing_id),
        QStringLiteral("Pathfinding: Bridge Crossing"),
        QStringLiteral("Two infantry files funnel onto a production bridge deck, "
                       "cross the river, and reform on the far bank."),
        15.0F,
        {32.0F, 54.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 5.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, 5.0F}, 4.5F, 0.45F});
    s.groups = {group(QStringLiteral("crossers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -11.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("crossers"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("crossers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("crossers")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("crossers")));
    s.expectations.push_back(expectation(Expect::BridgeCenterlineAligned,
                                         QStringLiteral("crossers"),
                                         {},
                                         0.0F,
                                         0.0F,
                                         0.50F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("crossers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_uphill_advance_id),
        QStringLiteral("Pathfinding: Uphill Advance"),
        QStringLiteral("Spearmen climb a smooth four-metre rise and settle on its "
                       "crown with grounded, continuous locomotion."),
        11.0F,
        {27.0F, 48.0F, 16.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, -1.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 2.0F}, 10.0F, 4.0F});
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-1.5F, 0.0F, -10.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 2.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(expectation(
        Expect::ElevationGainObserved, QStringLiteral("climbers"), {}, 2.5F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               2.5F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_wall_detour_id),
        QStringLiteral("Pathfinding: Wall Detour"),
        QStringLiteral("Infantry ordered through an intact palisade must remain "
                       "blocked by its footprint and route around the visible end."),
        15.0F,
        {32.0F, 55.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("detour_troops"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -9.0F},
              6,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("blocking_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 9,
                 {-8.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("detour_troops"));
    move.destination = {0.0F, 0.0F, 9.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("detour_troops")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("detour_troops")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("blocking_wall")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("detour_troops"),
                               {},
                               0.0F,
                               0.0F,
                               3.5F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_path_wall_breach_id),
        QStringLiteral("Pathfinding: Wall Breach"),
        QStringLiteral("Swordsmen destroy a designated weak wall section, then "
                       "path through the opened gap without crossing intact "
                       "neighbouring segments."),

        26.0F,
        {30.0F, 53.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    auto breach = building(QStringLiteral("breach_wall"),
                           Game::Units::SpawnType::WallSegment,
                           Nation::Carthage,
                           2,
                           1,
                           {0.0F, 0.0F, 0.0F});
    breach.health_override = breach.max_health_override = 55;
    s.groups = {
        group(QStringLiteral("breachers"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -8.0F},
              8,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("left_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {-10.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
        breach,
        building(QStringLiteral("right_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {4.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto attack = at(0.4F,
                     Command::Attack,
                     QStringLiteral("breachers"),
                     QStringLiteral("breach_wall"));
    auto pass = when_destroyed(QStringLiteral("breach_wall"),
                               Command::FormationMove,
                               QStringLiteral("breachers"),
                               {});
    pass.destination = {0.0F, 0.0F, 8.0F};
    s.steps = {attack, pass};
    add_visual_stability(s, {QStringLiteral("breachers")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("breachers")));
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("breachers")));
    s.expectations.push_back(
        expectation(Expect::GroupDestroyed, QStringLiteral("breach_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("left_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("right_wall")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("breachers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = pass.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_narrow_gap_column_id),
        QStringLiteral("Pathfinding: Narrow Gap Column"),
        QStringLiteral("A dozen swordsmen ordered through the single opening in a "
                       "long palisade file through it and reform beyond."),
        26.0F,
        {34.0F, 58.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("column"),
              Troop::Swordsman,
              1,
              2,
              {0.0F, 0.0F, -10.0F},
              12,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("west_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 6,
                 {-7.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("east_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 6,
                 {7.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("column"));
    move.destination = {0.0F, 0.0F, 10.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("column")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("west_wall")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("east_wall")));
    auto through = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("column"),
                               {},
                               0.0F,
                               0.0F,
                               6.0F);
    through.position = move.destination;
    s.expectations.push_back(through);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_building_alley_id),
        QStringLiteral("Pathfinding: Building Alley"),
        QStringLiteral("Infantry thread the alley between two barracks rather than "
                       "walking the long way round the block."),
        22.0F,
        {30.0F, 54.0F, 18.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        group(QStringLiteral("threaders"),
              Troop::Swordsman,
              1,
              2,
              {-1.5F, 0.0F, -11.0F},
              6,
              {3.0F, 0.0F, 0.0F}),
        building(QStringLiteral("west_block"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {-5.0F, 0.0F, 0.0F}),
        building(QStringLiteral("east_block"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {5.0F, 0.0F, 0.0F}),
        building(QStringLiteral("west_wing"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 5,
                 {-18.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("east_wing"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 5,
                 {10.0F, 0.0F, 0.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("threaders"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("threaders")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("threaders")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("west_block")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("east_block")));
    auto through = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("threaders"),
                               {},
                               0.0F,
                               0.0F,
                               5.0F);
    through.position = move.destination;
    s.expectations.push_back(through);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_diagonal_wall_seal_id),
        QStringLiteral("Pathfinding: Diagonal Wall Seals"),
        QStringLiteral("A palisade set on the diagonal holds: nobody slips between "
                       "two segments that only touch at their corners."),
        20.0F,
        {32.0F, 56.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    s.groups = {group(QStringLiteral("probers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-9.0F, 0.0F, -7.0F},
                      6,
                      {2.4F, 0.0F, 0.0F})};

    for (int index = 0; index < 25; ++index) {
      const float offset = static_cast<float>(index) * 2.0F;
      s.groups.push_back(building(QStringLiteral("diagonal_wall_%1").arg(index),
                                  Game::Units::SpawnType::WallSegment,
                                  Nation::Carthage,
                                  2,
                                  1,
                                  {-24.0F + offset, 0.0F, 24.0F - offset}));
    }
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("probers"));
    move.destination = {10.0F, 0.0F, 10.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("probers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("probers")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("diagonal_wall_12")));
    auto held = expectation(Expect::GroupHeldOutsideDestination,
                            QStringLiteral("probers"),
                            {},
                            0.0F,
                            0.0F,
                            6.0F);
    held.position = move.destination;
    s.expectations.push_back(held);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_bridge_column_id),
        QStringLiteral("Pathfinding: Bridge Column"),
        QStringLiteral("A wide line of infantry funnels onto one narrow bridge, "
                       "crosses on the deck, and reforms on the far bank."),
        26.0F,
        {34.0F, 58.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 6.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 3.5F, 0.45F});
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-12.0F, 0.0F, -12.0F},
                      12,
                      {2.4F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("column"));
    move.destination = {0.0F, 0.0F, 12.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("column")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("column")));
    auto crossed = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("column"),
                               {},
                               0.0F,
                               0.0F,
                               6.0F);
    crossed.position = move.destination;
    s.expectations.push_back(crossed);
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_path_hill_entrance_column_id),
        QStringLiteral("Pathfinding: Hill Entrance Column"),
        QStringLiteral("Spearmen ordered onto a crown they cannot scale directly "
                       "find the cut ramp and climb it in column."),
        24.0F,
        {32.0F, 56.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 0.0F}, 9.0F, 4.0F});
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-6.0F, 0.0F, -14.0F},
                      8,
                      {2.4F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 0.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(
        expectation(Expect::ElevationGainObserved, QStringLiteral("climbers")));
    auto crowned = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               6.0F);
    crowned.position = move.destination;
    s.expectations.push_back(crowned);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_road_junction_showcase_id),
        QStringLiteral("Roads: Junction Showcase"),
        QStringLiteral("A crossroads, a T-junction, a Y-branch, a sharp bend, and two "
                       "closely spaced side turnings in one view, so junction geometry "
                       "can be judged for stacking, seams, and notches."),
        16.0F,
        {34.0F, 55.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    const auto road =
        [](QVector3D start, QVector3D end, float width, const char* style) {
          return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
        };

    s.roads.push_back(road({-16.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 0.0F}, 4.0F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, 0.0F}, {16.0F, 0.0F, 0.0F}, 4.0F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 0.0F}, 4.0F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 14.0F}, 4.0F, "default"));

    s.roads.push_back(road({9.0F, 0.0F, 0.0F}, {13.0F, 0.0F, 9.0F}, 3.2F, "default"));
    s.roads.push_back(road({9.0F, 0.0F, 0.0F}, {15.0F, 0.0F, -7.0F}, 3.2F, "default"));

    s.roads.push_back(road({-16.0F, 0.0F, 10.0F}, {-8.0F, 0.0F, 10.0F}, 3.6F, "stone"));
    s.roads.push_back(road({-8.0F, 0.0F, 10.0F}, {-6.0F, 0.0F, 16.0F}, 3.6F, "stone"));
    s.roads.push_back(road({-12.0F, 0.0F, 10.0F}, {-12.0F, 0.0F, 5.0F}, 2.8F, "rough"));
    s.roads.push_back(road({-9.5F, 0.0F, 10.0F}, {-9.5F, 0.0F, 5.5F}, 2.8F, "rough"));
    s.groups = {group(QStringLiteral("column"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -11.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("column"));
    move.destination = {0.0F, 0.0F, 11.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("column")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("column"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_road_slope_showcase_id),
        QStringLiteral("Roads: Slope Showcase"),
        QStringLiteral(
            "One road climbs a rise head-on while a second traverses it "
            "across the fall line and the two cross on the flank, so "
            "terrain-following and slope junctions can be reviewed together."),
        11.0F,
        {34.0F, 50.0F, 22.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.elevation_patches.push_back({{0.0F, 0.0F, 0.0F}, 12.0F, 5.0F});
    const auto road =
        [](QVector3D start, QVector3D end, float width, const char* style) {
          return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
        };
    s.roads.push_back(road({0.0F, 0.0F, -16.0F}, {0.0F, 0.0F, -6.0F}, 4.0F, "stone"));
    s.roads.push_back(road({0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 4.0F, "stone"));
    s.roads.push_back(road({0.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 16.0F}, 4.0F, "stone"));
    s.roads.push_back(road({-16.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 6.0F}, 3.6F, "default"));
    s.roads.push_back(road({0.0F, 0.0F, 6.0F}, {16.0F, 0.0F, 6.0F}, 3.6F, "default"));
    s.groups = {group(QStringLiteral("climbers"),
                      Troop::Spearman,
                      1,
                      2,
                      {-1.5F, 0.0F, -13.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("climbers"));
    move.destination = {0.0F, 0.0F, 0.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("climbers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("climbers")));
    s.expectations.push_back(expectation(
        Expect::ElevationGainObserved, QStringLiteral("climbers"), {}, 3.0F));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("climbers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_road_bridge_approach_id),
        QStringLiteral("Roads: Bridge Approach"),
        QStringLiteral(
            "A road runs onto a bridge deck from both banks while the river "
            "keeps flowing underneath, which is the case where the deck used "
            "to sit on filled ground and the approaches stopped short."),
        17.0F,
        {30.0F, 46.0F, 16.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.suppress_terrain_scatter = true;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-28.0F, 0.0F, 0.0F}, {28.0F, 0.0F, 0.0F}, 6.5F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 6.0F}, 6.0F, 0.7F});
    s.roads.push_back(Game::Map::RoadSegment{
        {0.0F, 0.0F, -18.0F}, {0.0F, 0.0F, -6.0F}, 4.0F, QStringLiteral("default")});
    s.roads.push_back(Game::Map::RoadSegment{
        {0.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 18.0F}, 4.0F, QStringLiteral("default")});
    s.groups = {group(QStringLiteral("crossers"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-1.5F, 0.0F, -12.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    auto move = at(0.5F, Command::FormationMove, QStringLiteral("crossers"));
    move.destination = {0.0F, 0.0F, 12.0F};
    s.steps = {move};
    add_visual_stability(s, {QStringLiteral("crossers")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("crossers")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("crossers")));
    auto reached = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("crossers"),
                               {},
                               0.0F,
                               0.0F,
                               3.0F);
    reached.position = move.destination;
    s.expectations.push_back(reached);
    result.push_back(std::move(s));
  }

  {

    auto gate_line = [](ArenaScenarioDefinition& scenario, int gate_owner) {
      scenario.groups.push_back(building(QStringLiteral("west_wall"),
                                         Game::Units::SpawnType::WallSegment,
                                         Nation::RomanRepublic,
                                         gate_owner,
                                         4,
                                         {-7.0F, 0.0F, 0.0F},
                                         {2.0F, 0.0F, 0.0F}));
      scenario.groups.push_back(building(QStringLiteral("east_wall"),
                                         Game::Units::SpawnType::WallSegment,
                                         Nation::RomanRepublic,
                                         gate_owner,
                                         4,
                                         {7.0F, 0.0F, 0.0F},
                                         {2.0F, 0.0F, 0.0F}));
    };

    {
      auto s = definition(
          QString::fromLatin1(k_gate_friendly_passage_id),
          QStringLiteral("Gate: Friendly Passage"),
          QStringLiteral("The owner's infantry approach their own gate, it swings "
                         "open ahead of them, and they march through the wall line."),
          14.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      s.groups.push_back(group(
          QStringLiteral("garrison"), Troop::Swordsman, 1, 1, {-0.5F, 0.0F, -9.0F}, 6));
      auto move = at(0.5F, Command::FormationMove, QStringLiteral("garrison"));
      move.destination = {-0.5F, 0.0F, 9.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("garrison")});
      s.expectations.push_back(
          expectation(Expect::MovementAnimationObserved, QStringLiteral("garrison")));
      s.expectations.push_back(
          expectation(Expect::GateOpenedObserved, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("gate")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("garrison"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 3.0F);
      reached.position = move.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_gate_allied_access_id),
          QStringLiteral("Gate: Allied Access"),
          QStringLiteral(
              "A Carthaginian column sharing the wall owner's team is "
              "admitted through the gate on the same terms as its garrison."),
          14.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1},
                       {.owner_id = 2, .team_id = 2},
                       {.owner_id = 3, .team_id = 1}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      auto allies = group(
          QStringLiteral("allies"), Troop::Spearman, 3, 1, {-0.5F, 0.0F, -9.0F}, 6);
      allies.facing_degrees = 0.0F;
      s.groups.push_back(allies);
      auto move = at(0.5F, Command::FormationMove, QStringLiteral("allies"));
      move.destination = {-0.5F, 0.0F, 9.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("allies")});
      s.expectations.push_back(
          expectation(Expect::GateOpenedObserved, QStringLiteral("gate")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("allies"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 3.0F);
      reached.position = move.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_gate_enemy_blocked_id),
          QStringLiteral("Gate: Enemy Blocked"),
          QStringLiteral("Hostile infantry walk up to a shut gate, fail to trigger "
                         "it, and are held on their side of the wall."),
          12.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      auto gate = building(QStringLiteral("gate"),
                           Game::Units::SpawnType::WallGate,
                           Nation::RomanRepublic,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F});
      gate.health_override = gate.max_health_override = 6000;
      s.groups.push_back(gate);
      s.groups.push_back(group(
          QStringLiteral("raiders"), Troop::Spearman, 2, 1, {-0.5F, 0.0F, -9.0F}, 6));
      auto move = at(0.5F, Command::FormationMove, QStringLiteral("raiders"));
      move.destination = {-0.5F, 0.0F, 9.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("raiders")});
      s.expectations.push_back(
          expectation(Expect::GateRemainedClosed, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("gate")));
      auto held = expectation(Expect::GroupHeldOutsideDestination,
                              QStringLiteral("raiders"),
                              {},
                              0.0F,
                              0.0F,
                              4.0F);
      held.position = move.destination;
      s.expectations.push_back(held);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_gate_destroyed_breach_id),
          QStringLiteral("Gate: Destroyed Breach"),
          QStringLiteral("Attackers break a gate that will not open for them and "
                         "pour through the breach it leaves in the wall."),

          30.0F,
          {30.0F, 52.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      auto gate = building(QStringLiteral("gate"),
                           Game::Units::SpawnType::WallGate,
                           Nation::RomanRepublic,
                           1,
                           1,
                           {0.0F, 0.0F, 0.0F});
      gate.health_override = gate.max_health_override = 60;
      s.groups.push_back(gate);
      s.groups.push_back(group(QStringLiteral("breachers"),
                               Troop::Swordsman,
                               2,
                               1,
                               {-0.5F, 0.0F, -8.0F},
                               8));
      auto attack = at(
          0.4F, Command::Attack, QStringLiteral("breachers"), QStringLiteral("gate"));
      auto pass = when_destroyed(QStringLiteral("gate"),
                                 Command::FormationMove,
                                 QStringLiteral("breachers"),
                                 {});
      pass.destination = {-0.5F, 0.0F, 8.0F};
      s.steps = {attack, pass};
      add_visual_stability(s, {QStringLiteral("breachers")});
      s.expectations.push_back(
          expectation(Expect::AttackAnimationObserved, QStringLiteral("breachers")));
      s.expectations.push_back(
          expectation(Expect::GroupDestroyed, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("west_wall")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("east_wall")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("breachers"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 3.5F);
      reached.position = pass.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }

    {
      auto s = definition(
          QString::fromLatin1(k_gate_consecutive_transit_id),
          QStringLiteral("Gate: Consecutive Transit"),
          QStringLiteral("Three files cross the same gate back to back; it must "
                         "stay open under them and never shut on a body."),
          20.0F,
          {30.0F, 54.0F, 18.0F});
      s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
      s.suppress_terrain_scatter = true;
      s.suppress_spawn_anchor = true;
      s.suppress_ui_overlays = true;
      s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};
      gate_line(s, 1);
      s.groups.push_back(building(QStringLiteral("gate"),
                                  Game::Units::SpawnType::WallGate,
                                  Nation::RomanRepublic,
                                  1,
                                  1,
                                  {0.0F, 0.0F, 0.0F}));
      s.groups.push_back(group(QStringLiteral("column"),
                               Troop::Swordsman,
                               1,
                               3,
                               {-0.5F, 0.0F, -6.0F},
                               4,
                               {0.0F, 0.0F, -2.5F}));
      auto move = at(0.5F, Command::Move, QStringLiteral("column"));
      move.destination = {-0.5F, 0.0F, 8.0F};
      s.steps = {move};
      add_visual_stability(s, {QStringLiteral("column")});
      s.expectations.push_back(
          expectation(Expect::GateOpenedObserved, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::GroupExists, QStringLiteral("gate")));
      s.expectations.push_back(
          expectation(Expect::MovementAnimationObserved, QStringLiteral("column")));
      auto reached = expectation(Expect::GroupReachedDestination,
                                 QStringLiteral("column"),
                                 {},
                                 0.0F,
                                 0.0F,
                                 4.0F);
      reached.position = move.destination;
      s.expectations.push_back(reached);
      result.push_back(std::move(s));
    }
  }

  {
    auto s = definition(
        QString::fromLatin1(k_crossing_formations_id),
        QStringLiteral("Crossing Formations"),
        QStringLiteral("Two friendly groups cross without formation collapse."),
        10.0F,
        {24.0F, 52.0F, 30.0F});
    s.groups = {
        group(QStringLiteral("left"), Troop::Swordsman, 1, 3, {-7.0F, 0.0F, -5.0F}),
        group(QStringLiteral("right"), Troop::Spearman, 1, 3, {7.0F, 0.0F, 5.0F})};
    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("left")),
               at(0.5F, Command::FormationMove, QStringLiteral("right"))};
    s.steps[0].destination = {7.0F, 0.0F, 5.0F};
    s.steps[1].destination = {-7.0F, 0.0F, -5.0F};
    add_visual_stability(s, {QStringLiteral("left"), QStringLiteral("right")});
    s.expectations.push_back(
        expectation(Expect::FormationOrderPreserved, QStringLiteral("left"), {}, 1.0F));
    s.expectations.push_back(expectation(
        Expect::FormationOrderPreserved, QStringLiteral("right"), {}, 1.0F));
    result.push_back(std::move(s));
  }

  {

    auto s = definition(
        QString::fromLatin1(k_fog_of_war_recon_id),
        QStringLiteral("Fog of War Recon"),
        QStringLiteral(
            "A patrol crosses the map and returns. Exercises exploration, loss "
            "of sight over ground already walked, and enemies that appear and "
            "vanish with the patrol's vision."),
        34.0F,
        {58.0F, 58.0F, 30.0F});
    s.groups = {
        group(
            QStringLiteral("patrol"), Troop::Swordsman, 1, 2, {-16.0F, 0.0F, 14.0F}, 8),
        group(QStringLiteral("camp_guards"),
              Troop::Spearman,
              2,
              2,
              {16.0F, 0.0F, -14.0F},
              8)};

    s.steps = {at(0.5F, Command::FormationMove, QStringLiteral("patrol")),
               at(6.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(12.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(18.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(24.0F, Command::FormationMove, QStringLiteral("patrol")),
               at(29.0F, Command::FormationMove, QStringLiteral("patrol"))};
    s.steps[0].destination = {-9.0F, 0.0F, 8.0F};
    s.steps[1].destination = {-2.0F, 0.0F, 1.0F};
    s.steps[2].destination = {6.0F, 0.0F, -5.0F};
    s.steps[3].destination = {12.0F, 0.0F, -10.0F};
    s.steps[4].destination = {2.0F, 0.0F, -1.0F};
    s.steps[5].destination = {-11.0F, 0.0F, 9.0F};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("patrol")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("camp_guards")));
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("patrol")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.5F));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_sustained_battle_id),
                   QStringLiteral("Sustained Battle"),
                   QStringLiteral("Large sustained fight for smoothness and stalls."),
                   30.0F,
                   {32.0F, 54.0F, 28.0F});
    s.groups = {group(QStringLiteral("blue_swords"),
                      Troop::Swordsman,
                      1,
                      4,
                      {-4.0F, 0.0F, -11.0F},
                      16),
                group(QStringLiteral("blue_archers"),
                      Troop::Archer,
                      1,
                      2,
                      {7.0F, 0.0F, -14.0F},
                      16),
                group(QStringLiteral("blue_catapult"),
                      Troop::Catapult,
                      1,
                      1,
                      {-9.0F, 0.0F, -8.0F},
                      1),
                group(QStringLiteral("red_spears"),
                      Troop::Spearman,
                      2,
                      5,
                      {0.0F, 0.0F, 11.0F},
                      16),
                group(QStringLiteral("red_ballista"),
                      Troop::Ballista,
                      2,
                      1,
                      {9.0F, 0.0F, 8.0F},
                      1)};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("blue_swords"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_archers"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("red_spears"),
                  QStringLiteral("blue_swords")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("blue_catapult"),
                  QStringLiteral("red_spears")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("red_ballista"),
                  QStringLiteral("blue_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("blue_swords"),
                          QStringLiteral("blue_archers"),
                          QStringLiteral("red_spears")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_render_continuity_id),
        QStringLiteral("Render Continuity Stress"),
        QStringLiteral("Fixed-camera Ultra battle that samples every frame for "
                       "scene-wide flashes, rejects every reduced creature LOD, "
                       "and tracks each living formation member for submission "
                       "disappearance."),
        14.0F,
        {29.0F, 54.0F, 28.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.force_full_creature_lod = false;
    s.collect_animation_diagnostics = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.groups = {
        group(QStringLiteral("blue_swords"),
              Troop::Swordsman,
              1,
              3,
              {-5.0F, 0.0F, -9.0F},
              16),
        group(QStringLiteral("blue_archers"),
              Troop::Archer,
              1,
              2,
              {7.0F, 0.0F, -12.0F},
              16),
        group(QStringLiteral("blue_healer"),
              Troop::Healer,
              1,
              1,
              {-11.0F, 0.0F, -8.0F},
              1),
        group(QStringLiteral("blue_cavalry"),
              Troop::MountedKnight,
              1,
              2,
              {-12.0F, 0.0F, -16.0F},
              12),
        group(QStringLiteral("red_spears"),
              Troop::Spearman,
              2,
              4,
              {-4.0F, 0.0F, 9.0F},
              16),
        group(QStringLiteral("red_archers"),
              Troop::Archer,
              2,
              2,
              {8.0F, 0.0F, 12.0F},
              16),
        group(
            QStringLiteral("red_healer"), Troop::Healer, 2, 1, {12.0F, 0.0F, 8.0F}, 1),
        group(QStringLiteral("red_cavalry"),
              Troop::HorseSpearman,
              2,
              2,
              {-12.0F, 0.0F, 16.0F},
              12),
    };
    for (auto& continuity_group : s.groups) {
      continuity_group.max_health_override = 2000;
      continuity_group.health_override = 2000;
    }
    s.steps = {
        at(0.35F,
           Command::AttackMove,
           QStringLiteral("blue_swords"),
           QStringLiteral("red_spears")),
        at(0.35F,
           Command::Attack,
           QStringLiteral("blue_archers"),
           QStringLiteral("red_spears")),
        at(0.35F,
           Command::Charge,
           QStringLiteral("blue_cavalry"),
           QStringLiteral("red_archers")),
        at(0.35F,
           Command::AttackMove,
           QStringLiteral("red_spears"),
           QStringLiteral("blue_swords")),
        at(0.35F,
           Command::Attack,
           QStringLiteral("red_archers"),
           QStringLiteral("blue_swords")),
        at(0.35F,
           Command::Charge,
           QStringLiteral("red_cavalry"),
           QStringLiteral("blue_archers")),
    };
    s.expectations.push_back(expectation(Expect::NoFullscreenFlash));
    for (auto const& continuity_group : s.groups) {
      s.expectations.push_back(expectation(
          Expect::NoRenderVisibilityChurn, continuity_group.name, {}, 0.0F, 0.5F));
      s.expectations.push_back(
          expectation(Expect::FullCreatureDetailOnly, continuity_group.name));
      s.expectations.push_back(
          expectation(Expect::GroupIsRendered, continuity_group.name));
    }
    result.push_back(std::move(s));
  }

  {
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_250_id),
                                 QStringLiteral("Performance: 250 vs 250 Soldiers"),
                                 25,
                                 10));
    result.push_back(
        performance_battle_definition(QString::fromLatin1(k_performance_20v20_id),
                                      QStringLiteral("Performance: 20 vs 20 Units"),
                                      20));
    result.push_back(
        performance_battle_definition(QString::fromLatin1(k_performance_30v30_id),
                                      QStringLiteral("Performance: 30 vs 30 Units"),
                                      30));
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_500_id),
                                 QStringLiteral("Performance: 500 vs 500 Soldiers"),
                                 25));
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_1000_id),
                                 QStringLiteral("Performance: 1000 vs 1000 Soldiers"),
                                 50));
    result.push_back(
        massed_battle_definition(QString::fromLatin1(k_massed_battle_2000_id),
                                 QStringLiteral("Performance: 2000 vs 2000 Soldiers"),
                                 100));
    result.push_back(seven_ai_scale_definition());
  }

  {
    auto s = definition(
        QString::fromLatin1(k_campaign_scale_battle_id),
        QStringLiteral("Campaign-Scale Battle Performance"),
        QStringLiteral("Cannae-sized 79-unit mixed battle using production LOD, "
                       "batching, combat, cavalry, archery, and healing paths."),
        15.0F,
        {72.0F, 56.0F, 24.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = false;
    s.collect_animation_diagnostics = false;
    s.groups = {
        group(QStringLiteral("blue_spears"),
              Troop::Spearman,
              1,
              11,
              {-15.0F, 0.0F, -18.0F},
              16),
        group(QStringLiteral("blue_swords"),
              Troop::Swordsman,
              1,
              8,
              {-11.0F, 0.0F, -12.0F},
              16),
        group(QStringLiteral("blue_archers"),
              Troop::Archer,
              1,
              6,
              {-8.0F, 0.0F, -25.0F},
              16),
        group(QStringLiteral("blue_healers"),
              Troop::Healer,
              1,
              1,
              {-2.0F, 0.0F, -28.0F},
              16),
        group(QStringLiteral("blue_horse_archers"),
              Troop::HorseArcher,
              1,
              3,
              {-25.0F, 0.0F, -22.0F},
              16),
        group(QStringLiteral("blue_cavalry"),
              Troop::MountedKnight,
              1,
              6,
              {9.0F, 0.0F, -20.0F},
              16),
        group(QStringLiteral("red_spears"),
              Troop::Spearman,
              2,
              13,
              {-17.0F, 0.0F, 17.0F},
              16),
        group(QStringLiteral("red_swords"),
              Troop::Swordsman,
              2,
              11,
              {-14.0F, 0.0F, 11.0F},
              16),
        group(QStringLiteral("red_archers"),
              Troop::Archer,
              2,
              7,
              {-9.0F, 0.0F, 25.0F},
              16),
        group(QStringLiteral("red_healers"),
              Troop::Healer,
              2,
              2,
              {0.0F, 0.0F, 28.0F},
              16),
        group(QStringLiteral("red_horse_archers"),
              Troop::HorseArcher,
              2,
              2,
              {-25.0F, 0.0F, 22.0F},
              16),
        group(QStringLiteral("red_cavalry"),
              Troop::MountedKnight,
              2,
              9,
              {7.0F, 0.0F, 20.0F},
              16),
    };
    s.steps = {
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("blue_spears"),
           QStringLiteral("red_swords")),
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("blue_swords"),
           QStringLiteral("red_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("blue_archers"),
           QStringLiteral("red_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("blue_horse_archers"),
           QStringLiteral("red_swords")),
        at(0.25F,
           Command::Charge,
           QStringLiteral("blue_cavalry"),
           QStringLiteral("red_archers")),
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("red_spears"),
           QStringLiteral("blue_swords")),
        at(0.25F,
           Command::AttackMove,
           QStringLiteral("red_swords"),
           QStringLiteral("blue_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("red_archers"),
           QStringLiteral("blue_spears")),
        at(0.25F,
           Command::Attack,
           QStringLiteral("red_horse_archers"),
           QStringLiteral("blue_swords")),
        at(0.25F,
           Command::Charge,
           QStringLiteral("red_cavalry"),
           QStringLiteral("blue_archers")),
    };
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("blue_spears")),
        expectation(Expect::GroupExists, QStringLiteral("red_spears")),
        expectation(Expect::FrameBudget, {}, {}, 10.0F, 2.0F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_roman_marching_camp_id),
        QStringLiteral("Roman Marching Camp"),
        QStringLiteral("A full castrum laid out on its own street grid: the via "
                       "praetoria runs from the north gate to the principia, the "
                       "via principalis crosses it between the flanking gates, "
                       "barrack blocks fill the northern half, officers' houses "
                       "and contubernia the southern one, and the camp's people "
                       "keep working the streets between them."),
        30.0F,
        {52.0F, 58.0F, 28.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 11.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.roads = {
        street({0.0F, 0.0F, -18.0F}, {0.0F, 0.0F, -4.0F}, 3.8F, "stone"),
        street({-18.0F, 0.0F, -4.0F}, {18.0F, 0.0F, -4.0F}, 4.4F, "stone"),
        street({0.0F, 0.0F, 5.0F}, {0.0F, 0.0F, 18.0F}, 3.8F, "stone"),
        street({-15.0F, 0.0F, 11.5F}, {15.0F, 0.0F, 11.5F}, 3.0F, "stone"),
        street({-15.0F, 0.0F, -15.0F}, {15.0F, 0.0F, -15.0F}, 2.4F, "stone"),
        street({-15.0F, 0.0F, 15.0F}, {15.0F, 0.0F, 15.0F}, 2.4F, "stone"),
        street({-15.0F, 0.0F, -15.0F}, {-15.0F, 0.0F, 15.0F}, 2.4F, "stone"),
        street({15.0F, 0.0F, -15.0F}, {15.0F, 0.0F, 15.0F}, 2.4F, "stone"),
    };

    s.resource_patches = {
        patch("tent", 4, {-14.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 4.0F}, 0.8F),
        patch("tent", 4, {14.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 4.0F}, 0.8F),
        patch("weapon_rack", 2, {-3.4F, 0.0F, -13.0F}, {0.0F, 0.0F, 4.0F}, 1.0F),
        patch("weapon_rack", 2, {3.4F, 0.0F, -13.0F}, {0.0F, 0.0F, 4.0F}, 1.0F),
        patch("fire_camp", 2, {-4.5F, 0.0F, 4.5F}, {9.0F, 0.0F, 0.0F}, 0.85F),
        patch("fire_camp", 1, {0.0F, 0.0F, -16.5F}, {}, 0.8F),
        patch("supply_cart", 3, {-12.0F, 0.0F, 14.5F}, {4.0F, 0.0F, 0.0F}, 0.95F),
        patch("supply_cart", 2, {9.0F, 0.0F, 14.5F}, {4.0F, 0.0F, 0.0F}, 0.95F),
        patch("plant", 5, {-16.8F, 0.0F, -6.0F}, {0.0F, 0.0F, 4.0F}, 0.9F),
        patch("plant", 5, {16.8F, 0.0F, -6.0F}, {0.0F, 0.0F, 4.0F}, 0.9F),
        patch("olive_tree", 4, {-26.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 5.0F}, 1.1F),
        patch("olive_tree", 4, {26.0F, 0.0F, 4.0F}, {0.0F, 0.0F, 5.0F}, 1.1F),
    };

    s.groups = {
        building(QStringLiteral("castrum_principia"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {0.0F, 0.0F, 1.0F}),
        building(QStringLiteral("castrum_praetorium"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {7.0F, 0.0F, 0.5F}),
        building(QStringLiteral("castrum_quaestorium"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-7.0F, 0.0F, 0.5F}),
        building(QStringLiteral("castrum_barracks_west"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-7.5F, 0.0F, -10.0F},
                 {},
                 180.0F),
        building(QStringLiteral("castrum_barracks_east"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {7.5F, 0.0F, -10.0F},
                 {},
                 180.0F),
        building(QStringLiteral("castrum_contubernia_west"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 2,
                 {-10.0F, 0.0F, 7.0F},
                 {5.0F, 0.0F, 0.0F}),
        building(QStringLiteral("castrum_contubernia_east"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 2,
                 {10.0F, 0.0F, 7.0F},
                 {5.0F, 0.0F, 0.0F}),
        residents(QStringLiteral("castrum_street_life"),
                  Nation::RomanRepublic,
                  1,
                  5,
                  {0.0F, 0.0F, -4.0F},
                  {4.0F, 0.0F, 0.0F},
                  16.0F),
        residents(QStringLiteral("castrum_quintana_crowd"),
                  Nation::RomanRepublic,
                  1,
                  4,
                  {0.0F, 0.0F, 11.5F},
                  {5.0F, 0.0F, 0.0F},
                  13.0F),
        group(QStringLiteral("castrum_works"),
              Troop::Builder,
              1,
              2,
              {-11.0F, 0.0F, 2.0F},
              1,
              {3.0F, 0.0F, 0.0F}),
        group(QStringLiteral("castrum_gate_watch"),
              Troop::Spearman,
              1,
              2,
              {0.0F, 0.0F, -15.5F},
              4,
              {5.0F, 0.0F, 0.0F}),
    };
    add_rampart(s,
                QStringLiteral("castrum"),
                Nation::RomanRepublic,
                1,
                {.extent_x = 18.0F, .extent_z = 18.0F, .gate_offset_z = -4.0F});

    s.steps = {at(0.2F, Command::Hold, QStringLiteral("castrum_gate_watch"))};

    add_settlement_acceptance(s,
                              {QStringLiteral("castrum_principia"),
                               QStringLiteral("castrum_praetorium"),
                               QStringLiteral("castrum_quaestorium"),
                               QStringLiteral("castrum_barracks_west"),
                               QStringLiteral("castrum_barracks_east"),
                               QStringLiteral("castrum_contubernia_west"),
                               QStringLiteral("castrum_contubernia_east")});
    require_groups_exist(s, rampart_groups(QStringLiteral("castrum")));
    add_visual_stability(s,
                         {QStringLiteral("castrum_street_life"),
                          QStringLiteral("castrum_quintana_crowd"),
                          QStringLiteral("castrum_works")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("castrum_street_life")));
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("castrum_quintana_crowd")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_carthage_trade_town_id),
        QStringLiteral("Carthaginian Trade Town"),
        QStringLiteral("A dense Punic town inside an oblong wall: a bazaar street "
                       "lined with stalls and carts runs the whole width of the "
                       "town, courtyard houses crowd the lanes off it, the "
                       "mercenary quarter holds the eastern end, and the "
                       "townspeople trade and move between them all day."),
        30.0F,
        {56.0F, 58.0F, 322.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 13.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.roads = {
        street({-20.0F, 0.0F, 0.0F}, {20.0F, 0.0F, 0.0F}, 4.2F, "stone"),
        street({-6.0F, 0.0F, -14.0F}, {-6.0F, 0.0F, 14.0F}, 3.2F, "stone"),
        street({9.0F, 0.0F, -14.0F}, {9.0F, 0.0F, 14.0F}, 2.8F, "stone"),
        street({-18.0F, 0.0F, -10.0F}, {18.0F, 0.0F, -10.0F}, 2.8F, "stone"),
        street({-18.0F, 0.0F, 9.5F}, {18.0F, 0.0F, 9.5F}, 2.8F, "stone"),
        street({-18.0F, 0.0F, -12.5F}, {-11.0F, 0.0F, -6.5F}, 2.2F, "stone"),
        street({12.0F, 0.0F, 4.5F}, {18.0F, 0.0F, 11.5F}, 2.2F, "stone"),
    };

    s.resource_patches = {
        patch("tent", 4, {-16.0F, 0.0F, -1.6F}, {4.0F, 0.0F, 0.0F}, 0.8F),
        patch("supply_cart", 4, {2.0F, 0.0F, -1.4F}, {3.2F, 0.0F, 0.0F}, 0.95F),
        patch("supply_cart", 2, {-2.0F, 0.0F, 1.8F}, {4.0F, 0.0F, 0.0F}, 0.9F),
        patch("weapon_rack", 2, {12.5F, 0.0F, -12.0F}, {4.0F, 0.0F, 0.0F}, 1.0F),
        patch("fire_camp", 1, {-12.0F, 0.0F, 12.0F}, {}, 0.85F),
        patch("fire_camp", 1, {12.0F, 0.0F, 12.0F}, {}, 0.85F),
        patch("olive_tree", 3, {4.0F, 0.0F, 12.2F}, {5.0F, 0.0F, 0.0F}, 1.0F),
        patch("plant", 5, {-18.6F, 0.0F, -6.0F}, {0.0F, 0.0F, 3.6F}, 0.9F),
        patch("plant", 5, {18.6F, 0.0F, -9.0F}, {0.0F, 0.0F, 3.6F}, 0.9F),
        patch("olive_tree", 5, {-28.0F, 0.0F, -4.0F}, {0.0F, 0.0F, 5.0F}, 1.15F),
        patch("olive_tree", 5, {28.0F, 0.0F, 2.0F}, {0.0F, 0.0F, 5.0F}, 1.15F),
    };

    s.groups = {
        building(QStringLiteral("punic_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {-1.0F, 0.0F, -5.5F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_exchange"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {3.0F, 0.0F, 5.5F}),
        building(QStringLiteral("punic_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {14.5F, 0.0F, -5.5F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_houses_northwest"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 2,
                 {-14.5F, 0.0F, -5.5F},
                 {5.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_houses_north"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 1,
                 {3.5F, 0.0F, -5.5F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_houses_southwest"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 2,
                 {-14.5F, 0.0F, 5.5F},
                 {5.0F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_houses_south"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 1,
                 {-2.5F, 0.0F, 5.5F}),
        building(QStringLiteral("punic_houses_southeast"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 1,
                 {14.5F, 0.0F, 5.0F}),
        residents(QStringLiteral("punic_bazaar_crowd"),
                  Nation::Carthage,
                  2,
                  6,
                  {0.0F, 0.0F, 0.0F},
                  {4.5F, 0.0F, 0.0F},
                  18.0F),
        residents(QStringLiteral("punic_lane_life"),
                  Nation::Carthage,
                  2,
                  4,
                  {0.0F, 0.0F, 9.5F},
                  {5.0F, 0.0F, 0.0F},
                  15.0F),
        nation_group(QStringLiteral("punic_works"),
                     Troop::Builder,
                     Nation::Carthage,
                     2,
                     2,
                     {-16.0F, 0.0F, 12.0F},
                     1,
                     {3.0F, 0.0F, 0.0F}),
        nation_group(QStringLiteral("punic_gate_watch"),
                     Troop::Spearman,
                     Nation::Carthage,
                     2,
                     2,
                     {-6.0F, 0.0F, -12.0F},
                     4,
                     {5.0F, 0.0F, 0.0F}),
    };
    add_rampart(s,
                QStringLiteral("punic"),
                Nation::Carthage,
                2,
                {.extent_x = 20.0F, .extent_z = 14.0F, .gate_offset_x = -6.0F});

    s.steps = {at(0.2F, Command::Hold, QStringLiteral("punic_gate_watch"))};

    add_settlement_acceptance(s,
                              {QStringLiteral("punic_market"),
                               QStringLiteral("punic_exchange"),
                               QStringLiteral("punic_barracks"),
                               QStringLiteral("punic_houses_northwest"),
                               QStringLiteral("punic_houses_north"),
                               QStringLiteral("punic_houses_southwest"),
                               QStringLiteral("punic_houses_south"),
                               QStringLiteral("punic_houses_southeast")});
    require_groups_exist(s, rampart_groups(QStringLiteral("punic")));
    add_visual_stability(s,
                         {QStringLiteral("punic_bazaar_crowd"),
                          QStringLiteral("punic_lane_life"),
                          QStringLiteral("punic_works")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("punic_bazaar_crowd")));
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("punic_lane_life")));
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_humanoid_gait_review_id),
                   QStringLiteral("Humanoid Gait Review"),
                   QStringLiteral("One archer, swordsman, and spearman cross a close "
                                  "side-on camera at a walk and then at a run, so "
                                  "silhouette, joint continuity, and both gait cycles "
                                  "can be read frame by frame."),
                   16.0F,
                   {7.5F, 9.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.95F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.force_full_creature_lod = true;
    s.groups = {
        group(
            QStringLiteral("gait_archer"), Troop::Archer, 1, 1, {-6.0F, 0.0F, 2.4F}, 1),
        group(QStringLiteral("gait_sword"),
              Troop::Swordsman,
              1,
              1,
              {-6.0F, 0.0F, 0.0F},
              1),
        group(QStringLiteral("gait_spear"),
              Troop::Spearman,
              1,
              1,
              {-6.0F, 0.0F, -2.4F},
              1)};

    auto walk = [](float time, const QString& name, float to_x, float z) {
      auto step = at(time, Command::FormationMove, name);
      step.destination = {to_x, 0.0F, z};
      return step;
    };
    auto sprint = [](float time, const QString& name, float to_x, float z) {
      auto step = at(time, Command::Run, name);
      step.destination = {to_x, 0.0F, z};
      step.enabled = true;
      return step;
    };

    s.steps = {walk(0.5F, QStringLiteral("gait_archer"), 6.0F, 2.4F),
               walk(0.5F, QStringLiteral("gait_sword"), 6.0F, 0.0F),
               walk(0.5F, QStringLiteral("gait_spear"), 6.0F, -2.4F),
               sprint(8.5F, QStringLiteral("gait_archer"), -6.0F, 2.4F),
               sprint(8.5F, QStringLiteral("gait_sword"), -6.0F, 0.0F),
               sprint(8.5F, QStringLiteral("gait_spear"), -6.0F, -2.4F)};

    add_visual_stability(s,
                         {QStringLiteral("gait_archer"),
                          QStringLiteral("gait_sword"),
                          QStringLiteral("gait_spear")});
    for (auto const& name : {QStringLiteral("gait_archer"),
                             QStringLiteral("gait_sword"),
                             QStringLiteral("gait_spear")}) {
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(expectation(Expect::MovementAnimationObserved, name));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_world_prop_lineup_id),
        QStringLiteral("World Prop Lineup"),
        QStringLiteral("Every authored world prop on clean ground in two rows for "
                       "direct mesh, silhouette, scale, and material review."),
        12.0F,
        {17.0F, 24.0F, 0.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.groups = {group(QStringLiteral("scale_reference"),
                      Troop::Swordsman,
                      1,
                      1,
                      {-10.5F, 0.0F, 0.0F},
                      1)};
    add_visual_stability(s, {QStringLiteral("scale_reference")});
    s.resource_patches = {
        {QStringLiteral("firecamp"), 1, {-7.5F, 0.0F, -3.0F}, {}, 1.0F},
        {QStringLiteral("tent"), 1, {-4.5F, 0.0F, -3.0F}, {}, 1.0F},
        {QStringLiteral("supply_cart"), 1, {-1.5F, 0.0F, -3.0F}, {}, 1.0F},
        {QStringLiteral("weapon_rack"), 1, {1.5F, 0.0F, -3.0F}, {}, 1.0F},
        {QStringLiteral("ruins"), 1, {4.5F, 0.0F, -3.0F}, {}, 1.0F},
        {QStringLiteral("magic_shrine"), 1, {7.5F, 0.0F, -3.0F}, {}, 1.0F},
        {QStringLiteral("dead_tree"), 1, {-7.5F, 0.0F, 3.0F}, {}, 1.0F},
        {QStringLiteral("boulder"), 1, {-4.5F, 0.0F, 3.0F}, {}, 1.0F},
        {QStringLiteral("iron_ore"), 1, {-1.5F, 0.0F, 3.0F}, {}, 1.0F},
        {QStringLiteral("plant"), 1, {1.5F, 0.0F, 3.0F}, {}, 1.0F},
        {QStringLiteral("pine_tree"), 1, {4.5F, 0.0F, 3.0F}, {}, 1.0F},
        {QStringLiteral("olive_tree"), 1, {7.5F, 0.0F, 3.0F}, {}, 1.0F},
        {QStringLiteral("abandoned_home"), 1, {-4.5F, 0.0F, 9.0F}, {}, 1.0F},
        {QStringLiteral("statue"), 1, {1.5F, 0.0F, 9.0F}, {}, 1.0F},
    };
    result.push_back(std::move(s));
  }

  for (const auto& fixture : std::array{
           std::tuple{
               k_sanctuary_precinct_day_id, "Sanctuary Precinct: Day", 11.0F, 0.0F},
           std::tuple{
               k_sanctuary_precinct_night_id, "Sanctuary Precinct: Night", 0.75F, 0.0F},
           std::tuple{k_sanctuary_precinct_storm_id,
                      "Sanctuary Precinct: Storm",
                      15.0F,
                      Game::Map::k_weather_intensity_heavy}}) {
    auto s = definition(
        QString::fromLatin1(std::get<0>(fixture)),
        QString::fromLatin1(std::get<1>(fixture)),
        QStringLiteral("A sacred quarter shared by both nations: Roman and Punic "
                       "temples face each other across a paved way lined with "
                       "commemorative statues, with derelict homes and ruins "
                       "crowding the edge of the precinct."),
        14.0F,
        {40.0F, 52.0F, 26.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.environment.start_time = std::get<2>(fixture);
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.weather.rain = std::get<3>(fixture);
    s.weather.storm = std::get<3>(fixture) * 0.6F;

    s.roads = {
        street({-18.0F, 0.0F, 0.0F}, {18.0F, 0.0F, 0.0F}, 4.2F, "stone"),
        street({0.0F, 0.0F, -12.0F}, {0.0F, 0.0F, 12.0F}, 3.0F, "stone"),
    };

    s.groups = {
        building(QStringLiteral("precinct_roman_temple"),
                 Game::Units::SpawnType::Temple,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-11.0F, 0.0F, -6.5F}),
        building(QStringLiteral("precinct_punic_temple"),
                 Game::Units::SpawnType::Temple,
                 Nation::Carthage,
                 2,
                 1,
                 {11.0F, 0.0F, 6.5F},
                 {},
                 180.0F),
        building(QStringLiteral("precinct_roman_home"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 2,
                 {-13.0F, 0.0F, 7.0F},
                 {6.0F, 0.0F, 0.0F}),
        building(QStringLiteral("precinct_punic_home"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 2,
                 {7.0F, 0.0F, -7.0F},
                 {6.0F, 0.0F, 0.0F},
                 180.0F),
        residents(QStringLiteral("precinct_pilgrims"),
                  Nation::RomanRepublic,
                  1,
                  5,
                  {-2.0F, 0.0F, 1.5F},
                  {3.0F, 0.0F, 0.0F},
                  12.0F),
    };

    s.resource_patches = {
        patch("statue", 4, {-7.5F, 0.0F, -2.4F}, {5.0F, 0.0F, 0.0F}, 1.0F),
        patch("statue", 4, {-7.5F, 0.0F, 2.4F}, {5.0F, 0.0F, 0.0F}, 1.0F),
        patch("abandoned_home", 2, {-16.0F, 0.0F, -11.0F}, {7.0F, 0.0F, 0.0F}, 1.0F),
        patch("abandoned_home", 2, {9.0F, 0.0F, 12.0F}, {7.0F, 0.0F, 0.0F}, 0.9F),
        patch("ruins", 1, {16.0F, 0.0F, -12.0F}, {}, 0.85F),
        patch("olive_tree", 4, {-18.0F, 0.0F, 4.0F}, {0.0F, 0.0F, 4.5F}, 1.05F),
        patch("plant", 6, {14.0F, 0.0F, -4.0F}, {0.0F, 0.0F, 3.0F}, 0.9F),
    };

    add_settlement_acceptance(s,
                              {QStringLiteral("precinct_roman_temple"),
                               QStringLiteral("precinct_punic_temple"),
                               QStringLiteral("precinct_roman_home"),
                               QStringLiteral("precinct_punic_home")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("precinct_pilgrims")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_architecture_and_props_showcase_id),
        QStringLiteral("Architecture and Dark Props"),
        QStringLiteral("Clean side-by-side Roman and Carthaginian architecture "
                       "review with authored ritual, ruin, and cursed-world props."),
        12.0F,
        {36.0F, 50.0F, 0.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        building(QStringLiteral("showcase_roman_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-9.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_roman_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-3.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_roman_home"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {3.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_roman_tower"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {9.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_punic_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {-9.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_punic_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {-3.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_punic_home"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 1,
                 {3.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_punic_tower"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {9.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_roman_temple"),
                 Game::Units::SpawnType::Temple,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {15.0F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_punic_temple"),
                 Game::Units::SpawnType::Temple,
                 Nation::Carthage,
                 2,
                 1,
                 {15.0F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_roman_farm"),
                 Game::Units::SpawnType::Farm,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {21.5F, 0.0F, -5.0F}),
        building(QStringLiteral("showcase_punic_farm"),
                 Game::Units::SpawnType::Farm,
                 Nation::Carthage,
                 2,
                 1,
                 {21.5F, 0.0F, 2.5F},
                 {},
                 180.0F),
        building(QStringLiteral("showcase_roman_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 13,
                 {-10.0F, 0.0F, -8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("showcase_punic_wall"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 13,
                 {-10.0F, 0.0F, 6.0F},
                 {2.0F, 0.0F, 0.0F}),
    };
    s.resource_patches = {
        {QStringLiteral("magic_shrine"), 1, {-8.0F, 0.0F, 10.5F}, {}, 0.78F},
        {QStringLiteral("ruins"), 1, {-4.0F, 0.0F, 10.5F}, {}, 0.68F},
        {QStringLiteral("dead_tree"), 1, {0.0F, 0.0F, 10.5F}, {}, 0.90F},
        {QStringLiteral("iron_ore"), 1, {4.0F, 0.0F, 10.5F}, {}, 0.90F},
        {QStringLiteral("weapon_rack"), 1, {8.0F, 0.0F, 10.5F}, {}, 0.85F},
        {QStringLiteral("abandoned_home"), 1, {12.5F, 0.0F, 10.5F}, {}, 0.85F},
        {QStringLiteral("statue"), 2, {15.0F, 0.0F, -1.2F}, {0.0F, 0.0F, 2.6F}, 0.95F},
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("showcase_roman_market"),
                               QStringLiteral("showcase_roman_barracks"),
                               QStringLiteral("showcase_roman_home"),
                               QStringLiteral("showcase_roman_tower"),
                               QStringLiteral("showcase_punic_market"),
                               QStringLiteral("showcase_punic_barracks"),
                               QStringLiteral("showcase_punic_home"),
                               QStringLiteral("showcase_punic_tower"),
                               QStringLiteral("showcase_roman_temple"),
                               QStringLiteral("showcase_punic_temple"),
                               QStringLiteral("showcase_roman_farm"),
                               QStringLiteral("showcase_punic_farm"),
                               QStringLiteral("showcase_roman_wall"),
                               QStringLiteral("showcase_punic_wall")});
    for (auto const* farm : {"showcase_roman_farm", "showcase_punic_farm"}) {
      auto ripen = at(0.5F, Command::SetFarmGrowth, QString::fromLatin1(farm));
      ripen.value = 100;
      s.steps.push_back(std::move(ripen));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_roman_fortification_showcase_id),
        QStringLiteral("Roman Timber Fortification"),
        QStringLiteral(
            "Roman palisade review with disciplined wall runs, reinforced "
            "corners, a defended gate opening, towers, and an occupied ward."),
        16.0F,
        {42.0F, 56.0F, 28.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        building(QStringLiteral("roman_fort_north"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {0.0F, 0.0F, -8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_fort_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {-8.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("roman_fort_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 9,
                 {8.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("roman_fort_south_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 4,
                 {-5.0F, 0.0F, 8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_fort_south_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 4,
                 {5.0F, 0.0F, 8.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("roman_fort_tower_nw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-8.0F, 0.0F, -8.0F}),
        building(QStringLiteral("roman_fort_tower_ne"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {8.0F, 0.0F, -8.0F}),
        building(QStringLiteral("roman_fort_tower_sw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-8.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_tower_se"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {8.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_gate_left"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-2.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_gate_right"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {2.0F, 0.0F, 8.0F}),
        building(QStringLiteral("roman_fort_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-2.8F, 0.0F, -1.0F}),
        building(QStringLiteral("roman_fort_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {3.2F, 0.0F, 1.5F}),
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("roman_fort_north"),
                               QStringLiteral("roman_fort_west"),
                               QStringLiteral("roman_fort_east"),
                               QStringLiteral("roman_fort_south_west"),
                               QStringLiteral("roman_fort_south_east"),
                               QStringLiteral("roman_fort_tower_nw"),
                               QStringLiteral("roman_fort_tower_ne"),
                               QStringLiteral("roman_fort_tower_sw"),
                               QStringLiteral("roman_fort_tower_se"),
                               QStringLiteral("roman_fort_gate_left"),
                               QStringLiteral("roman_fort_gate_right"),
                               QStringLiteral("roman_fort_barracks"),
                               QStringLiteral("roman_fort_market")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_carthage_fortification_showcase_id),
        QStringLiteral("Carthaginian Dread Palisade"),
        QStringLiteral("Carthaginian timber fortress with bronze-bound logs, jagged "
                       "towers, a ritual gate, and a layered inner defensive ward."),
        16.0F,
        {46.0F, 60.0F, 330.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {
        building(QStringLiteral("punic_fort_north"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {0.0F, 0.0F, -10.0F},
                 {2.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_fort_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {-10.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_fort_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 11,
                 {10.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_fort_south_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 4,
                 {-7.0F, 0.0F, 10.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_fort_south_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 4,
                 {7.0F, 0.0F, 10.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("punic_fort_tower_nw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-10.0F, 0.0F, -10.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_fort_tower_ne"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {10.0F, 0.0F, -10.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_fort_tower_sw"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-10.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_fort_tower_se"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {10.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_fort_gate_left"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {-3.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_fort_gate_right"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::Carthage,
                 2,
                 1,
                 {3.0F, 0.0F, 10.0F}),
        building(QStringLiteral("punic_inner_north"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 5,
                 {0.0F, 0.0F, -2.0F},
                 {2.0F, 0.0F, 0.0F},
                 180.0F),
        building(QStringLiteral("punic_inner_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {-4.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_inner_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::Carthage,
                 2,
                 3,
                 {4.0F, 0.0F, 0.0F},
                 {0.0F, 0.0F, 2.0F},
                 90.0F),
        building(QStringLiteral("punic_fort_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 2,
                 1,
                 {0.0F, 0.0F, 1.0F},
                 {},
                 180.0F),
        building(QStringLiteral("punic_fort_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {6.8F, 0.0F, 3.4F},
                 {},
                 180.0F),
    };
    s.resource_patches = {
        {QStringLiteral("fire_camp"), 2, {-2.0F, 0.0F, 6.0F}, {4.0F, 0.0F, 0.0F}, 0.8F},
        {QStringLiteral("weapon_rack"),
         2,
         {-6.5F, 0.0F, 5.2F},
         {13.0F, 0.0F, 0.0F},
         0.9F},
    };
    add_settlement_acceptance(s,
                              {QStringLiteral("punic_fort_north"),
                               QStringLiteral("punic_fort_west"),
                               QStringLiteral("punic_fort_east"),
                               QStringLiteral("punic_fort_south_west"),
                               QStringLiteral("punic_fort_south_east"),
                               QStringLiteral("punic_fort_tower_nw"),
                               QStringLiteral("punic_fort_tower_ne"),
                               QStringLiteral("punic_fort_tower_sw"),
                               QStringLiteral("punic_fort_tower_se"),
                               QStringLiteral("punic_fort_gate_left"),
                               QStringLiteral("punic_fort_gate_right"),
                               QStringLiteral("punic_inner_north"),
                               QStringLiteral("punic_inner_west"),
                               QStringLiteral("punic_inner_east"),
                               QStringLiteral("punic_fort_barracks"),
                               QStringLiteral("punic_fort_market")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_rival_economies_id),
        QStringLiteral("Rival Economies"),
        QStringLiteral("Roman and Carthaginian AI builders develop opposing starter "
                       "settlements from equal economic positions."),
        80.0F,
        {58.0F, 62.0F, 25.0F});
    auto roman_builders = group(QStringLiteral("roman_economy_builders"),
                                Troop::Builder,
                                2,
                                3,
                                {-18.0F, 0.0F, 0.0F},
                                1);
    roman_builders.ai_controlled = true;
    roman_builders.nation_id = Nation::RomanRepublic;
    auto punic_builders = group(QStringLiteral("punic_economy_builders"),
                                Troop::Builder,
                                3,
                                3,
                                {18.0F, 0.0F, 0.0F},
                                1);
    punic_builders.ai_controlled = true;
    punic_builders.nation_id = Nation::Carthage;
    s.resource_patches = {
        {QStringLiteral("olive_tree"),
         8,
         {-31.0F, 0.0F, -9.0F},
         {0.0F, 0.0F, 2.5F},
         1.15F},
        {QStringLiteral("boulder"),
         6,
         {-27.0F, 0.0F, -12.0F},
         {2.3F, 0.0F, 0.0F},
         1.1F},
        {QStringLiteral("iron_ore"),
         4,
         {-30.0F, 0.0F, 12.0F},
         {2.4F, 0.0F, 0.0F},
         1.0F},
        {QStringLiteral("olive_tree"),
         8,
         {31.0F, 0.0F, -9.0F},
         {0.0F, 0.0F, 2.5F},
         1.15F},
        {QStringLiteral("boulder"), 6, {15.5F, 0.0F, 15.0F}, {2.3F, 0.0F, 0.0F}, 1.1F},
        {QStringLiteral("iron_ore"),
         4,
         {23.0F, 0.0F, -14.0F},
         {2.4F, 0.0F, 0.0F},
         1.0F},
    };
    s.groups = {
        building(QStringLiteral("roman_economy_home"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 2,
                 {-18.0F, 0.0F, 5.0F}),
        building(QStringLiteral("roman_economy_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {-18.0F, 0.0F, -3.0F}),
        building(QStringLiteral("roman_economy_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {-18.0F, 0.0F, -10.0F}),
        building(QStringLiteral("punic_economy_home"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 3,
                 2,
                 {18.0F, 0.0F, 5.0F}),
        building(QStringLiteral("punic_economy_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 3,
                 1,
                 {18.0F, 0.0F, -3.0F}),
        building(QStringLiteral("punic_economy_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::Carthage,
                 3,
                 1,
                 {18.0F, 0.0F, -10.0F},
                 {},
                 180.0F),
        roman_builders,
        punic_builders,
    };
    for (auto& economy_group : s.groups) {
      economy_group.ai_controlled = true;
    }
    add_settlement_acceptance(s,
                              {QStringLiteral("roman_economy_home"),
                               QStringLiteral("roman_economy_market"),
                               QStringLiteral("roman_economy_barracks"),
                               QStringLiteral("punic_economy_home"),
                               QStringLiteral("punic_economy_market"),
                               QStringLiteral("punic_economy_barracks")});
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_economy_builders")));
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_economy_builders")));
    s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                         QStringLiteral("roman_economy_builders"),
                                         {},
                                         2.0F));
    s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                         QStringLiteral("punic_economy_builders"),
                                         {},
                                         2.0F));
    s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                         QStringLiteral("roman_economy_builders")));
    s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                         QStringLiteral("punic_economy_builders")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_village_harvest_cycle_id),
        QStringLiteral("Village Harvest Cycle"),
        QStringLiteral("An open Roman hamlet working its land: the lane runs past "
                       "the market and the houses, woodcutters and quarriers work "
                       "the tree line, the boulder field and the ore seam, and the "
                       "villagers go about the day between them."),
        80.0F,
        {50.0F, 58.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 12.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.roads = {
        street({-26.0F, 0.0F, 0.0F}, {26.0F, 0.0F, 0.0F}, 3.4F, "default"),
        street({0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 15.0F}, 2.8F, "default"),
        street({-12.0F, 0.0F, 0.0F}, {-17.0F, 0.0F, -11.0F}, 2.4F, "default"),
    };

    s.resource_patches = {
        patch("olive_tree", 6, {-24.0F, 0.0F, -12.0F}, {0.0F, 0.0F, 4.5F}, 1.15F),
        patch("pine_tree", 4, {-28.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 5.0F}, 1.0F),
        patch("boulder", 5, {-18.0F, 0.0F, -16.0F}, {3.0F, 0.0F, 0.0F}, 1.15F),
        patch("iron_ore", 4, {17.0F, 0.0F, 11.0F}, {3.0F, 0.0F, 0.0F}, 1.0F),
        patch("fire_camp", 1, {-2.5F, 0.0F, 9.5F}, {}, 0.85F),
        patch("supply_cart", 3, {4.0F, 0.0F, -3.0F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("tent", 2, {-13.0F, 0.0F, 9.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("plant", 6, {-9.0F, 0.0F, 13.5F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    auto harvesters = group(QStringLiteral("village_harvesters"),
                            Troop::Builder,
                            2,
                            3,
                            {-4.0F, 0.0F, 6.0F},
                            1,
                            {3.2F, 0.0F, 0.0F});
    harvesters.nation_id = Nation::RomanRepublic;
    harvesters.ai_controlled = true;

    s.groups = {
        building(QStringLiteral("village_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {0.0F, 0.0F, -5.5F},
                 {},
                 180.0F),
        building(QStringLiteral("village_houses_west"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 3,
                 {-8.0F, 0.0F, 5.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("village_houses_east"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 2,
                 {8.6F, 0.0F, 5.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("village_granary"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {-8.0F, 0.0F, -6.0F},
                 {},
                 180.0F),
        building(QStringLiteral("village_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {13.0F, 0.0F, -7.0F},
                 {},
                 180.0F),
        residents(QStringLiteral("village_folk"),
                  Nation::RomanRepublic,
                  2,
                  5,
                  {0.0F, 0.0F, 0.0F},
                  {4.5F, 0.0F, 0.0F},
                  15.0F),
        harvesters,
    };

    add_settlement_acceptance(s,
                              {QStringLiteral("village_market"),
                               QStringLiteral("village_houses_west"),
                               QStringLiteral("village_houses_east"),
                               QStringLiteral("village_granary"),
                               QStringLiteral("village_barracks")});
    add_visual_stability(
        s, {QStringLiteral("village_folk"), QStringLiteral("village_harvesters")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("village_folk")));
    s.expectations.push_back(expectation(
        Expect::OwnerHarvestsResource, QStringLiteral("village_harvesters"), {}, 2.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_village_day_life_id),
        QStringLiteral("Village Day Life"),
        QStringLiteral("A close morning pass over a hamlet that keeps itself busy: "
                       "the lane crowd works between the market, the houses and the "
                       "temple porch, woodcutters and quarriers run their own round "
                       "from the tree line to the barracks yard without being told "
                       "twice, and the flock grazes the meadow behind the houses. "
                       "This is the ambience reference scene - the camera sits close "
                       "enough to read hands and gait."),
        120.0F,
        {36.0F, 47.0F, 26.0F});
    s.camera_focus = QVector3D(-2.0F, 0.0F, 1.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 11.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.wildlife = Game::Wildlife::default_settings();
    s.wildlife.enabled = true;
    s.wildlife.seed = 20260805U;
    s.wildlife.wolves.enabled = false;
    s.wildlife.wolves.group_count = 0;
    s.wildlife.sheep.enabled = true;
    s.wildlife.sheep.group_count = 1;
    s.wildlife.sheep.group_size_min = 5;
    s.wildlife.sheep.group_size_max = 6;
    s.wildlife.sheep.roam_radius = 6.0F;
    s.wildlife.sheep.spawn_areas = {{-16.0F, 15.0F, 4.0F}};
    s.wildlife.birds.enabled = true;
    s.wildlife.birds.group_count = 1;
    s.wildlife.birds.spawn_areas = {{6.0F, -14.0F, 8.0F}};

    s.roads = {
        street({-26.0F, 0.0F, 0.0F}, {26.0F, 0.0F, 0.0F}, 3.4F, "stone"),
        street({-2.0F, 0.0F, 0.0F}, {-2.0F, 0.0F, 13.0F}, 2.6F, "stone"),
        street({9.0F, 0.0F, 0.0F}, {9.0F, 0.0F, -10.0F}, 2.6F, "stone"),
    };

    s.resource_patches = {
        patch("olive_tree", 5, {-17.0F, 0.0F, -11.0F}, {0.0F, 0.0F, 4.2F}, 1.15F),
        patch("olive_tree", 4, {-22.0F, 0.0F, -9.0F}, {0.0F, 0.0F, 4.4F}, 1.1F),
        patch("pine_tree", 4, {-21.0F, 0.0F, 4.0F}, {0.0F, 0.0F, 4.4F}, 1.05F),
        patch("boulder", 5, {-15.0F, 0.0F, 14.0F}, {3.2F, 0.0F, 0.0F}, 1.15F),
        patch("boulder", 3, {-4.0F, 0.0F, 17.0F}, {3.2F, 0.0F, 0.0F}, 1.05F),
        patch("fire_camp", 1, {-5.0F, 0.0F, 11.5F}, {}, 0.85F),
        patch("supply_cart", 3, {2.5F, 0.0F, -2.0F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("tent", 2, {-20.0F, 0.0F, 12.0F}, {4.2F, 0.0F, 0.0F}, 0.8F),
        patch("weapon_rack", 1, {12.0F, 0.0F, 3.5F}, {}, 1.0F),
        patch("plant", 7, {-8.0F, 0.0F, 15.0F}, {2.8F, 0.0F, 0.0F}, 0.9F),
    };

    auto woodcutters = group(QStringLiteral("village_woodcutters"),
                             Troop::Builder,
                             2,
                             2,
                             {-13.0F, 0.0F, -6.0F},
                             1,
                             {3.2F, 0.0F, 0.0F});
    woodcutters.nation_id = Nation::RomanRepublic;

    auto quarriers = group(QStringLiteral("village_quarriers"),
                           Troop::Builder,
                           2,
                           2,
                           {-11.0F, 0.0F, 10.0F},
                           1,
                           {3.2F, 0.0F, 0.0F});
    quarriers.nation_id = Nation::RomanRepublic;

    s.groups = {
        building(QStringLiteral("day_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {0.0F, 0.0F, -6.0F},
                 {},
                 180.0F),
        building(QStringLiteral("day_temple"),
                 Game::Units::SpawnType::Temple,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {9.0F, 0.0F, -12.0F},
                 {},
                 180.0F),
        building(QStringLiteral("day_houses_west"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 3,
                 {-10.0F, 0.0F, 6.0F},
                 {6.6F, 0.0F, 0.0F}),
        building(QStringLiteral("day_houses_east"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 2,
                 {8.5F, 0.0F, 6.0F},
                 {6.6F, 0.0F, 0.0F}),
        building(QStringLiteral("day_granary"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {-10.0F, 0.0F, -7.0F},
                 {},
                 180.0F),
        building(QStringLiteral("day_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 2,
                 1,
                 {14.0F, 0.0F, -4.0F},
                 {},
                 180.0F),
        residents(QStringLiteral("day_lane_folk"),
                  Nation::RomanRepublic,
                  2,
                  4,
                  {-3.0F, 0.0F, 2.5F},
                  {4.0F, 0.0F, 0.0F},
                  12.0F),
        residents(QStringLiteral("day_market_folk"),
                  Nation::RomanRepublic,
                  2,
                  3,
                  {3.5F, 0.0F, -3.0F},
                  {3.6F, 0.0F, 0.0F},
                  10.0F),
        residents(QStringLiteral("day_yard_folk"),
                  Nation::RomanRepublic,
                  2,
                  3,
                  {-9.0F, 0.0F, 9.0F},
                  {3.6F, 0.0F, 0.0F},
                  9.0F),
        residents(QStringLiteral("day_temple_folk"),
                  Nation::RomanRepublic,
                  2,
                  3,
                  {10.0F, 0.0F, -8.0F},
                  {3.4F, 0.0F, 0.0F},
                  9.0F),
        woodcutters,
        quarriers,
    };

    s.steps = {
        harvest_at(1.0F, QStringLiteral("village_woodcutters"), QStringLiteral("tree")),
        harvest_at(
            1.6F, QStringLiteral("village_quarriers"), QStringLiteral("boulder")),
    };

    add_settlement_acceptance(s,
                              {QStringLiteral("day_market"),
                               QStringLiteral("day_temple"),
                               QStringLiteral("day_houses_west"),
                               QStringLiteral("day_houses_east"),
                               QStringLiteral("day_granary"),
                               QStringLiteral("day_barracks")});
    add_visual_stability(s,
                         {QStringLiteral("day_lane_folk"),
                          QStringLiteral("day_market_folk"),
                          QStringLiteral("day_yard_folk"),
                          QStringLiteral("day_temple_folk"),
                          QStringLiteral("village_woodcutters"),
                          QStringLiteral("village_quarriers")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("day_lane_folk")));

    s.expectations.push_back(expectation(Expect::OwnerHarvestsResource,
                                         QStringLiteral("village_woodcutters"),
                                         {},
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_colony_founding_id),
        QStringLiteral("Colony Founding"),
        QStringLiteral("A Carthaginian colony breaks ground on the far side of the "
                       "lane from a finished Roman village, so a settlement under "
                       "construction and a settlement in full daily use can be "
                       "judged against each other in one view."),
        85.0F,
        {54.0F, 60.0F, 24.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 12.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 3, .team_id = 3}};

    s.roads = {
        street({-30.0F, 0.0F, 0.0F}, {30.0F, 0.0F, 0.0F}, 3.6F, "stone"),
        street({-16.0F, 0.0F, 0.0F}, {-16.0F, 0.0F, -13.0F}, 2.8F, "stone"),
        street({14.0F, 0.0F, 0.0F}, {14.0F, 0.0F, 12.0F}, 2.8F, "stone"),
    };

    s.resource_patches = {
        patch("olive_tree", 5, {26.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 5.0F}, 1.15F),
        patch("boulder", 5, {18.0F, 0.0F, 16.0F}, {3.0F, 0.0F, 0.0F}, 1.1F),
        patch("iron_ore", 3, {26.0F, 0.0F, 6.0F}, {3.0F, 0.0F, 0.0F}, 1.0F),
        patch("tent", 3, {10.0F, 0.0F, 8.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("supply_cart", 3, {6.0F, 0.0F, 4.0F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("fire_camp", 1, {12.0F, 0.0F, 4.5F}, {}, 0.85F),
        patch("weapon_rack", 1, {-12.0F, 0.0F, -8.0F}, {}, 1.0F),
        patch("plant", 6, {-24.0F, 0.0F, 6.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    auto colonists = group(QStringLiteral("colony_builders"),
                           Troop::Builder,
                           3,
                           3,
                           {14.0F, 0.0F, 7.0F},
                           1,
                           {3.2F, 0.0F, 0.0F});
    colonists.nation_id = Nation::Carthage;
    colonists.ai_controlled = true;

    auto colony_home = building(QStringLiteral("colony_first_home"),
                                Game::Units::SpawnType::Home,
                                Nation::Carthage,
                                3,
                                1,
                                {19.0F, 0.0F, 6.0F});
    colony_home.ai_controlled = true;

    s.groups = {
        building(QStringLiteral("old_village_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-16.0F, 0.0F, -6.5F},
                 {},
                 180.0F),
        building(QStringLiteral("old_village_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 3,
                 {-16.0F, 0.0F, 6.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("old_village_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-26.0F, 0.0F, -7.0F},
                 {},
                 180.0F),
        residents(QStringLiteral("old_village_folk"),
                  Nation::RomanRepublic,
                  1,
                  4,
                  {-16.0F, 0.0F, 0.0F},
                  {4.5F, 0.0F, 0.0F},
                  13.0F),
        colony_home,
        colonists,
    };

    add_settlement_acceptance(s,
                              {QStringLiteral("old_village_market"),
                               QStringLiteral("old_village_houses"),
                               QStringLiteral("old_village_barracks"),
                               QStringLiteral("colony_first_home")});
    add_visual_stability(
        s, {QStringLiteral("old_village_folk"), QStringLiteral("colony_builders")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("old_village_folk")));
    s.expectations.push_back(expectation(Expect::OwnerCompletesConstruction,
                                         QStringLiteral("colony_builders"),
                                         {},
                                         1.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_village_raid_id),
        QStringLiteral("Raid on the Village"),
        QStringLiteral("Carthaginian raiders come over the fields at an inhabited "
                       "Roman village while its people are still in the street and "
                       "the watch turns out of the barracks to meet them."),
        36.0F,
        {40.0F, 55.0F, 22.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 15.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};

    s.roads = {
        street({-22.0F, 0.0F, 2.0F}, {22.0F, 0.0F, 2.0F}, 3.2F, "default"),
        street({-4.0F, 0.0F, 2.0F}, {-4.0F, 0.0F, -12.0F}, 2.6F, "default"),
    };

    s.resource_patches = {
        patch("fire_camp", 1, {2.0F, 0.0F, -2.0F}, {}, 0.9F),
        patch("supply_cart", 2, {6.0F, 0.0F, 5.0F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("tent", 2, {-14.0F, 0.0F, 6.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("olive_tree", 5, {-22.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 4.5F}, 1.1F),
        patch("plant", 6, {8.0F, 0.0F, -8.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    s.groups = {
        building(QStringLiteral("raid_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-4.0F, 0.0F, -5.0F},
                 {},
                 180.0F),
        building(QStringLiteral("raid_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 3,
                 {2.0F, 0.0F, 7.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("raid_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-13.0F, 0.0F, -4.0F},
                 {},
                 180.0F),
        residents(QStringLiteral("raid_villagers"),
                  Nation::RomanRepublic,
                  1,
                  5,
                  {0.0F, 0.0F, 2.0F},
                  {4.5F, 0.0F, 0.0F},
                  12.0F),
        group(QStringLiteral("raid_watch"),
              Troop::Swordsman,
              1,
              2,
              {-13.0F, 0.0F, 1.0F},
              6,
              {4.0F, 0.0F, 0.0F}),
        nation_group(QStringLiteral("raiders"),
                     Troop::Spearman,
                     Nation::Carthage,
                     2,
                     2,
                     {14.0F, 0.0F, -14.0F},
                     6,
                     {4.5F, 0.0F, 0.0F}),
        nation_group(QStringLiteral("raider_horse"),
                     Troop::MountedKnight,
                     Nation::Carthage,
                     2,
                     1,
                     {20.0F, 0.0F, -10.0F},
                     4),
    };

    s.steps = {
        at(1.0F,
           Command::AttackMove,
           QStringLiteral("raiders"),
           QStringLiteral("raid_watch")),
        at(2.5F,
           Command::Charge,
           QStringLiteral("raider_horse"),
           QStringLiteral("raid_watch")),
        at(4.0F,
           Command::AttackMove,
           QStringLiteral("raid_watch"),
           QStringLiteral("raiders")),
    };

    add_settlement_acceptance(s,
                              {QStringLiteral("raid_market"),
                               QStringLiteral("raid_houses"),
                               QStringLiteral("raid_barracks")});
    add_visual_stability(s, {QStringLiteral("raid_villagers")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("raiders")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("raid_watch")));
    s.expectations.push_back(
        expectation(Expect::GroupHealthReduced, QStringLiteral("raiders")));
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("raid_villagers")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_frontier_outpost_id),
        QStringLiteral("Frontier Watch Outpost"),
        QStringLiteral("A small forward camp rather than a town: a watchtower over "
                       "a short palisade spur, a tent line, the cook fire, the "
                       "supply carts, and the section that mans it."),
        26.0F,
        {32.0F, 54.0F, 26.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 11.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.roads = {
        street({-18.0F, 0.0F, 8.0F}, {18.0F, 0.0F, 8.0F}, 3.0F, "rough"),
        street({0.0F, 0.0F, 8.0F}, {0.0F, 0.0F, -2.0F}, 2.4F, "rough"),
    };

    s.resource_patches = {
        patch("tent", 4, {-9.0F, 0.0F, 2.0F}, {4.5F, 0.0F, 0.0F}, 0.85F),
        patch("fire_camp", 1, {0.0F, 0.0F, 4.5F}, {}, 0.95F),
        patch("weapon_rack", 2, {-6.0F, 0.0F, -2.5F}, {4.0F, 0.0F, 0.0F}, 1.0F),
        patch("supply_cart", 2, {6.0F, 0.0F, 5.0F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("pine_tree", 5, {-20.0F, 0.0F, -10.0F}, {0.0F, 0.0F, 4.5F}, 1.05F),
        patch("pine_tree", 4, {18.0F, 0.0F, -8.0F}, {0.0F, 0.0F, 4.5F}, 1.05F),
        patch("boulder", 3, {13.0F, 0.0F, -4.0F}, {3.0F, 0.0F, 0.0F}, 1.1F),
    };

    s.groups = {
        building(QStringLiteral("outpost_tower"),
                 Game::Units::SpawnType::DefenseTower,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {0.0F, 0.0F, -6.0F}),
        building(QStringLiteral("outpost_palisade_west"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 5,
                 {-6.0F, 0.0F, -6.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("outpost_palisade_east"),
                 Game::Units::SpawnType::WallSegment,
                 Nation::RomanRepublic,
                 1,
                 5,
                 {6.0F, 0.0F, -6.0F},
                 {2.0F, 0.0F, 0.0F}),
        building(QStringLiteral("outpost_quarters"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {8.0F, 0.0F, 0.0F}),
        residents(QStringLiteral("outpost_servants"),
                  Nation::RomanRepublic,
                  1,
                  2,
                  {2.0F, 0.0F, 6.0F},
                  {3.5F, 0.0F, 0.0F},
                  9.0F),
        group(QStringLiteral("outpost_watch"),
              Troop::Spearman,
              1,
              2,
              {-3.0F, 0.0F, -3.0F},
              5,
              {6.0F, 0.0F, 0.0F}),
        group(QStringLiteral("outpost_archers"),
              Troop::Archer,
              1,
              1,
              {4.0F, 0.0F, -3.5F},
              4),
    };

    s.steps = {at(0.2F, Command::Hold, QStringLiteral("outpost_watch")),
               at(0.2F, Command::Hold, QStringLiteral("outpost_archers"))};

    add_settlement_acceptance(s,
                              {QStringLiteral("outpost_tower"),
                               QStringLiteral("outpost_palisade_west"),
                               QStringLiteral("outpost_palisade_east"),
                               QStringLiteral("outpost_quarters")});
    add_visual_stability(s, {QStringLiteral("outpost_servants")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("outpost_servants")));
    s.expectations.push_back(
        expectation(Expect::GroupIsRendered, QStringLiteral("outpost_watch")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_riverside_mill_town_id),
        QStringLiteral("Riverside Mill Town"),
        QStringLiteral("A town built on both banks of a river and stitched together "
                       "by one bridge: the road crosses it, the houses face it, and "
                       "the townspeople use it while a carrying party makes the "
                       "crossing under scrutiny."),
        34.0F,
        {46.0F, 56.0F, 16.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 12.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.rivers.push_back(
        Game::Map::RiverSegment{{-30.0F, 0.0F, 0.0F}, {30.0F, 0.0F, 0.0F}, 6.0F});
    s.bridges.push_back(
        Game::Map::Bridge{{0.0F, 0.0F, -5.5F}, {0.0F, 0.0F, 5.5F}, 4.5F, 0.45F});

    s.roads = {
        street({0.0F, 0.0F, -18.0F}, {0.0F, 0.0F, -5.5F}, 3.6F, "stone"),
        street({0.0F, 0.0F, 5.5F}, {0.0F, 0.0F, 18.0F}, 3.6F, "stone"),
        street({-18.0F, 0.0F, -9.0F}, {18.0F, 0.0F, -9.0F}, 3.0F, "stone"),
        street({-18.0F, 0.0F, 9.0F}, {18.0F, 0.0F, 9.0F}, 3.0F, "stone"),
    };

    s.resource_patches = {
        patch("olive_tree", 4, {-22.0F, 0.0F, -19.0F}, {0.0F, 0.0F, 4.0F}, 1.1F),
        patch("olive_tree", 4, {22.0F, 0.0F, 11.0F}, {0.0F, 0.0F, 4.0F}, 1.1F),
        patch("supply_cart", 3, {-8.0F, 0.0F, -11.5F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("fire_camp", 1, {6.0F, 0.0F, 12.5F}, {}, 0.85F),
        patch("tent", 2, {13.0F, 0.0F, -11.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("plant", 6, {-15.0F, 0.0F, 11.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
        patch("plant", 6, {8.0F, 0.0F, -19.5F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    s.groups = {
        building(QStringLiteral("mill_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-6.0F, 0.0F, -13.5F},
                 {},
                 180.0F),
        building(QStringLiteral("mill_north_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 3,
                 {7.5F, 0.0F, -16.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("mill_south_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 3,
                 {-9.0F, 0.0F, 14.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("mill_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {12.0F, 0.0F, 14.0F},
                 {},
                 180.0F),
        residents(QStringLiteral("mill_north_folk"),
                  Nation::RomanRepublic,
                  1,
                  3,
                  {-4.0F, 0.0F, -9.0F},
                  {4.5F, 0.0F, 0.0F},
                  11.0F),
        residents(QStringLiteral("mill_south_folk"),
                  Nation::RomanRepublic,
                  1,
                  3,
                  {4.0F, 0.0F, 9.0F},
                  {4.5F, 0.0F, 0.0F},
                  11.0F),
        group(QStringLiteral("mill_carriers"),
              Troop::Civilian,
              1,
              2,
              {-1.5F, 0.0F, -12.0F},
              1,
              {3.0F, 0.0F, 0.0F}),
    };

    auto crossing = at(1.5F, Command::FormationMove, QStringLiteral("mill_carriers"));
    crossing.destination = {0.0F, 0.0F, 12.0F};
    s.steps = {crossing};

    add_settlement_acceptance(s,
                              {QStringLiteral("mill_market"),
                               QStringLiteral("mill_north_houses"),
                               QStringLiteral("mill_south_houses"),
                               QStringLiteral("mill_barracks")});
    add_visual_stability(s,
                         {QStringLiteral("mill_north_folk"),
                          QStringLiteral("mill_south_folk"),
                          QStringLiteral("mill_carriers")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("mill_north_folk")));
    s.expectations.push_back(
        expectation(Expect::BridgeTraversalObserved, QStringLiteral("mill_carriers")));
    auto landed = expectation(Expect::GroupReachedDestination,
                              QStringLiteral("mill_carriers"),
                              {},
                              0.0F,
                              0.0F,
                              4.0F);
    landed.position = crossing.destination;
    s.expectations.push_back(landed);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_quarry_camp_id),
        QStringLiteral("Hill Quarry Camp"),
        QStringLiteral("A pure extraction camp on broken ground: quarriers work a "
                       "boulder field and an ore seam on the ridge while the depot, "
                       "the carts and the tent line below take the yield."),
        80.0F,
        {46.0F, 56.0F, 34.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 13.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.elevation_patches = {{{-2.0F, 0.0F, -16.0F}, 16.0F, 3.4F}};

    s.roads = {
        street({-16.0F, 0.0F, 8.0F}, {18.0F, 0.0F, 8.0F}, 3.2F, "rough"),
        street({-2.0F, 0.0F, 8.0F}, {-2.0F, 0.0F, -8.0F}, 2.8F, "rough"),
    };

    s.resource_patches = {
        patch("boulder", 6, {-14.0F, 0.0F, -14.0F}, {3.4F, 0.0F, 0.0F}, 1.25F),
        patch("iron_ore", 5, {4.0F, 0.0F, -17.0F}, {3.2F, 0.0F, 0.0F}, 1.1F),
        patch("boulder", 4, {10.0F, 0.0F, -10.0F}, {3.0F, 0.0F, 0.0F}, 1.1F),
        patch("tent", 3, {-12.0F, 0.0F, 11.0F}, {4.5F, 0.0F, 0.0F}, 0.85F),
        patch("fire_camp", 1, {-2.0F, 0.0F, 11.5F}, {}, 0.9F),
        patch("supply_cart", 4, {2.0F, 0.0F, 11.0F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("dead_tree", 3, {16.0F, 0.0F, -14.0F}, {4.0F, 0.0F, 0.0F}, 1.0F),
        patch("pine_tree", 4, {-22.0F, 0.0F, -4.0F}, {0.0F, 0.0F, 4.5F}, 1.05F),
    };

    auto quarriers = group(QStringLiteral("quarry_crew"),
                           Troop::Builder,
                           2,
                           4,
                           {-3.0F, 0.0F, 4.0F},
                           1,
                           {3.2F, 0.0F, 0.0F});
    quarriers.nation_id = Nation::Carthage;
    quarriers.ai_controlled = true;

    s.groups = {
        building(QStringLiteral("quarry_depot"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 2,
                 1,
                 {-8.0F, 0.0F, 4.0F},
                 {},
                 180.0F),
        building(QStringLiteral("quarry_quarters"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 2,
                 2,
                 {8.0F, 0.0F, 4.0F},
                 {5.2F, 0.0F, 0.0F},
                 180.0F),
        residents(QStringLiteral("quarry_camp_life"),
                  Nation::Carthage,
                  2,
                  3,
                  {2.0F, 0.0F, 8.0F},
                  {4.0F, 0.0F, 0.0F},
                  12.0F),
        quarriers,
    };

    add_settlement_acceptance(
        s, {QStringLiteral("quarry_depot"), QStringLiteral("quarry_quarters")});
    add_visual_stability(
        s, {QStringLiteral("quarry_camp_life"), QStringLiteral("quarry_crew")});
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("quarry_camp_life")));
    s.expectations.push_back(expectation(
        Expect::OwnerHarvestsResource, QStringLiteral("quarry_crew"), {}, 2.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_trade_road_convoy_id),
        QStringLiteral("Trade Road Convoy"),
        QStringLiteral("Two allied market towns at either end of one paved road, "
                       "with a carrying party walking the whole length of it past "
                       "the milestones while both towns keep working."),
        44.0F,
        {58.0F, 58.0F, 8.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.environment.start_time = 11.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 3, .team_id = 1}};

    s.roads = {
        street({-30.0F, 0.0F, 0.0F}, {30.0F, 0.0F, 0.0F}, 4.0F, "stone"),
        street({-24.0F, 0.0F, 0.0F}, {-24.0F, 0.0F, -10.0F}, 2.8F, "stone"),
        street({24.0F, 0.0F, 0.0F}, {24.0F, 0.0F, 10.0F}, 2.8F, "stone"),
    };

    s.resource_patches = {
        patch("supply_cart", 3, {-20.0F, 0.0F, 3.5F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("supply_cart", 3, {12.0F, 0.0F, -3.5F}, {3.4F, 0.0F, 0.0F}, 0.95F),
        patch("tent", 2, {-4.0F, 0.0F, 4.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("fire_camp", 1, {0.0F, 0.0F, -4.0F}, {}, 0.85F),
        patch("olive_tree", 4, {-14.0F, 0.0F, -12.0F}, {5.0F, 0.0F, 0.0F}, 1.1F),
        patch("olive_tree", 4, {6.0F, 0.0F, 11.0F}, {5.0F, 0.0F, 0.0F}, 1.1F),
        patch("plant", 8, {-10.0F, 0.0F, 3.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    s.groups = {
        building(QStringLiteral("west_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-24.0F, 0.0F, -6.0F},
                 {},
                 180.0F),
        building(QStringLiteral("west_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 2,
                 {-26.0F, 0.0F, 6.0F},
                 {5.2F, 0.0F, 0.0F}),
        building(QStringLiteral("east_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::Carthage,
                 3,
                 1,
                 {24.0F, 0.0F, 6.0F}),
        building(QStringLiteral("east_houses"),
                 Game::Units::SpawnType::Home,
                 Nation::Carthage,
                 3,
                 2,
                 {26.0F, 0.0F, -6.0F},
                 {5.2F, 0.0F, 0.0F},
                 180.0F),
        residents(QStringLiteral("west_town_folk"),
                  Nation::RomanRepublic,
                  1,
                  3,
                  {-24.0F, 0.0F, 0.0F},
                  {4.0F, 0.0F, 0.0F},
                  11.0F),
        residents(QStringLiteral("east_town_folk"),
                  Nation::Carthage,
                  3,
                  3,
                  {24.0F, 0.0F, 0.0F},
                  {4.0F, 0.0F, 0.0F},
                  11.0F),
        group(QStringLiteral("convoy"),
              Troop::Civilian,
              1,
              3,
              {-18.0F, 0.0F, 0.0F},
              1,
              {2.6F, 0.0F, 0.0F}),
    };

    auto haul = at(1.0F, Command::FormationMove, QStringLiteral("convoy"));
    haul.destination = {18.0F, 0.0F, 0.0F};
    s.steps = {haul};

    add_settlement_acceptance(s,
                              {QStringLiteral("west_market"),
                               QStringLiteral("west_houses"),
                               QStringLiteral("east_market"),
                               QStringLiteral("east_houses")});
    add_visual_stability(s,
                         {QStringLiteral("west_town_folk"),
                          QStringLiteral("east_town_folk"),
                          QStringLiteral("convoy")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("convoy")));
    s.expectations.push_back(expectation(Expect::MovementAnimationObserved,
                                         QStringLiteral("west_town_folk")));
    auto arrived = expectation(Expect::GroupReachedDestination,
                               QStringLiteral("convoy"),
                               {},
                               0.0F,
                               0.0F,
                               4.0F);
    arrived.position = haul.destination;
    s.expectations.push_back(arrived);
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_water_showcase_id),
        QStringLiteral("River and Lake Water Showcase"),
        QStringLiteral(
            "Places a flowing river and an irregular calm lake side by side for "
            "shared material, foam, shoreline, depth, and silhouette review."),
        10.0F,
        {42.0F, 56.0F, 18.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.rivers.push_back(
        Game::Map::RiverSegment{{-12.0F, 0.0F, -28.0F}, {-10.0F, 0.0F, 28.0F}, 5.5F});
    s.lakes.push_back(Game::Map::Lake{{10.0F, 0.0F, 1.0F}, 19.0F, 14.0F, -18.0F});
    auto observer = group(
        QStringLiteral("water_observer"), Troop::Archer, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    observer.nation_id = Nation::Carthage;
    s.groups = {observer};
    s.expectations = {
        expectation(Expect::GroupIsRendered, QStringLiteral("water_observer")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wall_corner_showcase_id),
        QStringLiteral("Wall Corner Showcase"),
        QStringLiteral(
            "Closed Roman and Carthaginian palisade rings that exercise every "
            "join shape: four outer corners, a tee spur reaching an inner "
            "corner, a four way crossing, free ends and an isolated stub. The "
            "fixed camera frames both rings so joins and wall bases can be "
            "reviewed for clean merges without overlaps, duplicate posts, or "
            "floor gaps."),
        8.0F,
        {34.0F, 44.0F, 22.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 1.0F);

    auto ring = [](const QString& prefix,
                   Nation nation,
                   int owner,
                   float cx) -> std::vector<ArenaScenarioGroup> {
      return {
          building(prefix + QStringLiteral("_north"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   7,
                   {cx, 0.0F, -6.0F},
                   {2.0F, 0.0F, 0.0F}),
          building(prefix + QStringLiteral("_south"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   7,
                   {cx, 0.0F, 6.0F},
                   {2.0F, 0.0F, 0.0F}),
          building(prefix + QStringLiteral("_west"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   5,
                   {cx - 6.0F, 0.0F, 0.0F},
                   {0.0F, 0.0F, 2.0F},
                   90.0F),
          building(prefix + QStringLiteral("_east"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   5,
                   {cx + 6.0F, 0.0F, 0.0F},
                   {0.0F, 0.0F, 2.0F},
                   90.0F),

          building(prefix + QStringLiteral("_spur"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   2,
                   {cx, 0.0F, -3.0F},
                   {0.0F, 0.0F, 2.0F},
                   90.0F),

          building(prefix + QStringLiteral("_cross_inner"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   1,
                   {cx - 4.0F, 0.0F, 0.0F}),
          building(prefix + QStringLiteral("_cross_outer"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   1,
                   {cx - 8.0F, 0.0F, 0.0F}),

          building(prefix + QStringLiteral("_stub"),
                   Game::Units::SpawnType::WallSegment,
                   nation,
                   owner,
                   1,
                   {cx + 2.0F, 0.0F, 2.0F}),
      };
    };

    const auto roman =
        ring(QStringLiteral("roman_ring"), Nation::RomanRepublic, 1, -8.0F);
    const auto punic = ring(QStringLiteral("punic_ring"), Nation::Carthage, 2, 8.0F);
    s.groups.insert(s.groups.end(), roman.begin(), roman.end());
    s.groups.insert(s.groups.end(), punic.begin(), punic.end());

    add_settlement_acceptance(s,
                              {QStringLiteral("roman_ring_north"),
                               QStringLiteral("roman_ring_south"),
                               QStringLiteral("roman_ring_west"),
                               QStringLiteral("roman_ring_east"),
                               QStringLiteral("roman_ring_spur"),
                               QStringLiteral("roman_ring_cross_inner"),
                               QStringLiteral("roman_ring_cross_outer"),
                               QStringLiteral("roman_ring_stub"),
                               QStringLiteral("punic_ring_north"),
                               QStringLiteral("punic_ring_south"),
                               QStringLiteral("punic_ring_west"),
                               QStringLiteral("punic_ring_east"),
                               QStringLiteral("punic_ring_spur"),
                               QStringLiteral("punic_ring_cross_inner"),
                               QStringLiteral("punic_ring_cross_outer"),
                               QStringLiteral("punic_ring_stub")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_roster_lineup_id),
        QStringLiteral("Iron Sepulcher Roster"),
        QStringLiteral("Presents the complete Iron Sepulcher roster - skeleton "
                       "swordsman, skeleton archer, and grave priest - beside a Roman "
                       "and a Carthaginian line for silhouette, scale, and material "
                       "review."),
        10.0F,
        {13.5F, 40.0F, 0.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.groups = {nation_group(QStringLiteral("skeleton_swordsmen"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {-4.5F, 0.0F, 2.0F},
                             18),
                nation_group(QStringLiteral("skeleton_archers"),
                             Troop::SkeletonArcher,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {0.0F, 0.0F, 2.0F},
                             18),
                nation_group(QStringLiteral("grave_priest"),
                             Troop::GravePriest,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {4.5F, 0.0F, 2.0F},
                             1),
                nation_group(QStringLiteral("roman_line"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             1,
                             {-3.0F, 0.0F, -3.0F},
                             15),
                nation_group(QStringLiteral("carthage_line"),
                             Troop::Spearman,
                             Nation::Carthage,
                             3,
                             1,
                             {3.0F, 0.0F, -3.0F},
                             24)};
    for (auto const& name : {QStringLiteral("skeleton_swordsmen"),
                             QStringLiteral("skeleton_archers"),
                             QStringLiteral("grave_priest"),
                             QStringLiteral("roman_line"),
                             QStringLiteral("carthage_line")}) {
      s.expectations.push_back(expectation(Expect::GroupExists, name));
      s.expectations.push_back(expectation(Expect::GroupIsRendered, name));
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_spell_fx_showcase_id),
        QStringLiteral("Iron Sepulcher Spell FX"),
        QStringLiteral("Close visual review of a grave priest casting fireballs "
                       "beside a skeleton guard, including projectile trail, impact "
                       "ignition, and the target's persistent burning treatment."),
        9.0F,
        {6.8F, 27.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(-1.3F, 0.0F, -0.1F);

    auto priest = nation_group(QStringLiteral("grave_priest"),
                               Troop::GravePriest,
                               Nation::IronSepulcher,
                               2,
                               1,
                               {-1.2F, 0.0F, -2.2F},
                               1);
    auto guard = nation_group(QStringLiteral("skeleton_guard"),
                              Troop::SkeletonSwordsman,
                              Nation::IronSepulcher,
                              2,
                              1,
                              {-3.0F, 0.0F, -1.5F},
                              1);
    guard.health_override = 420;
    guard.max_health_override = 1400;
    auto target = nation_group(QStringLiteral("roman_target"),
                               Troop::Swordsman,
                               Nation::RomanRepublic,
                               1,
                               1,
                               {-1.2F, 0.0F, 2.0F},
                               1);
    target.health_override = target.max_health_override = 1400;
    s.groups = {std::move(priest), std::move(guard), std::move(target)};
    s.steps = {at(0.45F,
                  Command::Attack,
                  QStringLiteral("grave_priest"),
                  QStringLiteral("roman_target"))};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("grave_priest")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("skeleton_guard")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("roman_target")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("grave_priest"),
                                         QStringLiteral("roman_target")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_fireball_review_id),
        QStringLiteral("Iron Sepulcher Fireball Review"),
        QStringLiteral("Fireball close-up on clean ground: no melee, no dust, no "
                       "burning bystanders. The camera sits on the flight path so "
                       "the cast, the ball in flight, its smoke trail and the "
                       "detonation can be judged frame by frame."),
        14.0F,
        {8.0F, 24.0F, 90.0F});
    s.suppress_terrain_scatter = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.camera_focus = QVector3D(-1.2F, 0.9F, 0.0F);

    auto priest = nation_group(QStringLiteral("fireball_caster"),
                               Troop::GravePriest,
                               Nation::IronSepulcher,
                               2,
                               1,
                               {-1.2F, 0.0F, -3.4F},
                               1);
    auto target = nation_group(QStringLiteral("fireball_target"),
                               Troop::Swordsman,
                               Nation::RomanRepublic,
                               1,
                               1,
                               {-1.2F, 0.0F, 3.4F},
                               1);

    target.health_override = target.max_health_override = 20000;
    s.groups = {std::move(priest), std::move(target)};
    s.steps = {at(0.4F,
                  Command::Attack,
                  QStringLiteral("fireball_caster"),
                  QStringLiteral("fireball_target")),
               at(0.4F, Command::Hold, QStringLiteral("fireball_target"))};
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("fireball_caster")));
    s.expectations.push_back(
        expectation(Expect::GroupExists, QStringLiteral("fireball_target")));
    s.expectations.push_back(expectation(Expect::ProjectileFlightObserved,
                                         QStringLiteral("fireball_caster"),
                                         QStringLiteral("fireball_target")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactObserved,
                                         QStringLiteral("fireball_caster"),
                                         QStringLiteral("fireball_target")));
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_rome_infantry_id),
        QStringLiteral("Sepulcher vs Rome: Infantry"),
        QStringLiteral("Equivalent-value melee test: three Roman swordsmen against a "
                       "skeleton warband of equal recruitment value. No eligible "
                       "soldier on either side may idle once the lines meet."),
        16.0F,
        {18.0F, 44.0F, 28.0F});
    s.groups = {nation_group(QStringLiteral("roman_swords"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -6.0F}),
                nation_group(QStringLiteral("skeleton_swords"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             3,
                             {-1.5F, 0.0F, 6.0F}),
                nation_group(QStringLiteral("skeleton_bows"),
                             Troop::SkeletonArcher,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {5.5F, 0.0F, 8.5F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("roman_swords"),
                  QStringLiteral("skeleton_swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("skeleton_swords"),
                  QStringLiteral("roman_swords")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("skeleton_bows"),
                  QStringLiteral("roman_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("roman_swords"),
                          QStringLiteral("skeleton_swords"),
                          QStringLiteral("skeleton_bows")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("roman_swords"), {}, 0.45F));
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("skeleton_swords"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("roman_swords")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("skeleton_swords")));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("skeleton_swords"),
                                         QStringLiteral("roman_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_rome_ranged_id),
        QStringLiteral("Sepulcher vs Rome: Ranged"),
        QStringLiteral("Missile exchange between a Roman archer line with a swordsman "
                       "screen and cursed skeleton archers led by a grave priest."),
        16.0F,
        {24.0F, 50.0F, 26.0F});
    s.groups = {nation_group(QStringLiteral("roman_bows"),
                             Troop::Archer,
                             Nation::RomanRepublic,
                             1,
                             4,
                             {0.0F, 0.0F, -10.0F}),
                nation_group(QStringLiteral("roman_screen"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             1,
                             {0.0F, 0.0F, -6.0F}),
                nation_group(QStringLiteral("skeleton_bows"),
                             Troop::SkeletonArcher,
                             Nation::IronSepulcher,
                             2,
                             3,
                             {0.0F, 0.0F, 10.0F}),
                nation_group(QStringLiteral("grave_priest"),
                             Troop::GravePriest,
                             Nation::IronSepulcher,
                             2,
                             1,
                             {6.5F, 0.0F, 13.0F},
                             1)};
    s.select_spawned_units = false;
    s.steps = {at(0.5F,
                  Command::Attack,
                  QStringLiteral("roman_bows"),
                  QStringLiteral("skeleton_bows")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("skeleton_bows"),
                  QStringLiteral("roman_bows")),
               at(0.5F,
                  Command::Attack,
                  QStringLiteral("grave_priest"),
                  QStringLiteral("roman_screen")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("roman_screen"),
                  QStringLiteral("skeleton_bows"))};
    add_visual_stability(s,
                         {QStringLiteral("roman_bows"),
                          QStringLiteral("roman_screen"),
                          QStringLiteral("skeleton_bows"),
                          QStringLiteral("grave_priest")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("skeleton_bows"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("roman_bows")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("skeleton_bows")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("roman_bows"),
                                         QStringLiteral("skeleton_bows")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("skeleton_bows"),
                                         QStringLiteral("roman_bows")));
    s.expectations.push_back(expectation(Expect::ProjectileImpactSynchronized,
                                         QStringLiteral("grave_priest"),
                                         QStringLiteral("roman_screen")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_carthage_infantry_id),
        QStringLiteral("Sepulcher vs Carthage: Infantry"),
        QStringLiteral("Equivalent-value melee test: a mixed Carthaginian sword and "
                       "spear line against four skeleton swordsmen."),
        16.0F,
        {23.0F, 48.0F, 28.0F});
    s.groups = {nation_group(QStringLiteral("punic_swords"),
                             Troop::Swordsman,
                             Nation::Carthage,
                             1,
                             2,
                             {-3.0F, 0.0F, -9.0F}),
                nation_group(QStringLiteral("punic_spears"),
                             Troop::Spearman,
                             Nation::Carthage,
                             1,
                             2,
                             {4.0F, 0.0F, -9.0F}),
                nation_group(QStringLiteral("skeleton_swords"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             4,
                             {0.0F, 0.0F, 9.0F})};
    s.steps = {at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("punic_swords"),
                  QStringLiteral("skeleton_swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("punic_spears"),
                  QStringLiteral("skeleton_swords")),
               at(0.5F,
                  Command::AttackMove,
                  QStringLiteral("skeleton_swords"),
                  QStringLiteral("punic_swords"))};
    add_visual_stability(s,
                         {QStringLiteral("punic_swords"),
                          QStringLiteral("punic_spears"),
                          QStringLiteral("skeleton_swords")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("punic_spears"), {}, 0.45F));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_swords")));
    s.expectations.push_back(expectation(Expect::AttackAnimationObserved,
                                         QStringLiteral("skeleton_swords")));
    s.expectations.push_back(expectation(Expect::NoEligibleTroopIdleDuringCombat,
                                         QStringLiteral("skeleton_swords"),
                                         QStringLiteral("punic_swords"),
                                         1.25F,
                                         1.0F,
                                         8.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_vs_carthage_cavalry_id),
        QStringLiteral("Sepulcher vs Carthage: Cavalry"),
        QStringLiteral("Carthaginian cavalry charges a standing skeleton block to "
                       "prove impact displacement, contact damage, and melee lock "
                       "against undead formations."),
        14.0F,
        {18.0F, 44.0F, 20.0F});
    s.groups = {nation_group(QStringLiteral("punic_cavalry"),
                             Troop::MountedKnight,
                             Nation::Carthage,
                             1,
                             2,
                             {0.0F, 0.0F, -7.0F},
                             4),
                nation_group(QStringLiteral("skeleton_block"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             2,
                             3,
                             {0.0F, 0.0F, 5.0F})};
    s.steps = {at(0.0F,
                  Command::Charge,
                  QStringLiteral("punic_cavalry"),
                  QStringLiteral("skeleton_block")),
               when_near(QStringLiteral("punic_cavalry"),
                         QStringLiteral("skeleton_block"),
                         4.5F,
                         Command::SetCamera)};
    s.steps.back().camera_distance = 15.0F;
    s.steps.back().camera_angle = 48.0F;
    s.steps.back().camera_yaw = 20.0F;
    add_visual_stability(
        s, {QStringLiteral("punic_cavalry"), QStringLiteral("skeleton_block")});
    s.expectations.push_back(expectation(
        Expect::AllGroupsRespondWithin, QStringLiteral("punic_cavalry"), {}, 0.45F));
    s.expectations.push_back(expectation(Expect::AttackHasVisibleContact,
                                         QStringLiteral("punic_cavalry"),
                                         QStringLiteral("skeleton_block")));
    s.expectations.push_back(expectation(Expect::ChargeImpactPrecedesMeleeLock,
                                         QStringLiteral("punic_cavalry")));
    s.expectations.push_back(
        expectation(Expect::DeathAnimationObserved, QStringLiteral("skeleton_block")));
    s.expectations.push_back(expectation(Expect::LaunchedCasualtyObserved,
                                         QStringLiteral("skeleton_block")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_awakening_id),
        QStringLiteral("Sepulcher Shrine Awakening"),
        QStringLiteral("A cursed shrine stands alone on empty ground. Roman swordsmen "
                       "advance into its radius, the sepulcher wakes, and the summoned "
                       "guardians fight the intruders."),
        26.0F,
        {28.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.resource_patches = {{QStringLiteral("magic_shrine"),
                           1,
                           QVector3D(0.0F, 0.0F, 6.0F),
                           QVector3D(0.0F, 0.0F, 0.0F),
                           1.0F}};

    s.undead_zones = {undead_zone(QStringLiteral("shrine_sentinels"),
                                  Game::Map::WorldProp::Type::MagicShrine,
                                  QVector3D(0.0F, 0.0F, 6.0F),
                                  6.0F,
                                  99,
                                  {})};
    s.groups = {nation_group(QStringLiteral("intruders"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("intruders"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("intruders")});
    s.expectations.push_back(
        expectation(Expect::MovementAnimationObserved, QStringLiteral("intruders")));
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("intruders")));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneDormantBefore,
                                              QStringLiteral("shrine_sentinels"),
                                              0.0F,
                                              2.0F));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("shrine_sentinels"), 4.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_ruins_awakening_waves_id),
        QStringLiteral("Sepulcher Ruins Awakening Waves"),
        QStringLiteral("A Carthaginian column enters sepulcher ruins, clears the "
                       "opening guardians, and is met by the follow-up wave that the "
                       "zone releases only after the first is destroyed."),
        45.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.resource_patches = {{QStringLiteral("ruins"),
                           1,
                           QVector3D(0.0F, 0.0F, 6.0F),
                           QVector3D(0.0F, 0.0F, 0.0F),
                           1.1F}};
    s.undead_zones = {
        undead_zone(QStringLiteral("ruins_guard"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 1}}),
                     undead_wave(QStringLiteral("after_clear"),
                                 {{Game::Units::SpawnType::SkeletonArcher, 1}})})};
    s.groups = {nation_group(QStringLiteral("punic_column"),
                             Troop::Swordsman,
                             Nation::Carthage,
                             1,
                             4,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("punic_column"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("punic_column")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_column")));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneDormantBefore, QStringLiteral("ruins_guard"), 0.0F, 2.0F));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("ruins_guard"), 2.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_siege_id),
        QStringLiteral("Sepulcher Shrine Siege"),
        QStringLiteral("A Roman assault wakes the shrine and fights for its flag. "
                       "The shrine is the sepulcher's barracks, but it cannot be "
                       "taken while a single guardian still stands, so the column "
                       "has to break the garrison before the banner comes down."),
        90.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.resource_patches = {{QStringLiteral("magic_shrine"),
                           1,
                           QVector3D(0.0F, 0.0F, 6.0F),
                           QVector3D(0.0F, 0.0F, 0.0F),
                           1.0F}};

    s.undead_zones = {
        undead_zone(QStringLiteral("shrine_sentinels"),
                    Game::Map::WorldProp::Type::MagicShrine,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("assault"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             6,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("assault"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("assault")});
    s.expectations.push_back(
        expectation(Expect::AttackAnimationObserved, QStringLiteral("assault")));
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("shrine_sentinels"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneCleared,
                                              QStringLiteral("shrine_sentinels")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_zone_shrine_spawn_id),
        QStringLiteral("Sepulcher Zone Shrine Spawn"),
        QStringLiteral("Bare ground, no authored prop: the awakening zone raises "
                       "its own magic shrine at the centre. Roman scouts walk in, "
                       "the zone wakes, and the shrine stands through the fight as "
                       "the sepulcher's barracks."),
        26.0F,
        {28.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("bare_barrow"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("scouts"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("scouts"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);
    add_visual_stability(s, {QStringLiteral("scouts")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("bare_barrow"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneShrineStands,
                                              QStringLiteral("bare_barrow")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_twin_zone_shrines_id),
        QStringLiteral("Sepulcher Twin Zone Shrines"),
        QStringLiteral("Two awakening zones share one field. Each raises its own "
                       "shrine, and a Roman column walks into each of them, so the "
                       "two garrisons and their two barracks stand side by side."),
        30.0F,
        {34.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("west_barrow"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(-10.0F, 0.0F, 6.0F),
                    5.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 1}})}),
        undead_zone(QStringLiteral("east_barrow"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(10.0F, 0.0F, 6.0F),
                    5.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 1}})})};
    s.groups = {nation_group(QStringLiteral("west_column"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {-10.0F, 0.0F, -12.0F}),
                nation_group(QStringLiteral("east_column"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {10.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("west_column")),
               at(1.0F, Command::FormationMove, QStringLiteral("east_column"))};
    s.steps[0].destination = QVector3D(-10.0F, 0.0F, 4.0F);
    s.steps[1].destination = QVector3D(10.0F, 0.0F, 4.0F);
    add_visual_stability(
        s, {QStringLiteral("west_column"), QStringLiteral("east_column")});
    for (auto const& zone_id :
         {QStringLiteral("west_barrow"), QStringLiteral("east_barrow")}) {
      s.expectations.push_back(
          zone_expectation(Expect::UndeadZoneAwakened, zone_id, 1.0F));
      s.expectations.push_back(
          zone_expectation(Expect::UndeadZoneShrineStands, zone_id));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_demolition_id),
        QStringLiteral("Sepulcher Shrine Demolition"),
        QStringLiteral("The zone wakes on a Roman assault, then its shrine is "
                       "brought down. Losing the barracks crumbles every risen "
                       "guardian and leaves the ground quiet."),
        32.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("doomed_shrine"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("breakers"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             4,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("breakers"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);

    ArenaScenarioStep raze;
    raze.name = QStringLiteral("raze_shrine");
    raze.trigger = {Trigger::AtTime, 10.0F, {}, {}, 0.0F};
    raze.command = Command::ApplyDamage;
    raze.zone_id = QStringLiteral("doomed_shrine");
    raze.value = 100000;
    s.steps.push_back(std::move(raze));

    add_visual_stability(s, {QStringLiteral("breakers")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("doomed_shrine"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneShrineDestroyed,
                                              QStringLiteral("doomed_shrine")));
    s.expectations.push_back(
        zone_expectation(Expect::UndeadZoneCleared, QStringLiteral("doomed_shrine")));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_sepulcher_shrine_state_reload_id),
        QStringLiteral("Sepulcher Shrine State Reload"),
        QStringLiteral("A woken zone is written out and read back the way a saved "
                       "game does it. The reload must keep the shrine that already "
                       "stands and the guardians already raised, without planting a "
                       "second shrine or a fresh wave."),
        28.0F,
        {30.0F, 50.0F, 24.0F});
    s.suppress_spawn_anchor = true;
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.undead_zones = {
        undead_zone(QStringLiteral("saved_shrine"),
                    Game::Map::WorldProp::Type::Ruins,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    6.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 2}})})};
    s.groups = {nation_group(QStringLiteral("visitors"),
                             Troop::Swordsman,
                             Nation::RomanRepublic,
                             1,
                             3,
                             {0.0F, 0.0F, -12.0F})};
    s.steps = {at(1.0F, Command::FormationMove, QStringLiteral("visitors"))};
    s.steps.back().destination = QVector3D(0.0F, 0.0F, 4.0F);

    ArenaScenarioStep reload;
    reload.name = QStringLiteral("reload_zone_state");
    reload.trigger = {Trigger::AtTime, 10.0F, {}, {}, 0.0F};
    reload.command = Command::ReloadUndeadZoneState;
    s.steps.push_back(std::move(reload));

    add_visual_stability(s, {QStringLiteral("visitors")});
    s.expectations.push_back(zone_expectation(
        Expect::UndeadZoneAwakened, QStringLiteral("saved_shrine"), 2.0F));
    s.expectations.push_back(zone_expectation(Expect::UndeadZoneShrineStands,
                                              QStringLiteral("saved_shrine")));
    result.push_back(std::move(s));
  }

  for (const auto& fixture : std::array{
           std::tuple{k_lighting_sunrise_sunset_id, "Lighting: Sunrise", 6.25F, 0.0F},
           std::tuple{k_lighting_midday_id, "Lighting: Midday", 12.0F, 0.0F},
           std::tuple{k_lighting_sunset_id, "Lighting: Sunset", 19.25F, 0.0F},
           std::tuple{
               k_lighting_moonlit_night_id, "Lighting: Moonlit Night", 0.5F, 0.0F},
           std::tuple{k_lighting_heavy_rain_id, "Lighting: Heavy Rain", 15.0F, 0.9F}}) {
    auto s =
        definition(QString::fromLatin1(std::get<0>(fixture)),
                   QString::fromLatin1(std::get<1>(fixture)),
                   QStringLiteral("Locked-camera environment regression fixture "
                                  "covering troops, props, ground, and structures."),
                   8.0F,
                   {24.0F, 48.0F, 32.0F});
    s.environment.start_time = std::get<2>(fixture);
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.weather.rain = std::get<3>(fixture);
    s.weather.storm = std::get<3>(fixture) * 0.55F;
    s.groups = {group(QStringLiteral("roman_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-5.0F, 0.0F, 0.0F},
                      8,
                      {0.0F, 0.0F, 2.5F}),
                nation_group(QStringLiteral("carthage_line"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             3,
                             {5.0F, 0.0F, 0.0F},
                             8,
                             {0.0F, 0.0F, 2.5F}),
                building(QStringLiteral("home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-10.0F, 0.0F, 6.0F})};
    s.resource_patches = {{QStringLiteral("pine"),
                           4,
                           QVector3D(10.0F, 0.0F, 4.0F),
                           QVector3D(2.0F, 0.0F, 1.0F),
                           1.0F}};
    add_visual_stability(
        s, {QStringLiteral("roman_line"), QStringLiteral("carthage_line")});
    result.push_back(std::move(s));
  }

  struct PrecipitationFixture {
    const char* id;
    const char* title;
    bool snow;
    float intensity;
    float wind_strength;
    float wind_direction_deg;
    float hour;
    Render::GraphicsQuality quality;
  };

  for (const auto& fixture :
       std::array{PrecipitationFixture{k_weather_rain_light_id,
                                       "Weather: Light Rain",
                                       false,
                                       Game::Map::k_weather_intensity_light,
                                       0.15F,
                                       20.0F,
                                       14.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_rain_medium_id,
                                       "Weather: Medium Rain",
                                       false,
                                       Game::Map::k_weather_intensity_medium,
                                       0.30F,
                                       145.0F,
                                       14.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_rain_heavy_id,
                                       "Weather: Heavy Rain",
                                       false,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.60F,
                                       250.0F,
                                       14.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_light_id,
                                       "Weather: Light Snow",
                                       true,
                                       Game::Map::k_weather_intensity_light,
                                       0.15F,
                                       70.0F,
                                       11.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_medium_id,
                                       "Weather: Medium Snow",
                                       true,
                                       Game::Map::k_weather_intensity_medium,
                                       0.32F,
                                       70.0F,
                                       11.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_heavy_id,
                                       "Weather: Heavy Snow",
                                       true,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.65F,
                                       330.0F,
                                       11.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_crosswind_id,
                                       "Weather: Snow Crosswind",
                                       true,
                                       Game::Map::k_weather_intensity_medium,
                                       1.20F,
                                       270.0F,
                                       17.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_rain_budget_low_id,
                                       "Weather: Rain Budget (Low)",
                                       false,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.35F,
                                       145.0F,
                                       14.0F,
                                       Render::GraphicsQuality::Low},
                  PrecipitationFixture{k_weather_rain_budget_ultra_id,
                                       "Weather: Rain Budget (Ultra)",
                                       false,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.35F,
                                       145.0F,
                                       14.0F,
                                       Render::GraphicsQuality::Ultra}}) {
    auto s = definition(
        QString::fromLatin1(fixture.id),
        QString::fromLatin1(fixture.title),
        QStringLiteral("Locked-camera precipitation fixture: troops, structures and "
                       "vegetation under one authored weather setting."),
        8.0F,
        {24.0F, 48.0F, 32.0F});
    s.environment.start_time = fixture.hour;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.graphics_quality = fixture.quality;
    if (fixture.snow) {
      s.weather.snow = fixture.intensity;
    } else {
      s.weather.rain = fixture.intensity;
      s.weather.storm = fixture.intensity * 0.35F;
    }
    s.precipitation.enabled = true;
    s.precipitation.type =
        fixture.snow ? Game::Map::WeatherType::Snow : Game::Map::WeatherType::Rain;
    s.precipitation.intensity = fixture.intensity;
    s.precipitation.wind_strength = fixture.wind_strength;
    s.precipitation.wind_direction_deg = fixture.wind_direction_deg;
    s.groups = {group(QStringLiteral("roman_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-5.0F, 0.0F, 0.0F},
                      8,
                      {0.0F, 0.0F, 2.5F}),
                nation_group(QStringLiteral("carthage_line"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             3,
                             {5.0F, 0.0F, 0.0F},
                             8,
                             {0.0F, 0.0F, 2.5F}),
                building(QStringLiteral("home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-10.0F, 0.0F, 6.0F})};
    s.resource_patches = {{QStringLiteral("pine"),
                           4,
                           QVector3D(10.0F, 0.0F, 4.0F),
                           QVector3D(2.0F, 0.0F, 1.0F),
                           1.0F}};
    add_visual_stability(
        s, {QStringLiteral("roman_line"), QStringLiteral("carthage_line")});
    result.push_back(std::move(s));
  }

  for (const auto& transition : std::array{
           std::tuple{k_lighting_dawn_to_day_id,
                      "Lighting: Dawn To Day",
                      "Continuous clock sweeping dawn into full day to verify the "
                      "lighting curves interpolate without visible jumps.",
                      5.0F},
           std::tuple{k_lighting_afternoon_to_night_id,
                      "Lighting: Afternoon To Night",
                      "Continuous clock sweeping late afternoon into night to verify "
                      "sun-to-moon handover and shadow softening.",
                      17.0F}}) {
    auto s = definition(QString::fromLatin1(std::get<0>(transition)),
                        QString::fromLatin1(std::get<1>(transition)),
                        QString::fromLatin1(std::get<2>(transition)),
                        12.0F,
                        {26.0F, 46.0F, 30.0F});
    s.environment.start_time = std::get<3>(transition);
    s.environment.time_mode = Game::Map::TimeMode::Continuous;
    s.environment.day_length_seconds = 90.0F;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("roman_line"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-4.0F, 0.0F, 0.0F},
                      6,
                      {0.0F, 0.0F, 2.5F}),
                building(QStringLiteral("transition_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {8.0F, 0.0F, 4.0F})};
    add_visual_stability(s, {QStringLiteral("roman_line")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_structure_shadows_id),
        QStringLiteral("Lighting: Structure Shadows"),
        QStringLiteral("Low sun across walls, a defense tower and a barracks so "
                       "structure shadow casting and unit-in-shadow reception can be "
                       "reviewed together."),
        8.0F,
        {30.0F, 34.0F, 18.0F});
    s.environment.start_time = 7.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    s.groups = {building(QStringLiteral("shadow_wall"),
                         Game::Units::SpawnType::WallSegment,
                         Nation::RomanRepublic,
                         1,
                         9,
                         {-8.0F, 0.0F, -6.0F},
                         {2.0F, 0.0F, 0.0F}),
                building(QStringLiteral("shadow_tower"),
                         Game::Units::SpawnType::DefenseTower,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-10.0F, 0.0F, -6.0F}),
                building(QStringLiteral("shadow_barracks"),
                         Game::Units::SpawnType::Barracks,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {6.0F, 0.0F, -6.0F}),

                group(QStringLiteral("shaded_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-6.0F, 0.0F, 2.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    add_visual_stability(s, {QStringLiteral("shaded_line")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_sepulcher_readability_id),
        QStringLiteral("Lighting: Sepulcher Readability"),
        QStringLiteral("Iron Sepulcher roster beside Roman and Carthaginian lines at "
                       "night, checking pale standards and cold stone stay solemn and "
                       "every nation stays readable in deep shadow."),
        8.0F,
        {22.0F, 40.0F, 0.0F});
    s.environment.start_time = 1.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("rome_readable"),
                      Troop::Swordsman,
                      1,
                      1,
                      {-6.0F, 0.0F, 0.0F},
                      4,
                      {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("carthage_readable"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             1,
                             {0.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("sepulcher_readable"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             3,
                             1,
                             {6.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 2.2F})};
    s.resource_patches = {
        {QStringLiteral("firecamp"), 2, {-3.0F, 0.0F, 5.0F}, {6.0F, 0.0F, 0.0F}, 1.0F}};
    add_visual_stability(s,
                         {QStringLiteral("rome_readable"),
                          QStringLiteral("carthage_readable"),
                          QStringLiteral("sepulcher_readable")});
    result.push_back(std::move(s));
  }

  for (const auto& parity : std::array{std::tuple{k_lighting_parity_instanced_id,
                                                  "Lighting: Parity (Instanced)",
                                                  false},
                                       std::tuple{k_lighting_parity_single_id,
                                                  "Lighting: Parity (Non-Instanced)",
                                                  true}}) {
    auto s = definition(
        QString::fromLatin1(std::get<0>(parity)),
        QString::fromLatin1(std::get<1>(parity)),
        QStringLiteral("Paired fixture whose twin differs only in creature LOD "
                       "forcing; the two captures must match to prove instanced and "
                       "non-instanced paths light identically."),
        8.0F,
        {18.0F, 38.0F, 0.0F});
    s.environment.start_time = 16.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.force_full_creature_lod = std::get<2>(parity);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("parity_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-4.0F, 0.0F, 0.0F},
                      6,
                      {4.0F, 0.0F, 0.0F})};
    s.resource_patches = {
        {QStringLiteral("tent"), 1, {-6.0F, 0.0F, 5.0F}, {}, 1.0F},
        {QStringLiteral("supply_cart"), 1, {0.0F, 0.0F, 5.0F}, {}, 1.0F},
        {QStringLiteral("weapon_rack"), 1, {6.0F, 0.0F, 5.0F}, {}, 1.0F},
    };
    add_visual_stability(s, {QStringLiteral("parity_line")});
    result.push_back(std::move(s));
  }

  for (const auto& quality : std::array{std::tuple{k_lighting_shadow_quality_low_id,
                                                   "Lighting: Shadow Quality Low",
                                                   Render::GraphicsQuality::Low},
                                        std::tuple{k_lighting_shadow_quality_medium_id,
                                                   "Lighting: Shadow Quality Medium",
                                                   Render::GraphicsQuality::Medium},
                                        std::tuple{k_lighting_shadow_quality_high_id,
                                                   "Lighting: Shadow Quality High",
                                                   Render::GraphicsQuality::High},
                                        std::tuple{k_lighting_shadow_quality_ultra_id,
                                                   "Lighting: Shadow Quality Ultra",
                                                   Render::GraphicsQuality::Ultra}}) {
    auto s = definition(
        QString::fromLatin1(std::get<0>(quality)),
        QString::fromLatin1(std::get<1>(quality)),
        QStringLiteral("Large formations under a fixed low sun at one shadow quality "
                       "preset, for cross-quality shadow and frame-time comparison."),
        10.0F,
        {40.0F, 52.0F, 22.0F});
    s.environment.start_time = 17.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.graphics_quality = std::get<2>(quality);
    s.force_full_creature_lod = false;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("quality_rome"),
                      Troop::Swordsman,
                      1,
                      8,
                      {-11.0F, 0.0F, 0.0F},
                      12,
                      {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("quality_carthage"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             8,
                             {11.0F, 0.0F, 0.0F},
                             12,
                             {0.0F, 0.0F, 2.2F})};
    add_visual_stability(
        s, {QStringLiteral("quality_rome"), QStringLiteral("quality_carthage")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_commander_closeup_id),
        QStringLiteral("Lighting: Commander Close-Up"),
        QStringLiteral("Tight commander-mode framing at low sun so near-cascade "
                       "shadow detail and armour readability can be inspected."),
        8.0F,
        {7.0F, 22.0F, 35.0F});
    s.environment.start_time = 8.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.force_full_creature_lod = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("closeup_commander"),
                      Troop::RomanVeteranConsul,
                      1,
                      1,
                      {0.0F, 0.0F, 0.0F},
                      1),
                group(QStringLiteral("closeup_escort"),
                      Troop::Swordsman,
                      1,
                      1,
                      {2.4F, 0.0F, 1.2F},
                      2,
                      {1.6F, 0.0F, 0.0F})};
    add_visual_stability(
        s, {QStringLiteral("closeup_commander"), QStringLiteral("closeup_escort")});
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_lighting_dense_battle_id),
                   QStringLiteral("Lighting: Dense Battle"),
                   QStringLiteral("High-density formation fixture for shadow budgets, "
                                  "instancing parity, and frame-time tracking."),
                   12.0F,
                   {42.0F, 58.0F, 25.0F});
    s.environment.start_time = 17.5F;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.force_full_creature_lod = false;
    s.groups = {group(QStringLiteral("roman_mass"),
                      Troop::Swordsman,
                      1,
                      10,
                      {-12.0F, 0.0F, 0.0F},
                      16,
                      {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("carthage_mass"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             10,
                             {12.0F, 0.0F, 0.0F},
                             16,
                             {0.0F, 0.0F, 2.2F})};
    s.steps = {at(1.0F,
                  Command::AttackMove,
                  QStringLiteral("roman_mass"),
                  QStringLiteral("carthage_mass")),
               at(1.0F,
                  Command::AttackMove,
                  QStringLiteral("carthage_mass"),
                  QStringLiteral("roman_mass"))};
    add_visual_stability(
        s, {QStringLiteral("roman_mass"), QStringLiteral("carthage_mass")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_world_materials_id),
        QStringLiteral("Lighting: World Materials"),
        QStringLiteral("Terrain, water, vegetation, ruins, ore, and building "
                       "fixture for complete shared-lighting coverage."),
        10.0F,
        {34.0F, 55.0F, 30.0F});
    s.environment.start_time = 13.0F;
    s.groups = {building(QStringLiteral("barracks"),
                         Game::Units::SpawnType::Barracks,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-7.0F, 0.0F, 2.0F}),
                building(QStringLiteral("market"),
                         Game::Units::SpawnType::Marketplace,
                         Nation::Carthage,
                         2,
                         1,
                         {7.0F, 0.0F, 2.0F})};
    s.resource_patches = {
        {QStringLiteral("ruins"), 1, {-10.0F, 0.0F, 10.0F}, {}, 1.0F},
        {QStringLiteral("iron_ore"), 3, {0.0F, 0.0F, 10.0F}, {2.0F, 0.0F, 0.0F}, 1.0F},
        {QStringLiteral("olive"), 3, {9.0F, 0.0F, 10.0F}, {2.0F, 0.0F, 0.0F}, 1.0F}};
    add_settlement_acceptance(s,
                              {QStringLiteral("barracks"), QStringLiteral("market")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_last_stand_id),
        QStringLiteral("Promo: The Last Stand"),
        QStringLiteral("Golden-hour capture scene. A Roman shield line holds a "
                       "ridge against a Carthaginian horde while a cavalry wing "
                       "sweeps the archers behind it."),
        26.0F,
        {26.0F, 22.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 18.15F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.environment.exposure_override = 1.0F;

    s.environment.fog_density_override = 0.020F;
    s.groups = {
        group(QStringLiteral("roman_line"),
              Troop::Swordsman,
              1,
              8,
              {-9.1F, 0.0F, -7.0F},
              12),
        group(QStringLiteral("roman_spears"),
              Troop::Spearman,
              1,
              6,
              {-6.5F, 0.0F, -10.5F},
              12),
        group(QStringLiteral("roman_archers"),
              Troop::Archer,
              1,
              4,
              {-3.9F, 0.0F, -14.5F},
              8),
        group(QStringLiteral("roman_consul"),
              Troop::RomanVeteranConsul,
              1,
              1,
              {0.0F, 0.0F, -12.0F},
              1),
        group(QStringLiteral("punic_horde"),
              Troop::Swordsman,
              2,
              10,
              {-11.7F, 0.0F, 8.0F},
              12),
        group(QStringLiteral("punic_spears"),
              Troop::Spearman,
              2,
              8,
              {-9.1F, 0.0F, 12.0F},
              12),
        group(QStringLiteral("punic_cavalry"),
              Troop::MountedKnight,
              2,
              6,
              {16.0F, 0.0F, 6.0F},
              6),
    };
    s.resource_patches = {
        {QStringLiteral("pine"), 5, {-24.0F, 0.0F, -6.0F}, {3.0F, 0.0F, 2.0F}, 1.2F},
        {QStringLiteral("pine"), 4, {20.0F, 0.0F, -14.0F}, {3.0F, 0.0F, 2.0F}, 1.1F}};
    s.steps = {
        at(0.3F, Command::Hold, QStringLiteral("roman_line")),
        at(0.3F, Command::Hold, QStringLiteral("roman_spears")),
        at(0.4F,
           Command::AttackMove,
           QStringLiteral("punic_horde"),
           QStringLiteral("roman_line")),
        at(0.4F,
           Command::AttackMove,
           QStringLiteral("punic_spears"),
           QStringLiteral("roman_spears")),
        at(0.8F,
           Command::Attack,
           QStringLiteral("roman_archers"),
           QStringLiteral("punic_horde")),
        at(5.5F,
           Command::Charge,
           QStringLiteral("punic_cavalry"),
           QStringLiteral("roman_archers")),
        at(9.0F,
           Command::AttackMove,
           QStringLiteral("roman_line"),
           QStringLiteral("punic_horde")),
        at(9.0F,
           Command::AttackMove,
           QStringLiteral("roman_consul"),
           QStringLiteral("punic_horde")),
    };
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("roman_line")),
        expectation(Expect::GroupExists, QStringLiteral("punic_horde")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_line")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_horde")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_cavalry")),
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_horde")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_night_of_the_dead_id),
        QStringLiteral("Promo: Night of the Dead"),
        QStringLiteral("Night capture scene under the Iron Sepulcher profile. A "
                       "Roman column walks a cursed shrine awake and the risen "
                       "garrison closes on it from the haze."),
        30.0F,
        {24.0F, 24.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 2.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.collect_animation_diagnostics = false;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 21.4F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.lighting_profile = QStringLiteral("iron_sepulcher");

    s.environment.exposure_override = 1.45F;
    s.environment.fog_density_override = 0.010F;
    s.resource_patches = {{QStringLiteral("magic_shrine"),
                           1,
                           {0.0F, 0.0F, 6.0F},
                           {0.0F, 0.0F, 0.0F},
                           1.2F},
                          {QStringLiteral("cursed_tree"),
                           4,
                           {-13.0F, 0.0F, 4.0F},
                           {4.0F, 0.0F, 3.0F},
                           1.2F},
                          {QStringLiteral("cursed_tree"),
                           3,
                           {11.0F, 0.0F, 8.0F},
                           {4.0F, 0.0F, 3.0F},
                           1.2F}};
    s.undead_zones = {
        undead_zone(QStringLiteral("shrine_garrison"),
                    Game::Map::WorldProp::Type::MagicShrine,
                    QVector3D(0.0F, 0.0F, 6.0F),
                    9.0F,
                    99,
                    {undead_wave(QStringLiteral("initial"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 6},
                                  {Game::Units::SpawnType::SkeletonArcher, 3},
                                  {Game::Units::SpawnType::GravePriest, 1}}),
                     undead_wave(QStringLiteral("after_clear"),
                                 {{Game::Units::SpawnType::SkeletonSwordsman, 4}})})};
    s.groups = {group(QStringLiteral("roman_column"),
                      Troop::Swordsman,
                      1,
                      6,
                      {-6.5F, 0.0F, -16.0F},
                      10),
                group(QStringLiteral("roman_flank"),
                      Troop::Spearman,
                      1,
                      4,
                      {-3.9F, 0.0F, -20.0F},
                      10)};
    s.steps = {at(1.2F, Command::FormationMove, QStringLiteral("roman_column")),
               at(4.0F, Command::FormationMove, QStringLiteral("roman_flank"))};
    s.steps[0].destination = QVector3D(0.0F, 0.0F, 0.0F);
    s.steps[1].destination = QVector3D(0.0F, 0.0F, -4.0F);
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("roman_column")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_column")),
        expectation(Expect::AttackAnimationObserved, QStringLiteral("roman_column")),
        zone_expectation(Expect::UndeadZoneDormantBefore,
                         QStringLiteral("shrine_garrison"),
                         0.0F,
                         1.5F),
        zone_expectation(
            Expect::UndeadZoneAwakened, QStringLiteral("shrine_garrison"), 6.0F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_storm_charge_id),
        QStringLiteral("Promo: Storm Charge"),
        QStringLiteral("Storm capture scene. Roman cavalry charges a braced "
                       "Carthaginian spear wall through driving rain while both "
                       "infantry lines close behind them."),
        24.0F,
        {22.0F, 20.0F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.collect_animation_diagnostics = false;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 16.2F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.exposure_override = 1.0F;
    s.weather.rain = 0.85F;
    s.weather.storm = 0.65F;
    s.precipitation.enabled = true;
    s.precipitation.type = Game::Map::WeatherType::Rain;
    s.precipitation.intensity = 0.85F;
    s.precipitation.wind_strength = 0.55F;
    s.precipitation.wind_direction_deg = 205.0F;
    s.groups = {
        group(QStringLiteral("roman_cavalry"),
              Troop::MountedKnight,
              1,
              8,
              {-9.1F, 0.0F, -18.0F},
              6),
        group(QStringLiteral("roman_foot"),
              Troop::Swordsman,
              1,
              7,
              {-7.8F, 0.0F, -24.0F},
              12),
        group(QStringLiteral("punic_wall"),
              Troop::Spearman,
              2,
              9,
              {-10.4F, 0.0F, 6.0F},
              12),
        group(QStringLiteral("punic_support"),
              Troop::Swordsman,
              2,
              6,
              {-6.5F, 0.0F, 10.5F},
              12),
        group(QStringLiteral("punic_archers"),
              Troop::Archer,
              2,
              4,
              {-3.9F, 0.0F, 14.5F},
              8),
    };
    s.steps = {
        at(0.3F, Command::Hold, QStringLiteral("punic_wall")),
        at(0.8F,
           Command::Attack,
           QStringLiteral("punic_archers"),
           QStringLiteral("roman_cavalry")),
        at(1.5F,
           Command::Charge,
           QStringLiteral("roman_cavalry"),
           QStringLiteral("punic_wall")),
        at(3.0F,
           Command::AttackMove,
           QStringLiteral("roman_foot"),
           QStringLiteral("punic_wall")),
        at(8.0F,
           Command::AttackMove,
           QStringLiteral("punic_support"),
           QStringLiteral("roman_foot")),
    };
    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("punic_wall")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_cavalry")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_wall")),
        expectation(Expect::AttackAnimationObserved, QStringLiteral("punic_wall")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_commander_duel_id),
        QStringLiteral("Promo: The Duel"),
        QStringLiteral("Golden-hour capture scene. Both armies halt in line and "
                       "Scipio walks out to meet Hannibal in the ground between "
                       "them. Long enough for the consular riposte and the "
                       "encircling cut to come round several times."),
        40.0F,
        {15.0F, 12.0F, 90.0F});
    s.camera_focus = QVector3D(0.0F, 1.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.arena_floor_half_extent = 30.0F;

    s.terrain_height_scale_override = 2.6F;

    s.suppress_boundary_mountains = true;
    {

      constexpr int k_ring_mounds = 18;
      constexpr float k_ring_radius = 41.0F;
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) /
                            static_cast<float>(k_ring_mounds);
        float const wobble = 0.5F * std::sin(static_cast<float>(i) * 2.3F);
        s.elevation_patches.push_back(
            {{k_ring_radius * std::sin(angle), 0.0F, k_ring_radius * std::cos(angle)},
             20.0F,
             13.5F + (2.5F * wobble)});
      }
    }
    s.environment.start_time = 16.4F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    s.environment.fog_density_override = 0.020F;
    s.environment.exposure_override = 1.18F;

    auto scipio = group(QStringLiteral("scipio"),
                        Troop::RomanVeteranConsul,
                        1,
                        1,
                        {0.0F, 0.0F, -9.0F},
                        1);
    auto hannibal = group(QStringLiteral("hannibal"),
                          Troop::CarthageSwordCommander,
                          2,
                          1,
                          {0.0F, 0.0F, 9.0F},
                          1);

    scipio.health_override = scipio.max_health_override = 4600;
    hannibal.health_override = hannibal.max_health_override = 1800;

    s.groups = {
        scipio,
        hannibal,
        group(QStringLiteral("roman_line"),
              Troop::Swordsman,
              1,
              9,
              {-13.6F, 0.0F, -16.0F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("roman_spears"),
              Troop::Spearman,
              1,
              7,
              {-10.2F, 0.0F, -20.5F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("roman_horse"),
              Troop::MountedKnight,
              1,
              4,
              {19.0F, 0.0F, -18.0F},
              4,
              {3.6F, 0.0F, 0.0F}),
        group(QStringLiteral("punic_line"),
              Troop::Swordsman,
              2,
              9,
              {-13.6F, 0.0F, 16.0F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("punic_spears"),
              Troop::Spearman,
              2,
              7,
              {-10.2F, 0.0F, 20.5F},
              8,
              {3.4F, 0.0F, 0.0F}),
        group(QStringLiteral("punic_horse"),
              Troop::MountedKnight,
              2,
              4,
              {-24.0F, 0.0F, 18.0F},
              4,
              {3.6F, 0.0F, 0.0F}),
    };

    s.resource_patches = {
        {QStringLiteral("pine"), 6, {-34.0F, 0.0F, -30.0F}, {4.0F, 0.0F, 2.5F}, 1.3F},
        {QStringLiteral("pine"), 5, {24.0F, 0.0F, -32.0F}, {4.2F, 0.0F, 2.0F}, 1.2F},
        {QStringLiteral("pine"), 5, {-30.0F, 0.0F, 30.0F}, {4.2F, 0.0F, 2.0F}, 1.25F},
        {QStringLiteral("boulder"), 3, {30.0F, 0.0F, 8.0F}, {3.4F, 0.0F, 2.0F}, 1.1F},
        {QStringLiteral("boulder"), 2, {-31.0F, 0.0F, -6.0F}, {3.6F, 0.0F, 2.0F}, 1.0F},
    };

    for (auto const& held : {QStringLiteral("roman_line"),
                             QStringLiteral("roman_spears"),
                             QStringLiteral("roman_horse"),
                             QStringLiteral("punic_line"),
                             QStringLiteral("punic_spears"),
                             QStringLiteral("punic_horse")}) {
      s.steps.push_back(at(0.2F, Command::Hold, held));
    }
    s.steps.push_back(at(
        0.6F, Command::Attack, QStringLiteral("scipio"), QStringLiteral("hannibal")));
    s.steps.push_back(at(
        0.6F, Command::Attack, QStringLiteral("hannibal"), QStringLiteral("scipio")));

    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("scipio")),
        expectation(Expect::GroupDestroyed, QStringLiteral("hannibal")),
        expectation(Expect::GroupIsRendered, QStringLiteral("scipio")),
        expectation(Expect::GroupIsRendered, QStringLiteral("hannibal")),
        expectation(Expect::GroupIsRendered, QStringLiteral("roman_line")),
        expectation(Expect::GroupIsRendered, QStringLiteral("punic_line")),
        expectation(Expect::AttackHasVisibleContact,
                    QStringLiteral("scipio"),
                    QStringLiteral("hannibal")),
        expectation(Expect::HitReactionObserved, QStringLiteral("hannibal")),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_promo_wolf_attack_id),
        QStringLiteral("Promo: Wolves on the Fold"),
        QStringLiteral("Late-afternoon capture scene. A pack comes out of the east "
                       "at villagers working the ground outside their houses, and "
                       "the riders of the watch come down the street to answer it."),
        44.0F,
        {30.0F, 26.0F, 20.0F});
    s.camera_focus = QVector3D(0.0F, 1.0F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.force_full_creature_lod = true;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.arena_floor_half_extent = 32.0F;
    s.terrain_height_scale_override = 2.6F;

    s.suppress_boundary_mountains = true;
    s.suppress_combat_dust = true;
    {

      constexpr int k_ring_mounds = 26;
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const fi = static_cast<float>(i);
        float const angle =
            2.0F * std::numbers::pi_v<float> * fi / static_cast<float>(k_ring_mounds);
        float const wobble = std::sin(fi * 1.7F);
        float const wobble2 = std::sin((fi * 0.9F) + 1.3F);
        float const ring_radius = 44.0F + (2.0F * wobble2);
        float const mound_radius = 14.0F + (2.0F * wobble);
        float const height = 12.5F + (4.0F * wobble2);
        s.elevation_patches.push_back(
            {{ring_radius * std::sin(angle), 0.0F, ring_radius * std::cos(angle)},
             mound_radius,
             height});
      }
      for (int i = 0; i < k_ring_mounds; ++i) {
        float const fi = static_cast<float>(i) + 0.5F;
        float const angle =
            2.0F * std::numbers::pi_v<float> * fi / static_cast<float>(k_ring_mounds);
        float const wobble = std::sin((fi * 2.3F) + 0.7F);
        s.elevation_patches.push_back({{(48.0F + (2.0F * wobble)) * std::sin(angle),
                                        0.0F,
                                        (48.0F + (2.0F * wobble)) * std::cos(angle)},
                                       17.0F,
                                       15.0F + (3.0F * wobble)});
      }
    }

    s.environment.start_time = 15.2F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.environment.fog_density_override = 0.016F;
    s.environment.exposure_override = 1.5F;

    Game::Wildlife::WildlifeSettings wildlife = Game::Wildlife::default_settings();
    wildlife.enabled = true;
    wildlife.seed = 31337U;
    wildlife.sheep.enabled = false;
    wildlife.sheep.group_count = 0;
    wildlife.birds.enabled = false;
    wildlife.birds.group_count = 0;
    wildlife.wolves.enabled = true;
    wildlife.wolves.group_count = 1;
    wildlife.wolves.group_size_min = 8;
    wildlife.wolves.group_size_max = 8;
    wildlife.wolves.aggression = 1.0F;
    wildlife.wolves.alert_radius = 9.0F;
    wildlife.wolves.roam_radius = 30.0F;
    wildlife.wolves.respawn = false;
    wildlife.wolves.spawn_areas = {{34.0F, 0.0F, 7.0F}};
    s.wildlife = wildlife;

    s.roads = {
        street({-27.0F, 0.0F, -3.0F}, {18.0F, 0.0F, -3.0F}, 3.2F, "default"),
        street({-13.0F, 0.0F, -3.0F}, {-13.0F, 0.0F, 9.0F}, 2.4F, "default"),
    };

    s.resource_patches = {
        patch("fire_camp", 1, {-16.0F, 0.0F, 4.0F}, {}, 0.9F),
        patch("tent", 2, {-21.0F, 0.0F, 6.0F}, {4.5F, 0.0F, 0.0F}, 0.8F),
        patch("supply_cart", 2, {-9.0F, 0.0F, 6.0F}, {3.6F, 0.0F, 0.0F}, 0.95F),
        patch("olive_tree", 5, {-24.0F, 0.0F, -13.0F}, {0.0F, 0.0F, 4.5F}, 1.1F),
        patch("pine", 6, {30.0F, 0.0F, 16.0F}, {3.4F, 0.0F, 2.5F}, 1.2F),
        patch("pine", 4, {26.0F, 0.0F, -18.0F}, {3.6F, 0.0F, 2.0F}, 1.15F),
        patch("plant", 6, {6.0F, 0.0F, -12.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
    };

    auto villagers = residents(QStringLiteral("villagers"),
                               Nation::RomanRepublic,
                               1,
                               6,
                               {0.0F, 0.0F, 5.0F},
                               {2.8F, 0.0F, 0.0F},
                               3.0F);

    villagers.health_override = villagers.max_health_override = 120;

    auto riders = group(QStringLiteral("riders"),
                        Troop::MountedKnight,
                        1,
                        5,
                        {-22.0F, 0.0F, -3.0F},
                        1,
                        {2.8F, 0.0F, 0.0F});

    s.groups = {
        building(QStringLiteral("village_homes"),
                 Game::Units::SpawnType::Home,
                 Nation::RomanRepublic,
                 1,
                 3,
                 {-19.0F, 0.0F, -10.0F},
                 {6.0F, 0.0F, 0.0F}),
        building(QStringLiteral("village_market"),
                 Game::Units::SpawnType::Marketplace,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-13.0F, 0.0F, 8.0F},
                 {},
                 180.0F),
        building(QStringLiteral("village_barracks"),
                 Game::Units::SpawnType::Barracks,
                 Nation::RomanRepublic,
                 1,
                 1,
                 {-22.0F, 0.0F, 2.0F},
                 {},
                 90.0F),
        villagers,
        riders,
    };

    auto rescue = at(8.8F, Command::Run, QStringLiteral("riders"));
    rescue.destination = {-0.5F, 0.0F, 9.8F};

    auto sweep = at(20.0F, Command::Run, QStringLiteral("riders"));
    sweep.destination = {8.0F, 0.0F, 8.0F};

    auto picket = at(28.0F, Command::Run, QStringLiteral("riders"));
    picket.destination = {-1.0F, 0.0F, 6.0F};

    s.steps = {
        at(0.2F, Command::Hold, QStringLiteral("riders")),
        at(8.4F, Command::Stop, QStringLiteral("riders")),
        rescue,
        at(19.6F, Command::Stop, QStringLiteral("riders")),
        sweep,
        at(27.6F, Command::Stop, QStringLiteral("riders")),
        picket,
        at(34.0F, Command::Guard, QStringLiteral("riders")),
    };

    s.expectations = {
        expectation(Expect::GroupExists, QStringLiteral("villagers")),
        expectation(Expect::GroupIsRendered, QStringLiteral("villagers")),
        expectation(Expect::MovementAnimationObserved, QStringLiteral("villagers")),
        expectation(Expect::GroupIsRendered, QStringLiteral("riders")),
        expectation(Expect::DeathAnimationObserved, QStringLiteral("villagers")),
        expectation(Expect::WildlifeHuntObserved),
        expectation(Expect::GroupHealthReduced, QStringLiteral("villagers")),
        expectation(Expect::WildlifeCasualtyObserved),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_archers_id),
        QStringLiteral("Range Indicator: Archers"),
        QStringLiteral("Selected archers publish the projectile range ring the combat "
                       "system actually fires with."),
        3.0F,
        {34.0F, 52.0F, 30.0F});
    s.groups = {group(QStringLiteral("archers"),
                      Troop::Archer,
                      1,
                      3,
                      {-3.0F, 0.0F, 0.0F},
                      4,
                      {3.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("archers"), {}, 11.4F),
        expectation(Expect::GroupIsRendered, QStringLiteral("archers")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_siege_minimum_id),
        QStringLiteral("Range Indicator: Siege Minimum"),
        QStringLiteral("A catapult carrying a minimum firing distance draws both its "
                       "inner dead zone and its outer reach."),
        3.0F,
        {46.0F, 54.0F, 28.0F});
    auto catapult =
        group(QStringLiteral("catapult"), Troop::Catapult, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    catapult.attack_min_range_override = 5.0F;
    s.groups = {
        catapult,
        group(QStringLiteral("crew"), Troop::Archer, 1, 1, {-8.0F, 0.0F, 6.0F}, 4)};
    s.expectations = {
        expectation(Expect::RangeIndicatorObserved,
                    QStringLiteral("catapult"),
                    {},
                    18.0F,
                    0.0F,
                    5.0F),
        expectation(Expect::RangeIndicatorObserved, QStringLiteral("crew"), {}, 11.4F),
        expectation(Expect::GroupIsRendered, QStringLiteral("crew")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_mage_id),
        QStringLiteral("Range Indicator: Arcane Caster"),
        QStringLiteral("A grave priest draws the dotted arcane projectile ring, "
                       "distinct in pattern from bow and siege weapons."),
        3.0F,
        {34.0F, 52.0F, 30.0F});
    auto priest = group(
        QStringLiteral("priest"), Troop::GravePriest, 1, 1, {0.0F, 0.0F, 0.0F}, 1);
    priest.nation_id = Nation::IronSepulcher;
    s.groups = {priest};
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.expectations = {
        expectation(Expect::RangeIndicatorObserved, QStringLiteral("priest"), {}, 8.6F),
        expectation(Expect::GroupIsRendered, QStringLiteral("priest")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_elevation_id),
        QStringLiteral("Range Indicator: Elevation"),
        QStringLiteral("Archers on a hill keep the same ring radius: the "
                       "indicator states ground distance, not line of sight."),
        3.0F,
        {40.0F, 48.0F, 32.0F});
    s.elevation_patches.push_back({{0.0F, 0.0F, 0.0F}, 12.0F, 4.0F});
    s.groups = {group(QStringLiteral("hill_archers"),
                      Troop::Archer,
                      1,
                      2,
                      {-2.0F, 0.0F, 0.0F},
                      4,
                      {4.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("hill_archers"), {}, 11.4F),
        expectation(Expect::GroupIsRendered, QStringLiteral("hill_archers")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_hold_bonus_id),
        QStringLiteral("Range Indicator: Hold Bonus"),
        QStringLiteral("Holding position lengthens archer reach; the ring has "
                       "to grow with it in the same tick."),
        4.0F,
        {40.0F, 52.0F, 30.0F});
    s.groups = {group(QStringLiteral("held_archers"),
                      Troop::Archer,
                      1,
                      2,
                      {-2.0F, 0.0F, 0.0F},
                      4,
                      {4.0F, 0.0F, 0.0F})};
    s.steps = {at(0.5F, Command::Hold, QStringLiteral("held_archers"))};
    s.expectations = {
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("held_archers"), {}, 17.1F),
        expectation(Expect::GroupIsRendered, QStringLiteral("held_archers")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_range_indicator_mixed_selection_id),
        QStringLiteral("Range Indicator: Mixed Selection"),
        QStringLiteral("A large mixed selection stays legible: melee troops "
                       "draw nothing and the ring count stays under the cap."),
        3.5F,
        {70.0F, 56.0F, 28.0F});
    s.arena_floor_half_extent = 44.0F;
    s.groups = {group(QStringLiteral("line_archers"),
                      Troop::Archer,
                      1,
                      6,
                      {-16.0F, 0.0F, -6.0F},
                      4,
                      {6.0F, 0.0F, 0.0F}),
                group(QStringLiteral("outriders"),
                      Troop::HorseArcher,
                      1,
                      4,
                      {-12.0F, 0.0F, 6.0F},
                      3,
                      {7.0F, 0.0F, 0.0F}),
                group(QStringLiteral("engines"),
                      Troop::Ballista,
                      1,
                      2,
                      {-6.0F, 0.0F, 14.0F},
                      1,
                      {9.0F, 0.0F, 0.0F}),
                group(QStringLiteral("shield_line"),
                      Troop::Swordsman,
                      1,
                      4,
                      {-9.0F, 0.0F, -14.0F},
                      4,
                      {6.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(Expect::RangeIndicatorCountAtMost, {}, {}, 12.0F),
        expectation(
            Expect::RangeIndicatorObserved, QStringLiteral("engines"), {}, 21.0F),
        expectation(Expect::GroupIsRendered, QStringLiteral("line_archers")),
        expectation(Expect::GroupIsRendered, QStringLiteral("shield_line")),
        expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F)};
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_unarmed_support_brawl_id),
        QStringLiteral("Unarmed Support Brawl"),
        QStringLiteral("Roman and Carthaginian builders, civilians, and healers fight "
                       "matching roles at close range. Builders swing the mallet and "
                       "civilians the cudgel; healers validate the dedicated unarmed "
                       "BPAT guard, jab, cross, hook, contact, and recovery path."),
        10.0F,
        {5.5F, 10.0F, 0.0F});
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.camera_focus = QVector3D(0.5F, 0.8F, 0.0F);

    auto roman_builders = group(QStringLiteral("roman_builders"),
                                Troop::Builder,
                                1,
                                1,
                                {-2.3F, 0.0F, -1.40F},
                                1);
    auto carthage_builders = group(QStringLiteral("carthage_builders"),
                                   Troop::Builder,
                                   2,
                                   1,
                                   {-2.3F, 0.0F, 1.40F},
                                   1);
    auto roman_civilians = group(QStringLiteral("roman_civilians"),
                                 Troop::Civilian,
                                 1,
                                 1,
                                 {0.0F, 0.0F, -1.40F},
                                 1);
    auto carthage_civilians = group(QStringLiteral("carthage_civilians"),
                                    Troop::Civilian,
                                    2,
                                    1,
                                    {0.0F, 0.0F, 1.40F},
                                    1);
    auto roman_healers = group(
        QStringLiteral("roman_healers"), Troop::Healer, 1, 1, {2.3F, 0.0F, -1.40F}, 1);
    auto carthage_healers = group(QStringLiteral("carthage_healers"),
                                  Troop::Healer,
                                  2,
                                  1,
                                  {2.3F, 0.0F, 1.40F},
                                  1);
    for (auto* combatant : {&roman_builders,
                            &carthage_builders,
                            &roman_civilians,
                            &carthage_civilians,
                            &roman_healers,
                            &carthage_healers}) {
      combatant->health_override = 600;
      combatant->max_health_override = 600;
    }
    s.groups = {roman_builders,
                carthage_builders,
                roman_civilians,
                carthage_civilians,
                roman_healers,
                carthage_healers};

    struct Pair {
      const char* roman;
      const char* carthage;
    };
    constexpr std::array<Pair, 3> k_pairs{{
        {"roman_builders", "carthage_builders"},
        {"roman_civilians", "carthage_civilians"},
        {"roman_healers", "carthage_healers"},
    }};
    for (auto const& pair : k_pairs) {
      QString const roman = QString::fromLatin1(pair.roman);
      QString const carthage = QString::fromLatin1(pair.carthage);
      s.steps.push_back(at(0.35F, Command::Attack, roman, carthage));
      s.steps.push_back(at(0.35F, Command::Attack, carthage, roman));
      for (auto const& [actor, opponent] :
           {std::pair{roman, carthage}, std::pair{carthage, roman}}) {
        s.expectations.push_back(expectation(Expect::GroupIsRendered, actor));
        s.expectations.push_back(expectation(Expect::AttackAnimationObserved, actor));
        s.expectations.push_back(
            expectation(Expect::RepeatedAttackAnimationObserved, actor, {}, 2.0F));
        s.expectations.push_back(
            expectation(Expect::AttackHasVisibleContact, actor, opponent));
        s.expectations.push_back(expectation(Expect::NoLimbOverextension, actor));
        s.expectations.push_back(expectation(Expect::NoUnexpectedFallPose, actor));
      }
    }
    s.expectations.push_back(expectation(Expect::FrameBudget, {}, {}, 33.34F, 0.25F));
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace

auto definitions() -> const std::vector<ArenaScenarioDefinition>& {
  static const std::vector<ArenaScenarioDefinition> catalog = [] {
    auto values = build_definitions();
    auto showcase = build_showcase_definitions();
    values.insert(values.end(),
                  std::make_move_iterator(showcase.begin()),
                  std::make_move_iterator(showcase.end()));
    auto formation = build_formation_definitions();
    values.insert(values.end(),
                  std::make_move_iterator(formation.begin()),
                  std::make_move_iterator(formation.end()));
    auto wildlife = build_wildlife_definitions();
    values.insert(values.end(),
                  std::make_move_iterator(wildlife.begin()),
                  std::make_move_iterator(wildlife.end()));
    auto trailer = build_trailer_definitions();
    values.insert(values.end(),
                  std::make_move_iterator(trailer.begin()),
                  std::make_move_iterator(trailer.end()));
    auto ambience = build_ambience_definitions();
    values.insert(values.end(),
                  std::make_move_iterator(ambience.begin()),
                  std::make_move_iterator(ambience.end()));
    auto city = build_city_definitions();
    values.insert(values.end(),
                  std::make_move_iterator(city.begin()),
                  std::make_move_iterator(city.end()));

    for (auto& scenario : values) {
      if (scenario.rpg_mode && !scenario.rpg_commander_group.isEmpty()) {
        add_commander_control_metrics(scenario, scenario.rpg_commander_group);
      }
    }
    return values;
  }();
  return catalog;
}

auto find_definition(const QString& scenario_id) -> const ArenaScenarioDefinition* {
  auto const found =
      std::find_if(definitions().begin(),
                   definitions().end(),
                   [&](auto const& scenario) { return scenario.id == scenario_id; });
  return found == definitions().end() ? nullptr : &*found;
}

auto options() -> const std::vector<ScenarioOption>& {
  static const std::vector<ScenarioOption> catalog = [] {
    std::vector<ScenarioOption> values;
    values.reserve(definitions().size());
    for (auto const& scenario : definitions()) {
      values.push_back({scenario.id, scenario.label, scenario.description});
    }
    return values;
  }();
  return catalog;
}

auto find_option(const QString& scenario_id) -> const ScenarioOption* {
  auto const found =
      std::find_if(options().begin(), options().end(), [&](auto const& option) {
        return option.id == scenario_id;
      });
  return found == options().end() ? nullptr : &*found;
}

} // namespace Arena::Scenarios
