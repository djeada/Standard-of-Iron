#include <QDebug>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "animation/showcase_pose_manifest.h"
#include "app/commander/commander_control_controller.h"
#include "app/orders/movement_utils.h"
#include "arena_scenario.h"
#include "arena_viewport.h"
#include "arena_viewport_internal.h"
#include "game/core/component_economy.h"
#include "game/core/component_presentation.h"
#include "game/core/world.h"
#include "game/map/environment_lighting.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_commander_doctrine.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "render/ground/rain_renderer.h"
#include "render/mist_volume_builder.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/frame_continuity_analyzer.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

using namespace arena_viewport_internal;

namespace {

auto scenario_center(Engine::Core::World* world,
                     const std::vector<Engine::Core::EntityID>& entity_ids)
    -> QVector3D {
  if (world == nullptr || entity_ids.empty()) {
    return {};
  }

  QVector3D accumulated(0.0F, 0.0F, 0.0F);
  int count = 0;
  for (Engine::Core::EntityID const entity_id : entity_ids) {
    auto* entity = world->get_entity(entity_id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      continue;
    }
    accumulated +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    ++count;
  }

  if (count <= 0) {
    return {};
  }
  return accumulated / static_cast<float>(count);
}

} // namespace

auto ArenaViewport::ai_activity_summary() const -> QString {
  auto* ai_system =
      m_world != nullptr ? m_world->get_system<Game::Systems::AISystem>() : nullptr;
  if (ai_system == nullptr) {
    return QStringLiteral("no AI system");
  }
  QString sides;
  for (int owner : {2, 3}) {
    int units = 0;
    int buildings = 0;
    for (const auto entity_id : m_world->entities_with<Engine::Core::UnitComponent>()) {
      auto* entity = m_world->get_entity(entity_id);
      auto* component = entity != nullptr
                            ? entity->get_component<Engine::Core::UnitComponent>()
                            : nullptr;
      if (component == nullptr || component->owner_id != owner ||
          component->health <= 0) {
        continue;
      }
      if (entity->get_component<Engine::Core::BuildingComponent>() != nullptr) {
        ++buildings;
      } else {
        ++units;
      }
    }
    sides += QStringLiteral(" owner %1: %2 units %3 buildings;")
                 .arg(owner)
                 .arg(units)
                 .arg(buildings);
  }
  return QStringLiteral("ai players %1, decisions %2, commands %3, refused %4;%5")
      .arg(ai_system->ai_player_count())
      .arg(ai_system->completed_decision_count())
      .arg(ai_system->applied_command_count())
      .arg(ai_system->refused_command_count())
      .arg(sides);
}

auto ArenaViewport::scenario_elapsed_seconds() const -> float {
  return m_scenario_runner != nullptr ? m_scenario_runner->elapsed_seconds() : 0.0F;
}

auto ArenaViewport::scenario_group_center(const QString& group) const
    -> std::optional<QVector3D> {
  if (m_scenario_runner == nullptr || m_world == nullptr) {
    return std::nullopt;
  }
  auto const& entities = m_scenario_runner->group_entities(group);
  if (entities.empty()) {
    return std::nullopt;
  }
  bool any_alive = false;
  for (Engine::Core::EntityID const entity_id : entities) {
    if (m_world->get_entity(entity_id) != nullptr) {
      any_alive = true;
      break;
    }
  }
  if (!any_alive) {
    return std::nullopt;
  }
  return scenario_center(m_world.get(), entities);
}

namespace {

struct LivingCombatant {
  int owner{0};
  bool building{false};
  QVector3D position;
};

auto collect_living_combatants(Engine::Core::World& world,
                               bool include_buildings) -> std::vector<LivingCombatant> {
  std::vector<LivingCombatant> result;
  for (const auto entity_id : world.entities_with<Engine::Core::UnitComponent>()) {
    auto* entity = world.get_entity(entity_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        unit->owner_id <= 0) {
      continue;
    }
    const bool building = entity->has_component<Engine::Core::BuildingComponent>();
    if (building ? !include_buildings
                 : !Game::Units::can_use_attack_mode(unit->spawn_type)) {
      continue;
    }
    result.push_back({unit->owner_id,
                      building,
                      QVector3D(transform->position.x,
                                transform->position.y,
                                transform->position.z)});
  }
  return result;
}

auto centroid(const std::vector<QVector3D>& points) -> std::optional<QVector3D> {
  if (points.empty()) {
    return std::nullopt;
  }
  QVector3D sum;
  for (const auto& point : points) {
    sum += point;
  }
  return sum / static_cast<float>(points.size());
}

} // namespace

