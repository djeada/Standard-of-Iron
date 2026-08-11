#include "arena_trailer_scenarios.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Intent = Game::Formation::ArmyFormationIntent;
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

auto residents(QString name,
               Nation nation,
               int owner,
               int count,
               QVector3D origin,
               QVector3D spacing,
               float roam_radius) -> ArenaScenarioGroup {
  auto result =
      group(std::move(name), Troop::Civilian, owner, count, origin, 1, spacing);
  result.nation_id = nation;
  result.settlement_resident = true;
  result.settlement_roam_radius = roam_radius;
  return result;
}

auto street(QVector3D start,
            QVector3D end,
            float width,
            const char* style = "stone") -> Game::Map::RoadSegment {
  return Game::Map::RoadSegment{start, end, width, QString::fromLatin1(style)};
}

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing = {2.5F, 0.0F, 0.0F},
           float scale = 1.0F) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type), count, origin, spacing, scale};
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

auto move_to(float time, QString source, QVector3D destination) -> ArenaScenarioStep {
  auto result = at(time, Command::Move, std::move(source));
  result.destination = destination;
  return result;
}

auto harvest_at(float time,
                QString source,
                QString resource_kind) -> ArenaScenarioStep {
  auto result = at(time, Command::HarvestResource, std::move(source));
  result.resource_kind = std::move(resource_kind);
  return result;
}

auto form_step(float time,
               QStringList groups,
               Intent intent,
               QVector3D anchor,
               float facing_degrees,
               float frontage = 0.0F) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name =
      QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), groups.value(0));
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::FormArmy;
  result.group = groups.value(0);
  result.formation.groups = std::move(groups);
  result.formation.intent = intent;
  result.formation.anchor = anchor;
  result.formation.facing_degrees = facing_degrees;
  result.formation.frontage = frontage;
  return result;
}

auto march_step(float time,
                QStringList groups,
                QVector3D anchor,
                float facing_degrees,
                float frontage) -> ArenaScenarioStep {
  auto result = form_step(
      time, std::move(groups), Intent::Column, anchor, facing_degrees, frontage);
  result.formation.options.movement_policy =
      Game::Formation::MovementPolicy::MaintainFormation;
  return result;
}

auto expectation(Expect kind,
                 QString source = {},
                 QString target = {},
                 float threshold = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.target_group = std::move(target);
  result.threshold = threshold;
  return result;
}

auto no_visibility_churn(QString source, float start, float end) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = Expect::NoRenderVisibilityChurn;
  result.group = std::move(source);
  result.start_seconds = start;
  result.end_seconds = end;
  return result;
}

auto undead_wave(QString trigger, std::vector<Game::Map::UndeadWaveUnitSpawn> units)
    -> Game::Map::UndeadWave {
  Game::Map::UndeadWave wave;
  wave.trigger = std::move(trigger);
  wave.units = std::move(units);
  return wave;
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
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.select_spawned_units = false;
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.force_full_creature_lod = true;
  result.collect_animation_diagnostics = false;
  result.graphics_quality = Render::GraphicsQuality::Ultra;
  result.environment.time_mode = Game::Map::TimeMode::Locked;
  return result;
}

constexpr float k_river_z = 16.0F;
constexpr float k_bridge_x = -2.0F;
constexpr float k_fort_cx = 30.0F;
constexpr float k_fort_cz = -16.0F;
constexpr float k_fort_half = 10.0F;
constexpr float k_valley_street_z = -14.0F;
constexpr float k_roman_town_x = -26.0F;
constexpr float k_roman_hamlet_x = 10.0F;
constexpr float k_punic_town_x = 26.0F;
constexpr float k_punic_town_z = 32.0F;
constexpr float k_punic_camp_x = -8.0F;
constexpr float k_punic_camp_z = 46.0F;
constexpr float k_roman_camp_x = -40.0F;
constexpr float k_roman_camp_z = 44.0F;
constexpr float k_fort_gate_clearance = 4.0F;

void add_fort(ArenaScenarioDefinition& scenario, Nation nation, int owner) {
  const float west = k_fort_cx - k_fort_half;
  const float east = k_fort_cx + k_fort_half;
  const float north = k_fort_cz - k_fort_half;
  const float south = k_fort_cz + k_fort_half;

  auto wall_run =
      [&](const char* name, float first, float last, bool along_x, float fixed) {
        const int count = static_cast<int>((last - first) / 2.0F) + 1;
        const float center = (first + last) * 0.5F;
        scenario.groups.push_back(building(
            QString::fromLatin1(name),
            Game::Units::SpawnType::WallSegment,
            nation,
            owner,
            count,
            along_x ? QVector3D(center, 0.0F, fixed) : QVector3D(fixed, 0.0F, center),
            along_x ? QVector3D(2.0F, 0.0F, 0.0F) : QVector3D(0.0F, 0.0F, 2.0F),
            along_x ? 0.0F : 90.0F));
      };

  wall_run("fort_wall_north", west + 2.0F, east - 2.0F, true, north);
  wall_run("fort_wall_south", west + 2.0F, east - 2.0F, true, south);
  wall_run("fort_wall_east", north + 2.0F, south - 2.0F, false, east);
  wall_run(
      "fort_wall_west_n", north + 2.0F, k_fort_cz - k_fort_gate_clearance, false, west);
  wall_run(
      "fort_wall_west_s", k_fort_cz + k_fort_gate_clearance, south - 2.0F, false, west);

  scenario.groups.push_back(building(QStringLiteral("fort_gate"),
                                     Game::Units::SpawnType::WallGate,
                                     nation,
                                     owner,
                                     1,
                                     {west, 0.0F, k_fort_cz},
                                     {},
                                     90.0F));
  for (auto const& corner :
       {std::pair{QStringLiteral("fort_tower_nw"), QVector3D{west, 0.0F, north}},
        std::pair{QStringLiteral("fort_tower_ne"), QVector3D{east, 0.0F, north}},
        std::pair{QStringLiteral("fort_tower_sw"), QVector3D{west, 0.0F, south}},
        std::pair{QStringLiteral("fort_tower_se"), QVector3D{east, 0.0F, south}}}) {
    scenario.groups.push_back(building(corner.first,
                                       Game::Units::SpawnType::DefenseTower,
                                       nation,
                                       owner,
                                       1,
                                       corner.second));
  }

  scenario.groups.push_back(building(QStringLiteral("fort_barracks"),
                                     Game::Units::SpawnType::Barracks,
                                     nation,
                                     owner,
                                     1,
                                     {k_fort_cx + 4.0F, 0.0F, k_fort_cz - 4.0F},
                                     {},
                                     270.0F));
  scenario.groups.push_back(building(QStringLiteral("fort_quarters"),
                                     Game::Units::SpawnType::Home,
                                     nation,
                                     owner,
                                     2,
                                     {k_fort_cx + 2.0F, 0.0F, k_fort_cz + 5.0F},
                                     {5.4F, 0.0F, 0.0F},
                                     0.0F));
  scenario.resource_patches.push_back(patch(
      "tent", 2, {k_fort_cx - 4.0F, 0.0F, k_fort_cz - 5.0F}, {3.6F, 0.0F, 0.0F}, 0.8F));
  scenario.resource_patches.push_back(
      patch("fire_camp", 1, {k_fort_cx - 2.0F, 0.0F, k_fort_cz + 1.5F}, {}, 0.85F));
  scenario.resource_patches.push_back(
      patch("weapon_rack", 1, {k_fort_cx + 6.0F, 0.0F, k_fort_cz + 1.0F}, {}, 1.0F));
}

