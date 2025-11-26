#pragma once

namespace Game::Map::JsonKeys {

inline constexpr const char *NAME = "name";
inline constexpr const char *DESCRIPTION = "description";
inline constexpr const char *COORD_SYSTEM = "coordSystem";
inline constexpr const char *MAX_TROOPS_PER_PLAYER = "maxTroopsPerPlayer";
inline constexpr const char *GRID = "grid";
inline constexpr const char *BIOME = "biome";
inline constexpr const char *GROUND_TYPE = "groundType";
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
inline constexpr const char *TILE_SIZE = "tileSize";

inline constexpr const char *CENTER = "center";
inline constexpr const char *DISTANCE = "distance";
inline constexpr const char *TILT_DEG = "tiltDeg";
inline constexpr const char *YAW = "yaw";
inline constexpr const char *FOV_Y = "fovY";
inline constexpr const char *NEAR = "near";
inline constexpr const char *FAR = "far";

inline constexpr const char *SEED = "seed";
inline constexpr const char *PATCH_DENSITY = "patchDensity";
inline constexpr const char *PATCH_JITTER = "patchJitter";
inline constexpr const char *BLADE_HEIGHT = "bladeHeight";
inline constexpr const char *BLADE_WIDTH = "bladeWidth";
inline constexpr const char *SWAY_STRENGTH = "swayStrength";
inline constexpr const char *SWAY_SPEED = "swaySpeed";
inline constexpr const char *HEIGHT_NOISE = "heightNoise";
inline constexpr const char *GRASS_PRIMARY = "grassPrimary";
inline constexpr const char *GRASS_SECONDARY = "grassSecondary";
inline constexpr const char *GRASS_DRY = "grassDry";
inline constexpr const char *SOIL_COLOR = "soilColor";
inline constexpr const char *ROCK_LOW = "rockLow";
inline constexpr const char *ROCK_HIGH = "rockHigh";
inline constexpr const char *PLANT_DENSITY = "plantDensity";
inline constexpr const char *BACKGROUND_BLADE_DENSITY =
    "backgroundBladeDensity";
inline constexpr const char *TERRAIN_MACRO_NOISE_SCALE =
    "terrainMacroNoiseScale";
inline constexpr const char *TERRAIN_DETAIL_NOISE_SCALE =
    "terrainDetailNoiseScale";
inline constexpr const char *TERRAIN_SOIL_HEIGHT = "terrainSoilHeight";
inline constexpr const char *TERRAIN_SOIL_SHARPNESS = "terrainSoilSharpness";
inline constexpr const char *TERRAIN_ROCK_THRESHOLD = "terrainRockThreshold";
inline constexpr const char *TERRAIN_ROCK_SHARPNESS = "terrainRockSharpness";
inline constexpr const char *TERRAIN_AMBIENT_BOOST = "terrainAmbientBoost";
inline constexpr const char *TERRAIN_ROCK_DETAIL_STRENGTH =
    "terrainRockDetailStrength";
inline constexpr const char *BACKGROUND_SWAY_VARIANCE =
    "backgroundSwayVariance";
inline constexpr const char *BACKGROUND_SCATTER_RADIUS =
    "backgroundScatterRadius";
inline constexpr const char *GROUND_IRREGULARITY_ENABLED =
    "groundIrregularityEnabled";
inline constexpr const char *IRREGULARITY_SCALE = "irregularityScale";
inline constexpr const char *IRREGULARITY_AMPLITUDE = "irregularityAmplitude";

inline constexpr const char *TYPE = "type";
inline constexpr const char *X = "x";
inline constexpr const char *Z = "z";
inline constexpr const char *PLAYER_ID = "playerId";
inline constexpr const char *TEAM_ID = "teamId";
inline constexpr const char *MAX_POPULATION = "maxPopulation";
inline constexpr const char *NATION = "nation";

inline constexpr const char *VICTORY_TYPE = "type";
inline constexpr const char *KEY_STRUCTURES = "key_structures";
inline constexpr const char *DEFEAT_CONDITIONS = "defeat_conditions";
inline constexpr const char *SURVIVE_TIME_DURATION = "surviveTimeDuration";

inline constexpr const char *RADIUS = "radius";
inline constexpr const char *INTENSITY = "intensity";

inline constexpr const char *TERRAIN_TYPE = "terrainType";
inline constexpr const char *POSITION = "position";
inline constexpr const char *SCALE = "scale";
inline constexpr const char *ROTATION = "rotation";

inline constexpr const char *POINTS = "points";
inline constexpr const char *RIVER_WIDTH = "width";
inline constexpr const char *FLOW_SPEED = "flowSpeed";

inline constexpr const char *START = "start";
inline constexpr const char *END = "end";
inline constexpr const char *BRIDGE_WIDTH = "width";

inline constexpr const char *ROAD_STYLE = "style";

} // namespace Game::Map::JsonKeys
