#include "arena_ambience_scenarios.h"

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
  result.suppress_spawn_anchor = true;
  result.suppress_ui_overlays = true;
  result.suppress_combat_dust = true;
  result.force_full_creature_lod = true;
  result.collect_animation_diagnostics = true;
  result.graphics_quality = Render::GraphicsQuality::Ultra;
  result.environment.time_mode = Game::Map::TimeMode::Locked;
  return result;
}

auto figure(QString name,
            Troop troop,
            QVector3D origin,
            float facing) -> ArenaScenarioGroup {
  ArenaScenarioGroup result;
  result.name = std::move(name);
  result.troop_type = troop;
  result.nation_id = Nation::RomanRepublic;
  result.owner_id = 1;
  result.count = 1;
  result.individuals_per_unit = 1;
  result.origin = origin;
  result.spacing = {0.0F, 0.0F, 0.0F};
  result.facing_degrees = facing;
  return result;
}

auto resting(QString name,
             QString renderer,
             Troop troop,
             QVector3D origin,
             float facing,
             QString move,
             float start_delay) -> ArenaScenarioGroup {
  auto result = figure(std::move(name), troop, origin, facing);
  result.renderer_override = std::move(renderer);
  result.showcase_routine = QStringList{std::move(move)};
  result.showcase_start_delay = start_delay;
  result.showcase_loop = true;
  result.render_scale_override = 1.0F;
  return result;
}

auto settling(QString name,
              QString renderer,
              Troop troop,
              QVector3D origin,
              float facing,
              QString settle,
              QString move,
              float start_delay) -> ArenaScenarioGroup {
  auto result = figure(std::move(name), troop, origin, facing);
  result.renderer_override = std::move(renderer);
  result.showcase_routine = QStringList{std::move(settle), std::move(move)};
  result.showcase_start_delay = start_delay;
  result.showcase_loop = true;
  result.showcase_loop_from = 1;
  result.render_scale_override = 1.0F;
  return result;
}

auto move_at(float time, QString source, QVector3D destination) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2_walk").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::Move;
  result.group = std::move(source);
  result.destination = destination;
  return result;
}

auto patch(const char* prop_type,
           int count,
           QVector3D origin,
           QVector3D spacing = {2.5F, 0.0F, 0.0F},
           float scale = 1.0F) -> ArenaScenarioResourcePatch {
  return {QString::fromLatin1(prop_type), count, origin, spacing, scale, false};
}

auto seat(QVector3D origin, float scale) -> ArenaScenarioResourcePatch {
  return {QStringLiteral("boulder"), 1, origin, {}, scale, true};
}

auto stand_at(float time, QString source) -> ArenaScenarioStep {
  ArenaScenarioStep result;
  result.name = QStringLiteral("%1_%2").arg(QString::number(time, 'f', 2), source);
  result.trigger = {Trigger::AtTime, time, {}, {}, 0.0F};
  result.command = Command::Stand;
  result.group = std::move(source);
  return result;
}

auto expectation(Expect kind,
                 QString source = {},
                 float threshold = 0.0F,
                 float start = 0.0F) -> ArenaExpectation {
  ArenaExpectation result;
  result.kind = kind;
  result.group = std::move(source);
  result.threshold = threshold;
  result.start_seconds = start;
  return result;
}

void hold_the_frame(ArenaScenarioDefinition& scenario,
                    std::initializer_list<QString> groups) {
  for (auto const& name : groups) {
    scenario.expectations.push_back(expectation(Expect::GroupIsRendered, name));
    scenario.expectations.push_back(expectation(Expect::NoPoseOscillation, name));
    scenario.expectations.push_back(expectation(Expect::NoRootTeleport, name));
    scenario.expectations.push_back(expectation(Expect::NoUnexpectedFallPose, name));
    scenario.expectations.push_back(expectation(Expect::MovementIsContinuous, name));
  }
  scenario.expectations.push_back(expectation(Expect::FrameBudget, {}, 33.34F, 2.0F));
}