enum class SettlementScale {
  Camp,
  Hamlet,
  Village,
  Town
};

void add_rampart(ArenaScenarioDefinition& scenario,
                 const QString& prefix,
                 Nation nation,
                 int owner,
                 QVector3D center,
                 float half_x,
                 float half_z,
                 float gate_clearance) {
  const float west = center.x() - half_x;
  const float east = center.x() + half_x;
  const float north = center.z() - half_z;
  const float south = center.z() + half_z;

  auto run =
      [&](const char* suffix, float first, float last, bool along_x, float fixed) {
        if (last <= first) {
          return;
        }
        const int count = static_cast<int>((last - first) / 2.0F) + 1;
        const float middle = (first + last) * 0.5F;
        scenario.groups.push_back(building(
            prefix + QString::fromLatin1(suffix),
            Game::Units::SpawnType::WallSegment,
            nation,
            owner,
            count,
            along_x ? QVector3D(middle, 0.0F, fixed) : QVector3D(fixed, 0.0F, middle),
            along_x ? QVector3D(2.0F, 0.0F, 0.0F) : QVector3D(0.0F, 0.0F, 2.0F),
            along_x ? 0.0F : 90.0F));
      };

  run("_wall_n", west + 2.0F, east - 2.0F, true, north);
  run("_wall_s", west + 2.0F, east - 2.0F, true, south);
  run("_wall_w_n", north + 2.0F, center.z() - gate_clearance, false, west);
  run("_wall_w_s", center.z() + gate_clearance, south - 2.0F, false, west);
  run("_wall_e_n", north + 2.0F, center.z() - gate_clearance, false, east);
  run("_wall_e_s", center.z() + gate_clearance, south - 2.0F, false, east);

  for (auto const& gate : {std::pair{QStringLiteral("_gate_w"), west},
                           std::pair{QStringLiteral("_gate_e"), east}}) {
    scenario.groups.push_back(building(prefix + gate.first,
                                       Game::Units::SpawnType::WallGate,
                                       nation,
                                       owner,
                                       1,
                                       {gate.second, 0.0F, center.z()},
                                       {},
                                       90.0F));
  }

  struct Corner {
    const char* suffix;
    float x;
    float z;
  };
  const float mid_z = std::round((north + center.z()) * 0.25F) * 2.0F;
  const float mid_x = std::round((west + center.x()) * 0.25F) * 2.0F;
  for (auto const& corner : {Corner{"_tower_nw", west, north},
                             Corner{"_tower_ne", east, north},
                             Corner{"_tower_sw", west, south},
                             Corner{"_tower_se", east, south},
                             Corner{"_tower_wn", west, mid_z},
                             Corner{"_tower_en", east, mid_z},
                             Corner{"_tower_nm", mid_x, north},
                             Corner{"_tower_sm", mid_x, south}}) {
    scenario.groups.push_back(building(prefix + QString::fromLatin1(corner.suffix),
                                       Game::Units::SpawnType::DefenseTower,
                                       nation,
                                       owner,
                                       1,
                                       {corner.x, 0.0F, corner.z}));
  }

  scenario.resource_patches.push_back(
      patch("fire_camp", 1, {west + 3.0F, 0.0F, center.z() + 3.5F}, {}, 0.95F));
  scenario.resource_patches.push_back(
      patch("fire_camp", 1, {east - 3.0F, 0.0F, center.z() - 3.5F}, {}, 0.95F));
}

struct SettlementPlan {
  QString prefix;
  Nation nation{Nation::RomanRepublic};
  int owner{1};
  QVector3D center;
  SettlementScale scale{SettlementScale::Village};
  bool add_residents{true};
  bool walled{false};
  bool acropolis{false};
};

constexpr float k_street_half_width = 1.7F;
constexpr float k_cross_half_width = 1.3F;
constexpr float k_plot_margin = 1.2F;
constexpr float k_max_building_half = 2.0F;
constexpr float k_row_pitch = 6.4F;
constexpr float k_col_pitch = 6.4F;
constexpr float k_cross_street_pitch = 19.2F;

[[nodiscard]] auto first_row_offset() -> float {
  return k_street_half_width + k_plot_margin + k_max_building_half;
}

struct SettlementShape {
  int columns{4};
  int rows{1};
  int cross_streets{0};
  bool has_market{true};
  bool has_temple{false};
  bool has_barracks{false};
};

[[nodiscard]] auto shape_for(SettlementScale scale) -> SettlementShape {
  switch (scale) {
  case SettlementScale::Camp:
    return {.columns = 3,
            .rows = 1,
            .cross_streets = 0,
            .has_market = false,
            .has_temple = false,
            .has_barracks = true};
  case SettlementScale::Hamlet:
    return {.columns = 3,
            .rows = 1,
            .cross_streets = 0,
            .has_market = true,
            .has_temple = false,
            .has_barracks = false};
  case SettlementScale::Village:
    return {.columns = 5,
            .rows = 1,
            .cross_streets = 1,
            .has_market = true,
            .has_temple = true,
            .has_barracks = false};
  case SettlementScale::Town:
  default:
    return {.columns = 5,
            .rows = 2,
            .cross_streets = 1,
            .has_market = true,
            .has_temple = true,
            .has_barracks = true};
  }
}

