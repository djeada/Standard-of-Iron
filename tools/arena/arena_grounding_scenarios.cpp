#include "arena_grounding_scenarios.h"

#include <array>
#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;
using Game::Map::HillShape;
using Game::Map::TerrainType;

constexpr float k_back_row_z = -7.0F;
constexpr float k_middle_row_z = 0.0F;
constexpr float k_front_row_z = 6.0F;

auto troops(QString name,
            Troop troop,
            QVector3D origin,
            int individuals) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = 1;
  result.individuals_per_unit = individuals;
  result.origin = origin;
  result.spacing = {0.0F, 0.0F, 0.0F};
  result.facing_degrees = 90.0F;
  return result;
}

auto structure(QString name,
               Game::Units::SpawnType type,
               QVector3D origin,
               float facing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.spawn_type = type;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = 1;
  result.origin = origin;
  result.spacing = {0.0F, 0.0F, 0.0F};
  result.facing_degrees = facing;
  return result;
}

auto prop(const char* type,
          QVector3D origin,
          float scale = 1.0F) -> ArenaScenarioResourcePatch {
  ArenaScenarioResourcePatch patch;
  patch.prop_type = QString::fromLatin1(type);
  patch.count = 1;
  patch.origin = origin;
  patch.spacing = {};
  patch.scale = scale;

  return patch;
}

auto at(float time, Command command, QString group = {}) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), group);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = command;
  result.group = std::move(group);
  return result;
}

auto camera_step(float time,
                 float distance,
                 float angle,
                 float yaw) -> ArenaScenarioStep {
  auto step = at(time, Command::SetCamera);
  step.camera_distance = distance;
  step.camera_angle = angle;
  step.camera_yaw = yaw;
  return step;
}

auto hill(float x,
          float z,
          float radius,
          float height,
          HillShape shape = HillShape::Blob,
          float thickness = 0.0F,
          float rotation = 0.0F) -> Game::Map::TerrainFeature {
  Game::Map::TerrainFeature feature{};
  feature.type = TerrainType::Hill;
  feature.center_x = x;
  feature.center_z = z;
  feature.radius = radius;
  feature.height = height;
  feature.shape = shape;
  feature.thickness = thickness;
  feature.rotation_deg = rotation;
  return feature;
}

auto rise(float x, float z, float radius, float height, float plateau = 0.0F)
    -> ArenaScenarioElevationPatch {
  ArenaScenarioElevationPatch patch;
  patch.center = QVector3D(x, 0.0F, z);
  patch.radius = radius;
  patch.height = height;
  patch.plateau = plateau;
  return patch;
}

auto grounding_definition(const char* id,
                          QString label,
                          QString description) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition s;
  s.id = QString::fromLatin1(id);
  s.label = std::move(label);
  s.description = std::move(description);
  s.duration_seconds = 13.0F;
  s.arena_floor_half_extent = 24.0F;
  s.camera = {30.0F, 38.0F, 24.0F};
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;

  s.suppress_combat_dust = true;
  s.force_full_creature_lod = true;

  s.resource_patches = {
      prop("pine_tree", {-10.0F, 0.0F, k_back_row_z}),
      prop("olive_tree", {-6.0F, 0.0F, k_back_row_z}),
      prop("cypress_tree", {-2.5F, 0.0F, k_back_row_z}),
      prop("dead_tree", {1.0F, 0.0F, k_back_row_z}),
      prop("boulder", {4.5F, 0.0F, k_back_row_z}),
      prop("iron_ore", {8.0F, 0.0F, k_back_row_z}),
      prop("plant", {11.0F, 0.0F, k_back_row_z}),
      prop("tent", {-1.5F, 0.0F, k_middle_row_z}),
  };

  s.groups = {
      structure(QStringLiteral("home"),
                Game::Units::SpawnType::Home,
                {-9.0F, 0.0F, k_middle_row_z},
                20.0F),
      structure(QStringLiteral("tower"),
                Game::Units::SpawnType::DefenseTower,
                {6.0F, 0.0F, k_middle_row_z},
                0.0F),
      structure(QStringLiteral("wall"),
                Game::Units::SpawnType::WallSegment,
                {10.0F, 0.0F, k_middle_row_z},
                0.0F),
      troops(QStringLiteral("infantry"),
             Troop::Swordsman,
             {-7.5F, 0.0F, k_front_row_z},
             9),
      troops(
          QStringLiteral("fallen"), Troop::Spearman, {-3.0F, 0.0F, k_front_row_z}, 9),
      troops(QStringLiteral("cavalry"),
             Troop::MountedSwordsman,
             {1.5F, 0.0F, k_front_row_z},
             3),
      troops(
          QStringLiteral("catapult"), Troop::Catapult, {6.0F, 0.0F, k_front_row_z}, 0),
      troops(
          QStringLiteral("ballista"), Troop::Ballista, {9.5F, 0.0F, k_front_row_z}, 0),
  };

  auto fell = at(0.6F, Command::ApplyDamage, QStringLiteral("fallen"));
  fell.value = 650;

  auto ride_out = at(2.0F, Command::Move, QStringLiteral("cavalry"));
  ride_out.destination = QVector3D(1.5F, 0.0F, k_front_row_z + 5.0F);
  auto ride_back = at(6.5F, Command::Move, QStringLiteral("cavalry"));
  ride_back.destination = QVector3D(1.5F, 0.0F, k_front_row_z);

  s.steps = {fell,
             ride_out,
             ride_back,
             camera_step(3.0F, 22.0F, 34.0F, 70.0F),
             camera_step(6.0F, 18.0F, 30.0F, 160.0F),
             camera_step(9.0F, 26.0F, 40.0F, 250.0F),

             camera_step(10.5F, 20.0F, 12.0F, 100.0F)};

  for (const char* name : {"home", "tower", "wall", "catapult", "ballista"}) {
    ArenaExpectation exists;
    exists.kind = Expect::GroupExists;
    exists.group = QString::fromLatin1(name);
    s.expectations.push_back(exists);
  }
  for (const char* name : {"infantry", "cavalry"}) {
    ArenaExpectation rendered;
    rendered.kind = Expect::GroupIsRendered;
    rendered.group = QString::fromLatin1(name);
    s.expectations.push_back(rendered);
  }
  return s;
}

} // namespace

