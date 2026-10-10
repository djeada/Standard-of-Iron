#include <QDebug>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "arena_scenario.h"
#include "arena_viewport.h"
#include "arena_viewport_internal.h"
#include "game/core/ownership_constants.h"
#include "game/game_config.h"
#include "game/map/biome_settings.h"
#include "game/map/map_loader.h"
#include "game/map/terrain.h"
#include "game/map/terrain_noise.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/wall_network_service.h"
#include "game/systems/owner_registry.h"
#include "game/units/spawn_type.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"
#include "scene/camera.h"
#include "utils/resource_utils.h"

using namespace arena_viewport_internal;

namespace {

auto classify_terrain(float normalized_height) -> Game::Map::TerrainType {
  if (normalized_height > 0.72F) {
    return Game::Map::TerrainType::Mountain;
  }
  if (normalized_height > 0.28F) {
    return Game::Map::TerrainType::Hill;
  }
  return Game::Map::TerrainType::Flat;
}

} // namespace

auto ArenaViewport::terrain_review_max_camera_distance() const -> float {
  if (!m_terrain_review_definition.has_value()) {
    return 85.0F;
  }
  const auto& definition = *m_terrain_review_definition;
  const float world_width =
      static_cast<float>(definition.grid.width) * definition.grid.tile_size;
  const float world_height =
      static_cast<float>(definition.grid.height) * definition.grid.tile_size;
  return std::max(85.0F, std::hypot(world_width, world_height) * 1.35F);
}

