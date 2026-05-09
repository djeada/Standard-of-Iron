#include "map_boundary_fog_renderer.h"

#include "../../game/map/terrain_noise.h"
#include "../../game/map/terrain_service.h"
#include "../scene_renderer.h"
#include <QMatrix4x4>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

namespace {

const QMatrix4x4 k_identity_matrix;

constexpr int k_clear_outside_tiles = 2;
constexpr int k_band_outside = 16;
constexpr int k_cards_per_side = 3;
constexpr int k_curtain_rings = 2;
constexpr int k_mountain_depth_segments = 22;

constexpr std::array<float, k_curtain_rings> k_ring_offset_scale = {0.52F,
                                                                    1.06F};
constexpr std::array<float, k_curtain_rings> k_ring_height_scale = {2.55F,
                                                                    3.35F};
constexpr std::array<float, k_curtain_rings> k_ring_alpha = {0.34F, 0.20F};
constexpr std::array<float, k_curtain_rings> k_ring_width_scale = {1.18F,
                                                                   1.42F};

constexpr float k_fog_r = 0.55F;
constexpr float k_fog_g = 0.58F;
constexpr float k_fog_b = 0.56F;

constexpr float k_mountain_inner_tiles = 0.45F;
constexpr float k_mountain_outer_tiles = 13.80F;
constexpr float k_mountain_peak_tiles = 18.50F;
constexpr float k_mountain_base_lift = 0.025F;

inline auto smoothstep(float edge0, float edge1, float value) -> float {
  if (edge0 == edge1) {
    return value >= edge1 ? 1.0F : 0.0F;
  }
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto boundary_height_at(float world_x, float world_z) -> float {
  auto &terrain_service = Game::Map::TerrainService::instance();
  const auto *height_map = terrain_service.get_height_map();
  if (!terrain_service.is_initialized() || height_map == nullptr) {
    return 0.0F;
  }
  return std::max(0.0F, height_map->getHeightAt(world_x, world_z));
}

struct MountainPatchConfig {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D axis_u{1.0F, 0.0F, 0.0F};
  QVector3D axis_v{0.0F, 0.0F, 1.0F};
  int u_segments = 1;
  int v_segments = 1;
};

struct BoundaryMountainConfig {
  float half_width = 0.0F;
  float half_height = 0.0F;
  float inner_offset = 0.0F;
  float mountain_band = 0.0F;
  float tile_size = 1.0F;
  float max_relief_height = 0.0F;
  Game::Map::MountainNoiseSettings noise_settings{};
};

struct BoundaryMountainSample {
  float height = 0.0F;
  float presence = 0.0F;
  float relief = 0.0F;
};

auto resolve_mountain_noise_settings(int width, int height)
    -> Game::Map::MountainNoiseSettings {
  auto const &terrain_service = Game::Map::TerrainService::instance();
  if (terrain_service.is_initialized()) {
    auto const &biome = terrain_service.biome_settings();
    return {biome.seed == 0U ? 1337U : biome.seed,
            std::clamp(biome.height_noise_frequency, 0.01F, 2.0F), 4};
  }

  return {static_cast<std::uint32_t>((width * 92821) ^ (height * 68917)) ^
              0x7F4A7C15U,
          0.07F, 4};
}

auto sample_boundary_mountain(float world_x, float world_z,
                              const BoundaryMountainConfig &config)
    -> BoundaryMountainSample {
  const float outside_x = std::max(0.0F, std::abs(world_x) - config.half_width);
  const float outside_z =
      std::max(0.0F, std::abs(world_z) - config.half_height);
  const float outside_distance =
      std::sqrt(outside_x * outside_x + outside_z * outside_z);
  const float depth_hint =
      std::clamp((outside_distance - config.inner_offset) /
                     std::max(config.mountain_band, 0.001F),
                 0.0F, 1.0F);

  const float warp_frequency =
      std::clamp(config.noise_settings.frequency * 0.55F, 0.01F, 1.25F);
  const int warp_octaves = std::max(
      2, Game::Map::clamp_noise_octaves(config.noise_settings.octaves) - 1);
  const float warp_strength = config.tile_size * (1.15F + depth_hint * 1.35F);
  const float warp_x = Game::Map::fbm_noise(
      world_x * warp_frequency, world_z * warp_frequency,
      config.noise_settings.seed ^ 0x4B1D5A37U, warp_octaves);
  const float warp_z = Game::Map::fbm_noise(
      world_x * warp_frequency, world_z * warp_frequency,
      config.noise_settings.seed ^ 0x91E10DA5U, warp_octaves);
  const float sample_x = world_x + (warp_x - 0.5F) * 2.0F * warp_strength;
  const float sample_z = world_z + (warp_z - 0.5F) * 2.0F * warp_strength;

  Game::Map::MountainNoiseSettings spread_settings = config.noise_settings;
  spread_settings.frequency =
      std::clamp(config.noise_settings.frequency * 0.36F, 0.01F, 0.45F);
  spread_settings.octaves = std::max(
      2, Game::Map::clamp_noise_octaves(config.noise_settings.octaves) - 1);
  const float spread_noise =
      Game::Map::sample_mountain_region(sample_x, sample_z, spread_settings);
  const float band_noise = Game::Map::fbm_noise(
      sample_x * std::clamp(spread_settings.frequency * 1.4F, 0.01F, 0.7F),
      sample_z * std::clamp(spread_settings.frequency * 1.4F, 0.01F, 0.7F),
      config.noise_settings.seed ^ 0x71A9C13DU, 3);
  const float cluster_noise = Game::Map::fbm_noise(
      sample_x * std::clamp(spread_settings.frequency * 0.9F, 0.01F, 0.4F),
      sample_z * std::clamp(spread_settings.frequency * 0.9F, 0.01F, 0.4F),
      config.noise_settings.seed ^ 0x18D42F7BU, 2);
  Game::Map::MountainNoiseSettings chain_settings = config.noise_settings;
  chain_settings.frequency =
      std::clamp(config.noise_settings.frequency * 0.16F, 0.006F, 0.12F);
  chain_settings.octaves = 3;
  const float chain_noise =
      Game::Map::sample_mountain_region(sample_x, sample_z, chain_settings);
  const float gap_noise = Game::Map::fbm_noise(
      sample_x * std::clamp(chain_settings.frequency * 2.2F, 0.01F, 0.22F),
      sample_z * std::clamp(chain_settings.frequency * 2.2F, 0.01F, 0.22F),
      config.noise_settings.seed ^ 0x5D8E123FU, 2);
  const float cluster =
      smoothstep(0.34F, 0.68F, spread_noise * 0.58F + cluster_noise * 0.42F);
  const float chain_presence = smoothstep(
      0.46F, 0.70F,
      chain_noise * 0.72F + spread_noise * 0.20F + cluster_noise * 0.08F);
  const float gap_presence =
      1.0F - smoothstep(0.58F, 0.84F, gap_noise * 0.78F + band_noise * 0.22F);
  const float sparsity =
      std::clamp(std::pow(chain_presence, 1.20F) * gap_presence, 0.0F, 1.0F);
  const float local_inner = std::clamp(
      config.inner_offset + (0.5F - spread_noise) * config.tile_size * 4.6F +
          (0.5F - cluster_noise) * config.tile_size * 2.2F +
          (1.0F - sparsity) * config.mountain_band * 0.18F,
      0.0F, config.inner_offset + config.mountain_band * 0.55F);
  const float local_band =
      config.mountain_band *
      std::clamp(0.72F + sparsity * 0.40F + band_noise * 0.38F, 0.58F, 1.55F);
  const float depth_t = std::clamp((outside_distance - local_inner) /
                                       std::max(local_band, 0.001F),
                                   0.0F, 1.0F);

  const float region = Game::Map::sample_mountain_region(sample_x, sample_z,
                                                         config.noise_settings);
  const float macro_frequency =
      std::clamp(config.noise_settings.frequency * 0.42F, 0.01F, 0.5F);
  const float macro = Game::Map::fbm_noise(
      sample_x * macro_frequency, sample_z * macro_frequency,
      config.noise_settings.seed ^ 0x2C9F51E3U, 3);
  const float shape = std::clamp(region * 0.72F + macro * 0.28F, 0.0F, 1.0F);
  const float crag = Game::Map::fbm_noise(
      sample_x *
          std::clamp(config.noise_settings.frequency * 4.2F, 0.08F, 1.4F),
      sample_z *
          std::clamp(config.noise_settings.frequency * 4.2F, 0.08F, 1.4F),
      config.noise_settings.seed ^ 0xA4215F71U, 4);
  const float tooth = Game::Map::fbm_noise(
      sample_x *
          std::clamp(config.noise_settings.frequency * 9.0F, 0.14F, 2.4F),
      sample_z *
          std::clamp(config.noise_settings.frequency * 9.0F, 0.14F, 2.4F),
      config.noise_settings.seed ^ 0xD1B54A32U, 2);
  const float jaggedness =
      std::clamp(0.62F + crag * 0.42F + (tooth - 0.5F) * 0.36F, 0.42F, 1.35F);

  const float ridge_center = std::clamp(
      0.20F + shape * 0.34F + (band_noise - 0.5F) * 0.14F, 0.12F, 0.70F);
  const float ridge_width =
      0.16F + (1.0F - shape) * 0.10F + (1.0F - sparsity) * 0.04F;
  const float primary_ridge =
      smoothstep(0.02F, ridge_center, depth_t) *
      (1.0F - smoothstep(ridge_center + ridge_width, 0.96F, depth_t));
  const float secondary_center = std::clamp(
      ridge_center + 0.20F + (cluster_noise - 0.5F) * 0.10F, 0.28F, 0.88F);
  const float secondary_width = 0.11F + (1.0F - shape) * 0.05F;
  const float secondary_ridge =
      smoothstep(0.10F, secondary_center, depth_t) *
      (1.0F - smoothstep(secondary_center + secondary_width, 1.0F, depth_t));
  const float inner_escarpment =
      smoothstep(0.01F, 0.17F, depth_t) *
      (1.0F - smoothstep(0.26F + shape * 0.10F, 0.58F, depth_t));
  const float shoulder_profile = smoothstep(0.0F, 0.14F, depth_t) *
                                 (1.0F - smoothstep(0.84F, 1.0F, depth_t));
  const float relief_mask = std::clamp(
      (primary_ridge * (0.58F + 0.48F * shape) +
       secondary_ridge * (0.18F + 0.18F * cluster) +
       inner_escarpment * (0.26F + 0.22F * crag) + shoulder_profile * 0.16F) *
          std::clamp(0.62F + sparsity * 0.45F, 0.0F, 1.15F) *
          std::clamp(0.42F + cluster * 0.85F, 0.0F, 1.15F) * jaggedness,
      0.0F, 1.0F);

  const float boundary_blend = std::pow(std::max(0.0F, 1.0F - depth_t), 2.1F);
  const float base_height =
      boundary_height_at(world_x, world_z) * boundary_blend +
      config.tile_size * k_mountain_base_lift;
  const float height_scale = std::clamp(
      0.72F + sparsity * 0.34F + cluster * 0.16F + (crag - 0.5F) * 0.24F, 0.48F,
      1.28F);
  const float relief = config.max_relief_height * height_scale * relief_mask;
  const float presence =
      std::clamp((0.42F + sparsity * 0.58F) * (0.42F + 0.58F * cluster) *
                     smoothstep(0.025F, 0.13F, relief_mask),
                 0.0F, 1.0F);
  return {base_height + relief, presence, relief};
}

inline auto clamp01(const QVector3D &color) -> QVector3D {
  return {std::clamp(color.x(), 0.0F, 1.0F), std::clamp(color.y(), 0.0F, 1.0F),
          std::clamp(color.z(), 0.0F, 1.0F)};
}

auto build_boundary_mountain_params(float tile_size, int width,
                                    int height) -> TerrainChunkParams {
  TerrainChunkParams params;
  auto const &terrain_service = Game::Map::TerrainService::instance();
  Game::Map::BiomeSettings biome;
  if (terrain_service.is_initialized()) {
    biome = terrain_service.biome_settings();
  }

  const auto profiles = Game::Map::make_biome_profiles(biome);
  const auto &surface_profile = profiles.surface;
  const auto &climate_profile = profiles.climate;

  params.grass_primary = clamp01(surface_profile.grass_primary * 0.97F);
  params.grass_secondary = clamp01(surface_profile.grass_secondary * 0.93F);
  params.grass_dry = clamp01(surface_profile.grass_dry * 0.90F);
  params.soil_color = clamp01(surface_profile.soil_color * 0.82F);
  params.rock_low = clamp01(surface_profile.rock_low);
  params.rock_high = clamp01(surface_profile.rock_high);

  params.tint = QVector3D(0.96F, 0.98F, 0.96F);
  params.tile_size = std::max(0.25F, tile_size);
  params.macro_noise_scale =
      std::max(0.012F, surface_profile.terrain_macro_noise_scale * 0.95F);
  params.detail_noise_scale =
      std::max(0.045F, surface_profile.terrain_detail_noise_scale * 1.15F);
  params.slope_rock_threshold =
      std::clamp(surface_profile.terrain_rock_threshold + 0.30F, 0.40F, 0.90F);
  params.slope_rock_sharpness =
      std::clamp(surface_profile.terrain_rock_sharpness + 1.5F, 2.0F, 6.0F);
  params.soil_blend_height = surface_profile.terrain_soil_height - 1.25F;
  params.soil_blend_sharpness =
      std::clamp(surface_profile.terrain_soil_sharpness * 0.75F, 1.5F, 5.0F);

  constexpr float k_non_playable_margin_tiles = 48.0F;
  const float margin = k_non_playable_margin_tiles * params.tile_size;
  const float span_x =
      width > 0 ? static_cast<float>(width) * params.tile_size + margin * 2.0F
                : params.tile_size;
  const float span_z =
      height > 0 ? static_cast<float>(height) * params.tile_size + margin * 2.0F
                 : params.tile_size;
  const auto seed = static_cast<float>(biome.seed % 1024U);
  params.noise_offset =
      QVector2D(span_x * 0.37F + seed * 0.21F, span_z * 0.43F + seed * 0.17F);
  params.noise_angle = std::fmod(seed * 0.6180339887F, 1.0F) * 6.28318530718F;

  if (surface_profile.ground_irregularity_enabled) {
    params.height_noise_strength = std::clamp(
        surface_profile.irregularity_amplitude * 0.85F, 0.15F, 0.70F);
    params.height_noise_frequency =
        std::max(0.45F, surface_profile.irregularity_scale * 2.5F);
  } else {
    params.height_noise_strength = std::clamp(
        surface_profile.height_noise_amplitude * 0.22F, 0.10F, 0.20F);
    params.height_noise_frequency =
        std::max(0.6F, surface_profile.height_noise_frequency * 1.05F);
  }
  params.ambient_boost = surface_profile.terrain_ambient_boost * 0.95F;
  params.rock_detail_strength =
      surface_profile.terrain_rock_detail_strength * 0.42F;
  params.light_direction = QVector3D(0.35F, 0.85F, 0.42F).normalized();
  params.is_ground_plane = true;
  params.snow_coverage = std::clamp(climate_profile.snow_coverage, 0.0F, 1.0F);
  params.moisture_level =
      std::clamp(climate_profile.moisture_level, 0.0F, 1.0F);
  params.crack_intensity =
      std::clamp(climate_profile.crack_intensity, 0.0F, 1.0F);
  params.rock_exposure =
      std::clamp(climate_profile.rock_exposure * 0.72F +
                     climate_profile.crack_intensity * 0.18F +
                     (1.0F - climate_profile.moisture_level) * 0.08F,
                 0.04F, 0.82F);
  params.grass_saturation =
      std::clamp(climate_profile.grass_saturation, 0.0F, 1.5F);
  params.soil_roughness =
      std::clamp(climate_profile.soil_roughness, 0.0F, 1.0F);
  params.micro_bump_amp =
      std::clamp(0.070F + climate_profile.soil_roughness * 0.085F +
                     params.rock_exposure * 0.055F,
                 0.07F, 0.22F);
  params.micro_bump_freq =
      std::clamp(2.4F + climate_profile.soil_roughness * 2.3F +
                     climate_profile.crack_intensity * 1.1F,
                 2.2F, 6.2F);
  params.micro_normal_weight =
      std::clamp(0.68F + params.rock_exposure * 0.20F +
                     climate_profile.moisture_level * 0.08F,
                 0.68F, 0.94F);
  params.albedo_jitter =
      std::clamp(0.070F + climate_profile.soil_roughness * 0.060F +
                     climate_profile.crack_intensity * 0.035F,
                 0.06F, 0.18F);
  params.snow_color = clamp01(climate_profile.snow_color);
  return params;
}

void append_patch_mesh(std::vector<Vertex> &vertices,
                       std::vector<unsigned int> &indices,
                       const MountainPatchConfig &config,
                       const BoundaryMountainConfig &mountain_config) {
  if (config.u_segments <= 0 || config.v_segments <= 0) {
    return;
  }

  const int columns = config.u_segments + 1;
  const int rows = config.v_segments + 1;
  const std::size_t base_index = vertices.size();
  vertices.resize(base_index + static_cast<std::size_t>(columns * rows));
  std::vector<float> cell_presence(static_cast<std::size_t>(columns * rows),
                                   0.0F);
  std::vector<float> cell_relief(static_cast<std::size_t>(columns * rows),
                                 0.0F);

  const QVector3D flat_normal(0.0F, 1.0F, 0.0F);
  const float tex_scale = 0.16F / std::max(mountain_config.tile_size, 0.25F);

  for (int v = 0; v <= config.v_segments; ++v) {
    const float fv =
        static_cast<float>(v) / static_cast<float>(config.v_segments);
    for (int u = 0; u <= config.u_segments; ++u) {
      const float fu =
          static_cast<float>(u) / static_cast<float>(config.u_segments);
      QVector3D position =
          config.origin + config.axis_u * fu + config.axis_v * fv;
      const BoundaryMountainSample sample =
          sample_boundary_mountain(position.x(), position.z(), mountain_config);
      position.setY(sample.height);

      Vertex vertex{};
      vertex.position = {position.x(), position.y(), position.z()};
      vertex.normal = {flat_normal.x(), flat_normal.y(), flat_normal.z()};
      vertex.tex_coord = {position.x() * tex_scale, position.z() * tex_scale};

      const std::size_t local_index = static_cast<std::size_t>(v * columns + u);
      const std::size_t vertex_index = base_index + local_index;
      vertices[vertex_index] = vertex;
      cell_presence[local_index] = sample.presence;
      cell_relief[local_index] = sample.relief;
    }
  }

  const bool flip_winding =
      QVector3D::crossProduct(config.axis_v, config.axis_u).y() < 0.0F;

  for (int v = 0; v < config.v_segments; ++v) {
    for (int u = 0; u < config.u_segments; ++u) {
      const std::size_t local_a = static_cast<std::size_t>(v * columns + u);
      const std::size_t local_b = local_a + 1U;
      const std::size_t local_c = local_a + static_cast<std::size_t>(columns);
      const std::size_t local_d = local_c + 1U;
      const float avg_presence =
          (cell_presence[local_a] + cell_presence[local_b] +
           cell_presence[local_c] + cell_presence[local_d]) *
          0.25F;
      const float max_presence =
          std::max(std::max(cell_presence[local_a], cell_presence[local_b]),
                   std::max(cell_presence[local_c], cell_presence[local_d]));
      const float max_relief =
          std::max(std::max(cell_relief[local_a], cell_relief[local_b]),
                   std::max(cell_relief[local_c], cell_relief[local_d]));
      const float avg_relief = (cell_relief[local_a] + cell_relief[local_b] +
                                cell_relief[local_c] + cell_relief[local_d]) *
                               0.25F;
      if ((max_presence < 0.035F) && (avg_presence < 0.02F) &&
          (max_relief < mountain_config.tile_size * 0.35F) &&
          (avg_relief < mountain_config.tile_size * 0.18F)) {
        continue;
      }

      const unsigned int a = static_cast<unsigned int>(base_index + local_a);
      const unsigned int b = a + 1U;
      const unsigned int c = a + static_cast<unsigned int>(columns);
      const unsigned int d = c + 1U;

      if (flip_winding) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
        indices.push_back(b);
        indices.push_back(d);
        indices.push_back(c);
      } else {
        indices.push_back(a);
        indices.push_back(c);
        indices.push_back(b);
        indices.push_back(b);
        indices.push_back(c);
        indices.push_back(d);
      }
    }
  }
}

void compute_vertex_normals(std::vector<Vertex> &vertices,
                            const std::vector<unsigned int> &indices) {
  std::vector<QVector3D> accumulated(vertices.size(),
                                     QVector3D(0.0F, 0.0F, 0.0F));

  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    const unsigned int ia = indices[i];
    const unsigned int ib = indices[i + 1];
    const unsigned int ic = indices[i + 2];
    if (ia >= vertices.size() || ib >= vertices.size() ||
        ic >= vertices.size()) {
      continue;
    }

    const QVector3D a(vertices[ia].position[0], vertices[ia].position[1],
                      vertices[ia].position[2]);
    const QVector3D b(vertices[ib].position[0], vertices[ib].position[1],
                      vertices[ib].position[2]);
    const QVector3D c(vertices[ic].position[0], vertices[ic].position[1],
                      vertices[ic].position[2]);
    const QVector3D normal = QVector3D::crossProduct(b - a, c - a);
    if (normal.lengthSquared() <= 0.0F) {
      continue;
    }
    accumulated[ia] += normal;
    accumulated[ib] += normal;
    accumulated[ic] += normal;
  }

