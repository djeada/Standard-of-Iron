#include "map_boundary_fog_renderer.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "game/map/terrain.h"
#include "game/map/terrain_noise.h"
#include "game/map/terrain_service.h"
#include "render/scene_renderer.h"
#include "terrain_renderer.h"

namespace Render::GL {

namespace {

const QMatrix4x4 k_identity_matrix;

constexpr int k_clear_outside_tiles = 2;
constexpr int k_band_outside = 16;
constexpr int k_cards_per_side = 3;
constexpr int k_curtain_rings = 2;

constexpr std::array<float, k_curtain_rings> k_ring_offset_scale = {0.78F, 1.36F};
constexpr std::array<float, k_curtain_rings> k_ring_height_scale = {1.70F, 2.30F};
constexpr std::array<float, k_curtain_rings> k_ring_alpha = {0.30F, 0.20F};
constexpr std::array<float, k_curtain_rings> k_ring_width_scale = {1.18F, 1.42F};

constexpr float k_fog_r = 0.55F;
constexpr float k_fog_g = 0.58F;
constexpr float k_fog_b = 0.56F;

constexpr float k_foothill_tiles = 4.5F;
constexpr float k_mountain_outer_tiles = 28.0F;
constexpr float k_mountain_peak_tiles = 21.0F;
constexpr float k_mountain_edge_step_tiles = 0.75F;
constexpr int k_mountain_depth_segments = 40;
constexpr int k_mountain_min_side_segments = 12;
constexpr int k_mountain_max_side_segments = 768;

inline auto smoothstep(float edge0, float edge1, float value) -> float {
  if (edge0 == edge1) {
    return value >= edge1 ? 1.0F : 0.0F;
  }
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

inline auto ridge_bump(float t, float centre, float width) -> float {
  const float tent =
      std::clamp(1.0F - std::abs(t - centre) / std::max(width, 0.001F), 0.0F, 1.0F);
  return std::pow(tent, 1.25F);
}

constexpr float k_card_camera_clearance = 12.0F;

auto card_engulfs_camera(const QVector3D& card_center,
                         float card_width,
                         const QVector3D& camera_position) -> bool {
  const float dx = camera_position.x() - card_center.x();
  const float dz = camera_position.z() - card_center.z();
  const float clearance = card_width * 0.5F + k_card_camera_clearance;
  return dx * dx + dz * dz < clearance * clearance;
}

auto edge_height_at(const Game::Map::TerrainHeightMap* height_map,
                    float world_x,
                    float world_z) -> float {
  if (height_map == nullptr) {
    return 0.0F;
  }
  const int width = height_map->get_width();
  const int height = height_map->get_height();
  const auto& data = height_map->get_height_data();
  if (width <= 0 || height <= 0 ||
      data.size() <
          static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
    return 0.0F;
  }
  const float tile = std::max(height_map->get_tile_size(), 0.0001F);
  const float gx =
      std::clamp(world_x / tile + (static_cast<float>(width) * 0.5F - 0.5F),
                 0.0F,
                 static_cast<float>(width - 1));
  const float gz =
      std::clamp(world_z / tile + (static_cast<float>(height) * 0.5F - 0.5F),
                 0.0F,
                 static_cast<float>(height - 1));
  const int x0 = std::min(static_cast<int>(std::floor(gx)), width - 1);
  const int z0 = std::min(static_cast<int>(std::floor(gz)), height - 1);
  const int x1 = std::min(x0 + 1, width - 1);
  const int z1 = std::min(z0 + 1, height - 1);
  const float tx = gx - static_cast<float>(x0);
  const float tz = gz - static_cast<float>(z0);
  const auto at = [&](int x, int z) {
    return data[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x)];
  };
  const float h0 = at(x0, z0) * (1.0F - tx) + at(x1, z0) * tx;
  const float h1 = at(x0, z1) * (1.0F - tx) + at(x1, z1) * tx;
  return h0 * (1.0F - tz) + h1 * tz;
}

struct MountainPatchConfig {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D axis_u{1.0F, 0.0F, 0.0F};
  QVector3D axis_v{0.0F, 0.0F, 1.0F};
  int u_segments = 1;
  int v_segments = 1;