auto ambience_night_watch() -> ArenaScenarioDefinition {
  auto s = definition(
      QString::fromLatin1(k_ambience_night_watch_id),
      QStringLiteral("Ambience: Night Watch in the Rain"),
      QStringLiteral("Three soldiers rest around a camp fire in the late watch while "
                     "rain crosses the light and a flock grazes on the dark slope "
                     "behind them. Nothing happens and nothing is meant to: the scene "
                     "is staged for long-form ambience capture, so the only motion is "
                     "rain, firelight, breathing and one man feeding the fire."),
      500.0F,
      {24.0F, 28.0F, 80.0F});

  s.ground_type = QStringLiteral("soil_rocky");
  s.terrain_seed_override = 5501;
  s.arena_floor_half_extent = 34.0F;

  s.environment.start_time = 1.4F;
  s.environment.fog_density_override = 0.072F;
  s.environment.exposure_override = 0.62F;

  s.weather.rain = 0.55F;
  s.weather.storm = 0.22F;
  s.precipitation.enabled = true;
  s.precipitation.type = Game::Map::WeatherType::Rain;
  s.precipitation.intensity = 0.30F;
  s.precipitation.wind_strength = 0.22F;
  s.precipitation.wind_direction_deg = 250.0F;

  s.groups = {
      resting(QStringLiteral("fire_tender"),
              QStringLiteral("troops/roman/camp_rest"),
              Troop::Swordsman,
              {1.95F, 0.0F, 1.70F},
              218.0F,
              QStringLiteral("rest_kneel:5.0:0.0"),
              0.0F),
      settling(QStringLiteral("cross_legged"),
               QStringLiteral("troops/roman/camp_rest_helmed"),
               Troop::Swordsman,
               {-8.45F, 0.0F, 6.05F},
               145.0F,
               QStringLiteral("rest_sit_down:1.6:0.0"),
               QStringLiteral("rest_sit:6.0:0.0"),
               8.6F),
      settling(QStringLiteral("knees_up"),
               QStringLiteral("troops/roman/camp_rest_armed"),
               Troop::Swordsman,
               {0.20F, 0.0F, -2.95F},
               8.0F,
               QStringLiteral("rest_sit_knees_down:1.7:0.0"),
               QStringLiteral("rest_sit_knees:7.0:0.0"),
               1.2F),
      figure(
          QStringLiteral("far_sentry"), Troop::Spearman, {-8.6F, 0.0F, 7.4F}, 152.0F),
  };

  s.resource_patches = {
      patch("fire_camp", 1, {0.0F, 0.0F, 0.0F}, {}, 1.25F),
      seat({3.35F, 0.0F, 0.85F}, 0.62F),
      patch("tent", 1, {-10.4F, 0.0F, -6.8F}, {}, 0.85F),
      patch("tent", 1, {9.8F, 0.0F, -8.6F}, {}, 0.8F),
      patch("weapon_rack", 1, {5.1F, 0.0F, 3.4F}, {}, 0.85F),
      patch("supply_cart", 1, {-7.9F, 0.0F, 4.9F}, {}, 0.85F),
      patch("boulder", 3, {-13.5F, 0.0F, 2.6F}, {3.4F, 0.0F, 1.8F}, 1.15F),
      patch("boulder", 3, {12.4F, 0.0F, 5.2F}, {3.4F, 0.0F, 1.4F}, 1.1F),
      patch("pine_tree", 7, {-19.0F, 0.0F, -16.5F}, {3.8F, 0.0F, -2.4F}, 1.25F),
      patch("pine_tree", 6, {11.5F, 0.0F, -18.0F}, {3.8F, 0.0F, -2.0F}, 1.2F),
      patch("dead_tree", 2, {17.0F, 0.0F, -7.5F}, {5.0F, 0.0F, -2.0F}, 1.1F),
  };

  s.wildlife = Game::Wildlife::default_settings();
  s.wildlife.enabled = true;
  s.wildlife.seed = 5501U;
  s.wildlife.wolves.enabled = false;
  s.wildlife.wolves.group_count = 0;
  s.wildlife.birds.enabled = false;
  s.wildlife.birds.group_count = 0;
  s.wildlife.sheep.enabled = true;
  s.wildlife.sheep.group_count = 1;
  s.wildlife.sheep.group_size_min = 5;
  s.wildlife.sheep.group_size_max = 6;
  s.wildlife.sheep.roam_radius = 3.5F;
  s.wildlife.sheep.move_speed = 0.45F;
  s.wildlife.sheep.respawn = false;
  s.wildlife.sheep.spawn_areas = {{-13.5F, -11.0F, 3.0F}};

  s.steps = {
      stand_at(0.25F, QStringLiteral("far_sentry")),
      move_at(1.4F, QStringLiteral("cross_legged"), {-2.45F, 0.0F, 1.45F}),
  };

  hold_the_frame(s,
                 {QStringLiteral("fire_tender"),
                  QStringLiteral("cross_legged"),
                  QStringLiteral("knees_up"),
                  QStringLiteral("far_sentry")});
  return s;
}

} // namespace

auto build_ambience_definitions() -> std::vector<ArenaScenarioDefinition> {
  std::vector<ArenaScenarioDefinition> result;
  result.push_back(ambience_night_watch());
  return result;
}

} // namespace Arena::Scenarios