auto ArenaViewport::scenario_battle_center(
    int owner_filter, float engagement_radius) const -> std::optional<QVector3D> {
  if (m_world == nullptr) {
    return std::nullopt;
  }
  const auto combatants = collect_living_combatants(*m_world, true);
  const float radius_sq = engagement_radius * engagement_radius;
  std::vector<QVector3D> engaged;
  for (const auto& unit : combatants) {
    if (unit.building || (owner_filter > 0 && unit.owner != owner_filter)) {
      continue;
    }
    for (const auto& other : combatants) {
      if (other.owner == unit.owner) {
        continue;
      }
      if ((other.position - unit.position).lengthSquared() <= radius_sq) {
        engaged.push_back(unit.position);
        break;
      }
    }
  }
  return centroid(engaged);
}

auto ArenaViewport::scenario_army_center(int owner, float home_radius) const
    -> std::optional<QVector3D> {
  if (m_world == nullptr || m_scenario_runner == nullptr) {
    return std::nullopt;
  }
  std::optional<QVector3D> home;
  for (const auto& side : m_scenario_runner->definition().battle_sides) {
    if (side.owner_id == owner) {
      home = side.home;
      break;
    }
  }
  const float radius_sq = home_radius * home_radius;
  std::vector<QVector3D> afield;
  for (const auto& unit : collect_living_combatants(*m_world, false)) {
    if (unit.owner != owner) {
      continue;
    }
    if (home.has_value() && (unit.position - *home).lengthSquared() < radius_sq) {
      continue;
    }
    afield.push_back(unit.position);
  }
  return centroid(afield);
}

auto ArenaViewport::scenario_center_of_mass() const -> std::optional<QVector3D> {
  if (m_scenario_runner == nullptr || m_world == nullptr) {
    return std::nullopt;
  }
  auto const entities = m_scenario_runner->all_entities();
  if (entities.empty()) {
    return std::nullopt;
  }
  return scenario_center(m_world.get(), entities);
}

void ArenaViewport::set_scenario_duration_override(float seconds) {
  m_scenario_duration_override = std::max(0.0F, seconds);
  if (m_scenario_runner != nullptr && m_scenario_duration_override > 0.0F) {
    m_scenario_runner->set_duration_limit(m_scenario_duration_override);
  }
}

auto ArenaViewport::active_scenario_finished() const -> bool {
  return m_scenario_runner != nullptr && m_scenario_runner->finished();
}

auto ArenaViewport::active_scenario_report() const
    -> const Arena::ArenaScenarioReport* {
  return m_scenario_runner != nullptr ? &m_scenario_runner->report() : nullptr;
}

auto ArenaViewport::active_scenario_events() const
    -> std::vector<Arena::ArenaScenarioEvent> {
  return m_scenario_runner != nullptr ? m_scenario_runner->events()
                                      : std::vector<Arena::ArenaScenarioEvent>{};
}

void ArenaViewport::rebuild_static_mist(
    const Arena::ArenaScenarioDefinition& scenario) {
  m_static_mist.clear();
  m_applied_fog_banks.clear();
  m_mist_dirty = true;
  if (!scenario.weather_script.water_mist) {
    return;
  }
  auto const& terrain = m_session.terrain();
  auto const* height_map = terrain.get_height_map();
  std::vector<Game::Map::FogZone> fog_zones;
  if (m_terrain_review_definition.has_value()) {
    fog_zones = m_terrain_review_definition->fog_zones;
  }
  for (auto const& zone : m_arena_undead_zones) {
    if (zone.fog_density > 0.0F) {
      fog_zones.push_back(
          Game::Map::undead_zone_fog(zone.x, zone.z, zone.radius, zone.fog_density));
    }
  }
  static const std::vector<Game::Map::RiverSegment> k_no_rivers;
  static const std::vector<Game::Map::Lake> k_no_lakes;
  m_static_mist = Render::build_mist_volumes(
      {.fog_zones = &fog_zones,
       .rivers =
           height_map != nullptr ? &height_map->get_river_segments() : &k_no_rivers,
       .lakes = height_map != nullptr ? &height_map->get_lakes() : &k_no_lakes},
      [&terrain](float world_x, float world_z) {
        return terrain.is_initialized()
                   ? terrain.resolve_surface_world_y(world_x, world_z, 0.0F)
                   : 0.0F;
      });
}

