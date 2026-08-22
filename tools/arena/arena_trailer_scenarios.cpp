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
constexpr float k_river_width = 6.0F;
constexpr float k_river_bend_z = 2.0F;
constexpr float k_bridge_x = -2.0F;
constexpr float k_bridge_reach =
    Game::Map::river_bank_standing_half_width(k_river_width) +
    Game::Map::bridge_bank_landing(Game::Map::k_min_bridge_width, k_river_width);
constexpr float k_fort_cx = 30.0F;
constexpr float k_fort_cz = -16.0F;
constexpr float k_fort_half = 10.0F;
constexpr float k_valley_street_z = -14.0F;
constexpr float k_roman_town_x = -26.0F;
constexpr float k_roman_hamlet_x = 10.0F;
constexpr float k_punic_town_x = 26.0F;
constexpr float k_punic_town_z = 32.0F;
constexpr float k_punic_camp_x = -8.0F;
constexpr float k_punic_camp_z = 47.5F;
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
      {-45.0F, 0.0F, k_river_z}, {4.0F, 0.0F, k_river_z}, k_river_width});
  scenario.rivers.push_back(Game::Map::RiverSegment{
      {4.0F, 0.0F, k_river_z}, {18.0F, 0.0F, k_river_bend_z}, k_river_width});
  scenario.rivers.push_back(Game::Map::RiverSegment{
      {18.0F, 0.0F, k_river_bend_z}, {45.0F, 0.0F, k_river_bend_z}, k_river_width});
  scenario.bridges.push_back(
      Game::Map::Bridge{{k_bridge_x, 0.0F, k_river_z - k_bridge_reach},
                        {k_bridge_x, 0.0F, k_river_z + k_bridge_reach},
                        4.5F,
                        0.45F});

  scenario.roads.push_back(street({k_bridge_x, 0.0F, k_valley_street_z},
                                  {k_bridge_x, 0.0F, k_river_z - k_bridge_reach},
                                  2.8F));
  scenario.roads.push_back(street(
      {k_bridge_x, 0.0F, k_river_z + k_bridge_reach}, {k_bridge_x, 0.0F, 38.0F}, 2.8F));
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

void add_resource_gang(ArenaScenarioDefinition& scenario,
                       const QString& prefix,
                       QVector3D trees,
                       QVector3D rocks,
                       QVector3D seam,
                       int owner,
                       Nation nation) {

  scenario.resource_patches.push_back(
      patch("olive_tree", 5, trees, {0.0F, 0.0F, 3.8F}, 1.15F));
  scenario.resource_patches.push_back(
      patch("boulder", 5, rocks, {3.0F, 0.0F, 1.4F}, 1.1F));
  scenario.resource_patches.push_back(
      patch("iron_ore", 4, seam, {2.6F, 0.0F, 0.0F}, 1.0F));

  struct Gang {
    const char* suffix;
    QVector3D at;
    const char* kind;
    float start;
  };
  for (auto const& gang :
       {Gang{"_fellers", trees + QVector3D(3.4F, 0.0F, 2.0F), "tree", 1.2F},
        Gang{"_breakers", rocks + QVector3D(-3.2F, 0.0F, 2.4F), "boulder", 1.8F},
        Gang{"_miners", seam + QVector3D(0.0F, 0.0F, 3.0F), "iron_ore", 2.4F}}) {
    const QString name = prefix + QString::fromLatin1(gang.suffix);
    auto crew = group(name, Troop::Builder, owner, 3, gang.at, 1, {2.8F, 0.0F, 0.0F});
    crew.nation_id = nation;
    scenario.groups.push_back(crew);
    scenario.steps.push_back(
        harvest_at(gang.start, name, QString::fromLatin1(gang.kind)));

    scenario.steps.push_back(
        harvest_at(gang.start + 22.0F, name, QString::fromLatin1(gang.kind)));
  }
}

auto acrobat(QString name,
             QVector3D origin,
             float facing,
             QStringList routine,
             float start_delay) -> ArenaScenarioGroup {
  auto result =
      group(std::move(name), Troop::Civilian, 1, 1, origin, 1, {0.0F, 0.0F, 0.0F});
  result.facing_degrees = facing;
  result.showcase_routine = std::move(routine);
  result.showcase_start_delay = start_delay;
  result.showcase_loop = true;
  return result;
}

void add_tumblers(ArenaScenarioDefinition& scenario,
                  const QString& prefix,
                  QVector3D centre,
                  float facing) {

  struct Turn {
    const char* suffix;
    QVector3D offset;
    QStringList routine;
    float delay;
  };
  const std::vector<Turn> turns{
      {"_a",
       {-3.2F, 0.0F, 0.6F},
       {QStringLiteral("handstand:3.4:0.7"),
        QStringLiteral("jump:1.6:0.4"),
        QStringLiteral("front_flip:1.7:0.6")},
       0.0F},
      {"_b",
       {-0.4F, 0.0F, -1.8F},
       {QStringLiteral("front_flip:1.7:0.5"),
        QStringLiteral("side_aerial:1.9:0.6"),
        QStringLiteral("jump:1.6:0.5")},
       0.9F},
      {"_c",
       {2.6F, 0.0F, 1.1F},
       {QStringLiteral("side_aerial:1.9:0.5"), QStringLiteral("handstand:3.4:1.0")},
       1.8F},
      {"_d",
       {5.4F, 0.0F, -1.2F},
       {QStringLiteral("jump:1.6:0.4"),
        QStringLiteral("front_flip:1.7:0.5"),
        QStringLiteral("side_aerial:1.9:0.8")},
       2.6F},
  };

  for (auto const& turn : turns) {
    scenario.groups.push_back(acrobat(prefix + QString::fromLatin1(turn.suffix),
                                      centre + turn.offset,
                                      facing,
                                      turn.routine,
                                      turn.delay));
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
  s.wildlife.sheep.spawn_areas = {{0.0F, 26.0F, 4.0F}};
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
  s.wildlife.wolves.waves = {{24.0F, 4, {-16.0F, 26.0F, 2.0F}, "dawn_pack"}};

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
  auto masons = group(QStringLiteral("valley_masons"),
                      Troop::Builder,
                      1,
                      4,
                      {k_roman_town_x + 10.0F, 0.0F, k_valley_street_z - 7.0F},
                      1,
                      {2.8F, 0.0F, 0.0F});
  auto carpenters = group(QStringLiteral("valley_carpenters"),
                          Troop::Builder,
                          1,
                          3,
                          {k_roman_hamlet_x - 6.0F, 0.0F, k_valley_street_z + 6.0F},
                          1,
                          {2.8F, 0.0F, 0.0F});
  auto lane_folk = group(QStringLiteral("valley_lane_folk"),
                         Troop::Civilian,
                         1,
                         6,
                         {k_roman_town_x + 16.0F, 0.0F, k_valley_street_z + 2.0F},
                         1,
                         {3.0F, 0.0F, 0.0F});
  auto market_folk = group(QStringLiteral("valley_market_folk"),
                           Troop::Civilian,
                           1,
                           5,
                           {k_roman_hamlet_x + 2.0F, 0.0F, k_valley_street_z - 4.0F},
                           1,
                           {3.0F, 0.0F, 0.0F});
  s.groups.push_back(woodcutters);
  s.groups.push_back(quarriers);
  s.groups.push_back(watch);
  add_tumblers(s, QStringLiteral("valley_tumblers"), {-8.0F, 0.0F, -10.0F}, 200.0F);
  add_resource_gang(s,
                    QStringLiteral("valley_gang"),
                    {-20.0F, 0.0F, -2.0F},
                    {-4.0F, 0.0F, 2.0F},
                    {-17.0F, 0.0F, 8.0F},
                    1,
                    Nation::RomanRepublic);

  s.groups.push_back(masons);
  s.groups.push_back(carpenters);
  s.groups.push_back(lane_folk);
  s.groups.push_back(market_folk);

  s.steps = {
      harvest_at(1.0F, QStringLiteral("valley_woodcutters"), QStringLiteral("tree")),
      harvest_at(1.6F, QStringLiteral("valley_quarriers"), QStringLiteral("boulder")),
      move_to(2.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x + 8.0F, 0.0F, k_valley_street_z}),
      move_to(16.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x - 6.0F, 0.0F, k_valley_street_z - 3.0F}),
      move_to(34.0F, QStringLiteral("valley_watch"), {-2.0F, 0.0F, 24.0F}),
      move_to(38.0F,
              QStringLiteral("roman_town_folk"),
              {k_roman_town_x + 2.0F, 0.0F, k_valley_street_z + 4.0F}),
      harvest_at(2.4F, QStringLiteral("valley_masons"), QStringLiteral("boulder")),
      harvest_at(3.0F, QStringLiteral("valley_carpenters"), QStringLiteral("tree")),
      move_to(2.6F,
              QStringLiteral("valley_lane_folk"),
              {k_roman_hamlet_x - 8.0F, 0.0F, k_valley_street_z + 2.0F}),
      move_to(4.0F,
              QStringLiteral("valley_market_folk"),
              {k_roman_hamlet_x - 4.0F, 0.0F, k_valley_street_z - 2.0F}),
      move_to(26.0F,
              QStringLiteral("valley_lane_folk"),
              {k_roman_town_x + 12.0F, 0.0F, k_valley_street_z + 3.0F}),
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("roman_town_market")),
      expectation(Expect::GroupExists, QStringLiteral("valley_woodcutters")),
      expectation(Expect::GroupExists, QStringLiteral("valley_watch")),
      expectation(Expect::GroupIsRendered, QStringLiteral("valley_tumblers_a")),
      expectation(Expect::GroupIsRendered, QStringLiteral("valley_gang_fellers")),
      expectation(Expect::GroupIsRendered, QStringLiteral("valley_gang_miners")),
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
  s.wildlife.sheep.spawn_areas = {{0.0F, 26.0F, 4.0F}};

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