  for (std::size_t i = 0; i < vertices.size(); ++i) {
    QVector3D normal = accumulated[i];
    if (normal.lengthSquared() <= 0.0F) {
      normal = QVector3D(0.0F, 1.0F, 0.0F);
    } else {
      normal.normalize();
    }
    vertices[i].normal = {normal.x(), normal.y(), normal.z()};
  }
}

auto compute_geometry_signature(const std::vector<Vertex> &vertices)
    -> std::uint64_t {
  std::uint64_t signature = 1469598103934665603ULL;
  constexpr std::uint64_t k_prime = 1099511628211ULL;

  for (const Vertex &vertex : vertices) {
    const auto quantize = [](float value) -> std::int64_t {
      return static_cast<std::int64_t>(std::llround(value * 1000.0));
    };

    const std::uint64_t px =
        static_cast<std::uint64_t>(quantize(vertex.position[0]));
    const std::uint64_t py =
        static_cast<std::uint64_t>(quantize(vertex.position[1]));
    const std::uint64_t pz =
        static_cast<std::uint64_t>(quantize(vertex.position[2]));

    signature ^= px;
    signature *= k_prime;
    signature ^= py;
    signature *= k_prime;
    signature ^= pz;
    signature *= k_prime;
  }

  return signature;
}

} // namespace