void ArenaViewport::apply_scenario_weather() {
  if (m_scenario_runner == nullptr) {
    m_scripted_fog_density.reset();
    m_scripted_exposure.reset();
    if (m_mist_dirty && m_renderer != nullptr) {
      m_applied_mist = m_static_mist;
      m_renderer->set_mist_volumes(m_applied_mist);
      m_mist_dirty = false;
    }
    return;
  }
  auto const& scenario = m_scenario_runner->definition();
  Arena::ArenaWeatherBase base = m_weather_base;
  base.hour = m_environment_hour;
  {
    Game::Map::WeatherLightingInput weather = m_weather_lighting;
    weather.rain = base.rain;
    weather.storm = base.storm;
    weather.snow = base.snow;
    auto const lighting =
        Game::Map::lighting_for_hour(m_environment_hour, m_lighting_profile, weather);
    base.fog_density = scenario.environment.fog_density_override >= 0.0F
                           ? scenario.environment.fog_density_override
                           : lighting.fog_density;
    base.exposure = scenario.environment.exposure_override >= 0.0F
                        ? scenario.environment.exposure_override
                        : lighting.exposure;
  }
  m_weather_state = Arena::evaluate_weather(scenario.weather_script,
                                            m_scenario_runner->weather_schedule(),
                                            m_scenario_runner->elapsed_seconds(),
                                            base);
  auto const& state = m_weather_state;
  if (state.hour_scripted && !m_environment_hour_override.has_value()) {
    m_environment_hour = Game::Map::normalize_hour(state.hour);
  }
  m_scripted_fog_density = state.fog_density_scripted
                               ? std::optional<float>(state.fog_density)
                               : std::nullopt;
  m_scripted_exposure =
      state.exposure_scripted ? std::optional<float>(state.exposure) : std::nullopt;
  if (state.precipitation_scripted) {
    float const intensity = std::max({state.rain, state.storm, state.snow});
    m_weather_lighting.rain = state.rain;
    m_weather_lighting.storm = state.storm;
    m_weather_lighting.snow = state.snow;
    m_weather_type =
        state.snow > 0.0F ? Game::Map::WeatherType::Snow : Game::Map::WeatherType::Rain;
    bool const enabled = intensity > 0.001F;
    if (enabled != m_rain_enabled || std::abs(intensity - m_rain_intensity) > 1.0e-4F) {
      m_rain_enabled = enabled;
      m_rain_intensity = intensity;
      if (m_rain != nullptr) {
        m_rain->set_weather_type(m_weather_type);
        m_rain->set_enabled(m_rain_enabled);
        m_rain->set_intensity(m_rain_intensity);
      }
    }
  }
  if (state.wind_scripted && m_rain != nullptr) {
    m_rain->set_wind_strength(state.wind_strength);
    m_rain->set_wind_direction_deg(state.wind_direction_deg);
  }
  if (m_renderer != nullptr &&
      (m_mist_dirty || state.fog_banks != m_applied_fog_banks)) {
    auto const& terrain = m_session.terrain();
    auto banks = Arena::fog_bank_mist_volumes(
        state.fog_banks, m_scenario_origin, [&terrain](float world_x, float world_z) {
          return terrain.is_initialized()
                     ? terrain.resolve_surface_world_y(world_x, world_z, 0.0F)
                     : 0.0F;
        });
    m_applied_mist = Render::merge_mist_volumes(std::move(banks), m_static_mist);
    m_renderer->set_mist_volumes(m_applied_mist);
    m_applied_fog_banks = state.fog_banks;
    m_mist_dirty = false;
  }
}