auto targeted(float time,
              Command command,
              QString source,
              QString target,
              int value = 0) -> ArenaScenarioStep {
  auto result = at(time, command, std::move(source), std::move(target));
  result.value = value;
  return result;
}

auto build_site(ArenaScenarioDefinition& scenario,
                const QString& prefix,
                Game::Units::SpawnType type,
                Nation nation,
                int owner,
                int count,
                QVector3D origin,
                QVector3D spacing,
                float facing,
                int crew_size,
                QVector3D crew_origin,
                float start_time,
                int damaged_health) {
  const QString structure_name = prefix + QStringLiteral("_site");
  const QString crew_name = prefix + QStringLiteral("_crew");
  scenario.groups.push_back(
      building(structure_name, type, nation, owner, count, origin, spacing, facing));
  auto crew = group(crew_name, Troop::Builder, owner, crew_size, crew_origin, 1);
  crew.nation_id = nation;
  scenario.groups.push_back(crew);
  scenario.steps.push_back(
      targeted(0.4F, Command::SetHealth, structure_name, {}, damaged_health));
  scenario.steps.push_back(
      targeted(start_time, Command::RepairStructure, crew_name, structure_name));
}

auto trailer_works() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_works_id),
      QStringLiteral("Trailer I: The Works"),
      QStringLiteral("A settlement in the middle of being raised on dark farmland "
                     "soil: five crews at work at once on a temple, a house row, a "
                     "wall run, a barracks and a market, a siege yard beside them, "
                     "and the road past the site carrying a supply caravan through "
                     "the burnt-out shells of the last settlement."),
      52.0F,
      {38.0F, 40.0F, 26.0F});
  s.ground_type = QStringLiteral("soil_rocky");
  s.terrain_seed_override = 5041;
  s.collect_animation_diagnostics = true;
  s.arena_floor_half_extent = 46.0F;
  s.environment.start_time = 9.4F;
  s.environment.exposure_override = 1.45F;

  s.roads.push_back(street({-42.0F, 0.0F, 0.0F}, {42.0F, 0.0F, 0.0F}, 3.2F));
  s.roads.push_back(street({4.0F, 0.0F, 0.0F}, {4.0F, 0.0F, -22.0F}, 2.6F));
  s.roads.push_back(street({16.0F, 0.0F, 0.0F}, {16.0F, 0.0F, 20.0F}, 2.6F));

  build_site(s,
             QStringLiteral("works_temple"),
             Game::Units::SpawnType::Temple,
             Nation::RomanRepublic,
             1,
             1,
             {-24.0F, 0.0F, -14.0F},
             {},
             180.0F,
             4,
             {-19.0F, 0.0F, -10.0F},
             1.2F,
             260);
  build_site(s,
             QStringLiteral("works_homes"),
             Game::Units::SpawnType::Home,
             Nation::RomanRepublic,
             1,
             3,
             {-6.0F, 0.0F, -15.0F},
             {6.4F, 0.0F, 0.0F},
             180.0F,
             4,
             {-3.0F, 0.0F, -10.0F},
             1.6F,
             180);
  build_site(s,
             QStringLiteral("works_wall"),
             Game::Units::SpawnType::WallSegment,
             Nation::RomanRepublic,
             1,
             9,
             {24.0F, 0.0F, -16.0F},
             {2.0F, 0.0F, 0.0F},
             0.0F,
             5,
             {24.0F, 0.0F, -12.0F},
             2.0F,
             150);
  build_site(s,
             QStringLiteral("works_barracks"),
             Game::Units::SpawnType::Barracks,
             Nation::RomanRepublic,
             1,
             1,
             {26.0F, 0.0F, 12.0F},
             {},
             270.0F,
             4,
             {21.0F, 0.0F, 12.0F},
             2.4F,
             300);
  build_site(s,
             QStringLiteral("works_market"),
             Game::Units::SpawnType::Marketplace,
             Nation::RomanRepublic,
             1,
             1,
             {-22.0F, 0.0F, 11.0F},
             {},
             0.0F,
             3,
             {-17.0F, 0.0F, 11.0F},
             2.8F,
             240);

  build_site(s,
             QStringLiteral("works_tower"),
             Game::Units::SpawnType::DefenseTower,
             Nation::RomanRepublic,
             1,
             1,
             {-34.0F, 0.0F, -6.0F},
             {},
             90.0F,
             3,
             {-30.0F, 0.0F, -6.0F},
             3.2F,
             220);
  build_site(s,
             QStringLiteral("works_east_homes"),
             Game::Units::SpawnType::Home,
             Nation::RomanRepublic,
             1,
             2,
             {14.0F, 0.0F, -20.0F},
             {6.4F, 0.0F, 0.0F},
             180.0F,
             4,
             {14.0F, 0.0F, -15.0F},
             1.9F,
             160);
  build_site(s,
             QStringLiteral("works_south_wall"),
             Game::Units::SpawnType::WallSegment,
             Nation::RomanRepublic,
             1,
             7,
             {-16.0F, 0.0F, 20.0F},
             {2.0F, 0.0F, 0.0F},
             0.0F,
             4,
             {-16.0F, 0.0F, 16.0F},
             2.2F,
             140);

  auto siege_yard = group(QStringLiteral("works_siege_yard"),
                          Troop::Catapult,
                          1,
                          2,
                          {2.0F, 0.0F, 14.0F},
                          1,
                          {7.0F, 0.0F, 0.0F});
  auto siege_bolts = group(QStringLiteral("works_siege_bolts"),
                           Troop::Ballista,
                           1,
                           2,
                           {2.0F, 0.0F, 20.0F},
                           1,
                           {6.0F, 0.0F, 0.0F});
  auto siege_crew = group(QStringLiteral("works_siege_crew"),
                          Troop::Builder,
                          1,
                          5,
                          {6.0F, 0.0F, 17.0F},
                          1,
                          {2.6F, 0.0F, 0.0F});
  s.groups.push_back(siege_yard);
  s.groups.push_back(siege_bolts);
  s.groups.push_back(siege_crew);

  add_tumblers(s, QStringLiteral("works_tumblers"), {-16.0F, 0.0F, 7.0F}, 215.0F);
  add_resource_gang(s,
                    QStringLiteral("works_gang"),
                    {-28.0F, 0.0F, 12.0F},
                    {-10.0F, 0.0F, 13.0F},
                    {-23.0F, 0.0F, 2.0F},
                    1,
                    Nation::RomanRepublic);

  auto carriers = group(QStringLiteral("works_carriers"),
                        Troop::Civilian,
                        1,
                        6,
                        {-38.0F, 0.0F, 0.0F},
                        1,
                        {3.4F, 0.0F, 0.0F});
  auto road_folk = group(QStringLiteral("works_road_folk"),
                         Troop::Civilian,
                         1,
                         5,
                         {12.0F, 0.0F, -3.0F},
                         1,
                         {3.6F, 0.0F, 0.0F});
  s.groups.push_back(carriers);
  s.groups.push_back(road_folk);

  s.resource_patches = {
      patch("supply_cart", 4, {-30.0F, 0.0F, 5.0F}, {4.4F, 0.0F, 0.0F}, 1.0F),
      patch("supply_cart", 3, {8.0F, 0.0F, 5.2F}, {4.2F, 0.0F, 0.0F}, 1.0F),
      patch("supply_cart", 3, {-14.0F, 0.0F, -5.0F}, {4.2F, 0.0F, 0.0F}, 1.0F),
      patch("weapon_rack", 3, {8.0F, 0.0F, 11.0F}, {3.2F, 0.0F, 0.0F}, 1.0F),
      patch("weapon_rack", 2, {22.0F, 0.0F, 8.0F}, {3.2F, 0.0F, 0.0F}, 1.0F),
      patch("abandoned_home", 3, {-34.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 7.0F}, 1.0F),
      patch("abandoned_home", 2, {34.0F, 0.0F, -8.0F}, {0.0F, 0.0F, 7.5F}, 1.0F),
      patch("abandoned_home", 2, {20.0F, 0.0F, 30.0F}, {8.0F, 0.0F, 1.5F}, 1.0F),
      patch("ruins", 3, {-32.0F, 0.0F, 16.0F}, {0.0F, 0.0F, 6.0F}, 1.1F),
      patch("ruins", 2, {6.0F, 0.0F, -28.0F}, {8.0F, 0.0F, 2.0F}, 1.15F),
      patch("ruins", 2, {30.0F, 0.0F, 2.0F}, {6.5F, 0.0F, 3.0F}, 1.05F),
      patch("dead_tree", 4, {-24.0F, 0.0F, 26.0F}, {5.0F, 0.0F, 2.4F}, 1.1F),
      patch("tent", 5, {12.0F, 0.0F, 20.0F}, {3.8F, 0.0F, 0.0F}, 0.85F),
      patch("fire_camp", 2, {14.0F, 0.0F, 25.0F}, {9.0F, 0.0F, 0.0F}, 0.9F),
      patch("statue", 2, {1.5F, 0.0F, -20.0F}, {5.0F, 0.0F, 0.0F}, 1.1F),
      patch("boulder", 6, {-30.0F, 0.0F, -28.0F}, {3.0F, 0.0F, 1.4F}, 1.1F),
      patch("iron_ore", 4, {-36.0F, 0.0F, 26.0F}, {2.6F, 0.0F, 0.0F}, 1.0F),
      patch("olive_tree", 7, {-40.0F, 0.0F, -24.0F}, {0.0F, 0.0F, 4.2F}, 1.15F),
      patch("pine_tree", 6, {40.0F, 0.0F, 22.0F}, {0.0F, 0.0F, 4.6F}, 1.2F),
      patch("plant", 8, {-14.0F, 0.0F, 24.0F}, {3.0F, 0.0F, 0.0F}, 0.9F),
  };

  s.steps.push_back(
      move_to(1.0F, QStringLiteral("works_carriers"), {34.0F, 0.0F, 0.0F}));
  s.steps.push_back(
      move_to(2.0F, QStringLiteral("works_road_folk"), {-26.0F, 0.0F, -3.0F}));
  s.steps.push_back(targeted(6.0F,
                             Command::DeliverToStructure,
                             QStringLiteral("works_road_folk"),
                             QStringLiteral("works_barracks_site")));
  s.steps.push_back(
      move_to(30.0F, QStringLiteral("works_road_folk"), {18.0F, 0.0F, -4.0F}));

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("works_temple_site")),
      expectation(Expect::GroupExists, QStringLiteral("works_siege_yard")),
      expectation(Expect::GroupIsRendered, QStringLiteral("works_tumblers_a")),
      expectation(Expect::GroupIsRendered, QStringLiteral("works_gang_breakers")),
      expectation(Expect::GroupIsRendered, QStringLiteral("works_gang_miners")),
      expectation(Expect::GroupIsRendered, QStringLiteral("works_wall_crew")),
      expectation(Expect::GroupIsRendered, QStringLiteral("works_homes_crew")),
      expectation(Expect::GroupExists, QStringLiteral("works_east_homes_site")),
      expectation(Expect::GroupExists, QStringLiteral("works_south_wall_site")),
      expectation(Expect::GroupExists, QStringLiteral("works_tower_site")),
      expectation(Expect::MovementAnimationObserved, QStringLiteral("works_carriers")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("works_carriers")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("works_road_folk")),
  };
  return s;
}

