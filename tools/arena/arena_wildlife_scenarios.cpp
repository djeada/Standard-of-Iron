#include "arena_wildlife_scenarios.h"

#include <utility>

#include "arena_scenarios.h"
#include "game/wildlife/wildlife_config.h"

namespace Arena::Scenarios {
namespace {

using Command = ScenarioCommandKind;
using Expect = ArenaExpectationKind;
using Nation = Game::Systems::NationID;
using Trigger = ScenarioTriggerKind;
using Troop = Game::Units::TroopType;

auto definition(QString id,
                QString label,
                QString description,
                float duration,
                ArenaCameraView camera) -> ArenaScenarioDefinition {
  ArenaScenarioDefinition result;
  result.id = std::move(id);
  result.label = std::move(label);
  result.description = std::move(description);
  result.duration_seconds = duration;
  result.camera = camera;
  result.camera_focus = QVector3D(0.0F, 0.0F, 0.0F);
  result.select_spawned_units = false;
  return result;
}

auto observer_group(QVector3D origin) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = QStringLiteral("observer");
  result.troop_type = Troop::Civilian;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = 1;
  result.individuals_per_unit = 1;
  result.origin = origin;
  result.spacing = {0.0F, 0.0F, 0.0F};
  result.facing_degrees = 180.0F;
  return result;
}

auto patrol_group(QString name,
                  Troop troop,
                  QVector3D origin,
                  int count) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = count;
  result.individuals_per_unit = 4;
  result.origin = origin;
  result.spacing = {2.6F, 0.0F, 0.0F};
  result.facing_degrees = 180.0F;
  return result;
}

auto move_step(float time, QString group, QVector3D destination) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_move_%2").arg(group, QString::number(time, 'f', 1));
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::Move;
  result.group = std::move(group);
  result.destination = destination;
  return result;
}

auto expectation(Expect kind,
                 QString group = {},
                 float threshold = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(group);
  result.threshold = threshold;
  return result;
}

auto sheep_only(int groups,
                int size,
                float radius) -> Game::Wildlife::WildlifeSettings {
  Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 90210U;
  settings.wolves.enabled = false;
  settings.wolves.group_count = 0;
  settings.birds.enabled = false;
  settings.birds.group_count = 0;

  settings.sheep.enabled = true;
  settings.sheep.group_count = groups;
  settings.sheep.group_size_min = size;
  settings.sheep.group_size_max = size;
  settings.sheep.roam_radius = radius;
  settings.sheep.spawn_areas = {{0.0F, 0.0F, 3.0F}};
  return settings;
}

} // namespace