auto ArenaViewport::write_scenario_artifacts(const QString& directory,
                                             QString* error) const -> bool {
  if (m_scenario_runner == nullptr) {
    if (error != nullptr) {
      *error = QStringLiteral("no Arena scenario is active");
    }
    return false;
  }
  m_scenario_runner->set_environment_snapshot(environment_snapshot());
  return m_scenario_runner->write_artifacts(directory, error);
}

namespace {

auto resolve_scenario_ai_profile(const Arena::ArenaScenarioDefinition& definition,
                                 int owner_id,
                                 Game::Systems::NationID nation_id)
    -> Game::Systems::AI::AIPlayerProfile {
  const Arena::ArenaScenarioAIProfile* authored = nullptr;
  for (const auto& candidate : definition.ai_profiles) {
    if (candidate.owner_id == owner_id) {
      authored = &candidate;
      break;
    }
  }

  Game::Systems::AI::AIPlayerProfile profile;
  profile.strategy = nation_id == Game::Systems::NationID::Carthage
                         ? Game::Systems::AI::AIStrategy::Economic
                         : Game::Systems::AI::AIStrategy::Defensive;

  bool doctrine_applied = false;
  for (const auto& group : definition.groups) {
    if (group.owner_id != owner_id ||
        !Game::Units::is_commander_troop(group.troop_type)) {
      continue;
    }
    if (auto doctrine =
            Game::Systems::AI::doctrine_profile_for_troop(group.troop_type)) {
      profile = *doctrine;
      doctrine_applied = true;
      break;
    }
  }
  (void)doctrine_applied;

  if (authored == nullptr) {
    return profile;
  }
  if (!authored->strategy.isEmpty()) {
    profile.strategy =
        Game::Systems::AI::AIStrategyFactory::parse_strategy(authored->strategy);
  }
  if (!authored->posture.isEmpty()) {
    profile.posture = Game::Systems::AI::AIStrategyFactory::parse_posture(
        authored->posture, profile.posture);
  }
  if (authored->aggression >= 0.0F) {
    profile.personality.aggression = authored->aggression;
  }
  if (authored->defense >= 0.0F) {
    profile.personality.defense = authored->defense;
  }
  if (authored->harassment >= 0.0F) {
    profile.personality.harassment = authored->harassment;
  }
  if (!authored->difficulty.isEmpty()) {
    profile.difficulty = authored->difficulty;
  }
  return profile;
}

} // namespace

