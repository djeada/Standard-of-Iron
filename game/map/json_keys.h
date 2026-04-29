#pragma once

namespace Game::Map::JsonKeys {

inline constexpr const char *NAME = "name";
inline constexpr const char *DESCRIPTION = "description";
inline constexpr const char *COORD_SYSTEM = "coord_system";
inline constexpr const char *MAX_TROOPS_PER_PLAYER = "max_troops_per_player";
inline constexpr const char *GRID = "grid";
inline constexpr const char *BIOME = "biome";
inline constexpr const char *GROUND_TYPE = "ground_type";
inline constexpr const char *CAMERA = "camera";
inline constexpr const char *SPAWNS = "spawns";
inline constexpr const char *FIRECAMPS = "firecamps";
inline constexpr const char *TERRAIN = "terrain";
inline constexpr const char *RIVERS = "rivers";
inline constexpr const char *ROADS = "roads";
inline constexpr const char *BRIDGES = "bridges";
inline constexpr const char *VICTORY = "victory";
inline constexpr const char *THUMBNAIL = "thumbnail";

inline constexpr const char *WIDTH = "width";
inline constexpr const char *HEIGHT = "height";
inline constexpr const char *TILE_SIZE = "tile_size";

inline constexpr const char *CENTER = "center";
inline constexpr const char *DISTANCE = "distance";
inline constexpr const char *TILT_DEG = "tilt_deg";
inline constexpr const char *YAW = "yaw";
inline constexpr const char *FOV_Y = "fov_y";
inline constexpr const char *NEAR = "near";
inline constexpr const char *FAR = "far";

inline constexpr const char *SEED = "seed";
inline constexpr const char *PATCH_DENSITY = "patch_density";
inline constexpr const char *PATCH_JITTER = "patch_jitter";
inline constexpr const char *BLADE_HEIGHT = "blade_height";
inline constexpr const char *BLADE_WIDTH = "blade_width";
inline constexpr const char *SWAY_STRENGTH = "sway_strength";
inline constexpr const char *SWAY_SPEED = "sway_speed";
inline constexpr const char *HEIGHT_NOISE = "height_noise";
inline constexpr const char *GRASS_PRIMARY = "grass_primary";
inline constexpr const char *GRASS_SECONDARY = "grass_secondary";
inline constexpr const char *GRASS_DRY = "grass_dry";
inline constexpr const char *SOIL_COLOR = "soil_color";
inline constexpr const char *ROCK_LOW = "rock_low";
inline constexpr const char *ROCK_HIGH = "rock_high";
inline constexpr const char *PLANT_DENSITY = "plant_density";
inline constexpr const char *BACKGROUND_BLADE_DENSITY =
    "background_blade_density";
inline constexpr const char *TERRAIN_MACRO_NOISE_SCALE =
    "terrain_macro_noise_scale";
inline constexpr const char *TERRAIN_DETAIL_NOISE_SCALE =
    "terrain_detail_noise_scale";
inline constexpr const char *TERRAIN_SOIL_HEIGHT = "terrain_soil_height";
inline constexpr const char *TERRAIN_SOIL_SHARPNESS = "terrain_soil_sharpness";
inline constexpr const char *TERRAIN_ROCK_THRESHOLD = "terrain_rock_threshold";
inline constexpr const char *TERRAIN_ROCK_SHARPNESS = "terrain_rock_sharpness";
inline constexpr const char *TERRAIN_AMBIENT_BOOST = "terrain_ambient_boost";
inline constexpr const char *TERRAIN_ROCK_DETAIL_STRENGTH =
    "terrain_rock_detail_strength";
inline constexpr const char *BACKGROUND_SWAY_VARIANCE =
    "background_sway_variance";
inline constexpr const char *BACKGROUND_SCATTER_RADIUS =
    "background_scatter_radius";
inline constexpr const char *SNOW_COVERAGE = "snow_coverage";
inline constexpr const char *MOISTURE_LEVEL = "moisture_level";
inline constexpr const char *CRACK_INTENSITY = "crack_intensity";
inline constexpr const char *ROCK_EXPOSURE = "rock_exposure";
inline constexpr const char *GRASS_SATURATION = "grass_saturation";
inline constexpr const char *SOIL_ROUGHNESS = "soil_roughness";
inline constexpr const char *SNOW_COLOR = "snow_color";
inline constexpr const char *GROUND_IRREGULARITY_ENABLED =
    "ground_irregularity_enabled";
inline constexpr const char *IRREGULARITY_SCALE = "irregularity_scale";
inline constexpr const char *IRREGULARITY_AMPLITUDE = "irregularity_amplitude";

inline constexpr const char *TYPE = "type";
inline constexpr const char *X = "x";
inline constexpr const char *Z = "z";
inline constexpr const char *PLAYER_ID = "player_id";
inline constexpr const char *TEAM_ID = "team_id";
inline constexpr const char *MAX_POPULATION = "max_population";
inline constexpr const char *NATION = "nation";

inline constexpr const char *VICTORY_TYPE = "type";
inline constexpr const char *KEY_STRUCTURES = "key_structures";
inline constexpr const char *DEFEAT_CONDITIONS = "defeat_conditions";
inline constexpr const char *SURVIVE_TIME_DURATION = "survive_time_duration";

inline constexpr const char *RADIUS = "radius";
inline constexpr const char *INTENSITY = "intensity";

inline constexpr const char *TERRAIN_TYPE = "terrain_type";
inline constexpr const char *POSITION = "position";
inline constexpr const char *SCALE = "scale";
inline constexpr const char *ROTATION = "rotation";

inline constexpr const char *POINTS = "points";
inline constexpr const char *RIVER_WIDTH = "width";
inline constexpr const char *FLOW_SPEED = "flow_speed";

inline constexpr const char *START = "start";
inline constexpr const char *END = "end";
inline constexpr const char *BRIDGE_WIDTH = "width";

inline constexpr const char *ROAD_STYLE = "style";

inline constexpr const char *RAIN = "rain";
inline constexpr const char *RAIN_ENABLED = "enabled";
inline constexpr const char *RAIN_TYPE = "type";
inline constexpr const char *RAIN_CYCLE_DURATION = "cycle_duration";
inline constexpr const char *RAIN_ACTIVE_DURATION = "active_duration";
inline constexpr const char *RAIN_INTENSITY = "intensity";
inline constexpr const char *RAIN_FADE_DURATION = "fade_duration";
inline constexpr const char *RAIN_WIND_STRENGTH = "wind_strength";

} // namespace Game::Map::JsonKeys