  bool seam_row = false;
};

struct BoundaryMountainConfig {
  float edge_half_width = 0.0F;
  float edge_half_height = 0.0F;
  float foothill = 0.0F;
  float band = 0.0F;
  float tile_size = 1.0F;
  float peak = 0.0F;

  float detail_frequency_cap = 1.0F;
  Game::Map::MountainNoiseSettings noise_settings{};
  const Game::Map::TerrainHeightMap* height_map = nullptr;
};

struct BoundaryMountainSample {
  float height = 0.0F;
  float foot = 0.0F;
};

auto resolve_mountain_noise_settings(const Game::Map::TerrainService& terrain_service,
                                     int width,
                                     int height) -> Game::Map::MountainNoiseSettings {
  if (terrain_service.is_initialized()) {
    auto const& biome = terrain_service.biome_settings();
    return {biome.seed == 0U ? 1337U : biome.seed,
            std::clamp(biome.height_noise_frequency, 0.01F, 2.0F),
            4};
  }

  return {static_cast<std::uint32_t>((width * 92821) ^ (height * 68917)) ^ 0x7F4A7C15U,
          0.07F,
          4};
}

auto sample_boundary_mountain(float world_x,
                              float world_z,
                              const BoundaryMountainConfig& config)
    -> BoundaryMountainSample {
  const float tile = config.tile_size;
  const float outside_x = std::max(0.0F, std::abs(world_x) - config.edge_half_width);
  const float outside_z = std::max(0.0F, std::abs(world_z) - config.edge_half_height);
  const float distance = std::sqrt(outside_x * outside_x + outside_z * outside_z);
  const float band = std::max(config.band, 0.001F);
  const float t = std::clamp(distance / band, 0.0F, 1.0F);
  const float rise = smoothstep(config.foothill, band * 0.82F, distance);

  const auto& noise = config.noise_settings;
  const int octaves = Game::Map::clamp_noise_octaves(noise.octaves);

  const float warp_frequency = std::clamp(noise.frequency * 0.55F, 0.01F, 1.25F);
  const float warp_strength = tile * (0.9F + 1.6F * t);
  const float warp_x = Game::Map::fbm_noise(world_x * warp_frequency,
                                            world_z * warp_frequency,
                                            noise.seed ^ 0x4B1D5A37U,
                                            std::max(2, octaves - 1));
  const float warp_z = Game::Map::fbm_noise(world_x * warp_frequency,
                                            world_z * warp_frequency,
                                            noise.seed ^ 0x91E10DA5U,
                                            std::max(2, octaves - 1));
  const float sx = world_x + (warp_x - 0.5F) * 2.0F * warp_strength;
  const float sz = world_z + (warp_z - 0.5F) * 2.0F * warp_strength;

  Game::Map::MountainNoiseSettings chain_settings = noise;
  chain_settings.frequency = std::clamp(noise.frequency * 0.16F, 0.006F, 0.12F);
  chain_settings.octaves = 3;
  const float chain = Game::Map::sample_mountain_region(sx, sz, chain_settings);
  const float pass_frequency =
      std::clamp(chain_settings.frequency * 2.2F, 0.01F, 0.22F);
  const float pass_noise = Game::Map::fbm_noise(
      sx * pass_frequency, sz * pass_frequency, noise.seed ^ 0x5D8E123FU, 2);
  const float pass = smoothstep(0.60F, 0.86F, pass_noise);
  const float cluster_frequency = std::clamp(noise.frequency * 0.32F, 0.01F, 0.4F);
  const float cluster = Game::Map::fbm_noise(
      sx * cluster_frequency, sz * cluster_frequency, noise.seed ^ 0x18D42F7BU, 2);
  const float crag_frequency = std::min(std::clamp(noise.frequency * 2.6F, 0.05F, 0.9F),
                                        config.detail_frequency_cap * 0.6F);
  const float crag = Game::Map::fbm_noise(
      sx * crag_frequency, sz * crag_frequency, noise.seed ^ 0xA4215F71U, 4);
  const float tooth_frequency = std::min(
      std::clamp(noise.frequency * 5.0F, 0.10F, 1.6F), config.detail_frequency_cap);
  const float tooth = Game::Map::fbm_noise(
      sx * tooth_frequency, sz * tooth_frequency, noise.seed ^ 0xD1B54A32U, 2);
  const float roll_frequency = std::clamp(noise.frequency * 1.6F, 0.02F, 0.6F);
  const float roll = Game::Map::fbm_noise(
      sx * roll_frequency, sz * roll_frequency, noise.seed ^ 0x2C9F51E3U, 3);

  const float edge_height = edge_height_at(config.height_map, world_x, world_z);
  const float carry = 1.0F - smoothstep(0.0F, config.foothill * 1.6F, distance);
  float height = edge_height * carry;

  const float seam = smoothstep(0.0F, tile * 0.9F, distance);
  const float foothill_t = smoothstep(0.0F, config.foothill, distance);
  height += (roll - 0.5F) * 2.0F * tile * (0.30F + 1.10F * foothill_t) * seam *
            (1.0F - 0.6F * rise);

  const float massif = 0.55F + 0.45F * chain;
  const float climb = smoothstep(tile * 0.5F, band * 0.5F, distance);
  const float floor_height =
      config.peak *
      (0.12F * climb + 0.48F * std::pow(rise, 1.3F) * massif * (1.0F - 0.35F * pass));

  const float front_centre = std::clamp(0.40F + (cluster - 0.5F) * 0.16F, 0.30F, 0.52F);
  const float front_width = 0.17F + 0.06F * (1.0F - chain);
  const float front = ridge_bump(t, front_centre, front_width) * config.peak * 0.55F *
                      (0.50F + 0.50F * chain) * (1.0F - 0.55F * pass);
  const float back_centre = std::clamp(0.76F + (cluster - 0.5F) * 0.10F, 0.66F, 0.86F);
  const float back = ridge_bump(t, back_centre, 0.20F) * config.peak * 0.70F *
                     (0.62F + 0.38F * chain) * (1.0F - 0.40F * pass);
  const float wall = smoothstep(0.86F, 1.0F, t) * config.peak * 0.40F;
  const float relief = floor_height + front + back + wall;

  const float crag_amp =
      config.peak * 0.12F *
      smoothstep(0.05F, 0.45F, relief / std::max(config.peak, 0.001F));
  const float detail =
      ((crag - 0.5F) * 0.70F + (tooth - 0.5F) * 0.30F) * 2.0F * crag_amp;

  height += relief + detail;

  const float foot =
      1.0F - smoothstep(config.foothill * 0.4F, config.foothill * 2.6F, distance);
  return {height, std::clamp(foot, 0.0F, 1.0F)};
}

inline auto clamp01(const QVector3D& color) -> QVector3D {
  return {std::clamp(color.x(), 0.0F, 1.0F),
          std::clamp(color.y(), 0.0F, 1.0F),
          std::clamp(color.z(), 0.0F, 1.0F)};
}

auto build_boundary_mountain_params(const Game::Map::TerrainService& terrain_service,
                                    float tile_size) -> TerrainChunkParams {
  Game::Map::BiomeSettings biome;
  if (terrain_service.is_initialized()) {
    biome = terrain_service.biome_settings();
  }
  const auto profiles = Game::Map::make_biome_profiles(biome);
  TerrainChunkParams params =
      make_terrain_chunk_params(biome,
                                profiles.surface,
                                profiles.climate,
                                tile_size,
                                biome.seed,
                                Game::Map::TerrainType::Mountain,
                                1.0F);
  params.tint = clamp01(params.tint);
  params.is_ground_plane = false;
  return params;
}

struct PositionKey {
  std::int64_t x;
  std::int64_t y;
  std::int64_t z;
  auto operator==(const PositionKey& other) const -> bool {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct PositionKeyHash {
  auto operator()(const PositionKey& key) const -> std::size_t {
    std::uint64_t h = 1469598103934665603ULL;
    for (const std::int64_t v : {key.x, key.y, key.z}) {
      h ^= static_cast<std::uint64_t>(v);
      h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
  }
};

auto position_key(const std::array<float, 3>& position) -> PositionKey {
  const auto quantize = [](float value) -> std::int64_t {
    return static_cast<std::int64_t>(std::llround(value * 1000.0));
  };
  return {quantize(position[0]), quantize(position[1]), quantize(position[2])};
}

using NormalAccumulator = std::unordered_map<PositionKey, QVector3D, PositionKeyHash>;

void append_patch_mesh(std::vector<Vertex>& vertices,
                       std::vector<unsigned int>& indices,
                       NormalAccumulator& extra_normals,
                       const MountainPatchConfig& config,
                       const BoundaryMountainConfig& mountain_config) {
  if (config.u_segments <= 0 || config.v_segments <= 0) {
    return;
  }

  const int columns = config.u_segments + 1;
  const int rows = config.v_segments + 1;
  const std::size_t base_index = vertices.size();
  vertices.resize(base_index + static_cast<std::size_t>(columns * rows));

  for (int v = 0; v <= config.v_segments; ++v) {
    const float fv = static_cast<float>(v) / static_cast<float>(config.v_segments);
    for (int u = 0; u <= config.u_segments; ++u) {
      const float fu = static_cast<float>(u) / static_cast<float>(config.u_segments);
      QVector3D position = config.origin + config.axis_u * fu + config.axis_v * fv;
      const BoundaryMountainSample sample =
          sample_boundary_mountain(position.x(), position.z(), mountain_config);
      position.setY(sample.height);

      Vertex vertex{};
      vertex.position = {position.x(), position.y(), position.z()};
      vertex.normal = {0.0F, 1.0F, 0.0F};

      vertex.tex_coord = {sample.foot, 0.0F};

      const std::size_t local_index = static_cast<std::size_t>(v * columns + u);
      vertices[base_index + local_index] = vertex;
    }
  }

  const bool flip_winding =
      QVector3D::crossProduct(config.axis_v, config.axis_u).y() < 0.0F;

  for (int v = 0; v < config.v_segments; ++v) {
    for (int u = 0; u < config.u_segments; ++u) {
      const auto a = static_cast<unsigned int>(
          base_index + static_cast<std::size_t>(v * columns + u));
      const unsigned int b = a + 1U;
      const unsigned int c = a + static_cast<unsigned int>(columns);
      const unsigned int d = c + 1U;

      if (flip_winding) {
        indices.insert(indices.end(), {a, b, c, b, d, c});
      } else {
        indices.insert(indices.end(), {a, c, b, b, c, d});
      }
    }
  }

  if (!config.seam_row) {
    return;
  }

  const QVector3D inward =
      -config.axis_v / static_cast<float>(std::max(config.v_segments, 1));
  std::vector<QVector3D> inner(static_cast<std::size_t>(columns));
  for (int u = 0; u <= config.u_segments; ++u) {
    const float fu = static_cast<float>(u) / static_cast<float>(config.u_segments);
    QVector3D position = config.origin + config.axis_u * fu + inward;
    position.setY(
        edge_height_at(mountain_config.height_map, position.x(), position.z()));
    inner[static_cast<std::size_t>(u)] = position;
  }
  const auto seam_position = [&](int u) {
    const Vertex& vertex = vertices[base_index + static_cast<std::size_t>(u)];
    return QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]);
  };
  for (int u = 0; u < config.u_segments; ++u) {
    const QVector3D s0 = seam_position(u);
    const QVector3D s1 = seam_position(u + 1);
    const QVector3D i0 = inner[static_cast<std::size_t>(u)];
    const QVector3D i1 = inner[static_cast<std::size_t>(u + 1)];
    QVector3D n0 = QVector3D::crossProduct(s1 - s0, i0 - s0);
    QVector3D n1 = QVector3D::crossProduct(i0 - s1, i1 - s1);
    if (n0.y() < 0.0F) {
      n0 = -n0;
    }
    if (n1.y() < 0.0F) {
      n1 = -n1;
    }
    for (const QVector3D& corner : {s0, s1}) {
      extra_normals[position_key({corner.x(), corner.y(), corner.z()})] += n0 + n1;
    }
  }
}

void compute_vertex_normals(std::vector<Vertex>& vertices,
                            const std::vector<unsigned int>& indices,
                            const NormalAccumulator& extra_normals) {
  NormalAccumulator accumulated = extra_normals;
  accumulated.reserve(vertices.size());

  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    const unsigned int ia = indices[i];
    const unsigned int ib = indices[i + 1];
    const unsigned int ic = indices[i + 2];
    if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size()) {
      continue;
    }

