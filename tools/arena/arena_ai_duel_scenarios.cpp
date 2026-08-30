#include "arena_ai_duel_scenarios.h"

#include <cmath>
#include <numbers>
#include <utility>

#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Troop = Game::Units::TroopType;

constexpr float k_corner_offset = 38.0F;
constexpr float k_floor_half_extent = 58.0F;

struct SideOpening {

  int builders{4};
  int homes{0};
  int farms{0};
  int markets{0};
  int towers{0};

  int infantry{0};
  int missiles{0};
};

struct DuelSide {
  QString label;
  int owner_id{2};
  Nation nation{Nation::RomanRepublic};
  Troop commander{Troop::RomanVeteranConsul};

  QVector3D corner;

  float facing_degrees{0.0F};

  SideOpening opening{};
};

auto to_world_direction(const DuelSide& side, QVector3D local) -> QVector3D {
  const float radians = side.facing_degrees * std::numbers::pi_v<float> / 180.0F;
  const float sin_yaw = std::sin(radians);
  const float cos_yaw = std::cos(radians);
  return {local.z() * sin_yaw + local.x() * cos_yaw,
          0.0F,
          local.z() * cos_yaw - local.x() * sin_yaw};
}

auto to_world(const DuelSide& side, QVector3D local) -> QVector3D {
  return side.corner + to_world_direction(side, local);
}

auto duel_group(const DuelSide& side,
                QString name,
                Troop troop,
                int count,
                QVector3D local_offset,
                QVector3D spacing = {2.6F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = side.nation;
  result.owner_id = side.owner_id;
  result.count = count;
  result.origin = to_world(side, local_offset);
  result.spacing = to_world_direction(side, spacing);
  result.facing_degrees = side.facing_degrees;
  result.ai_controlled = true;
  return result;
}

auto duel_building(const DuelSide& side,
                   QString name,
                   Game::Units::SpawnType type,
                   int count,
                   QVector3D local_offset,
                   QVector3D spacing = {7.0F, 0.0F, 0.0F}) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = side.nation;
  result.owner_id = side.owner_id;
  result.count = count;
  result.origin = to_world(side, local_offset);
  result.spacing = to_world_direction(side, spacing);
  result.facing_degrees = side.facing_degrees;
  result.ai_controlled = true;
  return result;
}

void add_battlefield(ArenaScenarioDefinition& scenario) {
  using Game::Map::HillShape;
  using Game::Map::TerrainType;

  const auto reach = [](float x, float z) {
    return QVector3D{x, 0.0F, z};
  };

  scenario.rivers.push_back(
      Game::Map::RiverSegment{reach(-60.10F, 67.18F), reach(-31.47F, 27.93F), 12.0F});
  scenario.rivers.push_back(
      Game::Map::RiverSegment{reach(-31.47F, 27.93F), reach(-9.90F, 9.90F), 11.0F});
  scenario.rivers.push_back(
      Game::Map::RiverSegment{reach(-9.90F, 9.90F), reach(9.90F, -9.90F), 10.0F});
  scenario.rivers.push_back(
      Game::Map::RiverSegment{reach(9.90F, -9.90F), reach(27.93F, -31.47F), 11.0F});
  scenario.rivers.push_back(
      Game::Map::RiverSegment{reach(27.93F, -31.47F), reach(67.18F, -60.10F), 12.0F});

  constexpr float k_bridge_reach = 5.7F;

  constexpr float k_bridge_deck_width = 10.0F;

  scenario.bridges.push_back(Game::Map::Bridge{{-k_bridge_reach, 0.0F, -k_bridge_reach},
                                               {k_bridge_reach, 0.0F, k_bridge_reach},
                                               k_bridge_deck_width,
                                               0.5F});

  const auto hill = [](float x,
                       float z,
                       float radius,
                       float height,
                       HillShape shape,
                       float thickness,
                       float rotation) {
    Game::Map::TerrainFeature feature;
    feature.type = TerrainType::Hill;
    feature.center_x = x;
    feature.center_z = z;
    feature.radius = radius;
    feature.height = height;
    feature.shape = shape;
    feature.thickness = thickness;
    feature.rotation_deg = rotation;
    return feature;
  };

  constexpr float k_bank_ridge_rotation = -45.0F;

  scenario.terrain_features.push_back(hill(
      1.41F, -28.28F, 12.0F, 3.6F, HillShape::Corridor, 5.0F, k_bank_ridge_rotation));
  scenario.terrain_features.push_back(hill(
      -1.41F, 28.28F, 12.0F, 3.6F, HillShape::Corridor, 5.0F, k_bank_ridge_rotation));

  scenario.terrain_features.push_back(
      hill(-43.84F, 2.83F, 11.0F, 4.0F, HillShape::Blob, 7.0F, 0.0F));
  scenario.terrain_features.push_back(
      hill(43.84F, -2.83F, 11.0F, 4.0F, HillShape::Blob, 7.0F, 0.0F));

  scenario.terrain_features.push_back(
      hill(-38.89F, -6.36F, 11.0F, 3.6F, HillShape::Elbow, 5.5F, 135.0F));
  scenario.terrain_features.push_back(
      hill(38.89F, 6.36F, 11.0F, 3.6F, HillShape::Arc, 5.0F, -45.0F));

  constexpr float k_road_width = 4.0F;

  scenario.roads.push_back(Game::Map::RoadSegment{
      {-28.28F, 0.0F, -28.28F}, {-12.02F, 0.0F, -12.02F}, k_road_width});
  scenario.roads.push_back(Game::Map::RoadSegment{
      {28.28F, 0.0F, 28.28F}, {12.02F, 0.0F, 12.02F}, k_road_width});
}

