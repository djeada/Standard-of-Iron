#include "arena_facade_scenarios.h"

#include <QVector3D>

#include <array>
#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using SpawnType = Game::Units::SpawnType;

constexpr int k_roman_owner = 1;
constexpr int k_punic_owner = 2;

auto structure(QString name,
               SpawnType type,
               Nation nation,
               int owner,
               QVector3D origin,
               float facing) -> ArenaScenarioGroup {
  ArenaScenarioGroup group;
  group.name = std::move(name);
  group.spawn_type = type;
  group.nation_id = nation;
  group.owner_id = owner;
  group.count = 1;
  group.origin = origin;
  group.spacing = {0.0F, 0.0F, 0.0F};
  group.facing_degrees = facing;
  return group;
}

auto exists(QString group) -> ArenaExpectation {
  ArenaExpectation expectation;
  expectation.kind = Expect::GroupExists;
  expectation.group = std::move(group);
  return expectation;
}

struct FacadeEntry {
  const char* suffix;
  SpawnType type;
};

constexpr std::array<FacadeEntry, 6> k_facade_row{{
    {"home", SpawnType::Home},
    {"barracks", SpawnType::Barracks},
    {"marketplace", SpawnType::Marketplace},
    {"temple", SpawnType::Temple},
    {"tower", SpawnType::DefenseTower},
    {"farm", SpawnType::Farm},
}};

void add_row(ArenaScenarioDefinition& scenario,
             const char* prefix,
             Nation nation,
             int owner,
             float row_z,
             float facing,
             int health_percent = 100) {
  float x = -22.0F;
  for (const auto& entry : k_facade_row) {
    auto group = structure(
        QStringLiteral("%1_%2").arg(QLatin1String(prefix), QLatin1String(entry.suffix)),
        entry.type,
        nation,
        owner,
        QVector3D(x, 0.0F, row_z),
        facing);
    group.max_health_override = 100;
    group.health_override = health_percent;
    scenario.groups.push_back(std::move(group));
    x += 9.0F;
  }
}

} // namespace

auto build_facade_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> out;

  {

    ArenaScenarioDefinition s;
    s.id = QStringLiteral("facade_depth_showcase");
    s.label = QStringLiteral("Facade: Depth Layering");
    s.description = QStringLiteral(
        "Every procedural building both nations ship, drawn intact at trim-"
        "reading distance so applied panels, lintels, jambs, copings and "
        "reliefs can be checked for surfaces sharing a plane with the wall "
        "behind them.  Drive it with --capture-orbit to see a tie flicker; "
        "walls and gates have their own review in wall_corner_showcase.");
    s.duration_seconds = 14.0F;
    s.camera = {62.0F, 36.0F, 18.0F};
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.arena_floor_half_extent = 44.0F;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.collect_animation_diagnostics = false;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 13.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    add_row(s, "roman", Nation::RomanRepublic, k_roman_owner, -9.0F, 180.0F);
    add_row(s, "punic", Nation::Carthage, k_punic_owner, 9.0F, 180.0F);

    for (const auto& group : s.groups) {
      s.expectations.push_back(exists(group.name));
    }

    out.push_back(std::move(s));
  }

  {

    ArenaScenarioDefinition s;
    s.id = QStringLiteral("facade_damage_showcase");
    s.label = QStringLiteral("Facade: Damaged and Destroyed");
    s.description = QStringLiteral(
        "The same buildings dropped to their damaged and destroyed "
        "representations.  Decay tints each part separately, so coincident "
        "surfaces that read as one colour on an intact building show up here.");
    s.duration_seconds = 16.0F;
    s.camera = {62.0F, 36.0F, 18.0F};
    s.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
    s.arena_floor_half_extent = 44.0F;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.suppress_terrain_scatter = true;
    s.suppress_terrain_features = true;
    s.collect_animation_diagnostics = false;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.environment.start_time = 13.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;

    add_row(
        s, "damaged_roman", Nation::RomanRepublic, k_roman_owner, -9.0F, 180.0F, 45);
    add_row(s, "ruined_punic", Nation::Carthage, k_punic_owner, 9.0F, 180.0F, 12);

    for (const auto& group : s.groups) {
      s.expectations.push_back(exists(group.name));
    }

    out.push_back(std::move(s));
  }

  return out;
}

} // namespace Arena::Scenarios