void MapBoundaryFogRenderer::configure(int width, int height, float tile_size) {
  m_width = std::max(0, width);
  m_height = std::max(0, height);
  m_tile_size = std::max(0.0001F, tile_size);
  build_cards();
  build_mountains();
}

void MapBoundaryFogRenderer::submit(Renderer &renderer,
                                    ResourceManager *resources) {
  if ((resources == nullptr) ||
      (m_cards.empty() && m_mountain_indices.empty())) {
    return;
  }

  Shader *previous_shader = renderer.get_current_shader();
  renderer.set_current_shader(nullptr);

  if (!m_mountain_indices.empty()) {
    ensure_mountain_mesh();
    if (m_mountain_mesh != nullptr) {
      TerrainSurfaceCmd cmd;
      cmd.mesh = m_mountain_mesh.get();
      cmd.model = k_identity_matrix;
      cmd.params =
          build_boundary_mountain_params(m_tile_size, m_width, m_height);
      cmd.sort_key = 0x0080U;
      cmd.depth_write = true;
      renderer.terrain_surface(cmd);
    }
  }

  if (m_cards.empty()) {
    renderer.set_current_shader(previous_shader);
    return;
  }

  Mesh *quad = resources->quad();
  if (quad == nullptr) {
    renderer.set_current_shader(previous_shader);
    return;
  }

  Shader *gas_shader = renderer.get_shader("boundary_gas");
  if (gas_shader == nullptr) {
    gas_shader = renderer.load_shader("boundary_gas",
                                      ":/assets/shaders/boundary_gas.vert",
                                      ":/assets/shaders/boundary_gas.frag");
  }

  if (gas_shader != nullptr) {
    renderer.set_current_shader(gas_shader);
  }

  for (const BoundaryCard &card : m_cards) {
    QMatrix4x4 model;
    model.translate(card.center);
    model.rotate(card.yaw_degrees, 0.0F, 1.0F, 0.0F);
    model.scale(card.size.x(), card.size.y(), 1.0F);
    renderer.mesh(quad, model, card.color, nullptr, card.alpha);

    QMatrix4x4 backface_model = model;
    backface_model.rotate(180.0F, 0.0F, 1.0F, 0.0F);
    renderer.mesh(quad, backface_model, card.color, nullptr, card.alpha);
  }

  renderer.set_current_shader(previous_shader);
}

