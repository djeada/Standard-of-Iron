#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include "json_keys.h"
#include "map/environment_lighting.h"
#include "map/map_definition.h"
#include "map_loader_internal.h"
#include "systems/resource_json.h"

namespace Game::Map::loader_detail {

using namespace JsonKeys;

namespace {

auto read_grid(const QJsonObject& obj, GridDefinition& grid) -> bool {
  if (obj.contains(WIDTH)) {
    grid.width = obj.value(WIDTH).toInt(grid.width);
  }
  if (obj.contains(HEIGHT)) {
    grid.height = obj.value(HEIGHT).toInt(grid.height);
  }
  if (obj.contains(TILE_SIZE)) {
    grid.tile_size = float(obj.value(TILE_SIZE).toDouble(grid.tile_size));
  }
  return grid.width > 0 && grid.height > 0 && grid.tile_size > 0.0F;
}

auto read_camera(const QJsonObject& obj, CameraDefinition& cam) -> bool {
  if (obj.contains(CENTER)) {
    auto arr = obj.value(CENTER).toArray();
    if (arr.size() == 3) {
      cam.center = {float(arr[0].toDouble(0.0)),
                    float(arr[1].toDouble(0.0)),
                    float(arr[2].toDouble(0.0))};
    }
  }
  if (obj.contains(DISTANCE)) {
    cam.distance = float(obj.value(DISTANCE).toDouble(cam.distance));
  }
  if (obj.contains(TILT_DEG)) {
    cam.tilt_deg = float(obj.value(TILT_DEG).toDouble(cam.tilt_deg));
  }
  if (obj.contains(FOV_Y)) {
    cam.fov_y = float(obj.value(FOV_Y).toDouble(cam.fov_y));
  }
  if (obj.contains(NEAR)) {
    cam.near_plane = float(obj.value(NEAR).toDouble(cam.near_plane));
  }
  if (obj.contains(FAR)) {
    cam.far_plane = float(obj.value(FAR).toDouble(cam.far_plane));
  }
  if (obj.contains(YAW) || obj.contains("yaw_deg")) {

    const QString yaw_key = obj.contains(YAW) ? YAW : "yaw_deg";
    cam.yaw_deg = float(obj.value(yaw_key).toDouble(cam.yaw_deg));
  }
  return true;
}

void read_environment(const QJsonObject& obj, EnvironmentDefinition& environment) {
  if (obj.contains(START_TIME)) {
    environment.start_time = normalize_hour(
        static_cast<float>(obj.value(START_TIME).toDouble(environment.start_time)));
  }
  if (obj.contains(TIME_MODE)) {
    const QString value = obj.value(TIME_MODE).toString().trimmed().toLower();
    if (value != QStringLiteral("locked") && value != QStringLiteral("scripted") &&
        value != QStringLiteral("continuous")) {
      qWarning() << "MapLoader: unknown environment time_mode" << value
                 << "- defaulting to locked";
    }
    environment.time_mode = parse_time_mode(value);
  }
  if (obj.contains(DAY_LENGTH_SECONDS)) {
    environment.day_length_seconds = std::max(
        1.0F,
        static_cast<float>(
            obj.value(DAY_LENGTH_SECONDS).toDouble(environment.day_length_seconds)));
  }
  if (obj.contains(LIGHTING_PROFILE)) {
    const QString profile = obj.value(LIGHTING_PROFILE).toString().trimmed();
    if (!profile.isEmpty()) {
      environment.lighting_profile = profile;
    }
  }
  if (obj.contains(FOG_DENSITY)) {
    environment.fog_density_override =
        std::max(0.0F, static_cast<float>(obj.value(FOG_DENSITY).toDouble(0.0)));
  }
  if (obj.contains(EXPOSURE)) {
    environment.exposure_override =
        std::max(0.01F, static_cast<float>(obj.value(EXPOSURE).toDouble(1.0)));
  }
}

auto read_vector3(const QJsonValue& value, const QVector3D& fallback) -> QVector3D {
  if (!value.isArray()) {
    return fallback;
  }
  auto arr = value.toArray();
  if (arr.size() != 3) {
    return fallback;
  }
  return {float(arr[0].toDouble(fallback.x())),
          float(arr[1].toDouble(fallback.y())),
          float(arr[2].toDouble(fallback.z()))};
}

void read_biome_grass(const QJsonObject& obj, BiomeSettings& out) {
  if (obj.contains(PATCH_DENSITY)) {
    out.patch_density = float(obj.value(PATCH_DENSITY).toDouble(out.patch_density));
  }
  if (obj.contains(PATCH_JITTER)) {
    out.patch_jitter = float(obj.value(PATCH_JITTER).toDouble(out.patch_jitter));
  }
  if (obj.contains(BLADE_HEIGHT)) {
    auto arr = obj.value(BLADE_HEIGHT).toArray();
    if (arr.size() == 2) {
      out.blade_height_min = float(arr[0].toDouble(out.blade_height_min));
      out.blade_height_max = float(arr[1].toDouble(out.blade_height_max));
    }
  }
  if (obj.contains(BLADE_WIDTH)) {
    auto arr = obj.value(BLADE_WIDTH).toArray();
    if (arr.size() == 2) {
      out.blade_width_min = float(arr[0].toDouble(out.blade_width_min));
      out.blade_width_max = float(arr[1].toDouble(out.blade_width_max));
    }
  }
  if (obj.contains(BACKGROUND_BLADE_DENSITY)) {
    out.background_blade_density = float(
        obj.value(BACKGROUND_BLADE_DENSITY).toDouble(out.background_blade_density));
  }
  if (obj.contains(SWAY_STRENGTH)) {
    out.sway_strength = float(obj.value(SWAY_STRENGTH).toDouble(out.sway_strength));
  }
  if (obj.contains(SWAY_SPEED)) {
    out.sway_speed = float(obj.value(SWAY_SPEED).toDouble(out.sway_speed));
  }
  if (obj.contains(HEIGHT_NOISE)) {
    auto arr = obj.value(HEIGHT_NOISE).toArray();
    if (arr.size() == 2) {
      out.height_noise_amplitude = float(arr[0].toDouble(out.height_noise_amplitude));
      out.height_noise_frequency = float(arr[1].toDouble(out.height_noise_frequency));
    }
  }
}

void read_biome_palette(const QJsonObject& obj, BiomeSettings& out) {
  if (obj.contains(GRASS_PRIMARY)) {
    out.grass_primary = read_vector3(obj.value(GRASS_PRIMARY), out.grass_primary);
  }
  if (obj.contains(GRASS_SECONDARY)) {
    out.grass_secondary = read_vector3(obj.value(GRASS_SECONDARY), out.grass_secondary);
  }
  if (obj.contains(GRASS_DRY)) {
    out.grass_dry = read_vector3(obj.value(GRASS_DRY), out.grass_dry);
  }
  if (obj.contains(SOIL_COLOR)) {
    out.soil_color = read_vector3(obj.value(SOIL_COLOR), out.soil_color);
  }
  if (obj.contains(ROCK_LOW)) {
    out.rock_low = read_vector3(obj.value(ROCK_LOW), out.rock_low);
  }
  if (obj.contains(ROCK_HIGH)) {
    out.rock_high = read_vector3(obj.value(ROCK_HIGH), out.rock_high);
  }
  if (obj.contains(TERRAIN_MACRO_NOISE_SCALE)) {
    out.terrain_macro_noise_scale = float(
        obj.value(TERRAIN_MACRO_NOISE_SCALE).toDouble(out.terrain_macro_noise_scale));
  }
  if (obj.contains(TERRAIN_DETAIL_NOISE_SCALE)) {
    out.terrain_detail_noise_scale = float(
        obj.value(TERRAIN_DETAIL_NOISE_SCALE).toDouble(out.terrain_detail_noise_scale));
  }
  if (obj.contains(TERRAIN_SOIL_HEIGHT)) {
    out.terrain_soil_height =
        float(obj.value(TERRAIN_SOIL_HEIGHT).toDouble(out.terrain_soil_height));
  }
  if (obj.contains(TERRAIN_SOIL_SHARPNESS)) {
    out.terrain_soil_sharpness =
        float(obj.value(TERRAIN_SOIL_SHARPNESS).toDouble(out.terrain_soil_sharpness));
  }
  if (obj.contains(TERRAIN_ROCK_THRESHOLD)) {
    out.terrain_rock_threshold =
        float(obj.value(TERRAIN_ROCK_THRESHOLD).toDouble(out.terrain_rock_threshold));
  }
  if (obj.contains(TERRAIN_ROCK_SHARPNESS)) {
    out.terrain_rock_sharpness =
        float(obj.value(TERRAIN_ROCK_SHARPNESS).toDouble(out.terrain_rock_sharpness));
  }
  if (obj.contains(TERRAIN_AMBIENT_BOOST)) {
    out.terrain_ambient_boost =
        float(obj.value(TERRAIN_AMBIENT_BOOST).toDouble(out.terrain_ambient_boost));
  }
  if (obj.contains(TERRAIN_ROCK_DETAIL_STRENGTH)) {
    out.terrain_rock_detail_strength =
        float(obj.value(TERRAIN_ROCK_DETAIL_STRENGTH)
                  .toDouble(out.terrain_rock_detail_strength));
  }
}

void read_biome_scatter(const QJsonObject& obj, BiomeSettings& out) {
  if (obj.contains(BACKGROUND_SWAY_VARIANCE)) {
    out.background_sway_variance = float(
        obj.value(BACKGROUND_SWAY_VARIANCE).toDouble(out.background_sway_variance));
  }
  if (obj.contains(BACKGROUND_SCATTER_RADIUS)) {
    out.background_scatter_radius = float(
        obj.value(BACKGROUND_SCATTER_RADIUS).toDouble(out.background_scatter_radius));
  }
  if (obj.contains(PLANT_DENSITY)) {
    out.plant_density = float(obj.value(PLANT_DENSITY).toDouble(out.plant_density));
  }
  if (obj.contains(TREE_MIX)) {

    static constexpr std::array<std::pair<const char*, TreeSpecies>,
                                k_tree_species_count>
        k_species{{{"pine", TreeSpecies::Pine},
                   {"olive", TreeSpecies::Olive},
                   {"cypress", TreeSpecies::Cypress},
                   {"palm", TreeSpecies::Palm}}};
    const QJsonObject mix = obj.value(TREE_MIX).toObject();
    for (const auto& [name, species] : k_species) {
      const QString key = QString::fromLatin1(name);
      if (!mix.contains(key)) {
        continue;
      }
      auto& scale = out.tree_density_scale[static_cast<std::size_t>(species)];
      scale = std::max(0.0F, float(mix.value(key).toDouble(scale)));
    }
  }
}

void read_biome_surface(const QJsonObject& obj, BiomeSettings& out) {
  if (obj.contains(SPAWN_EDGE_PADDING)) {
    out.spawn_edge_padding =
        float(obj.value(SPAWN_EDGE_PADDING).toDouble(out.spawn_edge_padding));
  }
  if (obj.contains(GROUND_IRREGULARITY_ENABLED)) {
    out.ground_irregularity_enabled =
        obj.value(GROUND_IRREGULARITY_ENABLED).toBool(out.ground_irregularity_enabled);
  }
  if (obj.contains(PROCEDURAL_BOULDERS_ENABLED)) {
    out.procedural_boulders_enabled =
        obj.value(PROCEDURAL_BOULDERS_ENABLED).toBool(out.procedural_boulders_enabled);
  }
  if (obj.contains(PROCEDURAL_IRON_ORE_ENABLED)) {
    out.procedural_iron_ore_enabled =
        obj.value(PROCEDURAL_IRON_ORE_ENABLED).toBool(out.procedural_iron_ore_enabled);
  }
  if (obj.contains(PROCEDURAL_TREES_ENABLED)) {
    out.procedural_trees_enabled =
        obj.value(PROCEDURAL_TREES_ENABLED).toBool(out.procedural_trees_enabled);
  }
  if (obj.contains(IRREGULARITY_SCALE)) {
    out.irregularity_scale =
        float(obj.value(IRREGULARITY_SCALE).toDouble(out.irregularity_scale));
  }
  if (obj.contains(IRREGULARITY_AMPLITUDE)) {
    out.irregularity_amplitude =
        float(obj.value(IRREGULARITY_AMPLITUDE).toDouble(out.irregularity_amplitude));
  }
  if (obj.contains(SNOW_COVERAGE)) {
    out.snow_coverage = float(obj.value(SNOW_COVERAGE).toDouble(out.snow_coverage));
  }
  if (obj.contains(MOISTURE_LEVEL)) {
    out.moisture_level = float(obj.value(MOISTURE_LEVEL).toDouble(out.moisture_level));
  }
  if (obj.contains(CRACK_INTENSITY)) {
    out.crack_intensity =
        float(obj.value(CRACK_INTENSITY).toDouble(out.crack_intensity));
  }
  if (obj.contains(ROCK_EXPOSURE)) {
    out.rock_exposure = float(obj.value(ROCK_EXPOSURE).toDouble(out.rock_exposure));
  }
  if (obj.contains(GRASS_SATURATION)) {
    out.grass_saturation =
        float(obj.value(GRASS_SATURATION).toDouble(out.grass_saturation));
  }
  if (obj.contains(SOIL_ROUGHNESS)) {
    out.soil_roughness = float(obj.value(SOIL_ROUGHNESS).toDouble(out.soil_roughness));
  }
  if (obj.contains(SNOW_COLOR)) {
    out.snow_color = read_vector3(obj.value(SNOW_COLOR), out.snow_color);
  }
}

void read_biome(const QJsonObject& obj, BiomeSettings& out) {

  GroundType parsed_ground_type = GroundType::ForestMud;
  if (obj.contains(GROUND_TYPE)) {
    const QString ground_type_str = obj.value(GROUND_TYPE).toString();
    if (!try_parse_ground_type(ground_type_str, parsed_ground_type)) {
      qWarning() << "MapLoader: unknown ground type" << ground_type_str
                 << "- using default (forest_mud)";
      parsed_ground_type = GroundType::ForestMud;
    }
  }
  apply_ground_type_defaults(out, parsed_ground_type);

  if (obj.contains(SEED)) {
    out.seed =
        static_cast<std::uint32_t>(std::max(0.0, obj.value(SEED).toDouble(out.seed)));
  }
  read_biome_grass(obj, out);
  read_biome_palette(obj, out);
  read_biome_scatter(obj, out);
  read_biome_surface(obj, out);
}

void read_victory_config(const QJsonObject& obj, VictoryConfig& out) {

  if (obj.contains("type")) {
    out.victory_type = obj.value("type").toString("elimination");
  }

  if (obj.contains("key_structures") && obj.value("key_structures").isArray()) {
    out.key_structures.clear();
    auto arr = obj.value("key_structures").toArray();
    for (const auto val : arr) {
      out.key_structures.push_back(val.toString());
    }
  }

  if (obj.contains("duration")) {
    out.survive_time_duration = float(obj.value("duration").toDouble(0.0));
  }

  if (obj.contains("defeat_conditions") && obj.value("defeat_conditions").isArray()) {
    out.defeat_conditions.clear();
    auto arr = obj.value("defeat_conditions").toArray();
    for (const auto val : arr) {
      out.defeat_conditions.push_back(val.toString());
    }
  }

  if (obj.contains("min_count")) {
    out.required_key_structures = obj.value("min_count").toInt(0);
  }

  if (obj.contains("undead_objectives") && obj.value("undead_objectives").isArray()) {
    out.undead_objectives.clear();
    const auto arr = obj.value("undead_objectives").toArray();
    for (const auto val : arr) {
      if (!val.isObject()) {
        continue;
      }
      const QJsonObject entry = val.toObject();
      UndeadObjective objective;
      objective.type = entry.value("type").toString().trimmed().toLower();
      objective.zone_id = entry.value("zone_id").toString();
      objective.wave_count = entry.value("wave_count").toInt(1);
      if (objective.type.isEmpty() || objective.zone_id.isEmpty()) {
        qWarning() << "Skipping undead victory objective without type or zone_id";
        continue;
      }
      out.undead_objectives.push_back(std::move(objective));
    }
  }
}

void read_rain_config(const QJsonObject& obj, RainSettings& out) {
  if (obj.contains(RAIN_ENABLED)) {
    out.enabled = obj.value(RAIN_ENABLED).toBool(out.enabled);
  }
  if (obj.contains(RAIN_TYPE)) {
    QString const type_str = obj.value(RAIN_TYPE).toString("rain").toLower();
    if (type_str == "snow") {
      out.type = WeatherType::Snow;
    } else {
      out.type = WeatherType::Rain;
    }
  }
  if (obj.contains(RAIN_CYCLE_DURATION)) {
    out.cycle_duration =
        float(obj.value(RAIN_CYCLE_DURATION).toDouble(out.cycle_duration));
  }
  if (obj.contains(RAIN_ACTIVE_DURATION)) {
    out.active_duration =
        float(obj.value(RAIN_ACTIVE_DURATION).toDouble(out.active_duration));
  }
  if (obj.contains(RAIN_INTENSITY)) {
    const QJsonValue intensity = obj.value(RAIN_INTENSITY);
    if (intensity.isString()) {
      out.intensity = parse_weather_intensity(intensity.toString(), out.intensity);
    } else {
      out.intensity = float(intensity.toDouble(out.intensity));
    }
  }
  if (obj.contains(RAIN_FADE_DURATION)) {
    out.fade_duration =
        float(obj.value(RAIN_FADE_DURATION).toDouble(out.fade_duration));
  }
  if (obj.contains(RAIN_WIND_STRENGTH)) {
    out.wind_strength =
        float(obj.value(RAIN_WIND_STRENGTH).toDouble(out.wind_strength));
  }
  if (obj.contains(RAIN_WIND_DIRECTION)) {
    out.wind_direction_deg =
        float(obj.value(RAIN_WIND_DIRECTION).toDouble(out.wind_direction_deg));
  }
  out.intensity = std::clamp(out.intensity, 0.0F, 1.0F);
  out.wind_strength = std::max(0.0F, out.wind_strength);
}

} // namespace

auto authored_position(float raw_x,
                       float raw_z,
                       const GridDefinition& grid,
                       CoordSystem coord_sys) -> QVector3D {
  if (coord_sys == CoordSystem::World) {
    return {raw_x, 0.0F, raw_z};
  }

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  const float tile = std::max(min_tile_size, grid.tile_size);
  return {(raw_x - (grid.width * grid_center_offset - grid_center_offset)) * tile,
          0.0F,
          (raw_z - (grid.height * grid_center_offset - grid_center_offset)) * tile};
}

auto read_map_header(const QJsonObject& root,
                     MapDefinition& out_map,
                     QString* out_error) -> bool {
  out_map.name = root.value(NAME).toString(QT_TRANSLATE_NOOP("Maps", "Unnamed Map"));

  if (root.contains(COORD_SYSTEM)) {
    const QString coord_system =
        root.value(COORD_SYSTEM).toString().trimmed().toLower();
    if (coord_system == "world") {
      out_map.coordSystem = CoordSystem::World;
    } else {
      out_map.coordSystem = CoordSystem::Grid;
    }
  }

  constexpr int default_max_troops = 500;
  if (root.contains(MAX_TROOPS_PER_PLAYER)) {
    out_map.max_troops_per_player =
        root.value(MAX_TROOPS_PER_PLAYER).toInt(default_max_troops);
  }

  if (root.contains(GRID) && root.value(GRID).isObject()) {
    if (!read_grid(root.value(GRID).toObject(), out_map.grid)) {
      if (out_error != nullptr) {
        *out_error = "Invalid grid definition";
      }
      return false;
    }
  }

  if (root.contains(CAMERA) && root.value(CAMERA).isObject()) {
    read_camera(root.value(CAMERA).toObject(), out_map.camera);
  }
  return true;
}

void read_map_appearance(const QJsonObject& root, MapDefinition& out_map) {
  if (root.contains(BIOME) && root.value(BIOME).isObject()) {
    read_biome(root.value(BIOME).toObject(), out_map.biome);
  }

  if (root.contains(VICTORY) && root.value(VICTORY).isObject()) {
    read_victory_config(root.value(VICTORY).toObject(), out_map.victory);
  }

  if (root.contains(RAIN) && root.value(RAIN).isObject()) {
    read_rain_config(root.value(RAIN).toObject(), out_map.rain);
  }
}

void read_map_clock_and_resources(const QJsonObject& root, MapDefinition& out_map) {
  if (root.contains(TIME_OF_DAY)) {
    const QString tod_str = root.value(TIME_OF_DAY).toString().trimmed().toLower();
    if (tod_str == "morning") {
      out_map.time_of_day = TimeOfDay::Morning;
    } else if (tod_str == "day") {
      out_map.time_of_day = TimeOfDay::Day;
    } else if (tod_str == "afternoon") {
      out_map.time_of_day = TimeOfDay::Afternoon;
    } else if (tod_str == "night") {
      out_map.time_of_day = TimeOfDay::Night;
    } else {
      qWarning() << "MapLoader: unknown time_of_day value" << tod_str
                 << "- defaulting to day";
      out_map.time_of_day = TimeOfDay::Day;
    }
    out_map.environment.start_time = hour_for_time_of_day(out_map.time_of_day);
  }

  if (root.contains(ENVIRONMENT) && root.value(ENVIRONMENT).isObject()) {
    read_environment(root.value(ENVIRONMENT).toObject(), out_map.environment);
    out_map.time_of_day = time_of_day_for_hour(out_map.environment.start_time);
  }

  if (root.contains(STARTING_RESOURCES) && root.value(STARTING_RESOURCES).isObject()) {
    Game::Systems::read_resource_overlay(root.value(STARTING_RESOURCES).toObject())
        .apply_to(out_map.starting_resources);
  }
}

} // namespace Game::Map::loader_detail