void ArenaViewport::regenerate_terrain() {
  if (m_terrain_from_map && m_terrain_review_definition.has_value()) {
    qInfo() << "Arena: regenerate_terrain keeps the loaded map terrain";
    apply_map_terrain();
    update();
    return;
  }
  std::vector<float> heights(
      static_cast<size_t>(m_terrain_grid_extent * m_terrain_grid_extent), 0.0F);
  std::vector<Game::Map::TerrainType> terrain_types(heights.size(),
                                                    Game::Map::TerrainType::Flat);

  float const half_width = static_cast<float>(m_terrain_grid_extent) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(m_terrain_grid_extent) * 0.5F - 0.5F;
  float const safe_scale = std::max(0.0F, m_terrain_settings.height_scale);
  Game::Map::MountainNoiseSettings const noise_settings{
      static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed)),
      m_terrain_settings.frequency,
      m_terrain_settings.octaves};
  float max_height = 0.0F;

  for (int z = 0; z < m_terrain_grid_extent; ++z) {
    for (int x = 0; x < m_terrain_grid_extent; ++x) {
      float const world_x = (static_cast<float>(x) - half_width) * k_terrain_tile_size;
      float const world_z = (static_cast<float>(z) - half_height) * k_terrain_tile_size;
      float const radial =
          std::clamp(1.0F - QVector3D(world_x, 0.0F, world_z).length() /
                                (static_cast<float>(m_terrain_grid_extent) * 0.72F),
                     0.35F,
                     1.0F);
      float const blended =
          Game::Map::sample_mountain_region(world_x, world_z, noise_settings);
      float const height = safe_scale * blended * radial;
      auto const index = static_cast<size_t>(z * m_terrain_grid_extent + x);
      heights[index] = height;
      max_height = std::max(max_height, height);
    }
  }

  float const normalization = std::max(0.001F, max_height);
  for (size_t i = 0; i < heights.size(); ++i) {
    terrain_types[i] = classify_terrain(heights[i] / normalization);
  }

  float const arena_floor_height =
      std::min(safe_scale * 0.18F, std::max(0.0F, max_height * 0.25F));
  float const arena_half_extent =
      std::clamp(m_arena_floor_half_extent, 4.0F, half_width - 2.0F);

  float const taper = std::min(12.0F, half_width - arena_half_extent);
  for (int z = 0; z < m_terrain_grid_extent; ++z) {
    for (int x = 0; x < m_terrain_grid_extent; ++x) {
      float const world_x = (static_cast<float>(x) - half_width) * k_terrain_tile_size;
      float const world_z = (static_cast<float>(z) - half_height) * k_terrain_tile_size;
      float const edge = std::max(std::abs(world_x), std::abs(world_z));
      if (edge > arena_half_extent + taper) {
        continue;
      }

      float blend = 0.0F;
      if (taper > 0.0F && edge > arena_half_extent) {
        blend = std::clamp((edge - arena_half_extent) / taper, 0.0F, 1.0F);
        blend = blend * blend * (3.0F - (2.0F * blend));
      }
      auto const index = static_cast<size_t>(z * m_terrain_grid_extent + x);
      heights[index] = std::lerp(arena_floor_height, heights[index], blend);
      if (blend < 0.5F) {
        terrain_types[index] = Game::Map::TerrainType::Flat;
      }
    }
  }

  for (auto const& patch : m_arena_elevation_patches) {
    float const radius = std::max(patch.radius, k_terrain_tile_size);
    for (int z = 0; z < m_terrain_grid_extent; ++z) {
      for (int x = 0; x < m_terrain_grid_extent; ++x) {
        float const world_x =
            (static_cast<float>(x) - half_width) * k_terrain_tile_size;
        float const world_z =
            (static_cast<float>(z) - half_height) * k_terrain_tile_size;
        float const distance =
            std::hypot(world_x - patch.center.x(), world_z - patch.center.z());
        if (distance > radius) {
          continue;
        }
        float const inner = std::clamp(patch.plateau, 0.0F, radius * 0.95F);
        float t = distance <= inner
                      ? 1.0F
                      : std::clamp(
                            1.0F - ((distance - inner) / (radius - inner)), 0.0F, 1.0F);
        t = t * t * (3.0F - 2.0F * t);
        auto const index = static_cast<std::size_t>(z * m_terrain_grid_extent + x);
        heights[index] =
            std::max(heights[index], arena_floor_height + patch.height * t);
        terrain_types[index] = Game::Map::TerrainType::Flat;
      }
    }
  }

  Game::Map::BiomeSettings biome;
  Game::Map::apply_ground_type_defaults(biome, m_ground_type);
  if (m_suppress_procedural_props) {
    biome.procedural_boulders_enabled = false;
    biome.procedural_iron_ore_enabled = false;
    biome.procedural_trees_enabled = false;
  }
  if (m_terrain_snowbound) {

    biome.grass_primary = QVector3D(0.64F, 0.70F, 0.80F);
    biome.grass_secondary = QVector3D(0.72F, 0.77F, 0.86F);
    biome.grass_dry = QVector3D(0.56F, 0.61F, 0.71F);
    biome.soil_color = QVector3D(0.40F, 0.43F, 0.50F);
    biome.snow_color = QVector3D(0.74F, 0.80F, 0.90F);
    biome.snow_coverage = 1.0F;
    biome.plant_density *= 0.15F;
  }
  biome.seed = static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed));
  biome.height_noise_frequency = m_terrain_settings.frequency;
  biome.height_noise_amplitude =
      std::clamp(m_terrain_settings.height_scale * 0.05F, 0.05F, 1.25F);

  Game::Map::HillNavigation hills;
  if (!m_arena_terrain_features.empty()) {
    Game::Map::TerrainHeightMap featured(
        m_terrain_grid_extent, m_terrain_grid_extent, k_terrain_tile_size);
    featured.restore_from_data(heights, terrain_types, {}, {});
    featured.build_from_features(m_arena_terrain_features);
    heights = featured.get_height_data();
    terrain_types = featured.getTerrainTypes();
    hills = featured.hill_navigation();
  }

  Game::Map::TerrainHeightMap water_mask(
      m_terrain_grid_extent, m_terrain_grid_extent, k_terrain_tile_size);
  water_mask.restore_from_data(heights, terrain_types, {}, {}, {}, hills);
  water_mask.add_lakes(m_arena_lakes);
  water_mask.add_river_segments(m_arena_rivers);
  water_mask.add_fords(m_arena_fords);
  water_mask.add_bridges(m_arena_bridges);
  heights = water_mask.get_height_data();
  terrain_types = water_mask.getTerrainTypes();
  hills = water_mask.hill_navigation();
  const auto runtime_rivers = water_mask.get_river_segments();
  const auto runtime_lakes = water_mask.get_lakes();
  const auto runtime_bridges = water_mask.get_bridges();
  const auto runtime_fords = water_mask.get_fords();

  m_session.visibility().initialize(
      m_terrain_grid_extent, m_terrain_grid_extent, k_terrain_tile_size);
  m_session.terrain().restore_from_serialized(m_terrain_grid_extent,
                                              m_terrain_grid_extent,
                                              k_terrain_tile_size,
                                              heights,
                                              terrain_types,
                                              runtime_rivers,
                                              m_arena_roads,
                                              runtime_bridges,
                                              biome,
                                              m_world_props,
                                              {},
                                              runtime_lakes,
                                              hills,
                                              runtime_fords);
  Game::Systems::NavGrid::initialize(m_terrain_grid_extent, m_terrain_grid_extent);
  apply_initial_visibility();
  sync_camera_map_bounds(m_camera.get(), m_session.visibility());

  align_units_to_terrain();
  if (m_gl_initialized) {
    configure_rendering_from_terrain();
  }
  update();
}