void ArenaViewport::load_scenario(const QString& scenario_id) {
  auto const* definition = Arena::Scenarios::find_definition(scenario_id);
  if (definition == nullptr || m_world == nullptr) {
    return;
  }

  m_terrain_review_mode = false;
  m_terrain_review_definition.reset();
  if (m_renderer != nullptr) {
    m_renderer->set_clear_color(0.70F, 0.73F, 0.80F, 1.0F);
  }
  reset_arena();
  m_feedback.clear();
  apply_scenario_environment(*definition);
  clear_camera_key_state();
  const bool map_terrain = apply_scenario_terrain(*definition);
  const bool has_scenario_ai = register_scenario_owners(*definition);
  if (map_terrain && m_terrain_review_definition.has_value()) {
    spawn_terrain_review_structures();
  }
  QVector3D const scenario_origin =
      map_terrain ? QVector3D() : resolve_spawn_anchor_world();

  configure_scenario_wildlife(*definition, scenario_origin);

  Arena::ArenaScenarioHost host;
  host.spawn_unit = [this](const Arena::ArenaScenarioGroup& group,
                           const QVector3D& requested_position) {
    return spawn_scenario_group_entity(group, requested_position);
  };
  host.find_unit = [this](Engine::Core::EntityID entity_id) {
    return find_unit_handle(entity_id);
  };
  host.set_camera = [this, definition, scenario_origin](
                        const std::vector<Engine::Core::EntityID>& entities,
                        const Arena::ArenaCameraView& view) {
    if (m_camera != nullptr) {
      QVector3D const center = definition->camera_focus.has_value()
                                   ? scenario_origin + *definition->camera_focus
                                   : scenario_center(m_world.get(), entities);
      Arena::ArenaCameraView scaled = view;
      scaled.distance = view.distance * std::max(0.05F, m_scenario_distance_scale);
      if (m_scenario_tilt_override.has_value()) {
        scaled.angle = *m_scenario_tilt_override;
      }
      scaled.yaw += m_scenario_yaw_offset;
      m_camera->set_rts_view(center, scaled.distance, scaled.angle, scaled.yaw);
      apply_scenario_camera_projection(scaled.distance);
      m_capture_orbit_center = center;
      m_capture_orbit_view = scaled;
      m_capture_orbit_yaw = 0.0F;
      m_capture_orbit_ready = true;
    }
  };
  host.sample_ai_doctrine = [this](int owner_id) -> Arena::ArenaAIDoctrineSample {
    Arena::ArenaAIDoctrineSample sample;
    auto* ai_system =
        m_world != nullptr ? m_world->get_system<Game::Systems::AISystem>() : nullptr;
    if (ai_system == nullptr) {
      return sample;
    }
    const auto state = ai_system->ai_player_state(owner_id);
    if (!state.valid) {
      return sample;
    }
    sample.valid = true;
    sample.strategy =
        Game::Systems::AI::AIStrategyFactory::strategy_to_string(state.strategy);
    sample.posture =
        Game::Systems::AI::AIStrategyFactory::posture_to_string(state.posture);
    sample.state = Game::Systems::AI::AIStrategyFactory::state_to_string(state.state);
    if (const auto* plan = ai_system->plan_for(owner_id); plan != nullptr) {
      sample.wave_committed = plan->wave.committed;
      sample.wave_size = static_cast<int>(plan->wave.members.size());
    }
    return sample;
  };

  host.set_force_full_creature_lod = [this](bool enabled) {
    set_force_full_creature_lod(enabled);
  };
  bind_rpg_scenario_controls(host);

  host.terrain = &m_session.terrain();
  host.building_collision = &m_session.building_collision();

  m_presentation_hitches_fired.clear();
  m_rpg_scripted_attack_ticks = 0;
  if (m_renderer != nullptr && m_batch_fixed_step > 0.0F) {
    m_renderer->reset_animation_time();
  }

  m_scenario_runner = std::make_unique<Arena::ArenaScenarioRunner>(
      *m_world, std::move(host), *definition, scenario_origin);
  const bool verify_frame_continuity = std::any_of(
      definition->expectations.begin(),
      definition->expectations.end(),
      [](const Arena::ArenaExpectation& expectation) {
        return expectation.kind == Arena::ArenaExpectationKind::NoFullscreenFlash;
      });
  if (verify_frame_continuity) {
    m_frame_continuity_analyzer =
        std::make_unique<Render::Profiling::FrameContinuityAnalyzer>();
  }
  if (m_scenario_duration_override > 0.0F) {
    m_scenario_runner->set_duration_limit(m_scenario_duration_override);
  }
  m_last_scenario_issue_revision = 0U;
  m_scenario_finished_emitted = false;
  set_force_full_creature_lod(definition->force_full_creature_lod);
  Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(
      Arena::scenario_needs_animation_diagnostics(*definition) ||
      m_force_animation_diagnostics);

  configure_scenario_rockfall_traps(*definition, scenario_origin);
  configure_scenario_rafts(*definition);
  if (!m_scenario_runner->start()) {
    qWarning().noquote() << QStringLiteral(
                                "Arena scenario '%1' failed validation or startup")
                                .arg(scenario_id);
    for (const auto& issue : m_scenario_runner->report().issues) {
      qWarning().noquote() << QStringLiteral("  %1: %2").arg(issue.code, issue.message);
    }
    m_scenario_runner.reset();
    return;
  }

  place_scenario_resource_patches(*definition, scenario_origin);
  if (m_prewarm_unit_templates && m_renderer != nullptr) {
    m_renderer->prewarm_unit_templates(m_world.get(), {});
  }
  m_session.terrain().set_supernatural_presence(
      definition->undead_zones.empty() ? 0.0F : 1.0F);
  reconfigure_terrain_from_state();
  configure_scenario_undead_zones(*definition, scenario_origin);
  m_scenario_origin = scenario_origin;
  rebuild_static_mist(*definition);

  if (has_scenario_ai) {
    configure_scenario_ai_profiles(*definition);
  }

  if (definition->select_spawned_units && !m_promo_mode) {
    select_spawned_entities(m_scenario_runner->all_entities());
  } else if (auto* selection = selection_system()) {
    selection->clear_selection();
  }
  update();
}