[[nodiscard]] auto settlement_half_width(SettlementScale scale) -> float {
  const auto shape = shape_for(scale);
  return (static_cast<float>(shape.columns) * k_col_pitch * 0.5F) + 2.0F;
}

[[nodiscard]] auto
settlement_plots(const SettlementPlan& plan) -> std::vector<QVector3D> {
  const auto shape = shape_for(plan.scale);
  std::vector<QVector3D> plots;
  plots.reserve(static_cast<std::size_t>(shape.columns) * shape.rows * 2U);

  const float column_span = static_cast<float>(shape.columns - 1) * k_col_pitch;
  for (int row = 0; row < shape.rows; ++row) {
    const float depth = first_row_offset() + (static_cast<float>(row) * k_row_pitch);
    for (int side = 0; side < 2; ++side) {
      const float sign = (side == 0) ? -1.0F : 1.0F;
      for (int column = 0; column < shape.columns; ++column) {
        const float local_x =
            (static_cast<float>(column) * k_col_pitch) - (column_span * 0.5F);

        bool blocked = false;
        if (shape.cross_streets > 0 &&
            std::abs(local_x) < k_cross_half_width + k_max_building_half + 0.6F) {
          blocked = true;
        }
        if (blocked) {
          continue;
        }
        plots.push_back(plan.center + QVector3D(local_x, 0.0F, sign * depth));
      }
    }
  }
  return plots;
}

void add_settlement(ArenaScenarioDefinition& scenario, const SettlementPlan& plan) {
  const auto shape = shape_for(plan.scale);
  const float half_width = settlement_half_width(plan.scale);

  scenario.roads.push_back(street({plan.center.x() - half_width, 0.0F, plan.center.z()},
                                  {plan.center.x() + half_width, 0.0F, plan.center.z()},
                                  k_street_half_width * 2.0F));

  const float lane_reach =
      first_row_offset() + (static_cast<float>(shape.rows) * k_row_pitch);
  if (shape.cross_streets > 0) {
    scenario.roads.push_back(
        street({plan.center.x(), 0.0F, plan.center.z() - lane_reach},
               {plan.center.x(), 0.0F, plan.center.z() + lane_reach},
               k_cross_half_width * 2.0F));
  }

  auto plots = settlement_plots(plan);
  std::size_t next = 0U;
  auto take = [&]() -> QVector3D {
    return next < plots.size() ? plots[next++] : plan.center;
  };

  auto place = [&](const QString& suffix,
                   Game::Units::SpawnType type,
                   const QVector3D& at,
                   float facing) {
    scenario.groups.push_back(building(
        plan.prefix + suffix, type, plan.nation, plan.owner, 1, at, {}, facing));
  };

  if (shape.has_market) {
    const QVector3D at = take();
    place(QStringLiteral("_market"),
          Game::Units::SpawnType::Marketplace,
          at,
          at.z() < plan.center.z() ? 0.0F : 180.0F);
  }
  if (shape.has_temple) {
    if (plan.acropolis) {
      const float reach =
          first_row_offset() + (static_cast<float>(shape.rows) * k_row_pitch) + 4.0F;
      const QVector3D crown = plan.center + QVector3D(0.0F, 0.0F, -reach);
      scenario.elevation_patches.push_back({crown, 11.0F, 4.6F});
      place(QStringLiteral("_temple"), Game::Units::SpawnType::Temple, crown, 180.0F);
      scenario.resource_patches.push_back(
          patch("statue", 1, crown + QVector3D(-4.5F, 0.0F, 4.5F), {}, 1.1F));
      scenario.resource_patches.push_back(
          patch("statue", 1, crown + QVector3D(4.5F, 0.0F, 4.5F), {}, 1.1F));
      scenario.resource_patches.push_back(
          patch("fire_camp", 1, crown + QVector3D(0.0F, 0.0F, 5.5F), {}, 0.9F));
    } else {
      const QVector3D at = take();
      place(QStringLiteral("_temple"),
            Game::Units::SpawnType::Temple,
            at,
            at.z() < plan.center.z() ? 0.0F : 180.0F);
    }
  }
  if (shape.has_barracks) {
    const QVector3D at = take();
    place(QStringLiteral("_barracks"),
          Game::Units::SpawnType::Barracks,
          at,
          at.z() < plan.center.z() ? 0.0F : 180.0F);
  }

  int home_index = 0;
  while (next < plots.size()) {
    const QVector3D at = take();
    place(QStringLiteral("_home_%1").arg(home_index++),
          Game::Units::SpawnType::Home,
          at,
          at.z() < plan.center.z() ? 0.0F : 180.0F);
  }

  if (plan.walled) {
    const float plot_reach_x =
        (static_cast<float>(shape.columns - 1) * k_col_pitch * 0.5F) +
        k_max_building_half;
    const float plot_reach_z = first_row_offset() +
                               (static_cast<float>(shape.rows - 1) * k_row_pitch) +
                               k_max_building_half;
    const float acropolis_reach =
        plan.acropolis
            ? first_row_offset() + (static_cast<float>(shape.rows) * k_row_pitch) + 6.0F
            : 0.0F;
    auto to_even = [](float value) {
      return std::ceil(value / 2.0F) * 2.0F;
    };
    add_rampart(scenario,
                plan.prefix,
                plan.nation,
                plan.owner,
                plan.center,
                to_even(plot_reach_x + 3.0F),
                to_even(std::max(plot_reach_z, acropolis_reach) + 3.0F),
                4.0F);
  }

  scenario.resource_patches.push_back(
      patch("statue", 1, plan.center + QVector3D(-2.0F, 0.0F, 0.0F), {}, 1.0F));

  if (plan.add_residents && home_index > 0) {
    scenario.groups.push_back(residents(plan.prefix + QStringLiteral("_folk"),
                                        plan.nation,
                                        plan.owner,
                                        std::min(4, 2 + (home_index / 3)),
                                        plan.center + QVector3D(-2.0F, 0.0F, 0.0F),
                                        {3.6F, 0.0F, 0.0F},
                                        half_width * 0.7F));
  }
}