void ArenaViewport::reconfigure_terrain_from_state() {
  regenerate_terrain();
}

void ArenaViewport::configure_rendering_from_terrain() {
  auto& terrain_service = m_session.terrain();
  const auto* height_map = terrain_service.get_height_map();
  if (height_map == nullptr || m_surface == nullptr || m_features == nullptr ||
      m_scatter == nullptr) {
    return;
  }

  m_surface->ground()->configure(
      height_map->get_tile_size(), height_map->get_width(), height_map->get_height());
  m_surface->ground()->set_biome(terrain_service.biome_settings());
  m_surface->terrain()->configure(*height_map, terrain_service.biome_settings());
  m_features->configure(
      *height_map, terrain_service.road_segments(), terrain_service.biome_settings());
  if (m_terrain_review_mode && !m_terrain_review_content_enabled) {
    m_scatter->clear();
  } else {
    m_scatter->configure(*height_map,
                         terrain_service.biome_settings(),
                         terrain_service.authored_world_props(),
                         terrain_service.world_props());
  }
  if (m_rain != nullptr) {
    float const world_width =
        static_cast<float>(height_map->get_width()) * height_map->get_tile_size();
    float const world_height =
        static_cast<float>(height_map->get_height()) * height_map->get_tile_size();
    m_rain->configure(world_width,
                      world_height,
                      static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed)),
                      m_weather_type);
  }
  if (m_boundary_fog != nullptr) {

    if (m_suppress_boundary_mountains) {
      m_boundary_fog->configure(0, 0, height_map->get_tile_size());
    } else {
      m_boundary_fog->configure(height_map->get_width(),
                                height_map->get_height(),
                                height_map->get_tile_size());
    }
  }
  m_renderer->set_environment_lighting(active_lighting());
  set_wireframe_enabled(m_wireframe_enabled);
}

void ArenaViewport::set_time_of_day(Game::Map::TimeOfDay time_of_day) {
  m_time_of_day = time_of_day;
  m_environment_hour = Game::Map::hour_for_time_of_day(time_of_day);

  m_environment_definition.start_time = m_environment_hour;
  m_environment_definition.lighting_profile = m_lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
  emit lighting_changed(lighting_summary());
  update();
}

void ArenaViewport::set_environment_time(float hour) {
  m_environment_hour = Game::Map::normalize_hour(hour);
  m_environment_definition.start_time = m_environment_hour;
  m_environment_definition.lighting_profile = m_lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  m_time_of_day = Game::Map::time_of_day_for_hour(m_environment_hour);
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
  emit lighting_changed(lighting_summary());
  update();
}

void ArenaViewport::set_environment_hour_override(float hour) {
  m_environment_hour_override = Game::Map::normalize_hour(hour);
  set_environment_time(*m_environment_hour_override);
}

void ArenaViewport::set_lighting_profile(const QString& profile) {
  m_lighting_profile = profile.trimmed().isEmpty()
                           ? QStringLiteral("mediterranean_summer")
                           : profile.trimmed();
  m_environment_definition.start_time = m_environment_hour;
  m_environment_definition.lighting_profile = m_lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
  emit lighting_changed(lighting_summary());
  update();
}

void ArenaViewport::apply_environment_definition(
    const Game::Map::EnvironmentDefinition& environment) {
  m_environment_definition = environment;
  m_lighting_profile = environment.lighting_profile.trimmed().isEmpty()
                           ? QStringLiteral("mediterranean_summer")
                           : environment.lighting_profile;
  m_environment_definition.lighting_profile = m_lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  m_environment_hour = m_environment_clock.hour();
  m_time_of_day = Game::Map::time_of_day_for_hour(m_environment_hour);
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
  emit lighting_changed(lighting_summary());
}

void ArenaViewport::set_time_mode(const QString& mode) {
  m_environment_definition.start_time = m_environment_hour;
  m_environment_definition.time_mode = Game::Map::parse_time_mode(mode);
  m_environment_definition.lighting_profile = m_lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  emit lighting_changed(lighting_summary());
}

void ArenaViewport::set_day_length(float seconds) {
  m_environment_definition.start_time = m_environment_hour;
  m_environment_definition.day_length_seconds = std::max(1.0F, seconds);
  m_environment_definition.lighting_profile = m_lighting_profile;
  m_environment_clock.reset(m_environment_definition);
  emit lighting_changed(lighting_summary());
}

void ArenaViewport::set_shadow_quality(const QString& quality) {
  const QString normalized = quality.trimmed().toLower();
  Render::GraphicsQuality selected = Render::GraphicsQuality::High;
  if (normalized == QStringLiteral("low")) {
    selected = Render::GraphicsQuality::Low;
  } else if (normalized == QStringLiteral("medium")) {
    selected = Render::GraphicsQuality::Medium;
  } else if (normalized == QStringLiteral("ultra")) {
    selected = Render::GraphicsQuality::Ultra;
  }
  Render::GraphicsSettings::instance().set_quality(selected);
  update();
}