void ArenaViewport::apply_scenario_environment(
    const Arena::ArenaScenarioDefinition& scenario) {
  Render::GraphicsSettings::instance().set_quality(
      m_graphics_quality_override.value_or(scenario.graphics_quality));
  m_environment_definition = scenario.environment;
  m_environment_hour = scenario.environment.start_time;
  if (m_environment_hour_override.has_value()) {
    m_environment_hour = *m_environment_hour_override;
    m_environment_definition.start_time = m_environment_hour;
  }
  m_lighting_profile = scenario.environment.lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  m_rain_enabled = scenario.weather.rain > 0.0F || scenario.weather.storm > 0.0F ||
                   scenario.weather.snow > 0.0F;
  m_rain_intensity =
      std::max({scenario.weather.rain, scenario.weather.storm, scenario.weather.snow});
  m_weather_type = scenario.weather.snow > 0.0F ? Game::Map::WeatherType::Snow
                                                : Game::Map::WeatherType::Rain;
  m_weather_lighting = scenario.weather;
  m_weather_base = {};
  m_weather_base.rain = scenario.weather.rain;
  m_weather_base.storm = scenario.weather.storm;
  m_weather_base.snow = scenario.weather.snow;
  m_weather_base.wind_strength = scenario.precipitation.wind_strength;
  m_weather_base.wind_direction_deg = scenario.precipitation.wind_direction_deg;
  m_weather_state = {};
  m_scripted_fog_density.reset();
  m_scripted_exposure.reset();
  m_static_mist.clear();
  m_applied_fog_banks.clear();
  m_mist_dirty = true;
  if (m_rain != nullptr) {
    m_rain->set_enabled(m_rain_enabled);
    m_rain->set_intensity(m_rain_intensity);
    m_rain->set_weather_type(m_weather_type);
    m_rain->set_wind_strength(scenario.precipitation.wind_strength);
    m_rain->set_wind_direction_deg(scenario.precipitation.wind_direction_deg);
  }
}

auto ArenaViewport::apply_scenario_terrain(
    const Arena::ArenaScenarioDefinition& scenario) -> bool {
  m_arena_rivers = scenario.rivers;
  m_arena_lakes = scenario.lakes;
  m_arena_bridges = scenario.bridges;
  m_arena_roads = scenario.roads;
  m_arena_elevation_patches = scenario.elevation_patches;
  m_arena_terrain_features = scenario.terrain_features;
  m_arena_floor_half_extent = scenario.arena_floor_half_extent;
  m_terrain_grid_extent =
      scenario.terrain_grid_extent > 0 ? scenario.terrain_grid_extent : k_terrain_width;
  if (scenario.terrain_height_scale_override > 0.0F) {
    m_terrain_settings.height_scale = scenario.terrain_height_scale_override;
  }
  if (!scenario.ground_type.isEmpty()) {
    Game::Map::GroundType parsed = Game::Map::GroundType::ForestMud;
    if (Game::Map::try_parse_ground_type(scenario.ground_type, parsed)) {
      m_ground_type = parsed;
    } else {
      qWarning() << "Arena scenario" << scenario.id << "names unknown ground type"
                 << scenario.ground_type;
    }
  }
  if (scenario.terrain_seed_override > 0) {
    m_terrain_settings.seed = scenario.terrain_seed_override;
  }
  m_suppress_boundary_mountains = scenario.suppress_boundary_mountains;
  m_suppress_procedural_props = scenario.suppress_procedural_props;
  const bool snow_changed = m_terrain_snowbound != scenario.terrain_snowbound;
  m_terrain_snowbound = scenario.terrain_snowbound;
  const bool map_terrain = !scenario.campaign_map_path.isEmpty();
  if (map_terrain) {
    if (!initialize_terrain_from_map(scenario.campaign_map_path)) {
      qWarning() << "Arena scenario" << scenario.id << "cannot load its map"
                 << scenario.campaign_map_path;
    }
  } else if (snow_changed || m_terrain_snowbound || !m_arena_rivers.empty() ||
             !m_arena_lakes.empty() || !m_arena_bridges.empty() ||
             !m_arena_roads.empty() || !m_arena_elevation_patches.empty() ||
             m_terrain_grid_extent != k_terrain_width ||
             m_arena_floor_half_extent != k_default_floor_extent ||
             m_terrain_settings.height_scale != k_default_terrain_height_scale ||
             m_ground_type != m_ground_type_baseline ||
             m_terrain_settings.seed != m_terrain_seed_baseline ||
             m_suppress_boundary_mountains || m_suppress_procedural_props) {
    reconfigure_terrain_from_state();
  }
  return map_terrain;
}

