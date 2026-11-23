#include "arena_city_block_scenarios.h"

#include <algorithm>
#include <utility>

#include "arena_scenario_builders.h"
#include "arena_scenarios.h"
#include "game/systems/building_collision_registry.h"

namespace Arena::Scenarios {
namespace {

using namespace builders;

using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Spawn = Game::Units::SpawnType;
using Troop = Game::Units::TroopType;

// The strip of pavement a tenement lays in front of its own lot.
constexpr float k_pavement = 0.5F;

struct Lot {
  float width;
  float depth;
};

[[nodiscard]] auto home_lot() -> Lot {
  const auto size = Game::Systems::BuildingCollisionRegistry::get_building_size("home");
  return {size.width, size.depth};
}

auto terrace(QString name,
             Nation nation,
             int owner,
             int count,
             QVector3D first,
             QVector3D step,
             float facing) -> ArenaScenarioGroup {
  auto group = building(std::move(name),
                        Spawn::Home,
                        nation,
                        owner,
                        count,
                        first + (step * (static_cast<float>(count - 1) * 0.5F)),
                        step,
                        facing);
  group.visual_style = QStringLiteral("tenement");
  return group;
}

} // namespace

auto tenement_block_extent(int lots_wide, int lots_deep) -> TenementBlockExtent {
  const Lot lot = home_lot();
  return {static_cast<float>(lots_wide) * lot.width,
          (2.0F * lot.depth) + (static_cast<float>(lots_deep) * lot.width)};
}

auto lay_tenement_block(ArenaScenarioDefinition& scenario,
                        const TenementBlock& block) -> std::vector<QString> {
  const Lot lot = home_lot();
  const TenementBlockExtent extent =
      tenement_block_extent(block.lots_wide, block.lots_deep);
  const float x0 = block.origin.x();
  const float z0 = block.origin.z();
  std::vector<QString> names;
  const auto add = [&](const char* suffix,
                       int count,
                       QVector3D first,
                       QVector3D step,
                       float facing) {
    if (count <= 0) {
      return;
    }
    QString name = block.name + QString::fromLatin1(suffix);
    scenario.groups.push_back(
        terrace(name, block.nation, block.owner, count, first, step, facing));
    names.push_back(std::move(name));
  };

  const QVector3D along_x(lot.width, 0.0F, 0.0F);
  const QVector3D along_z(0.0F, 0.0F, lot.width);
  const float north_z = z0 + (lot.depth * 0.5F);
  const float south_z = z0 + extent.depth - (lot.depth * 0.5F);
  const auto lot_x = [&](int index) {
    return x0 + (lot.width * (static_cast<float>(index) + 0.5F));
  };

  const int alley = std::clamp(block.alley_lot, -1, block.lots_wide - 1);
  if (alley < 0) {
    add("_n", block.lots_wide, {lot_x(0), 0.0F, north_z}, along_x, 180.0F);
  } else {
    add("_nw", alley, {lot_x(0), 0.0F, north_z}, along_x, 180.0F);
    add("_ne",
        block.lots_wide - alley - 1,
        {lot_x(alley + 1), 0.0F, north_z},
        along_x,
        180.0F);
  }
  add("_s", block.lots_wide, {lot_x(0), 0.0F, south_z}, along_x, 0.0F);

  const float side_z = z0 + lot.depth + (lot.width * 0.5F);
  add("_w", block.lots_deep, {x0 + (lot.depth * 0.5F), 0.0F, side_z}, along_z, 270.0F);
  add("_e",
      block.lots_deep,
      {x0 + extent.width - (lot.depth * 0.5F), 0.0F, side_z},
      along_z,
      90.0F);
  return names;
}

namespace {

constexpr int k_lots_wide = 8;
constexpr int k_lots_deep = 3;
constexpr float k_side_street = 3.6F;
constexpr float k_cross_street = 4.2F;
constexpr float k_main_street = 5.6F;

struct BlockLight {
  const char* id;
  const char* label;
  float hour;
  float rain;
};

auto roman_block(const BlockLight& light) -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(light.id),
      QString::fromLatin1(light.label),
      QStringLiteral(
          "The Roman pilot block for city visual review. Nine blocks on a street "
          "grid: insula terraces two to four storeys high stand wall to wall "
          "around their courts, shops open onto the pavements of the main "
          "street, an older quarter of garden houses lies to the west and a "
          "small forum with its temple to the east. Every structure is an "
          "ordinary home, marketplace or temple; the insulae are homes with "
          "the tenement style."),
      40.0F,
      {96.0F, 44.0F, 205.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.arena_floor_half_extent = 96.0F;
  s.ground_type = QStringLiteral("grass_dry");
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.collect_animation_diagnostics = false;
  // An open horizon: the review is of the town, not of the arena's ring of
  // crags.
  s.suppress_boundary_mountains = true;
  s.environment.start_time = light.hour;
  s.environment.time_mode = Game::Map::TimeMode::Locked;
  s.weather.rain = light.rain;

  const TenementBlockExtent block = tenement_block_extent(k_lots_wide, k_lots_deep);
  const float pitch_x = block.width + k_side_street + (2.0F * k_pavement);
  const auto column_x = [&](int column) {
    return (static_cast<float>(column) * pitch_x) - (block.width * 0.5F);
  };
  const float mid_z = -block.depth * 0.5F;
  const float north_z = mid_z - k_cross_street - (2.0F * k_pavement) - block.depth;
  const float south_z = mid_z + block.depth + k_main_street + (2.0F * k_pavement);
  const auto row_z = [&](int row) {
    return row < 0 ? north_z : (row > 0 ? south_z : mid_z);
  };

  // Streets, laid before anything stands on them.
  const float west_edge = column_x(-1) - k_pavement;
  const float east_edge = column_x(1) + block.width + k_pavement;
  const float north_edge = north_z - k_pavement;
  const float south_edge = south_z + block.depth + k_pavement;
  const float reach_x0 = west_edge - k_side_street;
  const float reach_x1 = east_edge + k_side_street;
  const float reach_z0 = north_edge - k_cross_street;
  const float reach_z1 = south_edge + k_cross_street;
  for (const float x : {west_edge - (k_side_street * 0.5F),
                        column_x(0) - k_pavement - (k_side_street * 0.5F),
                        column_x(1) - k_pavement - (k_side_street * 0.5F),
                        east_edge + (k_side_street * 0.5F)}) {
    s.roads.push_back(
        street({x, 0.0F, reach_z0}, {x, 0.0F, reach_z1}, k_side_street, "stone"));
  }
  s.roads.push_back(street({reach_x0, 0.0F, north_edge - (k_cross_street * 0.5F)},
                           {reach_x1, 0.0F, north_edge - (k_cross_street * 0.5F)},
                           k_cross_street,
                           "stone"));
  s.roads.push_back(
      street({reach_x0, 0.0F, mid_z - k_pavement - (k_cross_street * 0.5F)},
             {reach_x1, 0.0F, mid_z - k_pavement - (k_cross_street * 0.5F)},
             k_cross_street,
             "stone"));
  const float main_z = mid_z + block.depth + k_pavement + (k_main_street * 0.5F);
  s.roads.push_back(street({reach_x0 - 14.0F, 0.0F, main_z},
                           {reach_x1 + 14.0F, 0.0F, main_z},
                           k_main_street,
                           "stone"));
  s.roads.push_back(street({reach_x0, 0.0F, south_edge + (k_cross_street * 0.5F)},
                           {reach_x1, 0.0F, south_edge + (k_cross_street * 0.5F)},
                           k_cross_street,
                           "stone"));

  std::vector<QString> terraces;
  int seed = 0;
  for (int row = -1; row <= 1; ++row) {
    for (int column = -1; column <= 1; ++column) {
      ++seed;
      if (row == 0 && column != 0) {
        continue;
      }
      const float x0 = column_x(column);
      const float z0 = row_z(row);
      const auto names = lay_tenement_block(
          s,
          {.name = QStringLiteral("insula_%1").arg(seed),
           .origin = {x0, 0.0F, z0},
           .lots_wide = k_lots_wide,
           .lots_deep = k_lots_deep,
           .alley_lot = (row == 0 && column == 0) ? 5 : 1 + ((seed * 3) % 6)});
      terraces.insert(terraces.end(), names.begin(), names.end());

      const QVector3D centre(
          x0 + (block.width * 0.5F), 0.0F, z0 + (block.depth * 0.5F));
      s.resource_patches.push_back(
          patch("olive_tree", 1, centre + QVector3D(-7.5F, 0.0F, 1.2F), {}, 0.95F));
      s.resource_patches.push_back(
          patch("cypress_tree", 1, centre + QVector3D(8.0F, 0.0F, -2.6F), {}, 0.9F));
      s.resource_patches.push_back(patch(
          "plant", 3, centre + QVector3D(-2.0F, 0.0F, 3.4F), {2.4F, 0.0F, 0.0F}, 0.9F));
      s.resource_patches.push_back(
          patch("supply_cart", 1, centre + QVector3D(2.5F, 0.0F, -2.4F), {}, 0.9F));
    }
  }

  // West: the older quarter of single-storey garden houses.
  {
    const float x0 = column_x(-1);
    for (int line = 0; line < 2; ++line) {
      const float z = mid_z + (line == 0 ? 2.6F : block.depth - 2.6F);
      s.groups.push_back(building(QStringLiteral("domus_row_%1").arg(line),
                                  Spawn::Home,
                                  Nation::RomanRepublic,
                                  1,
                                  5,
                                  {x0 + (block.width * 0.5F), 0.0F, z},
                                  {6.6F, 0.0F, 0.0F},
                                  line == 0 ? 180.0F : 0.0F));
    }
    const QVector3D garden(
        x0 + (block.width * 0.5F), 0.0F, mid_z + (block.depth * 0.5F));
    s.resource_patches.push_back(patch("olive_tree",
                                       4,
                                       garden + QVector3D(-10.5F, 0.0F, 0.0F),
                                       {7.0F, 0.0F, 0.0F},
                                       1.0F));
    s.resource_patches.push_back(patch("cypress_tree",
                                       2,
                                       garden + QVector3D(-14.0F, 0.0F, 0.0F),
                                       {28.0F, 0.0F, 0.0F},
                                       1.0F));
    s.resource_patches.push_back(patch(
        "plant", 6, garden + QVector3D(-12.0F, 0.0F, 1.8F), {4.8F, 0.0F, 0.0F}, 0.9F));
  }

  // East: a small forum, the temple closing its north end.
  {
    const float x0 = column_x(1);
    const QVector3D forum(
        x0 + (block.width * 0.5F), 0.0F, mid_z + (block.depth * 0.5F));
    // The whole square is paved, in strips a road can carry.
    constexpr int k_paving_strips = 4;
    const float strip = (block.depth + 1.0F) / static_cast<float>(k_paving_strips);
    for (int index = 0; index < k_paving_strips; ++index) {
      const float z = mid_z - 0.5F + (strip * (static_cast<float>(index) + 0.5F));
      s.roads.push_back(street({x0 - 0.5F, 0.0F, z},
                               {x0 + block.width + 0.5F, 0.0F, z},
                               strip + 1.2F,
                               "stone"));
    }
    s.groups.push_back(building(QStringLiteral("forum_temple"),
                                Spawn::Temple,
                                Nation::RomanRepublic,
                                1,
                                1,
                                forum + QVector3D(0.0F, 0.0F, -5.6F),
                                {},
                                0.0F));
    for (const float side : {-1.0F, 1.0F}) {
      s.groups.push_back(
          building(QStringLiteral("forum_stalls_%1").arg(side < 0.0F ? "west" : "east"),
                   Spawn::Marketplace,
                   Nation::RomanRepublic,
                   1,
                   3,
                   forum + QVector3D(side * (block.width * 0.5F - 3.2F), 0.0F, 0.6F),
                   {0.0F, 0.0F, 6.4F},
                   side < 0.0F ? 90.0F : 270.0F));
    }
    s.resource_patches.push_back(
        patch("statue", 1, forum + QVector3D(0.0F, 0.0F, 5.6F), {}, 1.2F));
    s.resource_patches.push_back(patch("cypress_tree",
                                       2,
                                       forum + QVector3D(-9.0F, 0.0F, -8.4F),
                                       {18.0F, 0.0F, 0.0F},
                                       1.0F));
  }

  // The street has people in it.
  s.groups.push_back(residents(QStringLiteral("main_street_folk"),
                               Nation::RomanRepublic,
                               1,
                               7,
                               {0.0F, 0.0F, main_z},
                               {7.0F, 0.0F, 0.0F},
                               20.0F));
  s.groups.push_back(
      residents(QStringLiteral("cross_street_folk"),
                Nation::RomanRepublic,
                1,
                5,
                {0.0F, 0.0F, mid_z - k_pavement - (k_cross_street * 0.5F)},
                {8.0F, 0.0F, 0.0F},
                16.0F));
  s.groups.push_back(residents(QStringLiteral("forum_folk"),
                               Nation::RomanRepublic,
                               1,
                               5,
                               {column_x(1) + (block.width * 0.5F), 0.0F, 2.0F},
                               {3.0F, 0.0F, 0.0F},
                               10.0F));
  auto watch = group(QStringLiteral("street_watch"),
                     Troop::Spearman,
                     1,
                     2,
                     {-12.0F, 0.0F, main_z},
                     0,
                     {5.0F, 0.0F, 0.0F});
  watch.facing_degrees = 90.0F;
  s.groups.push_back(std::move(watch));

  for (const auto& name : terraces) {
    s.expectations.push_back(expectation(Expect::GroupExists, name));
  }
  s.expectations.push_back(
      expectation(Expect::GroupExists, QStringLiteral("forum_temple")));
  for (const auto& name : {QStringLiteral("main_street_folk"),
                           QStringLiteral("cross_street_folk"),
                           QStringLiteral("forum_folk"),
                           QStringLiteral("street_watch")}) {
    s.expectations.push_back(expectation(Expect::GroupExists, name));
  }
  return s;
}

} // namespace

auto build_city_block_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  for (const BlockLight& light :
       {BlockLight{"city_block_roman", "City Block: Roman Insulae", 10.5F, 0.0F},
        BlockLight{"city_block_roman_wet_night",
                   "City Block: Roman Insulae, Wet Night",
                   21.5F,
                   Game::Map::k_weather_intensity_medium}}) {
    result.push_back(roman_block(light));
  }
  return result;
}

} // namespace Arena::Scenarios