auto ArenaViewport::active_lighting() const -> Game::Map::EnvironmentLightingState {
  auto weather = m_weather_lighting;
  const float active = m_rain_enabled ? m_rain_intensity : 0.0F;
  if (m_weather_type == Game::Map::WeatherType::Snow) {
    weather.snow = active;
    weather.rain = 0.0F;
  } else {
    weather.rain = active;
    weather.snow = 0.0F;
  }
  const float hour = m_promo_lighting.hour.value_or(m_environment_hour);
  auto lighting = Game::Map::lighting_for_hour(hour, m_lighting_profile, weather);

  if (m_environment_definition.fog_density_override >= 0.0F) {
    lighting.fog_density = m_environment_definition.fog_density_override;
  }
  if (m_environment_definition.exposure_override >= 0.0F) {
    lighting.exposure = m_environment_definition.exposure_override;
  }
  if (m_scripted_fog_density.has_value()) {
    lighting.fog_density = *m_scripted_fog_density;
  }
  if (m_scripted_exposure.has_value()) {
    lighting.exposure = *m_scripted_exposure;
  }
  const auto& look = m_promo_lighting;
  if (look.sun_azimuth.has_value() || look.sun_elevation.has_value()) {
    const QVector3D current = lighting.primary_direction.normalized();
    const float current_elevation =
        qRadiansToDegrees(std::asin(std::clamp(current.y(), -1.0F, 1.0F)));
    const float current_azimuth =
        qRadiansToDegrees(std::atan2(current.x(), current.z()));
    const float azimuth = qDegreesToRadians(look.sun_azimuth.value_or(current_azimuth));
    const float elevation =
        qDegreesToRadians(look.sun_elevation.value_or(current_elevation));
    lighting.primary_direction = QVector3D(std::cos(elevation) * std::sin(azimuth),
                                           std::sin(elevation),
                                           std::cos(elevation) * std::cos(azimuth));
  }
  if (look.sun_scale) {
    lighting.primary_intensity *= *look.sun_scale;
  }
  if (look.sun_color) {
    lighting.primary_color = *look.sun_color;
  }
  if (look.ambient_scale) {
    lighting.ambient_intensity *= *look.ambient_scale;
  }
  if (look.sky_color) {
    lighting.sky_color = *look.sky_color;
  }
  if (look.fog_color) {
    lighting.fog_color = *look.fog_color;
  }
  if (look.fog_density) {
    lighting.fog_density = *look.fog_density;
  }
  if (look.exposure) {
    lighting.exposure = *look.exposure;
  }
  if (look.shadow_strength) {
    lighting.shadow_strength = *look.shadow_strength;
  }
  if (look.shadow_softness) {
    lighting.shadow_softness = *look.shadow_softness;
  }
  return lighting.sanitized();
}

auto ArenaViewport::lighting_summary() const -> QString {
  const auto lighting = active_lighting();
  return QStringLiteral("%1 %2h · %3 · sun (%4,%5,%6) ×%7 · ambient %8 · fog %9 · "
                        "shadow %10/%11 · exposure %12 · cloud %13 · wet %14")
      .arg(QString::fromLatin1(Game::Map::time_of_day_name(m_time_of_day)))
      .arg(QString::number(m_environment_hour, 'f', 2))
      .arg(m_lighting_profile)
      .arg(QString::number(lighting.primary_direction.x(), 'f', 2))
      .arg(QString::number(lighting.primary_direction.y(), 'f', 2))
      .arg(QString::number(lighting.primary_direction.z(), 'f', 2))
      .arg(QString::number(lighting.primary_intensity, 'f', 2))
      .arg(QString::number(lighting.ambient_intensity, 'f', 2))
      .arg(QString::number(lighting.fog_density, 'f', 3))
      .arg(QString::number(lighting.shadow_strength, 'f', 2))
      .arg(QString::number(lighting.shadow_softness, 'f', 2))
      .arg(QString::number(lighting.exposure, 'f', 2))
      .arg(QString::number(lighting.cloud_cover, 'f', 2))
      .arg(QString::number(lighting.wetness, 'f', 2));
}

void ArenaViewport::set_terrain_seed(int seed) {
  m_terrain_settings.seed = std::max(0, seed);
  m_terrain_seed_baseline = m_terrain_settings.seed;
}

void ArenaViewport::set_terrain_height_scale(float value) {
  m_terrain_settings.height_scale = std::max(0.0F, value);
}