auto trailer_sanctuary() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_sanctuary_id),
      QStringLiteral("Trailer I: The Sanctuary"),
      QStringLiteral("A temple crowning a bare dry hill above the plain, with the "
                     "order of healers gathered in rings on the terrace and a "
                     "statue-lined processional road climbing to it out of the "
                     "olive country."),
      38.0F,
      {36.0F, 32.0F, 10.0F});
  s.ground_type = QStringLiteral("grass_dry");
  s.terrain_seed_override = 8123;
  s.collect_animation_diagnostics = true;
  s.arena_floor_half_extent = 40.0F;
  s.environment.start_time = 8.2F;
  s.environment.fog_density_override = 0.018F;
  s.environment.exposure_override = 1.5F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 6.0F);

  s.elevation_patches.push_back({{0.0F, 0.0F, -6.0F}, 22.0F, 7.5F});

  s.roads.push_back(street({0.0F, 0.0F, 34.0F}, {0.0F, 0.0F, 2.0F}, 3.4F));

  s.groups.push_back(building(QStringLiteral("sanctuary_temple"),
                              Game::Units::SpawnType::Temple,
                              Nation::RomanRepublic,
                              1,
                              1,
                              {0.0F, 0.0F, -8.0F},
                              {},
                              180.0F));

  for (int ring = 0; ring < 3; ++ring) {
    const float radius = 7.0F + (static_cast<float>(ring) * 4.0F);
    const float z = -1.0F + (static_cast<float>(ring) * 3.4F);
    auto order = group(QStringLiteral("sanctuary_order_%1").arg(ring),
                       Troop::Healer,
                       1,
                       8,
                       {-radius, 0.0F, z},
                       1,
                       {radius * 2.0F / 7.0F, 0.0F, 0.0F});
    order.facing_degrees = 180.0F;
    s.groups.push_back(order);
  }

  auto pilgrims = group(QStringLiteral("sanctuary_pilgrims"),
                        Troop::Civilian,
                        1,
                        7,
                        {-2.0F, 0.0F, 30.0F},
                        1,
                        {1.4F, 0.0F, 2.6F});
  auto attendants = group(QStringLiteral("sanctuary_attendants"),
                          Troop::Civilian,
                          1,
                          5,
                          {2.0F, 0.0F, 20.0F},
                          1,
                          {1.4F, 0.0F, 2.8F});
  s.groups.push_back(pilgrims);
  s.groups.push_back(attendants);

  s.resource_patches = {
      patch("statue", 6, {-4.6F, 0.0F, 6.0F}, {0.0F, 0.0F, 4.6F}, 1.15F),
      patch("statue", 6, {4.6F, 0.0F, 6.0F}, {0.0F, 0.0F, 4.6F}, 1.15F),
      patch("fire_camp", 2, {-6.0F, 0.0F, -2.0F}, {12.0F, 0.0F, 0.0F}, 0.9F),
      patch("olive_tree", 8, {-26.0F, 0.0F, 14.0F}, {0.0F, 0.0F, 4.4F}, 1.15F),
      patch("olive_tree", 8, {26.0F, 0.0F, 14.0F}, {0.0F, 0.0F, 4.4F}, 1.15F),
      patch("boulder", 5, {-18.0F, 0.0F, -20.0F}, {3.4F, 0.0F, 1.6F}, 1.15F),
      patch("plant", 8, {-12.0F, 0.0F, 26.0F}, {3.2F, 0.0F, 0.0F}, 0.9F),
  };

  s.steps = {
      move_to(1.0F, QStringLiteral("sanctuary_pilgrims"), {-2.0F, 0.0F, 8.0F}),
      move_to(2.5F, QStringLiteral("sanctuary_attendants"), {2.0F, 0.0F, 4.0F}),
      move_to(24.0F, QStringLiteral("sanctuary_pilgrims"), {-6.0F, 0.0F, 2.0F}),
  };

  s.expectations = {
      expectation(Expect::GroupExists, QStringLiteral("sanctuary_temple")),
      expectation(Expect::GroupIsRendered, QStringLiteral("sanctuary_order_0")),
      expectation(Expect::GroupIsRendered, QStringLiteral("sanctuary_order_2")),
      expectation(Expect::MovementAnimationObserved,
                  QStringLiteral("sanctuary_pilgrims")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("sanctuary_pilgrims")),
  };
  return s;
}

