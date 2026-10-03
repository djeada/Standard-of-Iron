#include "arena_lighting_scenarios.h"

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

} // namespace

auto build_lighting_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  for (const auto& fixture : std::array{
           std::tuple{k_lighting_sunrise_sunset_id, "Lighting: Sunrise", 6.25F, 0.0F},
           std::tuple{k_lighting_midday_id, "Lighting: Midday", 12.0F, 0.0F},
           std::tuple{k_lighting_sunset_id, "Lighting: Sunset", 19.25F, 0.0F},
           std::tuple{
               k_lighting_moonlit_night_id, "Lighting: Moonlit Night", 0.5F, 0.0F},
           std::tuple{k_lighting_heavy_rain_id, "Lighting: Heavy Rain", 15.0F, 0.9F}}) {
    auto s =
        definition(QString::fromLatin1(std::get<0>(fixture)),
                   QString::fromLatin1(std::get<1>(fixture)),
                   QStringLiteral("Locked-camera environment regression fixture "
                                  "covering troops, props, ground, and structures."),
                   8.0F,
                   {24.0F, 48.0F, 32.0F});
    s.environment.start_time = std::get<2>(fixture);
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.weather.rain = std::get<3>(fixture);
    s.weather.storm = std::get<3>(fixture) * 0.55F;
    s.groups = {group(QStringLiteral("roman_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-5.0F, 0.0F, 0.0F},
                      8,
                      {0.0F, 0.0F, 2.5F}),
                nation_group(QStringLiteral("carthage_line"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             3,
                             {5.0F, 0.0F, 0.0F},
                             8,
                             {0.0F, 0.0F, 2.5F}),
                building(QStringLiteral("home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-10.0F, 0.0F, 6.0F})};
    s.resource_patches = {{QStringLiteral("pine"),
                           4,
                           QVector3D(10.0F, 0.0F, 4.0F),
                           QVector3D(2.0F, 0.0F, 1.0F),
                           1.0F}};
    add_visual_stability(
        s, {QStringLiteral("roman_line"), QStringLiteral("carthage_line")});
    result.push_back(std::move(s));
  }

  struct PrecipitationFixture {
    const char* id;
    const char* title;
    bool snow;
    float intensity;
    float wind_strength;
    float wind_direction_deg;
    float hour;
    Render::GraphicsQuality quality;
  };

  for (const auto& fixture :
       std::array{PrecipitationFixture{k_weather_rain_light_id,
                                       "Weather: Light Rain",
                                       false,
                                       Game::Map::k_weather_intensity_light,
                                       0.15F,
                                       20.0F,
                                       14.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_rain_medium_id,
                                       "Weather: Medium Rain",
                                       false,
                                       Game::Map::k_weather_intensity_medium,
                                       0.30F,
                                       145.0F,
                                       14.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_rain_heavy_id,
                                       "Weather: Heavy Rain",
                                       false,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.60F,
                                       250.0F,
                                       14.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_light_id,
                                       "Weather: Light Snow",
                                       true,
                                       Game::Map::k_weather_intensity_light,
                                       0.15F,
                                       70.0F,
                                       11.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_medium_id,
                                       "Weather: Medium Snow",
                                       true,
                                       Game::Map::k_weather_intensity_medium,
                                       0.32F,
                                       70.0F,
                                       11.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_heavy_id,
                                       "Weather: Heavy Snow",
                                       true,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.65F,
                                       330.0F,
                                       11.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_snow_crosswind_id,
                                       "Weather: Snow Crosswind",
                                       true,
                                       Game::Map::k_weather_intensity_medium,
                                       1.20F,
                                       270.0F,
                                       17.0F,
                                       Render::GraphicsQuality::High},
                  PrecipitationFixture{k_weather_rain_budget_low_id,
                                       "Weather: Rain Budget (Low)",
                                       false,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.35F,
                                       145.0F,
                                       14.0F,
                                       Render::GraphicsQuality::Low},
                  PrecipitationFixture{k_weather_rain_budget_ultra_id,
                                       "Weather: Rain Budget (Ultra)",
                                       false,
                                       Game::Map::k_weather_intensity_heavy,
                                       0.35F,
                                       145.0F,
                                       14.0F,
                                       Render::GraphicsQuality::Ultra}}) {
    auto s = definition(
        QString::fromLatin1(fixture.id),
        QString::fromLatin1(fixture.title),
        QStringLiteral("Locked-camera precipitation fixture: troops, structures and "
                       "vegetation under one authored weather setting."),
        8.0F,
        {24.0F, 48.0F, 32.0F});
    s.environment.start_time = fixture.hour;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.graphics_quality = fixture.quality;
    if (fixture.snow) {
      s.weather.snow = fixture.intensity;
    } else {
      s.weather.rain = fixture.intensity;
      s.weather.storm = fixture.intensity * 0.35F;
    }
    s.precipitation.enabled = true;
    s.precipitation.type =
        fixture.snow ? Game::Map::WeatherType::Snow : Game::Map::WeatherType::Rain;
    s.precipitation.intensity = fixture.intensity;
    s.precipitation.wind_strength = fixture.wind_strength;
    s.precipitation.wind_direction_deg = fixture.wind_direction_deg;
    s.groups = {group(QStringLiteral("roman_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-5.0F, 0.0F, 0.0F},
                      8,
                      {0.0F, 0.0F, 2.5F}),
                nation_group(QStringLiteral("carthage_line"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             3,
                             {5.0F, 0.0F, 0.0F},
                             8,
                             {0.0F, 0.0F, 2.5F}),
                building(QStringLiteral("home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-10.0F, 0.0F, 6.0F})};
    s.resource_patches = {{QStringLiteral("pine"),
                           4,
                           QVector3D(10.0F, 0.0F, 4.0F),
                           QVector3D(2.0F, 0.0F, 1.0F),
                           1.0F}};
    add_visual_stability(
        s, {QStringLiteral("roman_line"), QStringLiteral("carthage_line")});
    result.push_back(std::move(s));
  }

  for (const auto& transition : std::array{
           std::tuple{k_lighting_dawn_to_day_id,
                      "Lighting: Dawn To Day",
                      "Continuous clock sweeping dawn into full day to verify the "
                      "lighting curves interpolate without visible jumps.",
                      5.0F},
           std::tuple{k_lighting_afternoon_to_night_id,
                      "Lighting: Afternoon To Night",
                      "Continuous clock sweeping late afternoon into night to verify "
                      "sun-to-moon handover and shadow softening.",
                      17.0F}}) {
    auto s = definition(QString::fromLatin1(std::get<0>(transition)),
                        QString::fromLatin1(std::get<1>(transition)),
                        QString::fromLatin1(std::get<2>(transition)),
                        12.0F,
                        {26.0F, 46.0F, 30.0F});
    s.environment.start_time = std::get<3>(transition);
    s.environment.time_mode = Game::Map::TimeMode::Continuous;
    s.environment.day_length_seconds = 90.0F;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("roman_line"),
                      Troop::Swordsman,
                      1,
                      2,
                      {-4.0F, 0.0F, 0.0F},
                      6,
                      {0.0F, 0.0F, 2.5F}),
                building(QStringLiteral("transition_home"),
                         Game::Units::SpawnType::Home,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {8.0F, 0.0F, 4.0F})};
    add_visual_stability(s, {QStringLiteral("roman_line")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_structure_shadows_id),
        QStringLiteral("Lighting: Structure Shadows"),
        QStringLiteral("Low sun across walls, a defense tower and a barracks so "
                       "structure shadow casting and unit-in-shadow reception can be "
                       "reviewed together."),
        8.0F,
        {30.0F, 34.0F, 18.0F});
    s.environment.start_time = 7.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;

    s.groups = {building(QStringLiteral("shadow_wall"),
                         Game::Units::SpawnType::WallSegment,
                         Nation::RomanRepublic,
                         1,
                         9,
                         {-8.0F, 0.0F, -6.0F},
                         {2.0F, 0.0F, 0.0F}),
                building(QStringLiteral("shadow_tower"),
                         Game::Units::SpawnType::DefenseTower,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-10.0F, 0.0F, -6.0F}),
                building(QStringLiteral("shadow_barracks"),
                         Game::Units::SpawnType::Barracks,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {6.0F, 0.0F, -6.0F}),

                group(QStringLiteral("shaded_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-6.0F, 0.0F, 2.0F},
                      6,
                      {3.0F, 0.0F, 0.0F})};
    add_visual_stability(s, {QStringLiteral("shaded_line")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_sepulcher_readability_id),
        QStringLiteral("Lighting: Sepulcher Readability"),
        QStringLiteral("Iron Sepulcher roster beside Roman and Carthaginian lines at "
                       "night, checking pale standards and cold stone stay solemn and "
                       "every nation stays readable in deep shadow."),
        8.0F,
        {22.0F, 40.0F, 0.0F});
    s.environment.start_time = 1.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("rome_readable"),
                      Troop::Swordsman,
                      1,
                      1,
                      {-6.0F, 0.0F, 0.0F},
                      4,
                      {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("carthage_readable"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             1,
                             {0.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("sepulcher_readable"),
                             Troop::SkeletonSwordsman,
                             Nation::IronSepulcher,
                             3,
                             1,
                             {6.0F, 0.0F, 0.0F},
                             4,
                             {0.0F, 0.0F, 2.2F})};
    s.resource_patches = {
        {QStringLiteral("firecamp"), 2, {-3.0F, 0.0F, 5.0F}, {6.0F, 0.0F, 0.0F}, 1.0F}};
    add_visual_stability(s,
                         {QStringLiteral("rome_readable"),
                          QStringLiteral("carthage_readable"),
                          QStringLiteral("sepulcher_readable")});
    result.push_back(std::move(s));
  }

  for (const auto& parity : std::array{std::tuple{k_lighting_parity_instanced_id,
                                                  "Lighting: Parity (Instanced)",
                                                  false},
                                       std::tuple{k_lighting_parity_single_id,
                                                  "Lighting: Parity (Non-Instanced)",
                                                  true}}) {
    auto s = definition(
        QString::fromLatin1(std::get<0>(parity)),
        QString::fromLatin1(std::get<1>(parity)),
        QStringLiteral("Paired fixture whose twin differs only in creature LOD "
                       "forcing; the two captures must match to prove instanced and "
                       "non-instanced paths light identically."),
        8.0F,
        {18.0F, 38.0F, 0.0F});
    s.environment.start_time = 16.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.force_full_creature_lod = std::get<2>(parity);
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("parity_line"),
                      Troop::Swordsman,
                      1,
                      3,
                      {-4.0F, 0.0F, 0.0F},
                      6,
                      {4.0F, 0.0F, 0.0F})};
    s.resource_patches = {
        {QStringLiteral("tent"), 1, {-6.0F, 0.0F, 5.0F}, {}, 1.0F},
        {QStringLiteral("supply_cart"), 1, {0.0F, 0.0F, 5.0F}, {}, 1.0F},
        {QStringLiteral("weapon_rack"), 1, {6.0F, 0.0F, 5.0F}, {}, 1.0F},
    };
    add_visual_stability(s, {QStringLiteral("parity_line")});
    result.push_back(std::move(s));
  }

  for (const auto& quality : std::array{std::tuple{k_lighting_shadow_quality_low_id,
                                                   "Lighting: Shadow Quality Low",
                                                   Render::GraphicsQuality::Low},
                                        std::tuple{k_lighting_shadow_quality_medium_id,
                                                   "Lighting: Shadow Quality Medium",
                                                   Render::GraphicsQuality::Medium},
                                        std::tuple{k_lighting_shadow_quality_high_id,
                                                   "Lighting: Shadow Quality High",
                                                   Render::GraphicsQuality::High},
                                        std::tuple{k_lighting_shadow_quality_ultra_id,
                                                   "Lighting: Shadow Quality Ultra",
                                                   Render::GraphicsQuality::Ultra}}) {
    auto s = definition(
        QString::fromLatin1(std::get<0>(quality)),
        QString::fromLatin1(std::get<1>(quality)),
        QStringLiteral("Large formations under a fixed low sun at one shadow quality "
                       "preset, for cross-quality shadow and frame-time comparison."),
        10.0F,
        {40.0F, 52.0F, 22.0F});
    s.environment.start_time = 17.5F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.graphics_quality = std::get<2>(quality);
    s.force_full_creature_lod = false;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("quality_rome"),
                      Troop::Swordsman,
                      1,
                      8,
                      {-11.0F, 0.0F, 0.0F},
                      12,
                      {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("quality_carthage"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             8,
                             {11.0F, 0.0F, 0.0F},
                             12,
                             {0.0F, 0.0F, 2.2F})};
    add_visual_stability(
        s, {QStringLiteral("quality_rome"), QStringLiteral("quality_carthage")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_commander_closeup_id),
        QStringLiteral("Lighting: Commander Close-Up"),
        QStringLiteral("Tight commander-mode framing at low sun so near-cascade "
                       "shadow detail and armour readability can be inspected."),
        8.0F,
        {7.0F, 22.0F, 35.0F});
    s.environment.start_time = 8.0F;
    s.environment.time_mode = Game::Map::TimeMode::Locked;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.force_full_creature_lod = true;
    s.select_spawned_units = false;
    s.suppress_spawn_anchor = true;
    s.suppress_ui_overlays = true;
    s.groups = {group(QStringLiteral("closeup_commander"),
                      Troop::RomanVeteranConsul,
                      1,
                      1,
                      {0.0F, 0.0F, 0.0F},
                      1),
                group(QStringLiteral("closeup_escort"),
                      Troop::Swordsman,
                      1,
                      1,
                      {2.4F, 0.0F, 1.2F},
                      2,
                      {1.6F, 0.0F, 0.0F})};
    add_visual_stability(
        s, {QStringLiteral("closeup_commander"), QStringLiteral("closeup_escort")});
    result.push_back(std::move(s));
  }

  {
    auto s =
        definition(QString::fromLatin1(k_lighting_dense_battle_id),
                   QStringLiteral("Lighting: Dense Battle"),
                   QStringLiteral("High-density formation fixture for shadow budgets, "
                                  "instancing parity, and frame-time tracking."),
                   12.0F,
                   {42.0F, 58.0F, 25.0F});
    s.environment.start_time = 17.5F;
    s.graphics_quality = Render::GraphicsQuality::Ultra;
    s.force_full_creature_lod = false;
    s.groups = {group(QStringLiteral("roman_mass"),
                      Troop::Swordsman,
                      1,
                      10,
                      {-12.0F, 0.0F, 0.0F},
                      16,
                      {0.0F, 0.0F, 2.2F}),
                nation_group(QStringLiteral("carthage_mass"),
                             Troop::Spearman,
                             Nation::Carthage,
                             2,
                             10,
                             {12.0F, 0.0F, 0.0F},
                             16,
                             {0.0F, 0.0F, 2.2F})};
    s.steps = {at(1.0F,
                  Command::AttackMove,
                  QStringLiteral("roman_mass"),
                  QStringLiteral("carthage_mass")),
               at(1.0F,
                  Command::AttackMove,
                  QStringLiteral("carthage_mass"),
                  QStringLiteral("roman_mass"))};
    add_visual_stability(
        s, {QStringLiteral("roman_mass"), QStringLiteral("carthage_mass")});
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_lighting_world_materials_id),
        QStringLiteral("Lighting: World Materials"),
        QStringLiteral("Terrain, water, vegetation, ruins, ore, and building "
                       "fixture for complete shared-lighting coverage."),
        10.0F,
        {34.0F, 55.0F, 30.0F});
    s.environment.start_time = 13.0F;
    s.groups = {building(QStringLiteral("barracks"),
                         Game::Units::SpawnType::Barracks,
                         Nation::RomanRepublic,
                         1,
                         1,
                         {-7.0F, 0.0F, 2.0F}),
                building(QStringLiteral("market"),
                         Game::Units::SpawnType::Marketplace,
                         Nation::Carthage,
                         2,
                         1,
                         {7.0F, 0.0F, 2.0F})};
    s.resource_patches = {
        {QStringLiteral("ruins"), 1, {-10.0F, 0.0F, 10.0F}, {}, 1.0F},
        {QStringLiteral("iron_ore"), 3, {0.0F, 0.0F, 10.0F}, {2.0F, 0.0F, 0.0F}, 1.0F},
        {QStringLiteral("olive"), 3, {9.0F, 0.0F, 10.0F}, {2.0F, 0.0F, 0.0F}, 1.0F}};
    add_settlement_acceptance(s,
                              {QStringLiteral("barracks"), QStringLiteral("market")});
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