void ArenaViewport::set_terrain_octaves(int value) {
  m_terrain_settings.octaves = Game::Map::clamp_noise_octaves(value);
}

void ArenaViewport::set_terrain_frequency(float value) {
  m_terrain_settings.frequency = std::clamp(value, 0.01F, 2.0F);
}

void ArenaViewport::set_wireframe_enabled(bool enabled) {
  m_wireframe_enabled = enabled;
  if (m_terrain_scene != nullptr && m_terrain_scene->terrain() != nullptr) {
    m_terrain_scene->terrain()->set_wireframe(enabled);
  }
  update();
}

void ArenaViewport::set_normals_overlay_enabled(bool enabled) {
  m_normals_overlay_enabled = enabled;
  update();
}

void ArenaViewport::set_ground_type(const QString& ground_type) {
  Game::Map::GroundType parsed = Game::Map::GroundType::ForestMud;
  if (!Game::Map::try_parse_ground_type(ground_type, parsed)) {
    qWarning() << "ArenaViewport: unknown ground type" << ground_type
               << "- defaulting to ForestMud";
  }
  m_ground_type_baseline = parsed;
  if (m_ground_type == parsed) {
    return;
  }
  m_ground_type = parsed;
  regenerate_terrain();
}

void ArenaViewport::set_rain_enabled(bool enabled) {
  m_rain_enabled = enabled;
  if (!enabled) {
    m_weather_lighting.rain = 0.0F;
    m_weather_lighting.storm = 0.0F;
    m_weather_lighting.snow = 0.0F;
  }
  if (m_rain != nullptr) {
    m_rain->set_enabled(enabled);
  }
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
  emit lighting_changed(lighting_summary());
  update();
}

void ArenaViewport::set_rain_intensity(float intensity) {
  m_rain_intensity = std::clamp(intensity, 0.0F, 1.0F);
  if (m_weather_type == Game::Map::WeatherType::Snow) {
    m_weather_lighting.snow = m_rain_intensity;
  } else {
    m_weather_lighting.rain = m_rain_intensity;
  }
  if (m_rain != nullptr) {
    m_rain->set_intensity(m_rain_intensity);
  }
  if (m_renderer != nullptr) {
    m_renderer->set_environment_lighting(active_lighting());
  }
  emit lighting_changed(lighting_summary());
  update();
}

void ArenaViewport::apply_initial_visibility() {
  auto& visibility = m_session.visibility();
  if (!m_fog_of_war_enabled) {
    visibility.reveal_all();
    return;
  }

  visibility.compute_immediate(*m_world, k_local_owner_id);
  if (m_fog != nullptr) {
    m_fog->set_enabled(true);
    const auto snapshot = visibility.snapshot();
    m_fog->update_mask(
        snapshot.width, snapshot.height, snapshot.tile_size, snapshot.cells);
  }
}

void ArenaViewport::set_fog_of_war_enabled(bool enabled) {
  if (m_fog_of_war_enabled == enabled) {
    return;
  }
  m_fog_of_war_enabled = enabled;
  m_visibility_accumulator = 0.0F;

  auto& visibility = m_session.visibility();
  if (!enabled) {
    if (m_fog != nullptr) {
      m_fog->set_enabled(false);
      m_fog->update_mask(0, 0, 1.0F, {});
    }
    apply_initial_visibility();
    update();
    return;
  }

  visibility.initialize(
      visibility.get_width(), visibility.get_height(), visibility.get_tile_size());
  apply_initial_visibility();
  update();
}

void ArenaViewport::update_fog_of_war(float dt) {
  if (!m_fog_of_war_enabled || m_fog == nullptr) {
    return;
  }
  auto& visibility = m_session.visibility();
  if (!visibility.is_initialized()) {
    return;
  }

  const float interval =
      Game::GameConfig::instance().gameplay().visibility_update_interval;
  m_visibility_accumulator += dt;
  if (m_visibility_accumulator >= interval) {
    m_visibility_accumulator = 0.0F;
    visibility.update(*m_world, k_local_owner_id);
  }

  const auto snapshot = visibility.snapshot();
  m_fog->update_mask(
      snapshot.width, snapshot.height, snapshot.tile_size, snapshot.cells);
}

auto ArenaViewport::terrain_height_at(float x, float z) const -> float {
  const auto& terrain = m_session.terrain();
  if (terrain.terrain_field().empty()) {
    return 0.0F;
  }
  return terrain.get_terrain_height(x, z);
}