auto trailer_gate_march() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_gate_march_id),
      QStringLiteral("Trailer II: Through The Gate"),
      QStringLiteral("Every arm of the army files out of one gate on the centre "
                     "line of the wall: swordsmen, spearmen and archers on foot, "
                     "the cavalry wing behind them and the siege train last, each "
                     "column funnelled through the opening rather than the stone."),
      64.0F,
      {40.0F, 36.0F, 8.0F});
  s.ground_type = QStringLiteral("grass_dry");
  s.terrain_seed_override = 3307;
  s.arena_floor_half_extent = 44.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 10.6F;
  s.environment.exposure_override = 1.45F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  constexpr float k_wall_z = 0.0F;
  constexpr float k_gate_half = 4.0F;
  constexpr float k_wall_reach = 22.0F;

  auto wall_run = [&](const char* name, float first, float last) {
    const int count = static_cast<int>((last - first) / 2.0F) + 1;
    const float middle = (first + last) * 0.5F;
    s.groups.push_back(building(QString::fromLatin1(name),
                                Game::Units::SpawnType::WallSegment,
                                Nation::RomanRepublic,
                                1,
                                count,
                                {middle, 0.0F, k_wall_z},
                                {2.0F, 0.0F, 0.0F},
                                0.0F));
  };
  wall_run("gate_wall_west", -k_wall_reach, -k_gate_half);
  wall_run("gate_wall_east", k_gate_half, k_wall_reach);
  s.groups.push_back(building(QStringLiteral("gate_wall_gate"),
                              Game::Units::SpawnType::WallGate,
                              Nation::RomanRepublic,
                              1,
                              1,
                              {0.0F, 0.0F, k_wall_z},
                              {},
                              0.0F));
  for (float x : {-k_wall_reach, k_wall_reach}) {
    s.groups.push_back(building(QStringLiteral("gate_tower_%1").arg(x < 0 ? 0 : 1),
                                Game::Units::SpawnType::DefenseTower,
                                Nation::RomanRepublic,
                                1,
                                1,
                                {x, 0.0F, k_wall_z}));
  }

  s.roads.push_back(street({0.0F, 0.0F, 26.0F}, {0.0F, 0.0F, -30.0F}, 3.2F));

  auto swords = group(
      QStringLiteral("gate_swords"), Troop::Swordsman, 1, 3, {0.0F, 0.0F, 22.0F}, 6);
  auto spears = group(
      QStringLiteral("gate_spears"), Troop::Spearman, 1, 2, {0.0F, 0.0F, 27.0F}, 6);
  auto archers = group(
      QStringLiteral("gate_archers"), Troop::Archer, 1, 2, {0.0F, 0.0F, 32.0F}, 6);
  auto horse = group(
      QStringLiteral("gate_horse"), Troop::MountedKnight, 1, 2, {0.0F, 0.0F, 37.0F}, 4);
  auto siege = group(
      QStringLiteral("gate_siege"), Troop::Catapult, 1, 2, {0.0F, 0.0F, 42.0F}, 1);
  for (auto* column : {&swords, &spears, &archers, &horse, &siege}) {
    column->facing_degrees = 180.0F;
    column->spacing = {2.2F, 0.0F, 0.0F};
  }
  s.groups.push_back(swords);
  s.groups.push_back(spears);
  s.groups.push_back(archers);
  s.groups.push_back(horse);
  s.groups.push_back(siege);

  s.resource_patches = {
      patch("statue", 2, {-5.0F, 0.0F, 8.0F}, {10.0F, 0.0F, 0.0F}, 1.1F),
      patch("weapon_rack", 2, {-8.0F, 0.0F, 5.0F}, {16.0F, 0.0F, 0.0F}, 1.0F),
      patch("tent", 4, {-16.0F, 0.0F, 12.0F}, {3.8F, 0.0F, 0.0F}, 0.85F),
      patch("supply_cart", 3, {14.0F, 0.0F, 12.0F}, {4.2F, 0.0F, 0.0F}, 1.0F),
      patch("fire_camp", 1, {-16.0F, 0.0F, 17.0F}, {}, 0.9F),
      patch("olive_tree", 6, {-34.0F, 0.0F, -14.0F}, {0.0F, 0.0F, 4.2F}, 1.15F),
      patch("boulder", 5, {32.0F, 0.0F, -18.0F}, {3.2F, 0.0F, 1.5F}, 1.1F),
  };

  auto file_through = [&](float time, const char* name, float exit_z) {
    auto step = march_step(time,
                           {QString::fromLatin1(name)},
                           {0.0F, 0.0F, exit_z},
                           180.0F,
                           k_gate_half * 1.4F);
    step.formation.options.movement_policy =
        Game::Formation::MovementPolicy::ReformAtDestination;
    return step;
  };
  s.steps = {
      file_through(1.0F, "gate_swords", -14.0F),
      file_through(9.0F, "gate_spears", -20.0F),
      file_through(17.0F, "gate_archers", -26.0F),
      file_through(25.0F, "gate_horse", -32.0F),
      file_through(33.0F, "gate_siege", -38.0F),
      form_step(44.0F,
                {QStringLiteral("gate_swords"),
                 QStringLiteral("gate_spears"),
                 QStringLiteral("gate_archers"),
                 QStringLiteral("gate_horse")},
                Intent::Line,
                {0.0F, 0.0F, -24.0F},
                180.0F,
                40.0F),
  };

  s.expectations = {
      expectation(Expect::GateOpenedObserved, QStringLiteral("gate_wall_gate")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("gate_swords")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("gate_spears")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("gate_archers")),
      expectation(Expect::UnitsClearOfBuildings, QStringLiteral("gate_horse")),
      expectation(Expect::MovementAnimationObserved, QStringLiteral("gate_swords")),
      no_visibility_churn(QStringLiteral("gate_swords"), 2.0F, 36.0F),
  };
  return s;
}

auto trailer_highland() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_highland_id),
      QStringLiteral("Trailer II: The High Pass"),
      QStringLiteral("The same army a hundred miles on: a column working across "
                     "an alpine saddle in falling snow, the rock showing through "
                     "the drifts and the pines thinning out above the road."),
      40.0F,
      {38.0F, 30.0F, 34.0F});
  s.ground_type = QStringLiteral("alpine_mix");
  s.terrain_seed_override = 7712;
  s.collect_animation_diagnostics = true;
  s.terrain_height_scale_override = 8.0F;
  s.arena_floor_half_extent = 48.0F;
  s.suppress_boundary_mountains = true;
  s.elevation_patches = {
      {{-26.0F, 0.0F, 36.0F}, 20.0F, 17.0F},
      {{6.0F, 0.0F, 38.0F}, 18.0F, 14.0F},
      {{36.0F, 0.0F, 26.0F}, 16.0F, 12.0F},
      {{-14.0F, 0.0F, -30.0F}, 22.0F, 15.0F},
      {{26.0F, 0.0F, -36.0F}, 18.0F, 13.0F},
      {{-40.0F, 0.0F, -20.0F}, 16.0F, 11.0F},
      {{-44.0F, 0.0F, 38.0F}, 18.0F, 16.0F},
      {{44.0F, 0.0F, 40.0F}, 18.0F, 15.0F},
      {{-44.0F, 0.0F, -40.0F}, 18.0F, 14.0F},
      {{46.0F, 0.0F, -14.0F}, 16.0F, 12.0F},
  };
  s.environment.start_time = 9.0F;
  s.environment.lighting_profile = QStringLiteral("trebia_winter_camp");
  s.environment.fog_density_override = 0.05F;
  s.weather.snow = 0.62F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Snow;
  s.precipitation.intensity = 0.62F;
  s.precipitation.wind_strength = 0.4F;
  s.precipitation.wind_direction_deg = 70.0F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  auto vanguard = group(QStringLiteral("pass_vanguard"),
                        Troop::MountedKnight,
                        1,
                        2,
                        {-34.0F, 0.0F, 12.0F},
                        4);
  auto column = group(
      QStringLiteral("pass_column"), Troop::Swordsman, 1, 4, {-40.0F, 0.0F, 14.0F}, 6);
  auto spears = group(
      QStringLiteral("pass_spears"), Troop::Spearman, 1, 3, {-44.0F, 0.0F, 15.0F}, 6);
  auto train = group(
      QStringLiteral("pass_train"), Troop::Ballista, 1, 2, {-46.0F, 0.0F, 16.0F}, 1);
  for (auto* file : {&vanguard, &column, &spears, &train}) {
    file->facing_degrees = 290.0F;
    file->spacing = {2.2F, 0.0F, 0.0F};
  }
  s.groups.push_back(vanguard);
  s.groups.push_back(column);
  s.groups.push_back(spears);
  s.groups.push_back(train);

  s.resource_patches = {
      patch("pine_tree", 9, {-26.0F, 0.0F, 26.0F}, {4.6F, 0.0F, 1.8F}, 1.2F),
      patch("pine_tree", 7, {18.0F, 0.0F, 22.0F}, {4.6F, 0.0F, 2.2F}, 1.15F),
      patch("boulder", 10, {-18.0F, 0.0F, -8.0F}, {4.0F, 0.0F, 2.2F}, 1.25F),
      patch("boulder", 8, {24.0F, 0.0F, 6.0F}, {3.8F, 0.0F, 2.0F}, 1.2F),
      patch("tent", 3, {30.0F, 0.0F, -20.0F}, {4.0F, 0.0F, 0.0F}, 0.85F),
      patch("fire_camp", 1, {33.0F, 0.0F, -16.0F}, {}, 0.95F),
  };

  const QVector3D pass_exit(36.0F, 0.0F, -10.0F);
  s.steps = {
      march_step(1.0F, {QStringLiteral("pass_vanguard")}, pass_exit, 290.0F, 6.0F),
      march_step(2.0F, {QStringLiteral("pass_column")}, pass_exit, 290.0F, 6.0F),
      march_step(3.0F, {QStringLiteral("pass_spears")}, pass_exit, 290.0F, 6.0F),
      march_step(4.0F, {QStringLiteral("pass_train")}, pass_exit, 290.0F, 4.0F),
  };

  s.expectations = {
      expectation(Expect::GroupIsRendered, QStringLiteral("pass_column")),
      expectation(Expect::MovementAnimationObserved, QStringLiteral("pass_column")),
      expectation(Expect::MovementIsContinuous, QStringLiteral("pass_column")),
  };
  return s;
}