struct ValleyOptions {
  bool fort{true};
  bool village_residents{true};
  bool cursed_grove{false};
};

void dress_valley(ArenaScenarioDefinition& scenario, const ValleyOptions& options) {
  scenario.arena_floor_half_extent = 56.0F;

  scenario.rivers.push_back(Game::Map::RiverSegment{
      {-45.0F, 0.0F, k_river_z}, {45.0F, 0.0F, k_river_z}, 6.0F});
  scenario.bridges.push_back(Game::Map::Bridge{{k_bridge_x, 0.0F, k_river_z - 5.5F},
                                               {k_bridge_x, 0.0F, k_river_z + 5.5F},
                                               4.5F,
                                               0.45F});

  scenario.roads.push_back(street({k_bridge_x, 0.0F, k_valley_street_z},
                                  {k_bridge_x, 0.0F, k_river_z - 5.5F},
                                  2.8F));
  scenario.roads.push_back(
      street({k_bridge_x, 0.0F, k_river_z + 5.5F}, {k_bridge_x, 0.0F, 38.0F}, 2.8F));
  scenario.roads.push_back(street({k_roman_town_x + 14.0F, 0.0F, k_valley_street_z},
                                  {k_roman_hamlet_x - 14.0F, 0.0F, k_valley_street_z},
                                  3.0F));
  scenario.roads.push_back(street({k_fort_cx - k_fort_half, 0.0F, k_fort_cz},
                                  {k_fort_cx - k_fort_half, 0.0F, k_valley_street_z},
                                  2.8F));

  add_settlement(scenario,
                 {.prefix = QStringLiteral("roman_town"),
                  .nation = Nation::RomanRepublic,
                  .owner = 1,
                  .center = {k_roman_town_x, 0.0F, k_valley_street_z},
                  .scale = SettlementScale::Town,
                  .add_residents = options.village_residents,
                  .walled = true,
                  .acropolis = true});

  add_settlement(scenario,
                 {.prefix = QStringLiteral("roman_hamlet"),
                  .nation = Nation::RomanRepublic,
                  .owner = 1,
                  .center = {k_roman_hamlet_x, 0.0F, k_valley_street_z},
                  .scale = SettlementScale::Hamlet,
                  .add_residents = options.village_residents});

  add_settlement(scenario,
                 {.prefix = QStringLiteral("punic_city"),
                  .nation = Nation::Carthage,
                  .owner = 2,
                  .center = {k_punic_town_x, 0.0F, k_punic_town_z},
                  .scale = SettlementScale::Town,
                  .add_residents = options.village_residents,
                  .walled = true,
                  .acropolis = true});

  add_settlement(scenario,
                 {.prefix = QStringLiteral("punic_camp"),
                  .nation = Nation::Carthage,
                  .owner = 2,
                  .center = {k_punic_camp_x, 0.0F, k_punic_camp_z},
                  .scale = SettlementScale::Camp,
                  .add_residents = false});

  add_settlement(scenario,
                 {.prefix = QStringLiteral("roman_camp"),
                  .nation = Nation::RomanRepublic,
                  .owner = 1,
                  .center = {k_roman_camp_x, 0.0F, k_roman_camp_z},
                  .scale = SettlementScale::Camp,
                  .add_residents = false});

  scenario.resource_patches.push_back(
      patch("olive_tree", 5, {-34.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 4.2F}, 1.15F));
  scenario.resource_patches.push_back(
      patch("pine_tree", 5, {-38.0F, 0.0F, 6.0F}, {0.0F, 0.0F, 4.6F}, 1.2F));
  scenario.resource_patches.push_back(
      patch("boulder", 3, {-32.0F, 0.0F, 14.0F}, {3.2F, 0.0F, 0.0F}, 1.1F));
  scenario.resource_patches.push_back(
      patch("supply_cart",
            3,
            {k_roman_town_x + 3.0F, 0.0F, k_valley_street_z + 2.4F},
            {3.4F, 0.0F, 0.0F},
            0.95F));
  scenario.resource_patches.push_back(
      patch("fire_camp",
            1,
            {k_roman_hamlet_x - 3.0F, 0.0F, k_valley_street_z - 3.0F},
            {},
            0.85F));
  scenario.resource_patches.push_back(
      patch("plant", 6, {-30.0F, 0.0F, 10.0F}, {2.8F, 0.0F, 0.0F}, 0.9F));

  if (options.fort) {
    add_fort(scenario, Nation::RomanRepublic, 1);
  }

  if (options.cursed_grove) {
    scenario.resource_patches.push_back(
        patch("magic_shrine", 1, {-22.0F, 0.0F, 35.0F}, {}, 1.2F));
    scenario.resource_patches.push_back(
        patch("dead_tree", 4, {-30.0F, 0.0F, 30.0F}, {4.0F, 0.0F, 3.0F}, 1.2F));
    scenario.resource_patches.push_back(
        patch("dead_tree", 3, {-16.0F, 0.0F, 40.0F}, {4.2F, 0.0F, 2.6F}, 1.2F));
    scenario.resource_patches.push_back(
        patch("ruins", 2, {-28.0F, 0.0F, 39.0F}, {5.0F, 0.0F, 2.0F}, 1.1F));
  } else {
    scenario.resource_patches.push_back(
        patch("pine_tree", 4, {-34.0F, 0.0F, 30.0F}, {4.4F, 0.0F, 3.0F}, 1.2F));
  }
}

auto default_wildlife(unsigned seed) -> Game::Wildlife::WildlifeSettings {
  auto settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = seed;
  settings.sheep.enabled = false;
  settings.sheep.group_count = 0;
  settings.wolves.enabled = false;
  settings.wolves.group_count = 0;
  settings.birds.enabled = false;
  settings.birds.group_count = 0;
  return settings;
}