auto ArenaViewport::environment_snapshot() const -> Arena::ArenaEnvironmentSnapshot {
  const auto lighting = active_lighting();
  const auto& settings = Render::GraphicsSettings::instance();
  const auto& shadows = settings.directional_shadows();

  Arena::ArenaEnvironmentSnapshot snapshot;
  snapshot.valid = true;
  snapshot.hour = m_environment_hour;
  snapshot.time_of_day =
      QString::fromLatin1(Game::Map::time_of_day_name(m_time_of_day));
  snapshot.time_mode = QString::fromLatin1(
      Game::Map::time_mode_name(m_environment_definition.time_mode));
  snapshot.lighting_profile = m_lighting_profile;
  snapshot.shadow_quality = [](Render::GraphicsQuality quality) -> QString {
    switch (quality) {
    case Render::GraphicsQuality::Low:
      return QStringLiteral("Low");
    case Render::GraphicsQuality::Medium:
      return QStringLiteral("Medium");
    case Render::GraphicsQuality::High:
      return QStringLiteral("High");
    case Render::GraphicsQuality::Ultra:
      return QStringLiteral("Ultra");
    }
    return QStringLiteral("Unknown");
  }(settings.quality());
  snapshot.directional_shadows_enabled = shadows.enabled;
  snapshot.shadow_resolution = shadows.resolution;
  snapshot.shadow_cascades = shadows.cascade_count;
  snapshot.shadow_distance = shadows.distance;
  snapshot.contact_shadow_casters = settings.contact_shadow_budget().max_casters;
  snapshot.primary_direction = lighting.primary_direction;
  snapshot.primary_color = lighting.primary_color;
  snapshot.sky_color = lighting.sky_color;
  snapshot.primary_intensity = lighting.primary_intensity;
  snapshot.ambient_intensity = lighting.ambient_intensity;
  snapshot.exposure = lighting.exposure;
  snapshot.fog_density = lighting.fog_density;
  snapshot.cloud_cover = lighting.cloud_cover;
  snapshot.wetness = lighting.wetness;
  return snapshot;
}

auto ArenaViewport::load_terrain_review_map(const QString& map_path,
                                            QString* error) -> bool {
  Game::Map::MapDefinition definition;
  const QString resolved_path = Utils::Resources::resolve_resource_path(map_path);
  if (!Game::Map::MapLoader::load_from_json_file(resolved_path, definition, error)) {
    return false;
  }

  reset_arena();
  clear_camera_key_state();
  m_terrain_review_mode = true;
  m_terrain_from_map = true;
  m_terrain_review_definition = std::move(definition);
  apply_environment_definition(m_terrain_review_definition->environment);
  if (m_renderer != nullptr) {
    m_renderer->set_clear_color(0.055F, 0.065F, 0.05F, 1.0F);
  }

  auto& terrain = m_session.terrain();
  terrain.initialize(*m_terrain_review_definition);
  m_session.visibility().initialize(m_terrain_review_definition->grid.width,
                                    m_terrain_review_definition->grid.height,
                                    m_terrain_review_definition->grid.tile_size);
  apply_initial_visibility();
  sync_camera_map_bounds(m_camera.get(), m_session.visibility());
  Game::Systems::NavGrid::initialize(m_terrain_review_definition->grid.width,
                                     m_terrain_review_definition->grid.height);

  if (m_terrain_review_content_enabled) {
    spawn_terrain_review_structures();
  }

  if (m_gl_initialized) {
    configure_rendering_from_terrain();
  }
  set_terrain_review_gameplay_camera();
  update();
  return true;
}

auto ArenaViewport::initialize_terrain_from_map(const QString& map_path) -> bool {
  Game::Map::MapDefinition definition;
  QString error;
  if (!Game::Map::MapLoader::load_from_json_file(
          Utils::Resources::resolve_resource_path(map_path), definition, &error)) {
    qWarning() << "Arena: cannot read map" << map_path << ":" << error;
    return false;
  }
  // Scenario fords (a battle script's crossing) sit on the map's own rivers.
  definition.fords.insert(
      definition.fords.end(), m_arena_fords.begin(), m_arena_fords.end());
  m_terrain_from_map = true;
  m_terrain_review_definition = std::move(definition);
  apply_map_terrain();
  return true;
}

void ArenaViewport::apply_map_terrain() {
  const auto& map = *m_terrain_review_definition;
  m_terrain_grid_extent = map.grid.width;

  m_session.terrain().initialize(map);
  m_session.visibility().initialize(
      map.grid.width, map.grid.height, map.grid.tile_size);
  Game::Systems::NavGrid::initialize(map.grid.width, map.grid.height);
  apply_initial_visibility();
  sync_camera_map_bounds(m_camera.get(), m_session.visibility());
  if (m_gl_initialized) {
    configure_rendering_from_terrain();
  }
}