auto trailer_forest_ambush() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_forest_ambush_id),
      QStringLiteral("Trailer II: The Wood"),
      QStringLiteral("Rain in the deep wood. A Roman column takes the forest road "
                     "between two stands of pine and the Carthaginian screen that "
                     "has been walking it comes in from both sides at once."),
      44.0F,
      {40.0F, 28.0F, 16.0F});
  s.ground_type = QStringLiteral("forest_mud");
  s.terrain_seed_override = 4421;
  s.arena_floor_half_extent = 42.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 15.4F;
  s.environment.fog_density_override = 0.06F;
  s.environment.exposure_override = 1.35F;
  s.weather.rain = 0.6F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Rain;
  s.precipitation.intensity = 0.6F;
  s.precipitation.wind_strength = 0.25F;
  s.precipitation.wind_direction_deg = 250.0F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};

  s.roads.push_back(street({0.0F, 0.0F, 34.0F}, {0.0F, 0.0F, -34.0F}, 3.0F, "earth"));

  auto column = group(
      QStringLiteral("wood_column"), Troop::Swordsman, 1, 4, {0.0F, 0.0F, 26.0F}, 6);
  auto escort = group(
      QStringLiteral("wood_escort"), Troop::Spearman, 1, 2, {0.0F, 0.0F, 32.0F}, 6);
  column.facing_degrees = 180.0F;
  escort.facing_degrees = 180.0F;
  column.spacing = {2.2F, 0.0F, 0.0F};
  escort.spacing = {2.2F, 0.0F, 0.0F};
  s.groups.push_back(column);
  s.groups.push_back(escort);

  auto ambush_west = group(QStringLiteral("wood_ambush_west"),
                           Troop::Swordsman,
                           2,
                           3,
                           {-16.0F, 0.0F, 4.0F},
                           6,
                           {0.0F, 0.0F, 4.4F});
  auto ambush_east = group(QStringLiteral("wood_ambush_east"),
                           Troop::Spearman,
                           2,
                           3,
                           {16.0F, 0.0F, 4.0F},
                           6,
                           {0.0F, 0.0F, 4.4F});
  auto ambush_bows = group(
      QStringLiteral("wood_ambush_bows"), Troop::Archer, 2, 2, {0.0F, 0.0F, -18.0F}, 6);
  ambush_west.spawn_at_start = false;
  ambush_east.spawn_at_start = false;
  ambush_west.facing_degrees = 90.0F;
  ambush_east.facing_degrees = 270.0F;
  s.groups.push_back(ambush_west);
  s.groups.push_back(ambush_east);
  s.groups.push_back(ambush_bows);

  s.resource_patches = {
      patch("pine_tree", 12, {-12.0F, 0.0F, -28.0F}, {3.4F, 0.0F, 4.6F}, 1.25F),
      patch("pine_tree", 12, {12.0F, 0.0F, -28.0F}, {3.4F, 0.0F, 4.6F}, 1.25F),
      patch("pine_tree", 10, {-20.0F, 0.0F, 16.0F}, {3.6F, 0.0F, 4.4F}, 1.2F),
      patch("pine_tree", 10, {20.0F, 0.0F, 16.0F}, {3.6F, 0.0F, 4.4F}, 1.2F),
      patch("olive_tree", 6, {-30.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 4.2F}, 1.1F),
      patch("dead_tree", 3, {26.0F, 0.0F, -14.0F}, {4.6F, 0.0F, 2.2F}, 1.15F),
      patch("boulder", 6, {-26.0F, 0.0F, 26.0F}, {3.4F, 0.0F, 1.8F}, 1.1F),
      patch("plant", 10, {-8.0F, 0.0F, 12.0F}, {3.2F, 0.0F, 0.0F}, 0.95F),
  };

  s.steps = {
      march_step(1.0F,
                 {QStringLiteral("wood_column"), QStringLiteral("wood_escort")},
                 {0.0F, 0.0F, -6.0F},
                 180.0F,
                 8.0F),
      at(11.0F,
         Command::SpawnAmbush,
         QStringLiteral("wood_ambush_west"),
         QStringLiteral("wood_column")),
      at(11.4F,
         Command::SpawnAmbush,
         QStringLiteral("wood_ambush_east"),
         QStringLiteral("wood_column")),
      at(12.0F,
         Command::Charge,
         QStringLiteral("wood_ambush_bows"),
         QStringLiteral("wood_escort")),
      at(13.0F,
         Command::AttackMove,
         QStringLiteral("wood_column"),
         QStringLiteral("wood_ambush_west")),
      at(13.4F,
         Command::AttackMove,
         QStringLiteral("wood_escort"),
         QStringLiteral("wood_ambush_east")),
  };

  s.expectations = {
      expectation(Expect::GroupIsRendered, QStringLiteral("wood_column")),
      expectation(Expect::MovementAnimationObserved, QStringLiteral("wood_column")),
      expectation(Expect::AttackAnimationObserved, QStringLiteral("wood_ambush_west")),
      expectation(Expect::GroupHealthReduced, QStringLiteral("wood_column")),
  };
  return s;
}

auto trailer_bridge_defense() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_bridge_defense_id),
      QStringLiteral("Trailer III: The Bridge"),
      QStringLiteral("One crossing on a wide river and a Roman line braced on the "
                     "near bank of it, archers shooting over their heads while the "
                     "Carthaginian column forces the deck."),
      44.0F,
      {40.0F, 26.0F, 4.0F});
  s.ground_type = QStringLiteral("soil_fertile");
  s.terrain_seed_override = 6605;
  s.arena_floor_half_extent = 44.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 16.6F;
  s.environment.exposure_override = 1.45F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};

  s.rivers.push_back(
      Game::Map::RiverSegment{{-42.0F, 0.0F, 0.0F}, {42.0F, 0.0F, 0.0F}, 9.0F});
  s.bridges.push_back(
      Game::Map::Bridge{{0.0F, 0.0F, -7.0F}, {0.0F, 0.0F, 7.0F}, 5.0F, 0.5F});
  s.roads.push_back(street({0.0F, 0.0F, -30.0F}, {0.0F, 0.0F, -7.0F}, 3.0F));
  s.roads.push_back(street({0.0F, 0.0F, 7.0F}, {0.0F, 0.0F, 30.0F}, 3.0F));

  auto shield_line = group(QStringLiteral("bridge_shields"),
                           Troop::Spearman,
                           1,
                           4,
                           {0.0F, 0.0F, -11.0F},
                           6,
                           {5.4F, 0.0F, 0.0F});
  auto reserve = group(QStringLiteral("bridge_reserve"),
                       Troop::Swordsman,
                       1,
                       3,
                       {0.0F, 0.0F, -17.0F},
                       6,
                       {5.4F, 0.0F, 0.0F});
  auto bows = group(QStringLiteral("bridge_bows"),
                    Troop::Archer,
                    1,
                    3,
                    {0.0F, 0.0F, -23.0F},
                    6,
                    {5.4F, 0.0F, 0.0F});
  for (auto* line : {&shield_line, &reserve, &bows}) {
    line->facing_degrees = 0.0F;
  }
  s.groups.push_back(shield_line);
  s.groups.push_back(reserve);
  s.groups.push_back(bows);

  auto storm_column = group(QStringLiteral("bridge_storm"),
                            Troop::Swordsman,
                            2,
                            4,
                            {0.0F, 0.0F, 13.0F},
                            6,
                            {2.2F, 0.0F, 0.0F});
  auto storm_second = group(QStringLiteral("bridge_storm_second"),
                            Troop::Spearman,
                            2,
                            3,
                            {0.0F, 0.0F, 20.0F},
                            6,
                            {2.2F, 0.0F, 0.0F});
  auto storm_bows = group(QStringLiteral("bridge_storm_bows"),
                          Troop::Archer,
                          2,
                          2,
                          {0.0F, 0.0F, 26.0F},
                          6,
                          {5.0F, 0.0F, 0.0F});
  for (auto* line : {&storm_column, &storm_second, &storm_bows}) {
    line->facing_degrees = 180.0F;
  }
  s.groups.push_back(storm_column);
  s.groups.push_back(storm_second);
  s.groups.push_back(storm_bows);

  s.resource_patches = {
      patch("olive_tree", 7, {-26.0F, 0.0F, -20.0F}, {0.0F, 0.0F, 4.2F}, 1.15F),
      patch("pine_tree", 7, {26.0F, 0.0F, 20.0F}, {0.0F, 0.0F, 4.4F}, 1.2F),
      patch("boulder", 6, {-20.0F, 0.0F, 12.0F}, {3.4F, 0.0F, 1.8F}, 1.1F),
      patch("weapon_rack", 2, {-8.0F, 0.0F, -20.0F}, {16.0F, 0.0F, 0.0F}, 1.0F),
      patch("supply_cart", 3, {-14.0F, 0.0F, -26.0F}, {4.2F, 0.0F, 0.0F}, 1.0F),
      patch("tent", 4, {14.0F, 0.0F, -26.0F}, {3.8F, 0.0F, 0.0F}, 0.85F),
      patch("fire_camp", 1, {17.0F, 0.0F, -22.0F}, {}, 0.9F),
      patch("abandoned_home", 1, {-30.0F, 0.0F, 24.0F}, {}, 1.0F),
  };

  s.steps = {
      at(0.5F, Command::Hold, QStringLiteral("bridge_shields")),
      march_step(
          1.5F, {QStringLiteral("bridge_storm")}, {0.0F, 0.0F, -10.0F}, 180.0F, 4.5F),
      march_step(6.0F,
                 {QStringLiteral("bridge_storm_second")},
                 {0.0F, 0.0F, -8.0F},
                 180.0F,
                 4.5F),
      at(10.0F,
         Command::AttackMove,
         QStringLiteral("bridge_bows"),
         QStringLiteral("bridge_storm")),
      at(12.0F,
         Command::AttackMove,
         QStringLiteral("bridge_storm_bows"),
         QStringLiteral("bridge_shields")),
      at(24.0F,
         Command::Charge,
         QStringLiteral("bridge_reserve"),
         QStringLiteral("bridge_storm")),
  };

  s.expectations = {
      expectation(Expect::BridgeTraversalObserved, QStringLiteral("bridge_storm")),
      expectation(Expect::AttackAnimationObserved, QStringLiteral("bridge_shields")),
      expectation(Expect::ProjectileFlightObserved,
                  QStringLiteral("bridge_bows"),
                  QStringLiteral("bridge_storm")),
      expectation(Expect::GroupHealthReduced, QStringLiteral("bridge_storm")),
  };
  return s;
}

