#include "arena_ai_duel_scenarios.h"

#include <cmath>
#include <numbers>
#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Troop = Game::Units::TroopType;

constexpr float k_corner_offset = 38.0F;
constexpr float k_floor_half_extent = 58.0F;

struct DuelSide {
  QString label;
  int owner_id{2};
  Nation nation{Nation::RomanRepublic};
  Troop commander{Troop::RomanVeteranConsul};

  QVector3D corner;

  float facing_degrees{0.0F};
};

auto to_world(const DuelSide& side, QVector3D local) -> QVector3D {
  const float radians = side.facing_degrees * std::numbers::pi_v<float> / 180.0F;
  const float sin_yaw = std::sin(radians);
  const float cos_yaw = std::cos(radians);
  return side.corner + QVector3D(local.z() * sin_yaw + local.x() * cos_yaw,
                                 0.0F,
                                 local.z() * cos_yaw - local.x() * sin_yaw);
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
  result.spacing = spacing;
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
  result.spacing = spacing;
  result.facing_degrees = side.facing_degrees;
  result.ai_controlled = true;
  return result;
}

void add_battlefield(ArenaScenarioDefinition& scenario) {
  using Game::Map::HillShape;
  using Game::Map::TerrainType;

  scenario.rivers.push_back(
      Game::Map::RiverSegment{{-46.0F, 0.0F, 30.0F}, {40.0F, 0.0F, -34.0F}, 7.0F});

  scenario.bridges.push_back(
      Game::Map::Bridge{{-9.0F, 0.0F, -10.0F}, {3.0F, 0.0F, 6.0F}, 9.0F, 0.5F});

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

  scenario.terrain_features.push_back(
      hill(-13.0F, -15.0F, 12.0F, 2.8F, HillShape::Corridor, 5.0F, 127.0F));
  scenario.terrain_features.push_back(
      hill(7.0F, 11.0F, 12.0F, 2.8F, HillShape::Corridor, 5.0F, 127.0F));

  scenario.terrain_features.push_back(
      hill(-38.0F, -6.0F, 11.0F, 3.2F, HillShape::Blob, 7.0F, 0.0F));
  scenario.terrain_features.push_back(
      hill(38.0F, 6.0F, 11.0F, 3.2F, HillShape::Blob, 7.0F, 0.0F));

  scenario.terrain_features.push_back(
      hill(-22.0F, -22.0F, 11.0F, 3.0F, HillShape::Elbow, 5.5F, 110.0F));
  scenario.terrain_features.push_back(
      hill(20.0F, 20.0F, 11.0F, 2.6F, HillShape::Arc, 5.0F, -60.0F));
}

void add_side(ArenaScenarioDefinition& scenario, const DuelSide& side) {
  const QString prefix = side.label;

  scenario.groups.push_back(duel_building(side,
                                          prefix + QStringLiteral("_barracks"),
                                          Game::Units::SpawnType::Barracks,
                                          1,
                                          {0.0F, 0.0F, 5.0F}));
  scenario.groups.push_back(duel_group(side,
                                       prefix + QStringLiteral("_commander"),
                                       side.commander,
                                       1,
                                       {0.0F, 0.0F, -2.0F}));
  scenario.groups.push_back(duel_group(side,
                                       prefix + QStringLiteral("_builder"),
                                       Troop::Builder,
                                       1,
                                       {-4.0F, 0.0F, -4.0F}));

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
    return {QString::fromLatin1(type), count, to_world(side, local), spacing, scale};
  };
  scenario.resource_patches.push_back(
      patch("olive_tree", 12, {-19.0F, 0.0F, -8.0F}, {0.0F, 0.0F, 2.6F}, 1.1F));
  scenario.resource_patches.push_back(
      patch("olive_tree", 12, {17.0F, 0.0F, -12.0F}, {0.0F, 0.0F, 2.6F}, 1.1F));
  scenario.resource_patches.push_back(
      patch("olive_tree", 10, {-6.0F, 0.0F, -20.0F}, {2.6F, 0.0F, 0.0F}, 1.05F));
  scenario.resource_patches.push_back(
      patch("boulder", 10, {-17.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 2.5F}, 1.05F));
  scenario.resource_patches.push_back(
      patch("boulder", 8, {14.0F, 0.0F, 8.0F}, {0.0F, 0.0F, 2.5F}, 1.05F));
  scenario.resource_patches.push_back(
      patch("iron_ore", 6, {8.0F, 0.0F, -18.0F}, {2.5F, 0.0F, 0.0F}, 1.0F));
  scenario.resource_patches.push_back(
      patch("iron_ore", 5, {-11.0F, 0.0F, 14.0F}, {2.5F, 0.0F, 0.0F}, 1.0F));
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
  scenario.camera = {205.0F, 60.0F, 45.0F};
  scenario.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  scenario.arena_floor_half_extent = k_floor_half_extent;
  scenario.select_spawned_units = false;
  scenario.suppress_spawn_anchor = true;
  scenario.suppress_ui_overlays = true;
  scenario.force_full_creature_lod = false;
  scenario.collect_animation_diagnostics = false;
  scenario.suppress_terrain_scatter = true;
  scenario.terrain_grid_extent = 128;

  scenario.ai_starting_resources = {
      .gold = 250, .food = 200, .wood = 250, .stone = 120, .iron = 80};

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
        1200.0F,
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
        1200.0F,
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
        1200.0F,
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

  return result;
}

} // namespace Arena::Scenarios