auto build_grounding_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = grounding_definition(
        k_grounding_flat_id,
        QStringLiteral("Grounding: Flat Ground"),
        QStringLiteral("The grounding cast on level ground: the reference every "
                       "sloped capture is compared against."));
    s.suppress_terrain_features = true;
    result.push_back(std::move(s));
  }

  {
    auto s = grounding_definition(
        k_grounding_hill_id,
        QStringLiteral("Grounding: Hillside"),
        QStringLiteral("The grounding cast spread over the flank of a broad hill: "
                       "foundations, bedded trunks, pitched horses, tilted siege "
                       "and fallen bodies lying along the slope."));
    s.elevation_patches.push_back(rise(0.0F, -16.0F, 28.0F, 6.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = grounding_definition(
        k_grounding_ridge_id,
        QStringLiteral("Grounding: Ridge Edge"),
        QStringLiteral("A narrow ridge runs through the middle row, so structures "
                       "and troops straddle its crest and shoulders."));
    s.terrain_features.push_back(
        hill(0.0F, 1.0F, 18.0F, 3.2F, HillShape::Corridor, 5.0F, 0.0F));
    result.push_back(std::move(s));
  }

  {
    auto s = grounding_definition(
        k_grounding_riverbank_id,
        QStringLiteral("Grounding: Riverbank"),
        QStringLiteral("A river cuts across behind the props, so the back row and "
                       "riverbank dressing sit on the falling bank."));
    s.rivers.push_back(
        Game::Map::RiverSegment{{-26.0F, 0.0F, -11.0F}, {26.0F, 0.0F, -11.0F}, 7.0F});
    s.elevation_patches.push_back(rise(0.0F, 10.0F, 16.0F, 1.8F));
    result.push_back(std::move(s));
  }

  {
    auto s = grounding_definition(
        k_grounding_road_id,
        QStringLiteral("Grounding: Road Over A Rise"),
        QStringLiteral("A paved road climbs over a rise under the front row, so "
                       "feet, hooves and wheels meet the road surface, not the "
                       "ground beneath it."));
    s.elevation_patches.push_back(rise(0.0F, k_front_row_z, 16.0F, 2.6F, 2.0F));
    s.roads.push_back(Game::Map::RoadSegment{
        {-24.0F, 0.0F, k_front_row_z}, {24.0F, 0.0F, k_front_row_z}, 4.2F});
    result.push_back(std::move(s));
  }

  {
    auto s = grounding_definition(
        k_grounding_scatter_id,
        QStringLiteral("Grounding: Mixed Scatter"),
        QStringLiteral("Procedural trees, rocks and plants over uneven hills around "
                       "the cast, for checking scatter contact across slopes."));
    s.elevation_patches.push_back(rise(-10.0F, -10.0F, 14.0F, 3.4F));
    s.elevation_patches.push_back(rise(12.0F, 4.0F, 12.0F, 2.8F));
    s.resource_patches.push_back({QStringLiteral("olive_tree"),
                                  5,
                                  {-16.0F, 0.0F, -14.0F},
                                  {2.2F, 0.0F, 1.4F},
                                  1.1F});
    s.resource_patches.push_back(
        {QStringLiteral("boulder"), 5, {10.0F, 0.0F, -2.0F}, {1.4F, 0.0F, 1.6F}, 1.0F});
    s.resource_patches.push_back(
        {QStringLiteral("plant"), 8, {-14.0F, 0.0F, -4.0F}, {1.1F, 0.0F, -0.9F}, 1.0F});
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