auto trailer_siege_walls() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_siege_walls_id),
      QStringLiteral("Trailer III: The Wall Comes Down"),
      QStringLiteral("Siege work against a walled Punic front: the catapult "
                     "battery ranges the rampart while the elephants go in at the "
                     "stone beside the gate and the assault waits behind them."),
      46.0F,
      {46.0F, 30.0F, 6.0F});
  s.ground_type = QStringLiteral("grass_dry");
  s.terrain_seed_override = 2914;
  s.arena_floor_half_extent = 44.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 17.4F;
  s.environment.fog_density_override = 0.03F;
  s.environment.exposure_override = 1.45F;
  s.camera_focus = QVector3D(0.0F, 0.0F, -4.0F);
  s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 2, .team_id = 2}};

  auto wall_run = [&](const char* name, float first, float last) {
    const int count = static_cast<int>((last - first) / 2.0F) + 1;
    const float middle = (first + last) * 0.5F;
    s.groups.push_back(building(QString::fromLatin1(name),
                                Game::Units::SpawnType::WallSegment,
                                Nation::Carthage,
                                2,
                                count,
                                {middle, 0.0F, -14.0F},
                                {2.0F, 0.0F, 0.0F},
                                0.0F));
  };
  wall_run("siege_wall_west", -24.0F, -6.0F);
  wall_run("siege_wall_east", 6.0F, 24.0F);
  s.groups.push_back(building(QStringLiteral("siege_gate"),
                              Game::Units::SpawnType::WallGate,
                              Nation::Carthage,
                              2,
                              1,
                              {0.0F, 0.0F, -14.0F},
                              {},
                              0.0F));
  for (float x : {-24.0F, 24.0F}) {
    s.groups.push_back(building(QStringLiteral("siege_tower_%1").arg(x < 0 ? 0 : 1),
                                Game::Units::SpawnType::DefenseTower,
                                Nation::Carthage,
                                2,
                                1,
                                {x, 0.0F, -14.0F}));
  }
  s.groups.push_back(building(QStringLiteral("siege_town_homes"),
                              Game::Units::SpawnType::Home,
                              Nation::Carthage,
                              2,
                              4,
                              {-9.0F, 0.0F, -22.0F},
                              {6.4F, 0.0F, 0.0F},
                              0.0F));

  auto battery = group(
      QStringLiteral("siege_battery"), Troop::Catapult, 1, 4, {0.0F, 0.0F, 10.0F}, 1);
  battery.spacing = {9.0F, 0.0F, 0.0F};
  battery.facing_degrees = 180.0F;
  auto tuskers = group(
      QStringLiteral("siege_tuskers"), Troop::Elephant, 1, 3, {0.0F, 0.0F, 8.0F}, 1);
  tuskers.spacing = {10.0F, 0.0F, 0.0F};
  tuskers.facing_degrees = 180.0F;
  auto assault = group(
      QStringLiteral("siege_assault"), Troop::Swordsman, 1, 4, {0.0F, 0.0F, 18.0F}, 6);
  assault.spacing = {6.0F, 0.0F, 0.0F};
  assault.facing_degrees = 180.0F;
  auto garrison = group(QStringLiteral("siege_garrison"),
                        Troop::Spearman,
                        2,
                        3,
                        {0.0F, 0.0F, -18.0F},
                        6,
                        {6.0F, 0.0F, 0.0F});
  s.groups.push_back(battery);
  s.groups.push_back(tuskers);
  s.groups.push_back(assault);
  s.groups.push_back(garrison);

  s.resource_patches = {
      patch("tent", 6, {-20.0F, 0.0F, 26.0F}, {3.8F, 0.0F, 0.0F}, 0.85F),
      patch("fire_camp", 2, {-18.0F, 0.0F, 31.0F}, {12.0F, 0.0F, 0.0F}, 0.9F),
      patch("weapon_rack", 3, {12.0F, 0.0F, 26.0F}, {3.2F, 0.0F, 0.0F}, 1.0F),
      patch("supply_cart", 4, {20.0F, 0.0F, 20.0F}, {4.2F, 0.0F, 0.0F}, 1.0F),
      patch("ruins", 2, {-32.0F, 0.0F, -20.0F}, {7.0F, 0.0F, 3.0F}, 1.1F),
      patch("olive_tree", 6, {32.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 4.2F}, 1.15F),
      patch("boulder", 6, {-34.0F, 0.0F, 8.0F}, {3.4F, 0.0F, 1.8F}, 1.1F),
  };

  s.steps = {
      at(1.0F,
         Command::AttackMove,
         QStringLiteral("siege_battery"),
         QStringLiteral("siege_wall_west")),
      at(4.0F,
         Command::AttackMove,
         QStringLiteral("siege_tuskers"),
         QStringLiteral("siege_wall_east")),
      at(16.0F,
         Command::AttackMove,
         QStringLiteral("siege_battery"),
         QStringLiteral("siege_gate")),
      at(20.0F,
         Command::AttackMove,
         QStringLiteral("siege_assault"),
         QStringLiteral("siege_garrison")),
      at(26.0F,
         Command::Attack,
         QStringLiteral("siege_garrison"),
         QStringLiteral("siege_assault")),
  };

  s.expectations = {
      expectation(Expect::ProjectileFlightObserved,
                  QStringLiteral("siege_battery"),
                  QStringLiteral("siege_wall_west")),
      expectation(Expect::ProjectileImpactObserved,
                  QStringLiteral("siege_battery"),
                  QStringLiteral("siege_wall_west")),
      expectation(Expect::StructureDamageCueObserved,
                  QStringLiteral("siege_wall_west")),
      expectation(
          Expect::GroupHealthReduced, QStringLiteral("siege_wall_west"), {}, 1.0F),
      expectation(Expect::StructureFacadeContactObserved,
                  QStringLiteral("siege_tuskers"),
                  QStringLiteral("siege_wall_east")),
  };
  return s;
}