void ArenaViewport::set_terrain_review_content_enabled(bool enabled) {
  m_terrain_review_content_enabled = enabled;
}

void ArenaViewport::spawn_terrain_review_structures() {
  if (!m_terrain_review_definition.has_value()) {
    return;
  }

  const auto& definition = *m_terrain_review_definition;
  auto& owners = m_session.owners();
  auto& nations = m_session.nations();
  const auto resolve_nation =
      [&owners, &nations](int owner_id,
                          std::optional<Game::Systems::NationID> authored_nation) {
        if (owner_id != Game::Core::NEUTRAL_OWNER_ID &&
            owners.get_owner_type(owner_id) == Game::Systems::OwnerType::Neutral) {
          owners.register_owner_with_id(owner_id,
                                        Game::Systems::OwnerType::AI,
                                        "Preview Player " + std::to_string(owner_id));
          owners.set_owner_team(owner_id, owner_id);
        }

        Game::Systems::NationID nation_id = nations.default_nation_id();
        if (authored_nation.has_value()) {
          nation_id = *authored_nation;
        } else if (const auto* owner_nation = nations.get_nation_for_player(owner_id)) {
          nation_id = owner_nation->id;
        }
        if (owner_id != Game::Core::NEUTRAL_OWNER_ID) {
          nations.set_player_nation(owner_id, nation_id);
        }
        return nation_id;
      };

  const auto parse_nation =
      [](const QString& value) -> std::optional<Game::Systems::NationID> {
    if (value.trimmed().isEmpty()) {
      return std::nullopt;
    }
    Game::Systems::NationID nation_id;
    if (!Game::Systems::try_parse_nation_id(value, nation_id)) {
      qWarning() << "Arena terrain review: unknown building nation" << value
                 << "- using the owner/default nation";
      return std::nullopt;
    }
    return nation_id;
  };

  constexpr float k_grid_center_offset = 0.5F;
  const auto grid_offset = [](int grid_size) {
    return -(static_cast<float>(grid_size) * k_grid_center_offset -
             k_grid_center_offset);
  };
  const auto world_to_grid = [&grid_offset](float world_coord, int grid_size) {
    return static_cast<int>(std::round(world_coord - grid_offset(grid_size)));
  };
  const auto grid_to_world = [&grid_offset](int grid_coord, int grid_size) {
    return static_cast<float>(grid_coord) + grid_offset(grid_size);
  };

  for (const auto& structure : definition.structures) {
    if (const auto* point =
            std::get_if<Game::Map::PointStructureGeometry>(&structure.geometry)) {
      spawn_single_building(
          structure.player_id,
          resolve_nation(structure.player_id, parse_nation(structure.nation)),
          structure.type,
          point->position,
          false,
          0,
          structure.rotation);
      continue;
    }

    const auto* line =
        std::get_if<Game::Map::LineStructureGeometry>(&structure.geometry);
    if (line == nullptr || structure.type != Game::Units::SpawnType::WallSegment) {
      qWarning() << "Arena terrain review: unsupported line structure"
                 << Game::Units::spawn_typeToQString(structure.type);
      continue;
    }
    const auto start = Game::Systems::WallGridPosition{
        .x = Game::Systems::WallNetworkService::snap_grid_coordinate(
            world_to_grid(line->start.x(), definition.grid.width)),
        .z = Game::Systems::WallNetworkService::snap_grid_coordinate(
            world_to_grid(line->start.z(), definition.grid.height))};
    const auto end = Game::Systems::WallGridPosition{
        .x = Game::Systems::WallNetworkService::snap_grid_coordinate(
            world_to_grid(line->end.x(), definition.grid.width)),
        .z = Game::Systems::WallNetworkService::snap_grid_coordinate(
            world_to_grid(line->end.z(), definition.grid.height))};
    const auto nation_id =
        resolve_nation(structure.player_id, parse_nation(structure.nation));
    for (const auto& segment :
         Game::Systems::WallNetworkService::build_axis_aligned_chain(start, end)) {
      spawn_single_building(structure.player_id,
                            nation_id,
                            Game::Units::SpawnType::WallSegment,
                            QVector3D(grid_to_world(segment.x, definition.grid.width),
                                      0.0F,
                                      grid_to_world(segment.z, definition.grid.height)),
                            false);
    }
  }
}