void MapBoundaryFogRenderer::build_cards() {
  m_cards.clear();

  if (m_width <= 0 || m_height <= 0) {
    return;
  }

  const float half_w = static_cast<float>(m_width) * 0.5F * m_tile_size;
  const float half_h = static_cast<float>(m_height) * 0.5F * m_tile_size;
  const float band_tiles =
      static_cast<float>(k_clear_outside_tiles + k_band_outside);
  const float band_world = band_tiles * m_tile_size;
  const float width_world = static_cast<float>(m_width) * m_tile_size;
  const float height_world = static_cast<float>(m_height) * m_tile_size;
  const float x_step = width_world / static_cast<float>(k_cards_per_side);
  const float z_step = height_world / static_cast<float>(k_cards_per_side);

  m_cards.reserve(
      static_cast<std::size_t>((4 * k_cards_per_side + 4) * k_curtain_rings));

  auto emit_card = [&](const QVector3D &center, float width, float height,
                       float yaw, float alpha) {
    BoundaryCard card;
    card.center = center;
    card.size = QVector2D(width, height);
    card.yaw_degrees = yaw;
    card.alpha = alpha;
    card.color = QVector3D(k_fog_r, k_fog_g, k_fog_b);
    m_cards.push_back(card);
  };

  for (int ring = 0; ring < k_curtain_rings; ++ring) {
    const float ring_offset = band_world * k_ring_offset_scale[ring];
    const float wall_height = band_world * k_ring_height_scale[ring];
    const float center_y = wall_height * 0.5F - band_world * 0.10F;
    const float alpha = k_ring_alpha[ring];
    const float side_width_scale = k_ring_width_scale[ring];
    const float corner_span = band_world * (1.45F + 0.20F * ring);
    const float corner_height = wall_height * (1.08F + 0.06F * ring);
    const float corner_y = corner_height * 0.5F - band_world * 0.10F;

    for (int i = 0; i < k_cards_per_side; ++i) {
      const float tx =
          (static_cast<float>(i) + 0.5F) / static_cast<float>(k_cards_per_side);
      const float x = tx * width_world - half_w;
      emit_card(QVector3D(x, center_y, -half_h - ring_offset),
                x_step * side_width_scale, wall_height, 0.0F, alpha);
      emit_card(QVector3D(x, center_y, half_h + ring_offset),
                x_step * side_width_scale, wall_height, 180.0F, alpha);
    }

    for (int i = 0; i < k_cards_per_side; ++i) {
      const float tz =
          (static_cast<float>(i) + 0.5F) / static_cast<float>(k_cards_per_side);
      const float z = tz * height_world - half_h;
      emit_card(QVector3D(-half_w - ring_offset, center_y, z),
                z_step * side_width_scale, wall_height, -90.0F, alpha);
      emit_card(QVector3D(half_w + ring_offset, center_y, z),
                z_step * side_width_scale, wall_height, 90.0F, alpha);
    }

    emit_card(QVector3D(-half_w - ring_offset, corner_y, -half_h - ring_offset),
              corner_span, corner_height, -45.0F, alpha);
    emit_card(QVector3D(half_w + ring_offset, corner_y, -half_h - ring_offset),
              corner_span, corner_height, 45.0F, alpha);
    emit_card(QVector3D(-half_w - ring_offset, corner_y, half_h + ring_offset),
              corner_span, corner_height, -135.0F, alpha);
    emit_card(QVector3D(half_w + ring_offset, corner_y, half_h + ring_offset),
              corner_span, corner_height, 135.0F, alpha);
  }
}

