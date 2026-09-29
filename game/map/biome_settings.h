#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>

#include "ground_type.h"

namespace Game::Map {

enum class TreeSpecies : std::uint8_t {
  Pine = 0,
  Olive,
  Cypress,
  Palm
};

inline constexpr std::size_t k_tree_species_count = 4;

struct BiomeSettings {
  GroundType ground_type = GroundType::ForestMud;
  QVector3D grass_primary{0.27F, 0.52F, 0.25F};
  QVector3D grass_secondary{0.38F, 0.61F, 0.29F};
  QVector3D grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D soil_color{0.28F, 0.24F, 0.18F};
  QVector3D rock_low{0.48F, 0.46F, 0.44F};
  QVector3D rock_high{0.68F, 0.69F, 0.73F};
  float patch_density = 4.5F;
  float patch_jitter = 0.95F;
  float background_blade_density = 0.65F;
  float blade_height_min = 0.55F;
  float blade_height_max = 1.35F;
  float blade_width_min = 0.025F;
  float blade_width_max = 0.055F;
  float sway_strength = 0.25F;
  float sway_speed = 1.4F;
  float height_noise_amplitude = 0.16F;
  float height_noise_frequency = 0.05F;
  float terrain_macro_noise_scale = 0.035F;
  float terrain_detail_noise_scale = 0.14F;
  float terrain_soil_height = 0.6F;
  float terrain_soil_sharpness = 3.8F;
  float terrain_rock_threshold = 0.42F;
  float terrain_rock_sharpness = 3.1F;
  float terrain_ambient_boost = 0.96F;
  float terrain_rock_detail_strength = 0.35F;
  float background_sway_variance = 0.2F;
  float background_scatter_radius = 0.35F;
  float plant_density = 0.5F;
  float spawn_edge_padding = 0.02F;
  std::uint32_t seed = 1337U;
  bool ground_irregularity_enabled = true;
  float irregularity_scale = 0.15F;
  float irregularity_amplitude = 0.08F;

  bool procedural_boulders_enabled = true;
  bool procedural_iron_ore_enabled = true;
  bool procedural_trees_enabled = true;

  float snow_coverage = 0.0F;
  float moisture_level = 0.5F;
  float crack_intensity = 0.0F;
  float rock_exposure = 0.3F;
  float grass_saturation = 1.0F;
  float soil_roughness = 0.5F;
  QVector3D snow_color{0.92F, 0.94F, 0.98F};

  std::array<float, k_tree_species_count> tree_density_scale{1.0F, 1.0F, 1.0F, 1.0F};
};

struct TerrainSurfaceProfile {
  GroundType ground_type = GroundType::ForestMud;
  QVector3D grass_primary{0.27F, 0.52F, 0.25F};
  QVector3D grass_secondary{0.38F, 0.61F, 0.29F};
  QVector3D grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D soil_color{0.28F, 0.24F, 0.18F};
  QVector3D rock_low{0.48F, 0.46F, 0.44F};
  QVector3D rock_high{0.68F, 0.69F, 0.73F};
  float height_noise_amplitude = 0.16F;
  float height_noise_frequency = 0.05F;
  float terrain_macro_noise_scale = 0.035F;
  float terrain_detail_noise_scale = 0.14F;
  float terrain_soil_height = 0.6F;
  float terrain_soil_sharpness = 3.8F;
  float terrain_rock_threshold = 0.42F;
  float terrain_rock_sharpness = 3.1F;
  float terrain_ambient_boost = 0.96F;
  float terrain_rock_detail_strength = 0.35F;
  bool ground_irregularity_enabled = true;
  float irregularity_scale = 0.15F;
  float irregularity_amplitude = 0.08F;
  std::uint32_t seed = 1337U;
};

struct TerrainScatterProfile {
  GroundType ground_type = GroundType::ForestMud;
  QVector3D grass_primary{0.27F, 0.52F, 0.25F};
  QVector3D grass_secondary{0.38F, 0.61F, 0.29F};
  QVector3D grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D soil_color{0.28F, 0.24F, 0.18F};
  QVector3D rock_low{0.48F, 0.46F, 0.44F};
  QVector3D rock_high{0.68F, 0.69F, 0.73F};
  float patch_density = 4.5F;
  float patch_jitter = 0.95F;
  float background_blade_density = 0.65F;
  float blade_height_min = 0.55F;
  float blade_height_max = 1.35F;
  float blade_width_min = 0.025F;
  float blade_width_max = 0.055F;
  float plant_density = 0.5F;
  float spawn_edge_padding = 0.02F;
  float background_scatter_radius = 0.35F;
  std::uint32_t seed = 1337U;
};

struct ClimateProfile {
  float snow_coverage = 0.0F;
  float moisture_level = 0.5F;
  float crack_intensity = 0.0F;
  float rock_exposure = 0.3F;
  float grass_saturation = 1.0F;
  float soil_roughness = 0.5F;
  QVector3D snow_color{0.92F, 0.94F, 0.98F};
};

struct WindProfile {
  float sway_strength = 0.25F;
  float sway_speed = 1.4F;
  float background_sway_variance = 0.2F;
};

struct BiomeProfiles {
  TerrainSurfaceProfile surface;
  TerrainScatterProfile scatter;
  ClimateProfile climate;
  WindProfile wind;
};

struct TreeScatterRule {
  bool allowed = false;
  float base_density = 0.0F;
  float density_scale = 0.0F;
  float scale_min = 1.0F;
  float scale_max = 2.0F;
};

struct TerrainScatterRules {
  std::array<TreeScatterRule, k_tree_species_count> trees{};