void add_side(ArenaScenarioDefinition& scenario, const DuelSide& side) {
  const QString prefix = side.label;
  const auto& opening = side.opening;

  scenario.groups.push_back(duel_building(side,
                                          prefix + QStringLiteral("_barracks"),
                                          Game::Units::SpawnType::Barracks,
                                          1,
                                          {0.0F, 0.0F, 5.74F}));
  scenario.groups.push_back(duel_group(side,
                                       prefix + QStringLiteral("_commander"),
                                       side.commander,
                                       1,
                                       {0.0F, 0.0F, 2.74F}));
  scenario.groups.push_back(duel_group(side,
                                       prefix + QStringLiteral("_builder"),
                                       Troop::Builder,
                                       opening.builders,
                                       {-10.0F, 0.0F, -7.26F},
                                       {2.4F, 0.0F, 0.0F}));

  if (opening.homes > 0) {
    scenario.groups.push_back(duel_building(side,
                                            prefix + QStringLiteral("_homes"),
                                            Game::Units::SpawnType::Home,
                                            opening.homes,
                                            {-6.0F, 0.0F, -3.26F},
                                            {7.0F, 0.0F, 0.0F}));
  }
  if (opening.farms > 0) {
    scenario.groups.push_back(duel_building(side,
                                            prefix + QStringLiteral("_farms"),
                                            Game::Units::SpawnType::Farm,
                                            opening.farms,
                                            {18.5F, 0.0F, 2.74F},
                                            {-11.0F, 0.0F, -14.0F}));
  }
  if (opening.markets > 0) {
    scenario.groups.push_back(duel_building(side,
                                            prefix + QStringLiteral("_market"),
                                            Game::Units::SpawnType::Marketplace,
                                            opening.markets,
                                            {-21.0F, 0.0F, -1.26F},
                                            {9.0F, 0.0F, 0.0F}));
  }
  if (opening.towers > 0) {
    scenario.groups.push_back(duel_building(side,
                                            prefix + QStringLiteral("_towers"),
                                            Game::Units::SpawnType::DefenseTower,
                                            opening.towers,
                                            {0.0F, 0.0F, 13.74F},
                                            {22.0F, 0.0F, 0.0F}));
  }
  if (opening.infantry > 0) {
    scenario.groups.push_back(duel_group(side,
                                         prefix + QStringLiteral("_infantry"),
                                         Troop::Spearman,
                                         opening.infantry,
                                         {-1.6F, 0.0F, 9.74F},
                                         {3.4F, 0.0F, 0.0F}));
  }
  if (opening.missiles > 0) {
    scenario.groups.push_back(duel_group(side,
                                         prefix + QStringLiteral("_missiles"),
                                         Troop::Archer,
                                         opening.missiles,
                                         {6.7F, 0.0F, 9.74F},
                                         {3.4F, 0.0F, 0.0F}));
  }

  ArenaScenarioBattleSide battle_side;
  battle_side.owner_id = side.owner_id;
  battle_side.label = side.label;
  battle_side.home = side.corner;
  battle_side.home_radius = 22.0F;
  scenario.battle_sides.push_back(std::move(battle_side));

  const auto patch = [&](const char* type,
                         int count,
                         QVector3D local,
                         QVector3D spacing,
                         float scale) -> ArenaScenarioResourcePatch {
    return {QString::fromLatin1(type),
            count,
            to_world(side, local),
            to_world_direction(side, spacing),
            scale};
  };
  scenario.resource_patches.push_back(
      patch("olive_tree", 12, {-11.0F, 0.0F, -12.26F}, {1.9F, 0.0F, 0.0F}, 1.1F));
  scenario.resource_patches.push_back(
      patch("olive_tree", 12, {30.0F, 0.0F, 23.74F}, {1.6F, 0.0F, 0.0F}, 1.1F));
  scenario.resource_patches.push_back(
      patch("olive_tree", 10, {-32.0F, 0.0F, 9.74F}, {1.8F, 0.0F, 0.0F}, 1.05F));
  scenario.resource_patches.push_back(
      patch("boulder", 10, {-34.0F, 0.0F, 23.74F}, {1.6F, 0.0F, 0.0F}, 1.05F));
  scenario.resource_patches.push_back(
      patch("boulder", 8, {-14.0F, 0.0F, 37.74F}, {-1.8F, 0.0F, 0.0F}, 1.05F));
  scenario.resource_patches.push_back(
      patch("iron_ore", 6, {24.0F, 0.0F, 29.74F}, {1.6F, 0.0F, 0.0F}, 1.0F));
  scenario.resource_patches.push_back(
      patch("iron_ore", 5, {34.0F, 0.0F, 13.74F}, {1.2F, 0.0F, 0.0F}, 1.0F));
}