auto trailer_night_snow() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_night_snow_id),
      QStringLiteral("Trailer IV: The Cold Ground"),
      QStringLiteral("Night on the high ground under snow. The army goes in at the "
                     "grave priests keeping the barrow and the risen come at it "
                     "from every side of the field at once."),
      50.0F,
      {46.0F, 34.0F, 20.0F});
  s.ground_type = QStringLiteral("alpine_mix");
  s.terrain_seed_override = 9931;
  s.arena_floor_half_extent = 44.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 22.4F;
  s.environment.lighting_profile = QStringLiteral("iron_sepulcher");
  s.environment.fog_density_override = 0.07F;
  s.environment.exposure_override = 2.1F;
  s.weather.snow = 0.55F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Snow;
  s.precipitation.intensity = 0.55F;
  s.precipitation.wind_strength = 0.3F;
  s.precipitation.wind_direction_deg = 20.0F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 9, .team_id = 9}};

  auto host_swords = group(QStringLiteral("night_host_swords"),
                           Troop::Swordsman,
                           1,
                           5,
                           {0.0F, 0.0F, 24.0F},
                           6,
                           {6.0F, 0.0F, 0.0F});
  auto host_spears = group(QStringLiteral("night_host_spears"),
                           Troop::Spearman,
                           1,
                           4,
                           {0.0F, 0.0F, 29.0F},
                           6,
                           {6.0F, 0.0F, 0.0F});
  auto host_bows = group(QStringLiteral("night_host_bows"),
                         Troop::Archer,
                         1,
                         3,
                         {0.0F, 0.0F, 34.0F},
                         6,
                         {6.0F, 0.0F, 0.0F});
  auto host_consul = group(QStringLiteral("night_host_consul"),
                           Troop::RomanVeteranConsul,
                           1,
                           1,
                           {0.0F, 0.0F, 20.0F},
                           1);
  s.groups.push_back(host_swords);
  s.groups.push_back(host_spears);
  s.groups.push_back(host_bows);
  s.groups.push_back(host_consul);

  auto priests = group(QStringLiteral("night_priests"),
                       Troop::GravePriest,
                       9,
                       7,
                       {0.0F, 0.0F, -6.0F},
                       1,
                       {5.4F, 0.0F, 0.0F});
  priests.nation_id = Nation::IronSepulcher;
  priests.facing_degrees = 0.0F;
  s.groups.push_back(priests);

  struct Flank {
    const char* name;
    QVector3D origin;
    float facing;
    Troop troop;
  };
  for (auto const& flank :
       {Flank{
            "night_risen_north", {0.0F, 0.0F, -26.0F}, 0.0F, Troop::SkeletonSwordsman},
        Flank{
            "night_risen_west", {-28.0F, 0.0F, 2.0F}, 90.0F, Troop::SkeletonSwordsman},
        Flank{
            "night_risen_east", {28.0F, 0.0F, 2.0F}, 270.0F, Troop::SkeletonSwordsman},
        Flank{"night_risen_bows",
              {-20.0F, 0.0F, -20.0F},
              45.0F,
              Troop::SkeletonArcher}}) {
    auto risen = group(QString::fromLatin1(flank.name),
                       flank.troop,
                       9,
                       3,
                       flank.origin,
                       6,
                       {5.0F, 0.0F, 0.0F});
    risen.nation_id = Nation::IronSepulcher;
    risen.facing_degrees = flank.facing;
    s.groups.push_back(risen);
  }

  s.resource_patches = {
      patch("magic_shrine", 1, {0.0F, 0.0F, -12.0F}, {}, 1.3F),
      patch("dead_tree", 8, {-16.0F, 0.0F, -18.0F}, {4.6F, 0.0F, 2.6F}, 1.2F),
      patch("dead_tree", 6, {18.0F, 0.0F, -16.0F}, {4.6F, 0.0F, 2.4F}, 1.2F),
      patch("ruins", 3, {-10.0F, 0.0F, -24.0F}, {9.0F, 0.0F, 2.0F}, 1.15F),
      patch("boulder", 8, {-30.0F, 0.0F, 16.0F}, {3.6F, 0.0F, 2.0F}, 1.15F),
      patch("pine_tree", 6, {32.0F, 0.0F, 22.0F}, {4.4F, 0.0F, 2.2F}, 1.15F),
      patch("fire_camp", 2, {-8.0F, 0.0F, 30.0F}, {16.0F, 0.0F, 0.0F}, 0.95F),
  };

  s.steps = {
      march_step(
          1.0F,
          {QStringLiteral("night_host_swords"), QStringLiteral("night_host_spears")},
          {0.0F, 0.0F, 6.0F},
          180.0F,
          30.0F),
      at(10.0F,
         Command::Attack,
         QStringLiteral("night_host_bows"),
         QStringLiteral("night_priests")),
      at(12.0F,
         Command::AttackMove,
         QStringLiteral("night_host_swords"),
         QStringLiteral("night_priests")),
      at(14.0F,
         Command::Charge,
         QStringLiteral("night_risen_west"),
         QStringLiteral("night_host_swords")),
      at(14.4F,
         Command::Charge,
         QStringLiteral("night_risen_east"),
         QStringLiteral("night_host_spears")),
      at(15.0F,
         Command::Charge,
         QStringLiteral("night_risen_north"),
         QStringLiteral("night_host_swords")),
      at(16.0F,
         Command::Attack,
         QStringLiteral("night_risen_bows"),
         QStringLiteral("night_host_bows")),
      at(20.0F,
         Command::AttackMove,
         QStringLiteral("night_host_spears"),
         QStringLiteral("night_risen_east")),
  };

  s.expectations = {
      expectation(Expect::GroupIsRendered, QStringLiteral("night_priests")),
      expectation(Expect::GroupIsRendered, QStringLiteral("night_risen_west")),
      expectation(Expect::AttackAnimationObserved, QStringLiteral("night_host_swords")),
      expectation(Expect::GroupHealthReduced, QStringLiteral("night_host_swords")),
  };
  return s;
}