auto trailer_dawn() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_dawn_id),
      QStringLiteral("Trailer I: The Land"),
      QStringLiteral("Dawn over the valley. The village works its morning: "
                     "residents on their errands, woodcutters and quarriers at "
                     "the tree line, the flock in the west pasture under a "
                     "passing flock of birds - and the wolves come out of the "
                     "deep wood at the sheep while the riders of the watch "
                     "answer."),
      55.0F,
      {36.0F, 40.0F, 24.0F});

  s.collect_animation_diagnostics = true;
  s.environment.start_time = 11.2F;
  s.environment.fog_density_override = 0.020F;
  s.environment.exposure_override = 1.5F;

  dress_valley(s, {.fort = true, .village_residents = true, .cursed_grove = false});

  s.wildlife = default_wildlife(20260810U);
  s.wildlife.sheep.enabled = true;
  s.wildlife.sheep.group_count = 1;
  s.wildlife.sheep.group_size_min = 7;
  s.wildlife.sheep.group_size_max = 7;
  s.wildlife.sheep.roam_radius = 5.0F;
  s.wildlife.sheep.spawn_areas = {{-13.0F, 7.0F, 4.0F}};
  s.wildlife.birds.enabled = true;
  s.wildlife.birds.group_count = 2;
  s.wildlife.birds.group_size_min = 9;
  s.wildlife.birds.group_size_max = 11;
  s.wildlife.birds.flight_height = 15.0F;
  s.wildlife.birds.roam_radius = 16.0F;
  s.wildlife.birds.spawn_areas = {{-4.0F, 12.0F, 6.0F}, {6.0F, -10.0F, 6.0F}};
  s.wildlife.birds.flyover_interval_min = 0.0F;
  s.wildlife.birds.flyover_interval_max = 0.0F;
  s.wildlife.wolves.enabled = true;
  s.wildlife.wolves.group_count = 0;
  s.wildlife.wolves.group_size_min = 4;
  s.wildlife.wolves.group_size_max = 4;
  s.wildlife.wolves.aggression = 1.0F;
  s.wildlife.wolves.roam_radius = 20.0F;
  s.wildlife.wolves.alert_radius = 9.0F;
  s.wildlife.wolves.respawn = false;
  s.wildlife.wolves.waves = {{24.0F, 4, {-27.0F, 7.0F, 2.0F}, "dawn_pack"}};

  auto woodcutters = group(QStringLiteral("valley_woodcutters"),
                           Troop::Builder,
                           1,
                           2,
                           {-31.0F, 0.0F, -11.0F},
                           1,
                           {3.2F, 0.0F, 0.0F});
  auto quarriers = group(QStringLiteral("valley_quarriers"),
                         Troop::Builder,
                         1,
                         2,
                         {-29.0F, 0.0F, 12.0F},
                         1,
                         {3.2F, 0.0F, 0.0F});
  auto watch = group(QStringLiteral("valley_watch"),
                     Troop::MountedKnight,
                     1,
                     2,
                     {6.0F, 0.0F, 6.0F},
                     4,
                     {2.8F, 0.0F, 0.0F});
  s.groups.push_back(woodcutters);
  s.groups.push_back(quarriers);
  s.groups.push_back(watch);

  s.steps = {
      harvest_at(1.0F, QStringLiteral("valley_woodcutters"), QStringLiteral("tree")),
      harvest_at(1.6F, QStringLiteral("valley_quarriers"), QStringLiteral("boulder")),
      move_to(2.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x + 8.0F, 0.0F, k_valley_street_z}),
      move_to(16.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x - 6.0F, 0.0F, k_valley_street_z - 3.0F}),
      move_to(34.0F, QStringLiteral("valley_watch"), {-14.0F, 0.0F, 7.0F}),
      move_to(38.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x + 2.0F, 0.0F, k_valley_street_z + 4.0F}),
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("roman_town_market")),
      expectation(Expect::GroupExists, QStringLiteral("valley_woodcutters")),
      expectation(Expect::GroupExists, QStringLiteral("valley_watch")),
      expectation(Expect::MovementAnimationObserved, QStringLiteral("roman_town_folk")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("roman_town_folk")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("valley_watch")),
      expectation(Expect::WildlifeGrazingObserved),
      expectation(Expect::WildlifeHuntObserved),
      expectation(Expect::WildlifeCasualtyObserved),
  };
  return s;
}