auto war_of_towns_wildlife() -> Game::Wildlife::WildlifeSettings {
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 4133U;
  settings.wolves.enabled = false;
  settings.wolves.group_count = 0;

  settings.sheep.enabled = true;
  settings.sheep.group_count = 2;
  settings.sheep.group_size_min = 8;
  settings.sheep.group_size_max = 10;
  settings.sheep.roam_radius = 9.0F;
  settings.sheep.spawn_areas = {{-24.0F, 2.0F, 3.0F}, {24.0F, -2.0F, 3.0F}};

  settings.birds = Game::Wildlife::default_bird_config();
  return settings;
}

auto duel_definition(const char* id,
                     QString label,
                     QString description,
                     float duration,
                     DuelSide north_west,
                     DuelSide south_east) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition scenario;
  scenario.id = QString::fromLatin1(id);
  scenario.label = std::move(label);
  scenario.description = std::move(description);
  scenario.duration_seconds = duration;
  scenario.camera = {88.0F, 50.0F, 45.0F};
  scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  scenario.arena_floor_half_extent = k_floor_half_extent;
  scenario.select_spawned_units = false;
  scenario.suppress_spawn_anchor = true;
  scenario.suppress_ui_overlays = true;
  scenario.force_full_creature_lod = true;
  scenario.collect_animation_diagnostics = false;

  scenario.suppress_terrain_scatter = false;
  scenario.terrain_grid_extent = 128;

  scenario.ai_starting_resources = {
      .gold = 2000, .food = 200, .wood = 250, .stone = 120, .iron = 80};

  north_west.corner = QVector3D(-k_corner_offset, 0.0F, -k_corner_offset);
  north_west.facing_degrees = 45.0F;
  south_east.corner = QVector3D(k_corner_offset, 0.0F, k_corner_offset);
  south_east.facing_degrees = 225.0F;

  add_battlefield(scenario);
  add_side(scenario, north_west);
  add_side(scenario, south_east);

  scenario.expectations.push_back(
      ArenaExpectation{.kind = Expect::BattleReachesDecision});
  return scenario;
}

auto side_expectation(Expect kind, QString label, float threshold) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.side = std::move(label);
  result.threshold = threshold;
  return result;
}

void add_economy_expectations(ArenaScenarioDefinition& scenario, const QString& side) {
  ArenaExpectation built;
  built.kind = Expect::SideBuildsAtLeast;
  built.side = side;
  built.threshold = 2.0F;
  scenario.expectations.push_back(std::move(built));

  ArenaExpectation recruited;
  recruited.kind = Expect::SideProducesReinforcements;
  recruited.side = side;
  recruited.threshold = 2.0F;
  scenario.expectations.push_back(std::move(recruited));
}

auto doctrine_expectation(QString label, const char* doctrine) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = Expect::SideDoctrineIs;
  result.side = std::move(label);
  result.counter_key = QString::fromLatin1(doctrine);
  return result;
}

} // namespace