  [[nodiscard]] constexpr auto
  tree(TreeSpecies species) const -> const TreeScatterRule& {
    return trees[static_cast<std::size_t>(species)];
  }

  [[nodiscard]] constexpr auto tree(TreeSpecies species) -> TreeScatterRule& {
    return trees[static_cast<std::size_t>(species)];
  }
};

inline auto
make_surface_profile(const BiomeSettings& settings) -> TerrainSurfaceProfile {
  TerrainSurfaceProfile profile;
  profile.ground_type = settings.ground_type;
  profile.grass_primary = settings.grass_primary;
  profile.grass_secondary = settings.grass_secondary;
  profile.grass_dry = settings.grass_dry;
  profile.soil_color = settings.soil_color;
  profile.rock_low = settings.rock_low;
  profile.rock_high = settings.rock_high;
  profile.height_noise_amplitude = settings.height_noise_amplitude;
  profile.height_noise_frequency = settings.height_noise_frequency;
  profile.terrain_macro_noise_scale = settings.terrain_macro_noise_scale;
  profile.terrain_detail_noise_scale = settings.terrain_detail_noise_scale;
  profile.terrain_soil_height = settings.terrain_soil_height;
  profile.terrain_soil_sharpness = settings.terrain_soil_sharpness;
  profile.terrain_rock_threshold = settings.terrain_rock_threshold;
  profile.terrain_rock_sharpness = settings.terrain_rock_sharpness;
  profile.terrain_ambient_boost = settings.terrain_ambient_boost;
  profile.terrain_rock_detail_strength = settings.terrain_rock_detail_strength;
  profile.ground_irregularity_enabled = settings.ground_irregularity_enabled;
  profile.irregularity_scale = settings.irregularity_scale;
  profile.irregularity_amplitude = settings.irregularity_amplitude;
  profile.seed = settings.seed;
  return profile;
}

inline auto
make_scatter_profile(const BiomeSettings& settings) -> TerrainScatterProfile {
  TerrainScatterProfile profile;
  profile.ground_type = settings.ground_type;
  profile.grass_primary = settings.grass_primary;
  profile.grass_secondary = settings.grass_secondary;
  profile.grass_dry = settings.grass_dry;
  profile.soil_color = settings.soil_color;
  profile.rock_low = settings.rock_low;
  profile.rock_high = settings.rock_high;
  profile.patch_density = settings.patch_density;
  profile.patch_jitter = settings.patch_jitter;
  profile.background_blade_density = settings.background_blade_density;
  profile.blade_height_min = settings.blade_height_min;
  profile.blade_height_max = settings.blade_height_max;
  profile.blade_width_min = settings.blade_width_min;
  profile.blade_width_max = settings.blade_width_max;
  profile.plant_density = settings.plant_density;
  profile.spawn_edge_padding = settings.spawn_edge_padding;
  profile.background_scatter_radius = settings.background_scatter_radius;
  profile.seed = settings.seed;
  return profile;
}

inline auto make_climate_profile(const BiomeSettings& settings) -> ClimateProfile {
  ClimateProfile profile;
  profile.snow_coverage = settings.snow_coverage;
  profile.moisture_level = settings.moisture_level;
  profile.crack_intensity = settings.crack_intensity;
  profile.rock_exposure = settings.rock_exposure;
  profile.grass_saturation = settings.grass_saturation;
  profile.soil_roughness = settings.soil_roughness;
  profile.snow_color = settings.snow_color;
  return profile;
}

inline auto make_wind_profile(const BiomeSettings& settings) -> WindProfile {
  WindProfile profile;
  profile.sway_strength = settings.sway_strength;
  profile.sway_speed = settings.sway_speed;
  profile.background_sway_variance = settings.background_sway_variance;
  return profile;
}

inline auto make_biome_profiles(const BiomeSettings& settings) -> BiomeProfiles {
  BiomeProfiles profiles;
  profiles.surface = make_surface_profile(settings);
  profiles.scatter = make_scatter_profile(settings);
  profiles.climate = make_climate_profile(settings);
  profiles.wind = make_wind_profile(settings);
  return profiles;
}

inline auto make_scatter_rules(GroundType ground_type) -> TerrainScatterRules {
  TerrainScatterRules rules;

  auto& pine = rules.tree(TreeSpecies::Pine);
  pine = {true, 0.2F, 0.3F, 1.9F, 3.4F};
  auto& olive = rules.tree(TreeSpecies::Olive);
  olive = {false, 0.05F, 0.08F, 3.2F, 5.8F};

  auto& cypress = rules.tree(TreeSpecies::Cypress);
  auto& palm = rules.tree(TreeSpecies::Palm);

  switch (ground_type) {
  case GroundType::GrassDry:
    pine.allowed = false;
    olive = {true, 0.08F, 0.10F, 2.8F, 5.0F};
    cypress = {true, 0.060F, 0.240F, 2.6F, 4.2F};
    palm = {true, 0.100F, 0.400F, 3.0F, 4.6F};
    break;
  case GroundType::ForestMud:
    pine = {true, 0.32F, 0.42F, 2.1F, 3.8F};
    cypress = {true, 0.072F, 0.120F, 2.8F, 4.6F};
    break;
  case GroundType::SoilFertile:
    pine.allowed = false;
    olive = {true, 0.08F, 0.12F, 3.0F, 5.4F};
    cypress = {true, 0.090F, 0.200F, 2.8F, 4.6F};
    palm = {true, 0.108F, 0.240F, 3.0F, 4.6F};
    break;
  case GroundType::SoilRocky:
    pine.allowed = false;
    olive.allowed = false;
    cypress = {true, 0.054F, 0.300F, 2.4F, 3.9F};
    break;
  case GroundType::AlpineMix:
    pine = {true, 0.10F, 0.20F, 1.7F, 3.0F};
    cypress = {true, 0.042F, 0.350F, 2.4F, 4.0F};
    break;
  }
  return rules;
}

inline void apply_ground_type_defaults(BiomeSettings& settings,
                                       GroundType ground_type) {
  settings.ground_type = ground_type;
  switch (ground_type) {
  case GroundType::ForestMud:

    settings.grass_primary = QVector3D(0.27F, 0.52F, 0.25F);
    settings.grass_secondary = QVector3D(0.38F, 0.61F, 0.29F);
    settings.grass_dry = QVector3D(0.76F, 0.70F, 0.48F);
    settings.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
    settings.rock_low = QVector3D(0.48F, 0.46F, 0.44F);
    settings.rock_high = QVector3D(0.68F, 0.69F, 0.73F);
    settings.terrain_ambient_boost = 0.96F;
    settings.terrain_rock_detail_strength = 0.35F;

    settings.patch_density = 4.5F;
    settings.patch_jitter = 0.95F;
    settings.background_blade_density = 0.70F;
    settings.blade_height_min = 0.60F;
    settings.blade_height_max = 1.40F;
    settings.blade_width_min = 0.028F;
    settings.blade_width_max = 0.058F;
    settings.sway_strength = 0.28F;
    settings.sway_speed = 1.3F;

    settings.terrain_macro_noise_scale = 0.035F;
    settings.terrain_detail_noise_scale = 0.14F;
    settings.terrain_soil_height = 0.65F;
    settings.terrain_soil_sharpness = 3.5F;
    settings.terrain_rock_threshold = 0.48F;
    settings.terrain_rock_sharpness = 3.2F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.15F;
    settings.irregularity_amplitude = 0.09F;
    settings.plant_density = 0.60F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.70F;
    settings.crack_intensity = 0.0F;
    settings.rock_exposure = 0.25F;
    settings.grass_saturation = 1.10F;
    settings.soil_roughness = 0.55F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::GrassDry:

    settings.grass_primary = QVector3D(0.27F, 0.44F, 0.20F);
    settings.grass_secondary = QVector3D(0.38F, 0.53F, 0.24F);
    settings.grass_dry = QVector3D(0.50F, 0.50F, 0.27F);
    settings.soil_color = QVector3D(0.52F, 0.44F, 0.32F);
    settings.rock_low = QVector3D(0.62F, 0.58F, 0.52F);
    settings.rock_high = QVector3D(0.78F, 0.75F, 0.70F);
    settings.terrain_ambient_boost = 1.06F;
    settings.terrain_rock_detail_strength = 0.28F;

    settings.patch_density = 2.8F;
    settings.patch_jitter = 0.75F;
    settings.background_blade_density = 0.35F;
    settings.blade_height_min = 0.35F;
    settings.blade_height_max = 0.80F;
    settings.blade_width_min = 0.018F;
    settings.blade_width_max = 0.038F;
    settings.sway_strength = 0.15F;
    settings.sway_speed = 1.8F;

    settings.terrain_macro_noise_scale = 0.028F;
    settings.terrain_detail_noise_scale = 0.22F;
    settings.terrain_soil_height = 0.50F;
    settings.terrain_soil_sharpness = 4.5F;
    settings.terrain_rock_threshold = 0.38F;
    settings.terrain_rock_sharpness = 2.8F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.10F;
    settings.irregularity_amplitude = 0.04F;
    settings.plant_density = 0.25F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.15F;
    settings.crack_intensity = 0.65F;
    settings.rock_exposure = 0.35F;
    settings.grass_saturation = 0.90F;
    settings.soil_roughness = 0.72F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::SoilRocky:

    settings.grass_primary = QVector3D(0.38F, 0.46F, 0.27F);
    settings.grass_secondary = QVector3D(0.45F, 0.54F, 0.31F);
    settings.grass_dry = QVector3D(0.58F, 0.52F, 0.38F);
    settings.soil_color = QVector3D(0.55F, 0.48F, 0.38F);
    settings.rock_low = QVector3D(0.42F, 0.40F, 0.36F);
    settings.rock_high = QVector3D(0.62F, 0.59F, 0.53F);
    settings.terrain_ambient_boost = 0.93F;
    settings.terrain_rock_detail_strength = 0.68F;

    settings.patch_density = 2.2F;
    settings.patch_jitter = 0.60F;
    settings.background_blade_density = 0.28F;
    settings.blade_height_min = 0.30F;
    settings.blade_height_max = 0.70F;
    settings.blade_width_min = 0.020F;
    settings.blade_width_max = 0.040F;
    settings.sway_strength = 0.18F;
    settings.sway_speed = 1.5F;

    settings.terrain_macro_noise_scale = 0.055F;
    settings.terrain_detail_noise_scale = 0.28F;
    settings.terrain_soil_height = 0.40F;
    settings.terrain_soil_sharpness = 5.0F;
    settings.terrain_rock_threshold = 0.28F;
    settings.terrain_rock_sharpness = 4.0F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.22F;
    settings.irregularity_amplitude = 0.14F;
    settings.plant_density = 0.18F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.28F;
    settings.crack_intensity = 0.32F;
    settings.rock_exposure = 0.82F;
    settings.grass_saturation = 0.78F;
    settings.soil_roughness = 0.88F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::AlpineMix:

    settings.grass_primary = QVector3D(0.31F, 0.42F, 0.29F);
    settings.grass_secondary = QVector3D(0.36F, 0.48F, 0.34F);
    settings.grass_dry = QVector3D(0.50F, 0.48F, 0.42F);
    settings.soil_color = QVector3D(0.42F, 0.40F, 0.38F);
    settings.rock_low = QVector3D(0.58F, 0.60F, 0.64F);
    settings.rock_high = QVector3D(0.88F, 0.90F, 0.94F);
    settings.terrain_ambient_boost = 1.12F;
    settings.terrain_rock_detail_strength = 0.52F;

    settings.patch_density = 1.8F;
    settings.patch_jitter = 0.50F;
    settings.background_blade_density = 0.22F;
    settings.blade_height_min = 0.20F;
    settings.blade_height_max = 0.50F;
    settings.blade_width_min = 0.015F;
    settings.blade_width_max = 0.032F;
    settings.sway_strength = 0.22F;
    settings.sway_speed = 2.0F;

    settings.terrain_macro_noise_scale = 0.042F;
    settings.terrain_detail_noise_scale = 0.18F;
    settings.terrain_soil_height = 0.55F;
    settings.terrain_soil_sharpness = 3.0F;
    settings.terrain_rock_threshold = 0.32F;
    settings.terrain_rock_sharpness = 2.5F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.18F;
    settings.irregularity_amplitude = 0.12F;
    settings.plant_density = 0.12F;

    settings.snow_coverage = 0.55F;
    settings.moisture_level = 0.45F;
    settings.crack_intensity = 0.10F;
    settings.rock_exposure = 0.60F;
    settings.grass_saturation = 0.95F;
    settings.soil_roughness = 0.62F;
    settings.snow_color = QVector3D(0.94F, 0.96F, 1.0F);
    break;

  case GroundType::SoilFertile:

    settings.grass_primary = QVector3D(0.24F, 0.50F, 0.22F);
    settings.grass_secondary = QVector3D(0.32F, 0.58F, 0.28F);
    settings.grass_dry = QVector3D(0.52F, 0.48F, 0.32F);
    settings.soil_color = QVector3D(0.20F, 0.16F, 0.12F);
    settings.rock_low = QVector3D(0.38F, 0.36F, 0.34F);
    settings.rock_high = QVector3D(0.52F, 0.50F, 0.48F);
    settings.terrain_ambient_boost = 0.94F;
    settings.terrain_rock_detail_strength = 0.22F;

    settings.patch_density = 5.2F;
    settings.patch_jitter = 0.90F;
    settings.background_blade_density = 0.80F;
    settings.blade_height_min = 0.55F;
    settings.blade_height_max = 1.25F;
    settings.blade_width_min = 0.030F;
    settings.blade_width_max = 0.062F;
    settings.sway_strength = 0.32F;
    settings.sway_speed = 1.2F;

    settings.terrain_macro_noise_scale = 0.025F;
    settings.terrain_detail_noise_scale = 0.10F;
    settings.terrain_soil_height = 0.75F;
    settings.terrain_soil_sharpness = 2.8F;
    settings.terrain_rock_threshold = 0.58F;
    settings.terrain_rock_sharpness = 2.2F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.08F;
    settings.irregularity_amplitude = 0.05F;
    settings.plant_density = 0.45F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.80F;
    settings.crack_intensity = 0.0F;
    settings.rock_exposure = 0.12F;
    settings.grass_saturation = 1.10F;
    settings.soil_roughness = 0.42F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;
  }
}

} // namespace Game::Map