auto trailer_muster() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_muster_id),
      QStringLiteral("Trailer II: The Legion"),
      QStringLiteral("Mid-morning. The fort turns its garrison out: the column "
                     "comes through the west gate behind the consul, marches "
                     "the valley road, crosses the bridge under scattering "
                     "birds, and deploys from column into line on the south "
                     "plain."),
      92.0F,
      {40.0F, 35.0F, 10.0F});
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 12.4F;
  s.environment.fog_density_override = 0.020F;
  s.environment.exposure_override = 1.25F;

  dress_valley(s, {.fort = true, .village_residents = true, .cursed_grove = false});

  s.wildlife = default_wildlife(20260811U);
  s.wildlife.birds.enabled = true;
  s.wildlife.birds.group_count = 2;
  s.wildlife.birds.group_size_min = 9;
  s.wildlife.birds.group_size_max = 12;
  s.wildlife.birds.flight_height = 14.0F;
  s.wildlife.birds.roam_radius = 12.0F;
  s.wildlife.birds.alert_radius = 13.0F;
  s.wildlife.birds.spawn_areas = {{k_bridge_x, k_river_z, 6.0F}, {-8.0F, 24.0F, 6.0F}};
  s.wildlife.birds.flyover_interval_min = 0.0F;
  s.wildlife.birds.flyover_interval_max = 0.0F;
  s.wildlife.sheep.enabled = true;
  s.wildlife.sheep.group_count = 1;
  s.wildlife.sheep.group_size_min = 6;
  s.wildlife.sheep.group_size_max = 6;
  s.wildlife.sheep.roam_radius = 4.0F;
  s.wildlife.sheep.alert_radius = 5.0F;
  s.wildlife.sheep.spawn_areas = {{-13.0F, 7.0F, 4.0F}};

  auto consul = group(QStringLiteral("legion_commander"),
                      Troop::RomanVeteranConsul,
                      1,
                      1,
                      {k_bridge_x, 0.0F, 8.0F},
                      1);
  auto swords = group(QStringLiteral("legion_swords"),
                      Troop::Swordsman,
                      1,
                      4,
                      {k_bridge_x - 5.4F, 0.0F, 2.0F},
                      10,
                      {3.6F, 0.0F, 0.0F});
  auto spears = group(QStringLiteral("legion_spears"),
                      Troop::Spearman,
                      1,
                      3,
                      {k_bridge_x - 3.6F, 0.0F, -3.0F},
                      10,
                      {3.6F, 0.0F, 0.0F});
  auto archers = group(QStringLiteral("legion_archers"),
                       Troop::Archer,
                       1,
                       2,
                       {k_bridge_x - 1.6F, 0.0F, -7.5F},
                       8,
                       {3.2F, 0.0F, 0.0F});
  auto cavalry = group(QStringLiteral("legion_cavalry"),
                       Troop::MountedKnight,
                       1,
                       2,
                       {24.0F, 0.0F, 22.0F},
                       5,
                       {3.4F, 0.0F, 0.0F});

  auto fort_watch = group(QStringLiteral("fort_watch"),
                          Troop::Spearman,
                          1,
                          2,
                          {k_fort_cx - 4.0F, 0.0F, k_fort_cz},
                          8,
                          {3.4F, 0.0F, 0.0F});

  s.groups.push_back(consul);
  s.groups.push_back(swords);
  s.groups.push_back(spears);
  s.groups.push_back(archers);
  s.groups.push_back(cavalry);
  s.groups.push_back(fort_watch);

  const QStringList legion = {QStringLiteral("legion_swords"),
                              QStringLiteral("legion_spears"),
                              QStringLiteral("legion_archers")};

  auto run_to = [](float time, QString source, QVector3D destination) {
    auto step = at(time, Command::Run, std::move(source));
    step.destination = destination;
    return step;
  };

  s.steps = {
      move_to(1.0F, QStringLiteral("fort_watch"), {12.0F, 0.0F, k_fort_cz}),
      move_to(2.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x + 7.0F, 0.0F, k_valley_street_z}),
      move_to(20.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x - 5.0F, 0.0F, k_valley_street_z + 3.0F}),
      run_to(0.5F, QStringLiteral("legion_commander"), {k_bridge_x, 0.0F, 25.0F}),
      run_to(8.0F, QStringLiteral("legion_swords"), {k_bridge_x, 0.0F, 30.0F}),
      run_to(22.0F, QStringLiteral("legion_spears"), {k_bridge_x - 3.0F, 0.0F, 27.0F}),
      run_to(44.0F, QStringLiteral("legion_archers"), {k_bridge_x + 3.0F, 0.0F, 24.0F}),
      form_step(52.0F, legion, Intent::Line, {0.0F, 0.0F, 33.0F}, 180.0F, 26.0F),
      run_to(54.0F, QStringLiteral("legion_cavalry"), {19.0F, 0.0F, 33.0F}),
      move_to(56.0F, QStringLiteral("legion_commander"), {0.0F, 0.0F, 30.0F}),
      form_step(74.0F, legion, Intent::Assault, {0.0F, 0.0F, 37.0F}, 180.0F, 24.0F),
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("fort_gate")),
      expectation(Expect::GroupExists, QStringLiteral("legion_swords")),
      no_visibility_churn(QStringLiteral("legion_swords"), 0.0F, 2.0F),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("legion_swords")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("legion_spears")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("roman_town_folk")),
      expectation(Expect::GroupExists, QStringLiteral("legion_commander")),
      expectation(Expect::MovementAnimationObserved, QStringLiteral("roman_town_folk")),
      expectation(Expect::GateOpenedObserved, QStringLiteral("fort_gate")),
  };
  return s;
}

auto trailer_clash() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_clash_id),
      QStringLiteral("Trailer III: The Battle"),
      QStringLiteral("Golden hour on the south plain. The legion stands with "
                     "its back to the river while the Carthaginian host comes "
                     "on behind its elephants; volleys, a cavalry counter-"
                     "charge meeting in the open, and the lines going in."),
      60.0F,
      {88.0F, 36.0F, 0.0F});
  s.camera_focus = QVector3D(0.0F, 0.0F, 18.0F);
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 18.1F;
  s.environment.fog_density_override = 0.024F;

  s.environment.exposure_override = 1.7F;

  dress_valley(s, {.fort = true, .village_residents = false, .cursed_grove = false});

  auto consul = group(QStringLiteral("roman_consul"),
                      Troop::RomanVeteranConsul,
                      1,
                      1,
                      {0.0F, 0.0F, 24.5F},
                      1);
  auto roman_swords = group(QStringLiteral("roman_line"),
                            Troop::Swordsman,
                            1,
                            7,
                            {-9.9F, 0.0F, 27.0F},
                            12,
                            {3.3F, 0.0F, 0.0F});
  auto roman_spears = group(QStringLiteral("roman_spears"),
                            Troop::Spearman,
                            1,
                            5,
                            {-6.6F, 0.0F, 24.0F},
                            12,
                            {3.3F, 0.0F, 0.0F});
  auto roman_archers = group(QStringLiteral("roman_archers"),
                             Troop::Archer,
                             1,
                             4,
                             {-5.0F, 0.0F, 21.5F},
                             8,
                             {3.3F, 0.0F, 0.0F});
  auto roman_horse = group(QStringLiteral("roman_horse"),
                           Troop::MountedKnight,
                           1,
                           4,
                           {18.0F, 0.0F, 24.0F},
                           6,
                           {3.4F, 0.0F, 0.0F});
  auto punic_swords = group(QStringLiteral("punic_line"),
                            Troop::Swordsman,
                            2,
                            8,
                            {-11.5F, 0.0F, 33.0F},
                            12,
                            {3.3F, 0.0F, 0.0F});
  auto punic_spears = group(QStringLiteral("punic_spears"),
                            Troop::Spearman,
                            2,
                            6,
                            {-8.2F, 0.0F, 35.5F},
                            12,
                            {3.3F, 0.0F, 0.0F});
  auto punic_archers = group(QStringLiteral("punic_archers"),
                             Troop::Archer,
                             2,
                             4,
                             {-5.0F, 0.0F, 37.0F},
                             8,
                             {3.3F, 0.0F, 0.0F});
  auto punic_horse = group(QStringLiteral("punic_horse"),
                           Troop::MountedKnight,
                           2,
                           4,
                           {-26.0F, 0.0F, 33.0F},
                           6,
                           {3.4F, 0.0F, 0.0F});
  auto elephants = group(QStringLiteral("punic_elephants"),
                         Troop::Elephant,
                         2,
                         3,
                         {-8.0F, 0.0F, 34.5F},
                         1,
                         {9.0F, 0.0F, 0.0F});

  for (auto* line : {&roman_swords, &roman_spears, &punic_swords, &punic_spears}) {
    line->health_override = line->max_health_override = 2600;
  }
  consul.health_override = consul.max_health_override = 6000;

  s.groups.push_back(consul);
  s.groups.push_back(roman_swords);
  s.groups.push_back(roman_spears);
  s.groups.push_back(roman_archers);
  s.groups.push_back(roman_horse);
  s.groups.push_back(punic_swords);
  s.groups.push_back(punic_spears);
  s.groups.push_back(punic_archers);
  s.groups.push_back(punic_horse);
  s.groups.push_back(elephants);

  auto aura = at(14.0F, Command::TriggerCommanderAura, QStringLiteral("roman_consul"));
  aura.value = 2;

  s.steps = {
      at(0.3F, Command::Hold, QStringLiteral("roman_line")),
      at(0.3F, Command::Hold, QStringLiteral("roman_spears")),
      at(1.0F,
         Command::Attack,
         QStringLiteral("roman_archers"),
         QStringLiteral("punic_line")),
      at(1.2F,
         Command::Attack,
         QStringLiteral("punic_archers"),
         QStringLiteral("roman_line")),
      at(3.5F,
         Command::Charge,
         QStringLiteral("punic_elephants"),
         QStringLiteral("roman_line")),
      at(4.0F,
         Command::Charge,
         QStringLiteral("punic_horse"),
         QStringLiteral("roman_archers")),
      at(5.0F,
         Command::Charge,
         QStringLiteral("roman_horse"),
         QStringLiteral("punic_horse")),
      at(6.0F,
         Command::AttackMove,
         QStringLiteral("punic_line"),
         QStringLiteral("roman_line")),
      at(6.5F,
         Command::AttackMove,
         QStringLiteral("punic_spears"),
         QStringLiteral("roman_spears")),
      at(8.5F,
         Command::AttackMove,
         QStringLiteral("roman_line"),
         QStringLiteral("punic_line")),
      at(9.0F,
         Command::AttackMove,
         QStringLiteral("roman_spears"),
         QStringLiteral("punic_spears")),
      at(13.0F,
         Command::Attack,
         QStringLiteral("roman_consul"),
         QStringLiteral("punic_line")),
      aura,
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("punic_line")),
      expectation(Expect::GroupExists, QStringLiteral("punic_elephants")),
      no_visibility_churn(QStringLiteral("roman_line"), 0.0F, 2.0F),
      no_visibility_churn(QStringLiteral("punic_line"), 0.0F, 2.0F),
  };
  return s;
}