auto build_ai_duel_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    DuelSide scipio;
    scipio.label = QStringLiteral("scipio");
    scipio.owner_id = 2;
    scipio.nation = Nation::RomanRepublic;
    scipio.commander = Troop::RomanVeteranConsul;

    DuelSide fabius;
    fabius.label = QStringLiteral("fabius");
    fabius.owner_id = 3;
    fabius.nation = Nation::RomanRepublic;
    fabius.commander = Troop::RomanLegionOrganizer;

    auto s = duel_definition(
        k_ai_duel_scipio_vs_fabius_id,
        QStringLiteral("AI Duel: Scipio vs Fabius"),
        QStringLiteral("Two Roman AI commanders build opposing corner towns and "
                       "fight to elimination. Scipio's aggressive doctrine must "
                       "carry the attack while Fabius' delaying doctrine holds "
                       "its own ground."),
        1800.0F,
        scipio,
        fabius);
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("scipio"), "aggressive:field"));
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("fabius"), "defensive:garrison"));
    add_economy_expectations(s, QStringLiteral("scipio"));
    add_economy_expectations(s, QStringLiteral("fabius"));
    s.expectations.push_back(
        side_expectation(Expect::SideCommitsToAttack, QStringLiteral("scipio"), 8.0F));
    s.expectations.push_back(
        side_expectation(Expect::SideKeepsGarrison, QStringLiteral("fabius"), 3.0F));
    result.push_back(std::move(s));
  }

  {
    DuelSide marcellus;
    marcellus.label = QStringLiteral("marcellus");
    marcellus.owner_id = 2;
    marcellus.nation = Nation::RomanRepublic;
    marcellus.commander = Troop::RomanFieldCommander;

    DuelSide hanno;
    hanno.label = QStringLiteral("hanno");
    hanno.owner_id = 3;
    hanno.nation = Nation::Carthage;
    hanno.commander = Troop::CarthageSpearCommander;

    auto s = duel_definition(
        k_ai_duel_marcellus_vs_hanno_id,
        QStringLiteral("AI Duel: Marcellus vs Hanno"),
        QStringLiteral("A Roman rushing doctrine against a Carthaginian economic "
                       "garrison, each developing its own corner town until one "
                       "is destroyed."),
        1800.0F,
        marcellus,
        hanno);
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("marcellus"), "rusher:field"));
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("hanno"), "economic:garrison"));
    add_economy_expectations(s, QStringLiteral("marcellus"));
    add_economy_expectations(s, QStringLiteral("hanno"));
    s.expectations.push_back(side_expectation(
        Expect::SideCommitsToAttack, QStringLiteral("marcellus"), 8.0F));
    s.expectations.push_back(
        side_expectation(Expect::SideKeepsGarrison, QStringLiteral("hanno"), 3.0F));
    result.push_back(std::move(s));
  }

  {
    DuelSide hannibal;
    hannibal.label = QStringLiteral("hannibal");
    hannibal.owner_id = 2;
    hannibal.nation = Nation::Carthage;
    hannibal.commander = Troop::CarthageSwordCommander;

    DuelSide hasdrubal;
    hasdrubal.label = QStringLiteral("hasdrubal");
    hasdrubal.owner_id = 3;
    hasdrubal.nation = Nation::Carthage;
    hasdrubal.commander = Troop::CarthageBowCommander;

    auto s = duel_definition(
        k_ai_duel_hannibal_vs_hasdrubal_id,
        QStringLiteral("AI Duel: Hannibal vs Hasdrubal"),
        QStringLiteral("Two Barcid doctrines, one decisive and one harassing, "
                       "fight the same battle from opposite temperaments."),
        1800.0F,
        hannibal,
        hasdrubal);
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("hannibal"), "aggressive:field"));
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("hasdrubal"), "harasser:field"));
    add_economy_expectations(s, QStringLiteral("hannibal"));
    add_economy_expectations(s, QStringLiteral("hasdrubal"));
    result.push_back(std::move(s));
  }

  {
    DuelSide scipio;
    scipio.label = QStringLiteral("scipio");
    scipio.owner_id = 2;
    scipio.nation = Nation::RomanRepublic;
    scipio.commander = Troop::RomanVeteranConsul;
    scipio.opening = SideOpening{.builders = 6,
                                 .homes = 3,
                                 .farms = 2,
                                 .markets = 1,
                                 .towers = 2,
                                 .infantry = 3,
                                 .missiles = 2};

    DuelSide hannibal;
    hannibal.label = QStringLiteral("hannibal");
    hannibal.owner_id = 3;
    hannibal.nation = Nation::Carthage;
    hannibal.commander = Troop::CarthageSwordCommander;
    hannibal.opening = scipio.opening;

    auto s = duel_definition(
        k_ai_war_of_towns_id,
        QStringLiteral("AI War of Towns"),
        QStringLiteral("Two established AI towns, each with homes, fields, a market "
                       "and a watch already standing, grow their economies and fight "
                       "over the ground between them. The scene to watch when the "
                       "question is how the computer plays a whole match rather than "
                       "how it opens one."),
        1800.0F,
        scipio,
        hannibal);
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("scipio"), "aggressive:field"));
    s.expectations.push_back(
        doctrine_expectation(QStringLiteral("hannibal"), "aggressive:field"));
    add_economy_expectations(s, QStringLiteral("scipio"));
    add_economy_expectations(s, QStringLiteral("hannibal"));
    s.wildlife = war_of_towns_wildlife();
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