    const QVector3D a(
        vertices[ia].position[0], vertices[ia].position[1], vertices[ia].position[2]);
    const QVector3D b(
        vertices[ib].position[0], vertices[ib].position[1], vertices[ib].position[2]);
    const QVector3D c(
        vertices[ic].position[0], vertices[ic].position[1], vertices[ic].position[2]);
    QVector3D normal = QVector3D::crossProduct(b - a, c - a);
    if (normal.lengthSquared() <= 0.0F) {
      continue;
    }
    if (normal.y() < 0.0F) {
      normal = -normal;
    }
    for (const unsigned int index : {ia, ib, ic}) {
      accumulated[position_key(vertices[index].position)] += normal;
    }
  }

  for (Vertex& vertex : vertices) {
    QVector3D normal(0.0F, 1.0F, 0.0F);
    const auto found = accumulated.find(position_key(vertex.position));
    if (found != accumulated.end() && found->second.lengthSquared() > 0.0F) {
      normal = found->second.normalized();
    }
    vertex.normal = {normal.x(), normal.y(), normal.z()};
  }
}

auto compute_geometry_signature(const std::vector<Vertex>& vertices) -> std::uint64_t {
  std::uint64_t signature = 1469598103934665603ULL;
  constexpr std::uint64_t k_prime = 1099511628211ULL;

  for (const Vertex& vertex : vertices) {
    const PositionKey key = position_key(vertex.position);
    for (const std::int64_t v : {key.x, key.y, key.z}) {
      signature ^= static_cast<std::uint64_t>(v);
      signature *= k_prime;
    }
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

void MapBoundaryFogRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if ((resources == nullptr) || (m_cards.empty() && m_mountain_indices.empty())) {
    return;
  }

  Shader* previous_shader = renderer.get_current_shader();
  renderer.set_current_shader(nullptr);

  if (!m_mountain_indices.empty()) {
    ensure_mountain_mesh();
    if (m_mountain_mesh != nullptr) {
      TerrainSurfaceCmd cmd;
      cmd.mesh = m_mountain_mesh.get();
      cmd.model = k_identity_matrix;
      cmd.params =
          build_boundary_mountain_params(world().terrain_or_empty(), m_tile_size);
      cmd.sort_key = 0x0080U;
      cmd.depth_write = true;
      cmd.horizon_dressing = true;
      renderer.terrain_surface(cmd);
    }
  }

  if (m_cards.empty()) {
    renderer.set_current_shader(previous_shader);
    return;
  }

  Mesh* quad = resources->quad();
  if (quad == nullptr) {
    renderer.set_current_shader(previous_shader);
    return;
  }

  Shader* gas_shader = renderer.get_shader("boundary_gas");
  if (gas_shader == nullptr) {
    gas_shader = renderer.load_shader("boundary_gas",
                                      ":/assets/shaders/boundary_gas.vert",
                                      ":/assets/shaders/boundary_gas.frag");
  }

  if (gas_shader != nullptr) {
    renderer.set_current_shader(gas_shader);
  }

  const Camera* camera = renderer.camera();
  const QVector3D camera_position =
      camera != nullptr ? camera->get_position() : QVector3D(0.0F, 0.0F, 0.0F);

  for (const BoundaryCard& card : m_cards) {
    if (card_engulfs_camera(card.center, card.size.x(), camera_position)) {
      continue;
    }
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
  const float band_tiles = static_cast<float>(k_clear_outside_tiles + k_band_outside);
  const float band_world = band_tiles * m_tile_size;
  const float width_world = static_cast<float>(m_width) * m_tile_size;
  const float height_world = static_cast<float>(m_height) * m_tile_size;
  const float x_step = width_world / static_cast<float>(k_cards_per_side);
  const float z_step = height_world / static_cast<float>(k_cards_per_side);

  m_cards.reserve(
      static_cast<std::size_t>((4 * k_cards_per_side + 4) * k_curtain_rings));

  auto emit_card =
      [&](const QVector3D& center, float width, float height, float yaw, float alpha) {
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
                x_step * side_width_scale,
                wall_height,
                0.0F,
                alpha);
      emit_card(QVector3D(x, center_y, half_h + ring_offset),
                x_step * side_width_scale,
                wall_height,
                180.0F,
                alpha);
    }

    for (int i = 0; i < k_cards_per_side; ++i) {
      const float tz =
          (static_cast<float>(i) + 0.5F) / static_cast<float>(k_cards_per_side);
      const float z = tz * height_world - half_h;
      emit_card(QVector3D(-half_w - ring_offset, center_y, z),
                z_step * side_width_scale,
                wall_height,
                -90.0F,
                alpha);
      emit_card(QVector3D(half_w + ring_offset, center_y, z),
                z_step * side_width_scale,
                wall_height,
                90.0F,
                alpha);
    }

    emit_card(QVector3D(-half_w - ring_offset, corner_y, -half_h - ring_offset),
              corner_span,
              corner_height,
              -45.0F,
              alpha);
    emit_card(QVector3D(half_w + ring_offset, corner_y, -half_h - ring_offset),
              corner_span,
              corner_height,
              45.0F,
              alpha);
    emit_card(QVector3D(-half_w - ring_offset, corner_y, half_h + ring_offset),
              corner_span,
              corner_height,
              -135.0F,
              alpha);
    emit_card(QVector3D(half_w + ring_offset, corner_y, half_h + ring_offset),
              corner_span,
              corner_height,
              135.0F,
              alpha);
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

  const float edge_half_w = (static_cast<float>(m_width) * 0.5F - 0.5F) * m_tile_size;
  const float edge_half_h = (static_cast<float>(m_height) * 0.5F - 0.5F) * m_tile_size;
  const float edge_width_world = edge_half_w * 2.0F;
  const float edge_height_world = edge_half_h * 2.0F;
  const float band = k_mountain_outer_tiles * m_tile_size;
  const float foothill = k_foothill_tiles * m_tile_size;
  const float peak = k_mountain_peak_tiles * m_tile_size;

  const float edge_step = m_tile_size * k_mountain_edge_step_tiles;
  const int side_x_segments = std::clamp(
      static_cast<int>(std::ceil(std::max(edge_width_world, edge_step) / edge_step)),
      k_mountain_min_side_segments,
      k_mountain_max_side_segments);
  const int side_z_segments = std::clamp(
      static_cast<int>(std::ceil(std::max(edge_height_world, edge_step) / edge_step)),
      k_mountain_min_side_segments,
      k_mountain_max_side_segments);
  const int depth_segments = k_mountain_depth_segments;

  const auto& terrain = world().terrain_or_empty();
  const BoundaryMountainConfig mountain_config{
      edge_half_w,
      edge_half_h,
      foothill,
      band,
      m_tile_size,
      peak,
      1.0F / (4.0F * std::max(edge_step, band / static_cast<float>(depth_segments))),
      resolve_mountain_noise_settings(terrain, m_width, m_height),
      terrain.is_initialized() ? terrain.get_height_map() : nullptr};

  NormalAccumulator extra_normals;

  append_patch_mesh(m_mountain_vertices,
                    m_mountain_indices,
                    extra_normals,
                    MountainPatchConfig{QVector3D(-edge_half_w, 0.0F, -edge_half_h),
                                        QVector3D(edge_width_world, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, -band),
                                        side_x_segments,
                                        depth_segments,
                                        true},
                    mountain_config);
  append_patch_mesh(m_mountain_vertices,
                    m_mountain_indices,
                    extra_normals,
                    MountainPatchConfig{QVector3D(-edge_half_w, 0.0F, edge_half_h),
                                        QVector3D(edge_width_world, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, band),
                                        side_x_segments,
                                        depth_segments,
                                        true},
                    mountain_config);
  append_patch_mesh(m_mountain_vertices,
                    m_mountain_indices,
                    extra_normals,
                    MountainPatchConfig{QVector3D(-edge_half_w, 0.0F, -edge_half_h),
                                        QVector3D(0.0F, 0.0F, edge_height_world),
                                        QVector3D(-band, 0.0F, 0.0F),
                                        side_z_segments,
                                        depth_segments,
                                        true},
                    mountain_config);
  append_patch_mesh(m_mountain_vertices,
                    m_mountain_indices,
                    extra_normals,
                    MountainPatchConfig{QVector3D(edge_half_w, 0.0F, -edge_half_h),
                                        QVector3D(0.0F, 0.0F, edge_height_world),
                                        QVector3D(band, 0.0F, 0.0F),
                                        side_z_segments,
                                        depth_segments,
                                        true},
                    mountain_config);

  for (const float sign_x : {-1.0F, 1.0F}) {
    for (const float sign_z : {-1.0F, 1.0F}) {
      append_patch_mesh(m_mountain_vertices,
                        m_mountain_indices,
                        extra_normals,
                        MountainPatchConfig{
                            QVector3D(sign_x * edge_half_w, 0.0F, sign_z * edge_half_h),
                            QVector3D(sign_x * band, 0.0F, 0.0F),
                            QVector3D(0.0F, 0.0F, sign_z * band),
                            depth_segments,
                            depth_segments,
                            false},
                        mountain_config);
    }
  }

  compute_vertex_normals(m_mountain_vertices, m_mountain_indices, extra_normals);
  if (!m_mountain_vertices.empty()) {
    m_mountain_min_height = m_mountain_vertices.front().position[1];
    m_mountain_max_height = m_mountain_min_height;
    for (const Vertex& vertex : m_mountain_vertices) {
      m_mountain_min_height = std::min(m_mountain_min_height, vertex.position[1]);
      m_mountain_max_height = std::max(m_mountain_max_height, vertex.position[1]);
    }
  }
  m_mountain_geometry_signature = compute_geometry_signature(m_mountain_vertices);
}

void MapBoundaryFogRenderer::ensure_mountain_mesh() {
  if ((m_mountain_mesh != nullptr) || m_mountain_indices.empty() ||
      m_mountain_vertices.empty()) {
    return;
  }

  m_mountain_mesh = std::make_unique<Mesh>(m_mountain_vertices, m_mountain_indices);
}

} // namespace Render::GL