auto ArenaViewport::register_scenario_owners(
    const Arena::ArenaScenarioDefinition& scenario) -> bool {
  auto& owners = m_session.owners();
  auto& nations = m_session.nations();
  auto& resources = m_session.economy();
  for (auto const& owner_team : scenario.owner_teams) {
    if (owners.get_owner_type(owner_team.owner_id) ==
        Game::Systems::OwnerType::Neutral) {
      owners.register_owner_with_id(
          owner_team.owner_id,
          Game::Systems::OwnerType::AI,
          QStringLiteral("Arena Ally %1").arg(owner_team.owner_id).toStdString());
    }
    owners.set_owner_team(owner_team.owner_id, owner_team.team_id);
  }
  bool has_scenario_ai = false;
  for (auto const& group : scenario.groups) {
    nations.set_player_nation(group.owner_id, group.nation_id);
    if (!group.ai_controlled) {
      continue;
    }
    has_scenario_ai = true;
    owners.register_owner_with_id(
        group.owner_id,
        Game::Systems::OwnerType::AI,
        QStringLiteral("Arena Economy AI %1").arg(group.owner_id).toStdString());
    owners.set_owner_team(group.owner_id, group.owner_id);
    nations.set_player_nation(group.owner_id, group.nation_id);
    resources.ensure_owner(group.owner_id);
    const auto& stock = scenario.ai_starting_resources;
    resources.set(group.owner_id, Game::Systems::ResourceType::Gold, stock.gold);
    resources.set(group.owner_id, Game::Systems::ResourceType::Food, stock.food);
    resources.set(group.owner_id, Game::Systems::ResourceType::Wood, stock.wood);
    resources.set(group.owner_id, Game::Systems::ResourceType::Stone, stock.stone);
    resources.set(group.owner_id, Game::Systems::ResourceType::Iron, stock.iron);
  }
  return has_scenario_ai;
}