auto trailer_pov() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_pov_id),
      QStringLiteral("Trailer III: Lead From The Front"),
      QStringLiteral("The same fight from behind the consul's shoulders: the "
                     "commander wades into the Carthaginian line under direct "
                     "control while the melee closes around him."),
      30.0F);
  s.environment.start_time = 18.1F;
  s.environment.fog_density_override = 0.024F;

  s.environment.exposure_override = 1.5F;
  s.rpg_mode = true;
  s.rpg_commander_group = QStringLiteral("rpg_commander");

  dress_valley(s, {.fort = true, .village_residents = false, .cursed_grove = false});

  auto commander = group(QStringLiteral("rpg_commander"),
                         Troop::RomanVeteranConsul,
                         1,
                         1,
                         {0.0F, 0.0F, 34.0F},
                         1);
  commander.facing_degrees = 180.0F;
  commander.health_override = commander.max_health_override = 9000;

  auto enemy_wave = group(QStringLiteral("enemy_wave"),
                          Troop::Swordsman,
                          2,
                          2,
                          {-3.0F, 0.0F, 31.4F},
                          6,
                          {6.0F, 0.0F, 0.0F});
  enemy_wave.health_override = enemy_wave.max_health_override = 420;

  auto enemy_second = group(QStringLiteral("enemy_second"),
                            Troop::Swordsman,
                            2,
                            2,
                            {-3.0F, 0.0F, 26.5F},
                            6,
                            {6.0F, 0.0F, 0.0F});
  enemy_second.health_override = enemy_second.max_health_override = 420;

  auto melee_left_roman = group(QStringLiteral("melee_left_roman"),
                                Troop::Swordsman,
                                1,
                                2,
                                {-13.0F, 0.0F, 32.0F},
                                10,
                                {3.3F, 0.0F, 0.0F});
  auto melee_left_punic = group(QStringLiteral("melee_left_punic"),
                                Troop::Swordsman,
                                2,
                                2,
                                {-13.0F, 0.0F, 27.0F},
                                10,
                                {3.3F, 0.0F, 0.0F});
  auto melee_right_roman = group(QStringLiteral("melee_right_roman"),
                                 Troop::Spearman,
                                 1,
                                 2,
                                 {10.0F, 0.0F, 32.0F},
                                 10,
                                 {3.3F, 0.0F, 0.0F});
  auto melee_right_punic = group(QStringLiteral("melee_right_punic"),
                                 Troop::Swordsman,
                                 2,
                                 2,
                                 {10.0F, 0.0F, 27.0F},
                                 10,
                                 {3.3F, 0.0F, 0.0F});

  s.groups.push_back(commander);
  s.groups.push_back(enemy_wave);
  s.groups.push_back(enemy_second);
  s.groups.push_back(melee_left_roman);
  s.groups.push_back(melee_left_punic);
  s.groups.push_back(melee_right_roman);
  s.groups.push_back(melee_right_punic);

  auto rpg_move = [](float time, QVector3D axes, bool run) {
    auto step = at(time, Command::RpgMove, QStringLiteral("rpg_commander"));
    step.destination = axes;
    step.value = run ? 1 : 0;
    return step;
  };
  auto hold_attack = [](float time, bool held) {
    auto step = at(time, Command::RpgAttackHold, QStringLiteral("rpg_commander"));
    step.enabled = held;
    return step;
  };

  s.steps = {
      at(0.3F,
         Command::AttackMove,
         QStringLiteral("melee_left_punic"),
         QStringLiteral("melee_left_roman")),
      at(0.3F,
         Command::AttackMove,
         QStringLiteral("melee_right_punic"),
         QStringLiteral("melee_right_roman")),
      at(0.6F,
         Command::Attack,
         QStringLiteral("enemy_wave"),
         QStringLiteral("rpg_commander")),
      at(6.0F,
         Command::Attack,
         QStringLiteral("enemy_second"),
         QStringLiteral("rpg_commander")),
      hold_attack(1.0F, true),

      rpg_move(9.4F, {0.0F, 0.0F, 1.0F}, false),
      rpg_move(11.2F, {0.0F, 0.0F, 0.0F}, false),
      hold_attack(20.0F, false),
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("rpg_commander")),
      expectation(Expect::GroupHealthReduced, QStringLiteral("enemy_wave")),
  };
  return s;
}