auto trailer_last_breath() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_last_breath_id),
      QStringLiteral("Trailer IV: Last Breath"),
      QStringLiteral("The end of the night from behind the consul's shoulders: "
                     "alone in the snow with the risen closing from every quarter, "
                     "cutting until the ring reaches him and he goes down."),
      34.0F,
      {12.0F, 14.0F, 0.0F});
  s.ground_type = QStringLiteral("alpine_mix");
  s.terrain_seed_override = 9931;
  s.arena_floor_half_extent = 30.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 22.6F;
  s.environment.lighting_profile = QStringLiteral("iron_sepulcher");
  s.environment.fog_density_override = 0.09F;
  s.environment.exposure_override = 2.1F;
  s.weather.snow = 0.6F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Snow;
  s.precipitation.intensity = 0.6F;
  s.precipitation.wind_strength = 0.32F;
  s.precipitation.wind_direction_deg = 20.0F;
  s.rpg_mode = true;
  s.rpg_commander_group = QStringLiteral("rpg_commander");
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  s.owner_teams = {{.owner_id = 1, .team_id = 1}, {.owner_id = 9, .team_id = 9}};

  auto commander = group(QStringLiteral("rpg_commander"),
                         Troop::RomanVeteranConsul,
                         1,
                         1,
                         {0.0F, 0.0F, 0.0F},
                         1);
  commander.facing_degrees = 0.0F;
  commander.health_override = commander.max_health_override = 4200;
  s.groups.push_back(commander);

  struct Ring {
    const char* name;
    QVector3D origin;
    float facing;
    Troop troop;
    bool deferred;
  };
  for (auto const& ring : {Ring{"last_risen_front",
                                {0.0F, 0.0F, 7.0F},
                                180.0F,
                                Troop::SkeletonSwordsman,
                                false},
                           Ring{"last_risen_left",
                                {-8.0F, 0.0F, 1.0F},
                                90.0F,
                                Troop::SkeletonSwordsman,
                                false},
                           Ring{"last_risen_right",
                                {8.0F, 0.0F, 1.0F},
                                270.0F,
                                Troop::SkeletonSwordsman,
                                false},
                           Ring{"last_risen_rear",
                                {0.0F, 0.0F, -8.0F},
                                0.0F,
                                Troop::SkeletonSwordsman,
                                true},
                           Ring{"last_risen_bows",
                                {-11.0F, 0.0F, -9.0F},
                                45.0F,
                                Troop::SkeletonArcher,
                                true}}) {
    auto risen = group(QString::fromLatin1(ring.name),
                       ring.troop,
                       9,
                       1,
                       ring.origin,
                       5,
                       {2.4F, 0.0F, 0.0F});
    risen.nation_id = Nation::IronSepulcher;
    risen.facing_degrees = ring.facing;
    risen.spawn_at_start = !ring.deferred;
    risen.health_override = risen.max_health_override = 260;
    s.groups.push_back(risen);
  }

  s.groups.push_back(building(QStringLiteral("last_shrine"),
                              Game::Units::SpawnType::Temple,
                              Nation::RomanRepublic,
                              1,
                              1,
                              {0.0F, 0.0F, 22.0F},
                              {},
                              180.0F));

  s.resource_patches = {
      patch("dead_tree", 6, {-14.0F, 0.0F, 10.0F}, {4.4F, 0.0F, 2.4F}, 1.2F),
      patch("dead_tree", 5, {14.0F, 0.0F, -10.0F}, {4.4F, 0.0F, 2.4F}, 1.2F),
      patch("ruins", 2, {-18.0F, 0.0F, -6.0F}, {8.0F, 0.0F, 2.0F}, 1.1F),
      patch("magic_shrine", 1, {16.0F, 0.0F, 14.0F}, {}, 1.2F),
      patch("boulder", 5, {18.0F, 0.0F, 4.0F}, {3.2F, 0.0F, 1.6F}, 1.1F),
  };

  s.steps = {
      at(0.4F,
         Command::Attack,
         QStringLiteral("last_risen_front"),
         QStringLiteral("rpg_commander")),
      at(0.6F,
         Command::Attack,
         QStringLiteral("last_risen_left"),
         QStringLiteral("rpg_commander")),
      at(0.8F,
         Command::Attack,
         QStringLiteral("last_risen_right"),
         QStringLiteral("rpg_commander")),
      at(1.2F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      at(3.6F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      at(6.0F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      at(7.0F,
         Command::SpawnAmbush,
         QStringLiteral("last_risen_rear"),
         QStringLiteral("rpg_commander")),
      at(8.4F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      at(9.0F,
         Command::SpawnAmbush,
         QStringLiteral("last_risen_bows"),
         QStringLiteral("rpg_commander")),
      at(11.0F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      at(11.5F,
         Command::Charge,
         QStringLiteral("last_risen_rear"),
         QStringLiteral("rpg_commander")),
      at(12.0F,
         Command::Charge,
         QStringLiteral("last_risen_bows"),
         QStringLiteral("rpg_commander")),
      targeted(13.0F, Command::SetHealth, QStringLiteral("rpg_commander"), {}, 1900),
      at(14.0F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      at(16.0F, Command::RpgPrimaryAttack, QStringLiteral("rpg_commander")),
      targeted(17.0F, Command::SetHealth, QStringLiteral("rpg_commander"), {}, 900),
      at(18.5F, Command::RpgGuard, QStringLiteral("rpg_commander")),
      targeted(20.0F, Command::SetHealth, QStringLiteral("rpg_commander"), {}, 260),
      targeted(21.5F,
               Command::ApplyDamage,
               QStringLiteral("rpg_commander"),
               QStringLiteral("last_risen_front"),
               9000),
  };

  s.expectations = {
      expectation(Expect::GroupIsRendered, QStringLiteral("rpg_commander")),
      expectation(Expect::AttackAnimationObserved, QStringLiteral("rpg_commander")),
      expectation(Expect::RpgHealthReduced, QStringLiteral("rpg_commander")),
      expectation(Expect::DeathAnimationObserved, QStringLiteral("rpg_commander")),
      expectation(Expect::GroupDestroyed, QStringLiteral("rpg_commander")),
  };
  return s;
}

auto trailer_wolf_rain() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_trailer_wolf_rain_id),
      QStringLiteral("Trailer I: The Fold"),
      QStringLiteral("Rain over the high pasture. A big flock grazes the wet "
                     "ground below the steading while the pack comes down out "
                     "of the trees at it, and the riders of the watch turn out "
                     "to answer."),
      44.0F,
      {30.0F, 26.0F, 14.0F});
  s.ground_type = QStringLiteral("forest_mud");
  s.terrain_seed_override = 6142;
  s.arena_floor_half_extent = 40.0F;
  s.collect_animation_diagnostics = true;
  s.environment.start_time = 16.4F;
  s.environment.fog_density_override = 0.05F;
  s.environment.exposure_override = 1.4F;
  s.weather.rain = 0.55F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Rain;
  s.precipitation.intensity = 0.55F;
  s.precipitation.wind_strength = 0.22F;
  s.precipitation.wind_direction_deg = 240.0F;
  s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);

  s.roads.push_back(
      street({-34.0F, 0.0F, -14.0F}, {30.0F, 0.0F, -14.0F}, 2.8F, "earth"));

  s.groups.push_back(building(QStringLiteral("fold_homes"),
                              Game::Units::SpawnType::Home,
                              Nation::RomanRepublic,
                              1,
                              3,
                              {-16.0F, 0.0F, -20.0F},
                              {6.4F, 0.0F, 0.0F},
                              180.0F));
  s.groups.push_back(building(QStringLiteral("fold_market"),
                              Game::Units::SpawnType::Marketplace,
                              Nation::RomanRepublic,
                              1,
                              1,
                              {6.0F, 0.0F, -21.0F},
                              {},
                              180.0F));

  auto shepherds = group(QStringLiteral("fold_shepherds"),
                         Troop::Civilian,
                         1,
                         4,
                         {-6.0F, 0.0F, -8.0F},
                         1,
                         {3.2F, 0.0F, 0.0F});
  auto watch = group(QStringLiteral("fold_watch"),
                     Troop::MountedKnight,
                     1,
                     3,
                     {-26.0F, 0.0F, -14.0F},
                     4,
                     {3.0F, 0.0F, 0.0F});
  s.groups.push_back(shepherds);
  s.groups.push_back(watch);

  s.resource_patches = {
      patch("pine_tree", 10, {22.0F, 0.0F, 16.0F}, {3.6F, 0.0F, 3.4F}, 1.2F),
      patch("pine_tree", 9, {-24.0F, 0.0F, 18.0F}, {3.6F, 0.0F, 3.4F}, 1.2F),
      patch("olive_tree", 6, {-30.0F, 0.0F, -6.0F}, {0.0F, 0.0F, 4.2F}, 1.1F),
      patch("boulder", 6, {16.0F, 0.0F, -8.0F}, {3.2F, 0.0F, 1.6F}, 1.1F),
      patch("supply_cart", 2, {-10.0F, 0.0F, -16.0F}, {4.2F, 0.0F, 0.0F}, 1.0F),
      patch("fire_camp", 1, {-2.0F, 0.0F, -18.0F}, {}, 0.9F),
      patch("plant", 10, {2.0F, 0.0F, 6.0F}, {3.0F, 0.0F, 0.0F}, 0.95F),
  };

  s.wildlife = default_wildlife(20260812U);
  s.wildlife.sheep.enabled = true;
  s.wildlife.sheep.group_count = 4;
  s.wildlife.sheep.group_size_min = 8;
  s.wildlife.sheep.group_size_max = 9;
  s.wildlife.sheep.roam_radius = 7.0F;
  s.wildlife.sheep.spawn_areas = {{-6.0F, 2.0F, 5.0F},
                                  {6.0F, 6.0F, 5.0F},
                                  {-2.0F, 12.0F, 5.0F},
                                  {10.0F, -2.0F, 5.0F}};
  s.wildlife.wolves.enabled = true;
  s.wildlife.wolves.group_count = 0;
  s.wildlife.wolves.group_size_min = 5;
  s.wildlife.wolves.group_size_max = 5;
  s.wildlife.wolves.aggression = 1.0F;
  s.wildlife.wolves.roam_radius = 22.0F;
  s.wildlife.wolves.alert_radius = 12.0F;
  s.wildlife.wolves.respawn = false;
  s.wildlife.wolves.waves = {{6.0F, 5, {18.0F, 14.0F, 2.5F}, "east_pack"},
                             {8.0F, 5, {-18.0F, 16.0F, 2.5F}, "west_pack"}};

  s.steps = {
      move_to(2.0F, QStringLiteral("fold_shepherds"), {-2.0F, 0.0F, -4.0F}),
      move_to(18.0F, QStringLiteral("fold_watch"), {-2.0F, 0.0F, 2.0F}),
      move_to(30.0F, QStringLiteral("fold_shepherds"), {-8.0F, 0.0F, -12.0F}),
  };

  s.expectations = {
      expectation(Expect::GroupIsRendered, QStringLiteral("fold_watch")),
      expectation(Expect::WildlifeGrazingObserved),
      expectation(Expect::WildlifeHuntObserved),
      expectation(Expect::WildlifeCasualtyObserved),
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
  result.push_back(trailer_works());
  result.push_back(trailer_sanctuary());
  result.push_back(trailer_gate_march());
  result.push_back(trailer_highland());
  result.push_back(trailer_forest_ambush());
  result.push_back(trailer_bridge_defense());
  result.push_back(trailer_siege_walls());
  result.push_back(trailer_night_snow());
  result.push_back(trailer_last_breath());
  result.push_back(trailer_wolf_rain());
  return result;
}

} // namespace Arena::Scenarios
