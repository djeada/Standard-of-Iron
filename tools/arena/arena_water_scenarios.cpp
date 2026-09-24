#include "arena_water_scenarios.h"

#include <utility>

#include "arena_scenarios.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

// One crossing, reused under every condition the water has to hold up in: a
// river that bends hard twice and runs into a lake, a bridge whose approach
// roads climb sloped banks, troops crossing it, a watch post on the inside of
// a bend, and bank dressing. Only the weather, hour, crossing height or fog
// changes between captures, so they compare like for like.
constexpr QVector3D k_bridge_start{4.6F, 0.0F, -12.0F};
constexpr QVector3D k_bridge_end{-2.6F, 0.0F, 0.0F};

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
  result.facing_degrees = 150.0F;
  return result;
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

auto move(float time, const char* group, QVector3D destination) -> ArenaScenarioStep {
  auto step = at(time, Command::Move, QString::fromLatin1(group));
  step.destination = destination;
  return step;
}

auto rise(float x, float z, float radius, float height) -> ArenaScenarioElevationPatch {
  ArenaScenarioElevationPatch patch;
  patch.center = QVector3D(x, 0.0F, z);
  patch.radius = radius;
  patch.height = height;
  return patch;
}

auto crossing(const char* id,
              QString label,
              QString description) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition s;
  s.id = QString::fromLatin1(id);
  s.label = std::move(label);
  s.description = std::move(description);
  s.duration_seconds = 12.0F;
  s.arena_floor_half_extent = 32.0F;
  s.camera = {34.0F, 42.0F, 20.0F};
  s.camera_focus = QVector3D(-2.0F, 0.0F, -5.0F);
  s.select_spawned_units = false;
  s.suppress_spawn_anchor = true;
  s.suppress_ui_overlays = true;
  s.suppress_combat_dust = true;
  s.force_full_creature_lod = true;

  s.lakes.push_back(Game::Map::Lake{{-27.0F, 0.0F, -6.0F}, 12.0F, 9.0F, 15.0F});
  constexpr float k_river_width = 6.0F;
  s.rivers = {
      {{-22.0F, 0.0F, -4.0F}, {-14.0F, 0.0F, -2.0F}, k_river_width},
      {{-14.0F, 0.0F, -2.0F}, {-4.0F, 0.0F, -9.0F}, k_river_width},
      {{-4.0F, 0.0F, -9.0F}, {6.0F, 0.0F, -3.0F}, k_river_width},
      {{6.0F, 0.0F, -3.0F}, {30.0F, 0.0F, 3.0F}, k_river_width},
  };
  s.bridges.push_back(Game::Map::Bridge{k_bridge_start, k_bridge_end, 8.0F, 0.5F});
  s.roads = {
      Game::Map::RoadSegment{k_bridge_end, {-7.7F, 0.0F, 8.6F}, 4.0F},
      Game::Map::RoadSegment{{-7.7F, 0.0F, 8.6F}, {-12.0F, 0.0F, 20.0F}, 4.0F},
      Game::Map::RoadSegment{k_bridge_start, {9.7F, 0.0F, -20.6F}, 4.0F},
  };
  // The banks rise away from the water on both approaches, and one of them
  // falls across the road too, so the deck ends meet sloped ground.
  s.elevation_patches = {rise(-10.0F, 5.0F, 9.0F, 1.4F),
                         rise(11.0F, -15.0F, 8.0F, 1.1F)};

  s.groups = {
      troops(QStringLiteral("column"), Troop::Swordsman, {-8.0F, 0.0F, 10.0F}, 9),
      troops(
          QStringLiteral("riders"), Troop::MountedSwordsman, {-11.0F, 0.0F, 15.0F}, 3),
      troops(QStringLiteral("watch"), Troop::Archer, {-6.5F, 0.0F, -3.0F}, 4),
  };
  s.resource_patches = {
      {QStringLiteral("olive_tree"), 3, {-16.0F, 0.0F, 3.0F}, {2.6F, 0.0F, 0.8F}, 1.0F},
      {QStringLiteral("boulder"), 3, {2.0F, 0.0F, 1.5F}, {1.6F, 0.0F, 0.6F}, 0.9F},
      {QStringLiteral("plant"), 5, {-1.0F, 0.0F, -15.0F}, {1.2F, 0.0F, 0.4F}, 1.0F},
      {QStringLiteral("pine_tree"), 2, {16.0F, 0.0F, -6.0F}, {2.4F, 0.0F, 1.0F}, 1.0F},
  };

  // The column crosses first, then the riders; a low pass looks along the
  // deck from a commander's height, where bridge ends and waterlines show.
  s.steps = {move(1.0F, "column", {9.0F, 0.0F, -18.0F}),
             move(3.0F, "riders", {10.0F, 0.0F, -21.0F}),
             camera_step(4.0F, 16.0F, 32.0F, 200.0F),
             camera_step(7.0F, 9.0F, 10.0F, 150.0F),
             camera_step(9.5F, 22.0F, 36.0F, 300.0F)};

  for (const char* name : {"column", "riders", "watch"}) {
    ArenaExpectation rendered;
    rendered.kind = Expect::GroupIsRendered;
    rendered.group = QString::fromLatin1(name);
    s.expectations.push_back(rendered);
  }
  return s;
}

} // namespace