auto trailer_barrow_night() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_night_id),
      QStringLiteral("Trailer IV: The Night"),
      QStringLiteral("Midnight. The victorious column pursues the beaten enemy "
                     "into the cursed grove south-west of the crossing - and "
                     "the barrow garrison stands up out of the ground around "
                     "it."),
      45.0F,
      {26.0F, 24.0F, 40.0F});
  s.camera_focus = QVector3D(-22.0F, 0.0F, 35.0F);
  s.environment.start_time = 21.5F;
  s.environment.lighting_profile = QStringLiteral("iron_sepulcher");
  s.environment.exposure_override = 2.3F;
  s.environment.fog_density_override = 0.010F;

  dress_valley(s, {.fort = true, .village_residents = false, .cursed_grove = true});

  s.resource_patches.push_back(patch("fire_camp", 1, {-6.0F, 0.0F, 26.0F}, {}, 0.9F));
  s.resource_patches.push_back(patch("fire_camp", 1, {-14.0F, 0.0F, 29.5F}, {}, 1.0F));
  s.resource_patches.push_back(patch("fire_camp", 1, {-24.0F, 0.0F, 29.0F}, {}, 1.0F));

  s.undead_zones = {[] {
    Game::Map::UndeadZone zone;
    zone.id = QStringLiteral("barrow_garrison");
    zone.anchor_type = Game::Map::WorldProp::Type::MagicShrine;
    zone.x = -22.0F;
    zone.z = 35.0F;
    zone.radius = 10.0F;
    zone.leash_radius = 18.0F;
    zone.owner_id = 99;
    zone.team_id = 99;
    zone.awaken_on = {QStringLiteral("unit_enters_radius")};
    zone.waves = {undead_wave(QStringLiteral("initial"),
                              {{Game::Units::SpawnType::SkeletonSwordsman, 8},
                               {Game::Units::SpawnType::SkeletonArcher, 4},
                               {Game::Units::SpawnType::GravePriest, 2}}),
                  undead_wave(QStringLiteral("after_clear"),
                              {{Game::Units::SpawnType::SkeletonSwordsman, 5}})};
    return zone;
  }()};

  auto column = group(QStringLiteral("night_column"),
                      Troop::Swordsman,
                      1,
                      3,
                      {2.0F, 0.0F, 24.0F},
                      10,
                      {3.0F, 0.0F, 0.0F});
  auto flank = group(QStringLiteral("night_flank"),
                     Troop::Spearman,
                     1,
                     2,
                     {5.0F, 0.0F, 27.0F},
                     10,
                     {3.0F, 0.0F, 0.0F});
  auto commander = group(QStringLiteral("night_commander"),
                         Troop::RomanVeteranConsul,
                         1,
                         1,
                         {-1.0F, 0.0F, 26.0F},
                         1);
  auto quarry = group(QStringLiteral("beaten_enemy"),
                      Troop::Swordsman,
                      2,
                      1,
                      {-14.0F, 0.0F, 31.0F},
                      6,
                      {2.6F, 0.0F, 0.0F});
  quarry.health_override = quarry.max_health_override = 900;

  column.health_override = column.max_health_override = 3000;
  flank.health_override = flank.max_health_override = 3000;
  commander.health_override = commander.max_health_override = 9000;

  s.groups.push_back(column);
  s.groups.push_back(flank);
  s.groups.push_back(commander);
  s.groups.push_back(quarry);

  auto run_to = [](float time, QString source, QVector3D destination) {
    auto step = at(time, Command::Run, std::move(source));
    step.destination = destination;
    return step;
  };
  s.steps = {
      run_to(0.5F, QStringLiteral("beaten_enemy"), {-26.0F, 0.0F, 33.0F}),
      run_to(2.0F, QStringLiteral("night_column"), {-16.0F, 0.0F, 33.0F}),
      run_to(3.0F, QStringLiteral("night_flank"), {-12.0F, 0.0F, 30.0F}),
      run_to(2.5F, QStringLiteral("night_commander"), {-19.0F, 0.0F, 34.0F}),
      at(16.0F,
         Command::AttackMove,
         QStringLiteral("night_column"),
         QStringLiteral("beaten_enemy")),
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("night_column")),
      [] {
        ArenaExpectation result;
        result.kind = Expect::UndeadZoneAwakened;
        result.zone_id = QStringLiteral("barrow_garrison");
        result.threshold = 8.0F;
        return result;
      }(),
  };
  return s;
}

auto trailer_flame_card() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_flame_card_id),
      QStringLiteral("Trailer: Flame Card"),
      QStringLiteral("The act-card stage. The frame itself is replaced by the "
                     "arena's procedural fire shader, so this scenario exists "
                     "only to give the promo runner a cheap pass to hang the "
                     "card shots on: an empty field, no props, no combat."),
      26.0F,
      {14.0F, 10.0F, 0.0F});
  s.arena_floor_half_extent = 12.0F;
  s.suppress_terrain_scatter = true;
  s.suppress_terrain_features = true;
  s.environment.start_time = 23.4F;

  auto marker = group(QStringLiteral("card_marker"),
                      Troop::Civilian,
                      1,
                      1,
                      {0.0F, 0.0F, 0.0F},
                      1,
                      {0.0F, 0.0F, 0.0F});
  s.groups = {marker};
  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("card_marker")),
  };
  return s;
}

} // namespace

auto build_trailer_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(trailer_dawn());
  result.push_back(trailer_muster());
  result.push_back(trailer_clash());
  result.push_back(trailer_pov());
  result.push_back(trailer_barrow_night());
  result.push_back(trailer_flame_card());
  return result;
}

} // namespace Arena::Scenarios