auto build_wildlife_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_grazing_herd_id),
        QStringLiteral("Wildlife: Grazing Herd"),
        QStringLiteral("An undisturbed herd grazes and drifts around its pasture so "
                       "the idle loop, herd cohesion and low-poly sheep silhouette "
                       "can be reviewed."),
        14.0F,
        {20.0F, 44.0F, 24.0F});
    s.wildlife = sheep_only(1, 8, 9.0F);
    s.groups = {observer_group({0.0F, 0.0F, -26.0F})};
    s.expectations = {
        expectation(Expect::WildlifeGrazingObserved),
        expectation(Expect::WildlifePopulationHeld, {}, 8.0F),
        expectation(Expect::FrameBudget, {}, 9.99F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_herd_flees_troops_id),
        QStringLiteral("Wildlife: Herd Flees Troops"),
        QStringLiteral("A patrol marches into a grazing herd; the flock response and "
                       "the run gait are what this fixture reviews."),
        16.0F,
        {22.0F, 46.0F, 26.0F});
    s.wildlife = sheep_only(1, 8, 9.0F);
    s.groups = {patrol_group(
        QStringLiteral("patrol"), Troop::Swordsman, {0.0F, 0.0F, -22.0F}, 2)};
    s.steps = {move_step(2.0F, QStringLiteral("patrol"), {0.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(Expect::WildlifeFleeObserved),
        expectation(Expect::WildlifePopulationHeld, {}, 8.0F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_wolf_hunt_id),
        QStringLiteral("Wildlife: Wolf Hunt"),
        QStringLiteral("A wolf pack closes on a nearby herd: stalking approach, the "
                       "bite loop and the herd's panic response in one shot."),
        20.0F,
        {24.0F, 48.0F, 28.0F});
    s.wildlife = sheep_only(1, 8, 8.0F);
    s.wildlife.wolves.enabled = true;
    s.wildlife.wolves.group_count = 1;
    s.wildlife.wolves.group_size_min = 3;
    s.wildlife.wolves.group_size_max = 3;
    s.wildlife.wolves.aggression = 1.0F;
    s.wildlife.wolves.roam_radius = 18.0F;
    s.wildlife.wolves.respawn = false;
    s.wildlife.wolves.spawn_areas = {{14.0F, 0.0F, 2.0F}};
    s.groups = {observer_group({0.0F, 0.0F, -30.0F})};
    s.expectations = {
        expectation(Expect::WildlifeHuntObserved),
        expectation(Expect::WildlifeFleeObserved),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_wolf_pack_id),
        QStringLiteral("Wildlife: Wolf Pack"),
        QStringLiteral("An undisturbed pack prowls its anchor with no prey in "
                       "reach, so the wolf silhouette, coat and gait can be "
                       "reviewed the way the grazing herd reviews the sheep."),
        14.0F,
        {20.0F, 44.0F, 24.0F});
    Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
    settings.enabled = true;
    settings.seed = 77341U;
    settings.sheep.enabled = false;
    settings.sheep.group_count = 0;
    settings.birds.enabled = false;
    settings.birds.group_count = 0;
    settings.wolves.enabled = true;
    settings.wolves.group_count = 1;
    settings.wolves.group_size_min = 5;
    settings.wolves.group_size_max = 5;
    settings.wolves.aggression = 0.2F;
    settings.wolves.roam_radius = 7.0F;
    settings.wolves.spawn_areas = {{0.0F, 0.0F, 3.0F}};
    s.wildlife = settings;
    s.groups = {observer_group({0.0F, 0.0F, -30.0F})};
    s.expectations = {
        expectation(Expect::WildlifePopulationHeld, {}, 5.0F),
        expectation(Expect::FrameBudget, {}, 9.99F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_bird_scatter_id),
        QStringLiteral("Wildlife: Bird Scatter"),
        QStringLiteral("Flocks cruise and perch until a column marches underneath, "
                       "which should burst them upward and outward."),
        16.0F,
        {26.0F, 44.0F, 20.0F});
    Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
    settings.enabled = true;
    settings.seed = 5150U;
    settings.sheep.enabled = false;
    settings.sheep.group_count = 0;
    settings.wolves.enabled = false;
    settings.wolves.group_count = 0;
    settings.birds.enabled = true;
    settings.birds.group_count = 2;
    settings.birds.group_size_min = 8;
    settings.birds.group_size_max = 10;
    settings.birds.roam_radius = 14.0F;
    settings.birds.alert_radius = 14.0F;
    settings.birds.flight_height = 6.0F;
    settings.birds.spawn_areas = {{0.0F, 0.0F, 4.0F}};
    s.wildlife = settings;
    s.groups = {patrol_group(
        QStringLiteral("column"), Troop::Spearman, {0.0F, 0.0F, -20.0F}, 2)};
    s.steps = {move_step(1.5F, QStringLiteral("column"), {0.0F, 0.0F, 0.0F})};
    s.expectations = {
        expectation(Expect::WildlifeBirdsScattered),
        expectation(Expect::FrameBudget, {}, 9.99F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_storm_pasture_id),
        QStringLiteral("Wildlife: Storm Pasture"),
        QStringLiteral("The same pasture under heavy rain and wind, so wildlife "
                       "shading and the weather pass can be compared side by side "
                       "with the clear-weather fixture."),
        14.0F,
        {22.0F, 46.0F, 24.0F});
    s.wildlife = sheep_only(1, 8, 9.0F);
    s.wildlife.birds.enabled = true;
    s.wildlife.birds.group_count = 1;
    s.wildlife.birds.group_size_min = 6;
    s.wildlife.birds.group_size_max = 6;
    s.wildlife.birds.spawn_areas = {{0.0F, 0.0F, 6.0F}};
    s.weather.rain = Game::Map::k_weather_intensity_heavy;
    s.weather.storm = 0.4F;
    s.precipitation.enabled = true;
    s.precipitation.type = Game::Map::WeatherType::Rain;
    s.precipitation.intensity = Game::Map::k_weather_intensity_heavy;
    s.precipitation.wind_strength = 0.35F;
    s.precipitation.wind_direction_deg = 210.0F;
    s.groups = {observer_group({0.0F, 0.0F, -26.0F})};
    s.expectations = {
        expectation(Expect::WildlifePopulationHeld, {}, 8.0F),
        expectation(Expect::FrameBudget, {}, 9.99F),
    };
    result.push_back(std::move(s));
  }

  {
    auto s = definition(
        QString::fromLatin1(k_wildlife_dense_population_id),
        QStringLiteral("Wildlife: Dense Population"),
        QStringLiteral("Six herds, three packs and six flocks at once: the frame "
                       "budget contract for ambient wildlife at map scale."),
        12.0F,
        {60.0F, 58.0F, 0.0F});
    Game::Wildlife::WildlifeSettings settings = Game::Wildlife::default_settings();
    settings.enabled = true;
    settings.seed = 31337U;
    settings.near_simulation_radius = 60.0F;
    settings.far_simulation_radius = 110.0F;

    settings.sheep.enabled = true;
    settings.sheep.group_count = 6;
    settings.sheep.group_size_min = 9;
    settings.sheep.group_size_max = 9;
    settings.sheep.roam_radius = 12.0F;
    settings.sheep.spawn_areas = {
        {-24.0F, 18.0F, 6.0F}, {24.0F, 18.0F, 6.0F}, {0.0F, -22.0F, 6.0F}};

    settings.wolves.enabled = true;
    settings.wolves.group_count = 3;
    settings.wolves.group_size_min = 4;
    settings.wolves.group_size_max = 4;
    settings.wolves.aggression = 0.6F;
    settings.wolves.spawn_areas = {{-28.0F, -14.0F, 5.0F}, {28.0F, -14.0F, 5.0F}};

    settings.birds.enabled = true;
    settings.birds.group_count = 6;
    settings.birds.group_size_min = 10;
    settings.birds.group_size_max = 10;
    settings.birds.spawn_areas = {{0.0F, 0.0F, 26.0F}};

    s.wildlife = settings;
    s.groups = {observer_group({0.0F, 0.0F, -46.0F})};
    s.expectations = {
        expectation(Expect::WildlifePopulationHeld, {}, 60.0F),
        expectation(Expect::FrameBudget, {}, 9.99F),
    };
    result.push_back(std::move(s));
  }

  return result;
}

} // namespace Arena::Scenarios