void ArenaViewport::set_terrain_review_overview_camera() {
  if (m_camera == nullptr || !m_terrain_review_definition.has_value()) {
    return;
  }
  const auto& definition = *m_terrain_review_definition;
  const float world_width =
      static_cast<float>(definition.grid.width) * definition.grid.tile_size;
  const float world_height =
      static_cast<float>(definition.grid.height) * definition.grid.tile_size;
  const float distance = std::max(world_width, world_height) * 1.28F;
  m_camera->set_rts_view({0.0F, 0.0F, 0.0F}, distance, 66.0F, 225.0F);
  const float aspect = height() > 0
                           ? static_cast<float>(width()) / static_cast<float>(height())
                           : 16.0F / 9.0F;
  m_camera->set_perspective(
      45.0F, aspect, 1.0F, std::max(definition.camera.far_plane, distance * 4.0F));
  update();
}

void ArenaViewport::set_terrain_review_gameplay_camera() {
  if (m_camera == nullptr || !m_terrain_review_definition.has_value()) {
    return;
  }
  const auto& definition = *m_terrain_review_definition;
  QVector3D center = definition.camera.center;
  if (definition.coordSystem == Game::Map::CoordSystem::Grid) {
    center.setX(
        (center.x() - (static_cast<float>(definition.grid.width) * 0.5F - 0.5F)) *
        definition.grid.tile_size);
    center.setZ(
        (center.z() - (static_cast<float>(definition.grid.height) * 0.5F - 0.5F)) *
        definition.grid.tile_size);
  }
  m_camera->set_rts_view(center,
                         definition.camera.distance,
                         definition.camera.tilt_deg,
                         definition.camera.yaw_deg);
  const float aspect = height() > 0
                           ? static_cast<float>(width()) / static_cast<float>(height())
                           : 16.0F / 9.0F;
  m_camera->set_perspective(definition.camera.fov_y,
                            aspect,
                            definition.camera.near_plane,
                            std::max(definition.camera.far_plane,
                                     terrain_review_max_camera_distance() * 2.5F));
  update();
}

void ArenaViewport::arm_terrain_review_orbit(float distance_scale, float tilt_deg) {
  if (m_camera == nullptr || !m_terrain_review_definition.has_value()) {
    return;
  }

  const auto& definition = *m_terrain_review_definition;
  QVector3D center = definition.camera.center;
  if (definition.coordSystem == Game::Map::CoordSystem::Grid) {
    center.setX(
        (center.x() - (static_cast<float>(definition.grid.width) * 0.5F - 0.5F)) *
        definition.grid.tile_size);
    center.setZ(
        (center.z() - (static_cast<float>(definition.grid.height) * 0.5F - 0.5F)) *
        definition.grid.tile_size);
  }

  std::vector<QVector3D> building_positions;
  building_positions.reserve(definition.structures.size());
  for (const auto& structure : definition.structures) {
    if (structure.type == Game::Units::SpawnType::WallSegment) {
      continue;
    }
    const auto* point =
        std::get_if<Game::Map::PointStructureGeometry>(&structure.geometry);
    if (point != nullptr) {
      building_positions.push_back(point->position);
    }
  }

  if (!building_positions.empty()) {
    constexpr float k_cluster_radius = 70.0F;
    const float radius_sq = k_cluster_radius * k_cluster_radius;
    std::size_t best_index = 0;
    int best_count = -1;
    for (std::size_t i = 0; i < building_positions.size(); ++i) {
      int count = 0;
      for (const auto& other : building_positions) {
        const float dx = other.x() - building_positions[i].x();
        const float dz = other.z() - building_positions[i].z();
        if (dx * dx + dz * dz <= radius_sq) {
          ++count;
        }
      }
      if (count > best_count) {
        best_count = count;
        best_index = i;
      }
    }

    QVector3D cluster_center;
    int members = 0;
    for (const auto& other : building_positions) {
      const float dx = other.x() - building_positions[best_index].x();
      const float dz = other.z() - building_positions[best_index].z();
      if (dx * dx + dz * dz <= radius_sq) {
        cluster_center += other;
        ++members;
      }
    }
    if (members > 0) {
      center = cluster_center / static_cast<float>(members);
    }
  }

  m_capture_orbit_center = center;
  m_capture_orbit_view = Arena::ArenaCameraView{
      .distance = definition.camera.distance * std::max(0.05F, distance_scale),
      .angle = tilt_deg > 0.0F ? tilt_deg : definition.camera.tilt_deg,
      .yaw = definition.camera.yaw_deg};
  m_capture_orbit_yaw = 0.0F;
  m_capture_orbit_ready = true;

  m_camera->set_rts_view(m_capture_orbit_center,
                         m_capture_orbit_view.distance,
                         m_capture_orbit_view.angle,
                         m_capture_orbit_view.yaw);
  update();
}

auto ArenaViewport::terrain_review_definition() const
    -> const Game::Map::MapDefinition* {
  return m_terrain_review_definition.has_value() ? &*m_terrain_review_definition
                                                 : nullptr;
}
