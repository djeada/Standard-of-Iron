#include "arena_settlement_life_scenarios.h"

#include <utility>
#include <vector>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto harvest_at(float time,
                QString source,
                QString resource_kind) -> ArenaScenarioStep {
  auto result = at(time, Command::HarvestResource, std::move(source));
  result.resource_kind = std::move(resource_kind);
  return result;
}

} // namespace

auto build_settlement_life_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

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
                     Troop::MountedSwordsman,
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

  return result;
}

} // namespace Arena::Scenarios