void MapBoundaryFogRenderer::build_mountains() {
  m_mountain_vertices.clear();
  m_mountain_indices.clear();
  m_mountain_geometry_signature = 0U;
  m_mountain_min_height = 0.0F;
  m_mountain_max_height = 0.0F;
  m_mountain_mesh.reset();

  if (m_width <= 0 || m_height <= 0) {
    return;
  }

  const float width_world = static_cast<float>(m_width) * m_tile_size;
  const float height_world = static_cast<float>(m_height) * m_tile_size;
  const float half_w = width_world * 0.5F;
  const float half_h = height_world * 0.5F;
  const float inner_offset = k_mountain_inner_tiles * m_tile_size;
  const float outer_offset = k_mountain_outer_tiles * m_tile_size;
  const float mountain_band =
      std::max(m_tile_size, outer_offset - inner_offset);
  const float max_relief_height = k_mountain_peak_tiles * m_tile_size;
  const float extended_width_world = width_world + inner_offset * 2.0F;
  const float extended_height_world = height_world + inner_offset * 2.0F;

  const int side_x_segments = std::clamp(
      static_cast<int>(std::ceil(extended_width_world / (m_tile_size * 1.25F))),
      12, 96);
  const int side_z_segments =
      std::clamp(static_cast<int>(
                     std::ceil(extended_height_world / (m_tile_size * 1.25F))),
                 12, 96);
  const int corner_segments = std::clamp(k_mountain_depth_segments + 2, 8, 24);

  const BoundaryMountainConfig mountain_config{
      half_w,
      half_h,
      inner_offset,
      mountain_band,
      m_tile_size,
      max_relief_height,
      resolve_mountain_noise_settings(m_width, m_height)};

  append_patch_mesh(
      m_mountain_vertices, m_mountain_indices,
      MountainPatchConfig{
          QVector3D(-half_w - inner_offset, 0.0F, -half_h - inner_offset),
          QVector3D(extended_width_world, 0.0F, 0.0F),
          QVector3D(0.0F, 0.0F, -mountain_band), side_x_segments,
          k_mountain_depth_segments},
      mountain_config);
  append_patch_mesh(
      m_mountain_vertices, m_mountain_indices,
      MountainPatchConfig{
          QVector3D(-half_w - inner_offset, 0.0F, half_h + inner_offset),
          QVector3D(extended_width_world, 0.0F, 0.0F),
          QVector3D(0.0F, 0.0F, mountain_band), side_x_segments,
          k_mountain_depth_segments},
      mountain_config);
  append_patch_mesh(
      m_mountain_vertices, m_mountain_indices,
      MountainPatchConfig{
          QVector3D(-half_w - inner_offset, 0.0F, -half_h - inner_offset),
          QVector3D(0.0F, 0.0F, extended_height_world),
          QVector3D(-mountain_band, 0.0F, 0.0F), side_z_segments,
          k_mountain_depth_segments},
      mountain_config);
  append_patch_mesh(
      m_mountain_vertices, m_mountain_indices,
      MountainPatchConfig{
          QVector3D(half_w + inner_offset, 0.0F, -half_h - inner_offset),
          QVector3D(0.0F, 0.0F, extended_height_world),
          QVector3D(mountain_band, 0.0F, 0.0F), side_z_segments,
          k_mountain_depth_segments},
      mountain_config);

  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(-half_w - outer_offset, 0.0F,
                                                  -half_h - outer_offset),
                                        QVector3D(mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, mountain_band),
                                        corner_segments, corner_segments},
                    mountain_config);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(half_w + outer_offset, 0.0F,
                                                  -half_h - outer_offset),
                                        QVector3D(-mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, mountain_band),
                                        corner_segments, corner_segments},
                    mountain_config);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(-half_w - outer_offset, 0.0F,
                                                  half_h + outer_offset),
                                        QVector3D(mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, -mountain_band),
                                        corner_segments, corner_segments},
                    mountain_config);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(half_w + outer_offset, 0.0F,
                                                  half_h + outer_offset),
                                        QVector3D(-mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, -mountain_band),
                                        corner_segments, corner_segments},
                    mountain_config);

  compute_vertex_normals(m_mountain_vertices, m_mountain_indices);
  if (!m_mountain_vertices.empty()) {
    m_mountain_min_height = m_mountain_vertices.front().position[1];
    m_mountain_max_height = m_mountain_min_height;
    for (const Vertex &vertex : m_mountain_vertices) {
      m_mountain_min_height =
          std::min(m_mountain_min_height, vertex.position[1]);
      m_mountain_max_height =
          std::max(m_mountain_max_height, vertex.position[1]);
    }
  }
  m_mountain_geometry_signature =
      compute_geometry_signature(m_mountain_vertices);
}

void MapBoundaryFogRenderer::ensure_mountain_mesh() {
  if ((m_mountain_mesh != nullptr) || m_mountain_indices.empty() ||
      m_mountain_vertices.empty()) {
    return;
  }

  m_mountain_mesh =
      std::make_unique<Mesh>(m_mountain_vertices, m_mountain_indices);
}

} // namespace Render::GL