auto ArenaViewport::spawn_scenario_group_entity(const Arena::ArenaScenarioGroup& group,
                                                const QVector3D& requested_position)
    -> Engine::Core::EntityID {
  const bool building_group =
      group.spawn_type.has_value() && Game::Units::is_building_spawn(*group.spawn_type);
  QVector3D const position =
      building_group ? requested_position
                     : App::Utils::snap_to_walkable_ground(requested_position);
  Engine::Core::EntityID const entity_id =
      building_group ? spawn_single_building(group.owner_id,
                                             group.nation_id,
                                             *group.spawn_type,
                                             position,
                                             group.ai_controlled,
                                             group.max_population)
                     : spawn_single_unit(group.owner_id,
                                         group.nation_id,
                                         group.troop_type,
                                         position,
                                         group.ai_controlled,
                                         group.keep_troop_speed);
  auto* entity = m_world != nullptr ? m_world->get_entity(entity_id) : nullptr;
  auto* transform = entity != nullptr
                        ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto* unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
  if (transform != nullptr && group.render_scale_override > 0.0F) {
    transform->scale = {group.render_scale_override,
                        group.render_scale_override,
                        group.render_scale_override};
  }
  if (transform != nullptr) {
    transform->rotation.y = group.facing_degrees;
    transform->desired_yaw = group.facing_degrees;
    transform->has_desired_yaw = true;
  }
  if (building_group && entity_id != 0U) {
    auto& collision = m_session.building_collision();
    collision.resize_building(
        entity_id,
        Game::Systems::BuildingCollisionRegistry::axis_aligned_size(
            Game::Systems::BuildingCollisionRegistry::get_building_size(
                Game::Units::spawn_typeToQString(*group.spawn_type).toStdString()),
            group.facing_degrees));
  }
  if (unit != nullptr) {
    unit->render_individuals_per_unit_override = group.individuals_per_unit;
    unit->render_rider = group.render_rider;
    if (group.max_health_override > 0) {
      unit->max_health = group.max_health_override;
    }
    if (group.health_override > 0) {
      unit->health = std::min(group.health_override, std::max(1, unit->max_health));
    }
  }
  if (entity != nullptr &&
      (group.attack_range_override > 0.0F || group.attack_min_range_override > 0.0F ||
       group.attacks_disabled)) {
    if (auto* attack = entity->get_component<Engine::Core::AttackComponent>()) {
      if (group.attack_range_override > 0.0F) {
        attack->range = group.attack_range_override;
      }
      if (group.attack_min_range_override > 0.0F) {
        attack->min_range = group.attack_min_range_override;
      }
      if (group.attacks_disabled) {
        attack->can_melee = false;
        attack->can_ranged = false;
        attack->cooldown = std::numeric_limits<float>::max();
        attack->melee_cooldown = std::numeric_limits<float>::max();
        attack->time_since_last = 0.0F;
        attack->in_melee_lock = false;
        attack->melee_lock_target_id = 0U;
      }
    }
  }
  if (!group.renderer_override.isEmpty() && entity != nullptr) {
    if (auto* renderable = entity->get_component<Engine::Core::RenderableComponent>()) {
      renderable->renderer_id = group.renderer_override.toStdString();
    }
  }
  if (!group.showcase_routine.isEmpty() && entity != nullptr) {
    auto* routine = entity->add_component<Engine::Core::ShowcaseRoutineComponent>();
    routine->loop = group.showcase_loop;
    routine->loop_from =
        static_cast<std::size_t>(std::max(0, group.showcase_loop_from));
    routine->start_delay = group.showcase_start_delay;
    for (const QString& entry : group.showcase_routine) {
      QStringList const parts = entry.split(QLatin1Char(':'));
      auto const move = Animation::humanoid_showcase_move_from_name(
          parts.value(0).trimmed().toStdString());
      if (move == Animation::HumanoidShowcaseMove::None) {
        continue;
      }
      Engine::Core::ShowcaseRoutineComponent::Step step{};
      step.move = static_cast<std::uint8_t>(move);
      step.duration = parts.size() > 1 ? parts.at(1).toFloat() : 0.0F;
      step.hold_after = parts.size() > 2 ? parts.at(2).toFloat() : 0.0F;
      routine->steps.push_back(step);
    }
    if (!group.showcase_released_renderer.isEmpty()) {
      routine->armed_renderer_id = group.renderer_override.toStdString();
      routine->released_renderer_id = group.showcase_released_renderer.toStdString();
    }
    if (group.showcase_throw_target.has_value()) {
      routine->has_throw_target = true;
      routine->throw_target_x = group.showcase_throw_target->x();
      routine->throw_target_z = group.showcase_throw_target->z();
    }
  }
  if (group.settlement_resident && entity != nullptr && transform != nullptr) {
    auto* resident = entity->add_component<Engine::Core::SettlementResidentComponent>();
    resident->hearth_x = transform->position.x;
    resident->hearth_z = transform->position.z;
    resident->hearth_assigned = true;
    resident->roam_radius = group.settlement_roam_radius;
  }
  return entity_id;
}

void ArenaViewport::configure_scenario_ai_profiles(
    const Arena::ArenaScenarioDefinition& scenario) {
  if (auto* ai_system = m_world->get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();
    QSet<int> configured_owners;
    for (const auto& group : scenario.groups) {
      if (!group.ai_controlled || configured_owners.contains(group.owner_id)) {
        continue;
      }
      configured_owners.insert(group.owner_id);
      ai_system->set_ai_profile(
          group.owner_id,
          resolve_scenario_ai_profile(scenario, group.owner_id, group.nation_id));
    }
  }
}

void ArenaViewport::update_active_scenario(float simulation_dt) {
  if (m_scenario_runner != nullptr && simulation_dt > 0.0F) {
    m_scenario_runner->update(simulation_dt);
  }
}