auto build_water_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  result.push_back(crossing(
      k_water_crossing_id,
      QStringLiteral("Water: River Crossing"),
      QStringLiteral("A bending river running into a lake, crossed by a bridge "
                     "whose roads climb sloped banks, in clear midday light.")));

  {
    auto s = crossing(k_water_ford_id,
                      QStringLiteral("Water: Ford"),
                      QStringLiteral("The same crossing as a low, wide ford the way "
                                     "the shipped maps author one: a bridge barely "
                                     "above a broader, shallower river."));
    for (auto& river : s.rivers) {
      river.width = 8.5F;
    }
    s.bridges.front().width = 10.0F;
    s.bridges.front().height = 0.05F;
    result.push_back(std::move(s));
  }

  {
    auto s = crossing(k_water_rain_id,
                      QStringLiteral("Water: Rain"),
                      QStringLiteral("The crossing in heavy rain: wet banks, a "
                                     "roughened surface and a darker sky."));
    s.weather.rain = 0.85F;
    s.weather.storm = 0.45F;
    s.precipitation.enabled = true;
    s.precipitation.type = Game::Map::WeatherType::Rain;
    s.precipitation.intensity = 0.85F;
    s.precipitation.wind_strength = 0.3F;
    result.push_back(std::move(s));
  }

  {
    auto s = crossing(k_water_snow_id,
                      QStringLiteral("Water: Snow"),
                      QStringLiteral("The crossing under snow: snow banks meeting "
                                     "cold water and a snow-dusted bridge."));
    s.ground_type = QStringLiteral("alpine_mix");
    s.terrain_snowbound = true;
    s.weather.snow = 0.7F;
    s.precipitation.enabled = true;
    s.precipitation.type = Game::Map::WeatherType::Snow;
    s.precipitation.intensity = 0.6F;
    result.push_back(std::move(s));
  }

  {
    auto s = crossing(k_water_dusk_id,
                      QStringLiteral("Water: Dusk"),
                      QStringLiteral("The crossing at dusk, with a low sun raking "
                                     "across the water and long shadows over it."));
    s.environment.start_time = 19.4F;
    result.push_back(std::move(s));
  }

  {
    auto s = crossing(k_water_night_id,
                      QStringLiteral("Water: Night"),
                      QStringLiteral("The crossing at night: moonlit water against "
                                     "dark banks."));
    s.environment.start_time = 23.0F;
    result.push_back(std::move(s));
  }

  {
    auto s = crossing(k_water_fog_boundary_id,
                      QStringLiteral("Water: Fog of War Boundary"),
                      QStringLiteral("The crossing under fog of war with only the "
                                     "north bank scouted, so the explored edge runs "
                                     "across water, banks and the bridge."));
    s.fog_of_war = true;
    // Nobody crosses: the sight line stays put on the north bank.
    s.steps.erase(s.steps.begin(), s.steps.begin() + 2);
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
