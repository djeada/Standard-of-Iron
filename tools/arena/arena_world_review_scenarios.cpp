#include "arena_world_review_scenarios.h"

#include <array>
#include <tuple>
#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

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

} // namespace

auto build_world_review_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

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
    auto carthage = s;
    carthage.id = QString::fromLatin1(k_humanoid_gait_review_carthage_id);
    carthage.label = QStringLiteral("Humanoid Gait Review: Carthage");
    for (auto& unit : carthage.groups) {
      unit.nation_id = Nation::Carthage;
    }
    result.push_back(std::move(carthage));
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_humanoid_gait_review_leaders_id),
        QStringLiteral("Humanoid Gait Review: Leaders"),
        QStringLiteral("A healer and three foot commanders cross the same close "
                       "side-on camera at a walk and then at a run, so the single "
                       "bodies that carry cloaks, staves, and command weapons can be "
                       "read against the line infantry gait."),
        16.0F,
        {9.0F, 10.5F, 0.0F});
    s.camera_focus = QVector3D(0.0F, 0.95F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.force_full_creature_lod = true;

    s.owner_teams = {{.owner_id = 1, .team_id = 1},
                     {.owner_id = 2, .team_id = 1},
                     {.owner_id = 3, .team_id = 1},
                     {.owner_id = 4, .team_id = 1}};
    auto leader =
        [](const QString& name, Troop troop, Nation nation, int owner, float z) {
          auto g = group(name, troop, owner, 1, {-6.0F, 0.0F, z}, 1);
          g.nation_id = nation;
          return g;
        };
    s.groups = {leader(QStringLiteral("gait_healer"),
                       Troop::Healer,
                       Nation::RomanRepublic,
                       1,
                       3.6F),
                leader(QStringLiteral("gait_consul"),
                       Troop::RomanVeteranConsul,
                       Nation::RomanRepublic,
                       2,
                       1.2F),
                leader(QStringLiteral("gait_sword_commander"),
                       Troop::CarthageSwordCommander,
                       Nation::Carthage,
                       3,
                       -1.2F),
                leader(QStringLiteral("gait_bow_commander"),
                       Troop::CarthageBowCommander,
                       Nation::Carthage,
                       4,
                       -3.6F)};

    auto line = group(
        QStringLiteral("gait_line"), Troop::Swordsman, 1, 1, {-6.0F, 0.0F, -7.0F}, 6);
    s.groups.push_back(line);

    auto walk = [](float time, const QString& name, float to_x, float z) {
      auto step = at(time, Command::Move, name);
      step.destination = {to_x, 0.0F, z};
      return step;
    };
    auto sprint = [](float time, const QString& name, float to_x, float z) {
      auto step = at(time, Command::Run, name);
      step.destination = {to_x, 0.0F, z};
      step.enabled = true;
      return step;
    };

    std::vector<std::pair<QString, float>> const lanes = {
        {QStringLiteral("gait_healer"), 3.6F},
        {QStringLiteral("gait_consul"), 1.2F},
        {QStringLiteral("gait_sword_commander"), -1.2F},
        {QStringLiteral("gait_bow_commander"), -3.6F},
        {QStringLiteral("gait_line"), -7.0F}};
    for (auto const& [name, z] : lanes) {
      s.steps.push_back(walk(0.5F, name, 6.0F, z));
    }
    for (auto const& [name, z] : lanes) {
      s.steps.push_back(sprint(8.5F, name, -6.0F, z));
    }

    add_visual_stability(s,
                         {QStringLiteral("gait_healer"),
                          QStringLiteral("gait_consul"),
                          QStringLiteral("gait_sword_commander"),
                          QStringLiteral("gait_bow_commander"),
                          QStringLiteral("gait_line")});
    for (auto const& [name, z] : lanes) {
      s.expectations.push_back(expectation(Expect::NoLimbOverextension, name));
      s.expectations.push_back(expectation(Expect::MovementAnimationObserved, name));
    }
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_idle_weapon_grip_review_id),
        QStringLiteral("Idle Weapon Grip Review"),
        QStringLiteral("An archer, a spearman, a swordsman and a healer stand idle "
                       "side by side under a close, low camera, so how each body "
                       "holds its weapon at rest -- both hands on bow and spear -- "
                       "can be read."),
        4.0F,
        {5.0F, 10.0F, 25.0F});
    s.camera_focus = QVector3D(0.0F, 0.85F, 0.0F);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.force_full_creature_lod = true;
    auto still = [](const QString& name, Troop troop, float x) {
      auto g = group(name, troop, 1, 1, {x, 0.0F, 0.0F}, 1);
      g.facing_degrees = 340.0F;
      return g;
    };
    s.groups = {still(QStringLiteral("grip_archer"), Troop::Archer, -1.8F),
                still(QStringLiteral("grip_spear"), Troop::Spearman, -0.6F),
                still(QStringLiteral("grip_sword"), Troop::Swordsman, 0.6F),
                still(QStringLiteral("grip_healer"), Troop::Healer, 1.8F)};
    add_visual_stability(s,
                         {QStringLiteral("grip_archer"),
                          QStringLiteral("grip_spear"),
                          QStringLiteral("grip_sword"),
                          QStringLiteral("grip_healer")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_world_prop_lineup_id),
        QStringLiteral("World Prop Lineup"),
        QStringLiteral("Every authored world prop on clean ground in three rows "
                       "for direct mesh, silhouette, scale, and material review."),
        12.0F,
        {30.0F, 34.0F, 2.0F});
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

        {QStringLiteral("pine_tree"), 1, {-7.5F, 0.0F, -6.0F}, {}, 1.0F, true},
        {QStringLiteral("cypress_tree"), 1, {-4.5F, 0.0F, -6.0F}, {}, 1.0F, true},
        {QStringLiteral("olive_tree"), 1, {-1.5F, 0.0F, -6.0F}, {}, 1.0F, true},
        {QStringLiteral("palm_tree"), 1, {1.5F, 0.0F, -6.0F}, {}, 1.0F, true},
        {QStringLiteral("ruins"), 1, {4.5F, 0.0F, -6.0F}, {}, 1.0F, true},
        {QStringLiteral("abandoned_home"), 1, {7.5F, 0.0F, -6.0F}, {}, 1.0F, true},

        {QStringLiteral("dead_tree"), 1, {-7.5F, 0.0F, 0.0F}, {}, 1.0F, true},
        {QStringLiteral("tent"), 1, {-4.5F, 0.0F, 0.0F}, {}, 1.0F, true},
        {QStringLiteral("statue"), 1, {-1.5F, 0.0F, 0.0F}, {}, 1.0F, true},
        {QStringLiteral("magic_shrine"), 1, {1.5F, 0.0F, 0.0F}, {}, 1.0F, true},
        {QStringLiteral("supply_cart"), 1, {4.5F, 0.0F, 0.0F}, {}, 1.0F, true},
        {QStringLiteral("weapon_rack"), 1, {7.5F, 0.0F, 0.0F}, {}, 1.0F, true},

        {QStringLiteral("firecamp"), 1, {-7.5F, 0.0F, 6.0F}, {}, 1.0F, true},
        {QStringLiteral("boulder"), 1, {-4.5F, 0.0F, 6.0F}, {}, 1.0F, true},
        {QStringLiteral("iron_ore"), 1, {-1.5F, 0.0F, 6.0F}, {}, 1.0F, true},
        {QStringLiteral("cursed_gold_vein"), 1, {1.5F, 0.0F, 6.0F}, {}, 1.0F, true},
        {QStringLiteral("plant"), 1, {4.5F, 0.0F, 6.0F}, {}, 1.0F, true},
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

  return result;
}

} // namespace Arena::Scenarios
