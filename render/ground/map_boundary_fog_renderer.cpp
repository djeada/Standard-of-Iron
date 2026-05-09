#include "map_boundary_fog_renderer.h"

#include "../../game/map/terrain_service.h"
#include "../scene_renderer.h"
#include <QMatrix4x4>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace Render::GL {

namespace {

const QMatrix4x4 k_identity_matrix;

constexpr int k_clear_outside_tiles = 2;
constexpr int k_band_outside = 16;
constexpr int k_cards_per_side = 3;
constexpr int k_curtain_rings = 2;
constexpr int k_mountain_depth_segments = 8;

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
constexpr float k_mountain_outer_tiles = 7.10F;
constexpr float k_mountain_peak_tiles = 5.60F;
constexpr float k_mountain_base_lift = 0.04F;

constexpr float k_mountain_r = 0.36F;
constexpr float k_mountain_g = 0.40F;
constexpr float k_mountain_b = 0.35F;

inline auto smooth_lerp(float a, float b, float t) -> float {
  const float smooth = t * t * (3.0F - 2.0F * t);
  return a + (b - a) * smooth;
}

inline auto smoothstep(float edge0, float edge1, float value) -> float {
  if (edge0 == edge1) {
    return value >= edge1 ? 1.0F : 0.0F;
  }
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

inline auto hash_coords(int x, int z, std::uint32_t seed) -> std::uint32_t {
  const std::uint32_t ux = static_cast<std::uint32_t>(x) * 73856093U;
  const std::uint32_t uz = static_cast<std::uint32_t>(z) * 19349663U;
  return ux ^ uz ^ (seed * 83492791U + 0x9e3779b9U);
}

inline auto hash_to_unit(std::uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return static_cast<float>(h & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
}

auto value_noise(float x, float z, std::uint32_t seed) -> float {
  const int ix0 = static_cast<int>(std::floor(x));
  const int iz0 = static_cast<int>(std::floor(z));
  const int ix1 = ix0 + 1;
  const int iz1 = iz0 + 1;

  const float tx = x - static_cast<float>(ix0);
  const float tz = z - static_cast<float>(iz0);

  const float n00 = hash_to_unit(hash_coords(ix0, iz0, seed));
  const float n10 = hash_to_unit(hash_coords(ix1, iz0, seed));
  const float n01 = hash_to_unit(hash_coords(ix0, iz1, seed));
  const float n11 = hash_to_unit(hash_coords(ix1, iz1, seed));

  const float nx0 = smooth_lerp(n00, n10, tx);
  const float nx1 = smooth_lerp(n01, n11, tx);
  return smooth_lerp(nx0, nx1, tz);
}

auto fbm_noise(float x, float z, std::uint32_t seed, int octaves) -> float {
  float sum = 0.0F;
  float amplitude = 1.0F;
  float frequency = 1.0F;
  float normalization = 0.0F;

  for (int i = 0; i < std::clamp(octaves, 1, 8); ++i) {
    sum +=
        value_noise(x * frequency, z * frequency, seed + i * 97U) * amplitude;
    normalization += amplitude;
    amplitude *= 0.5F;
    frequency *= 2.0F;
  }

  return normalization > 0.0F ? (sum / normalization) : 0.0F;
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
  std::uint32_t seed = 0U;
  bool corner = false;
};

void append_patch_mesh(std::vector<Vertex> &vertices,
                       std::vector<unsigned int> &indices,
                       const MountainPatchConfig &config, float tile_size,
                       float max_relief_height) {
  if (config.u_segments <= 0 || config.v_segments <= 0) {
    return;
  }

  const int columns = config.u_segments + 1;
  const int rows = config.v_segments + 1;
  const std::size_t base_index = vertices.size();
  vertices.resize(base_index + static_cast<std::size_t>(columns * rows));

  const QVector3D flat_normal(0.0F, 1.0F, 0.0F);
  const float tex_scale = 0.16F / std::max(tile_size, 0.25F);

  for (int v = 0; v <= config.v_segments; ++v) {
    const float fv =
        static_cast<float>(v) / static_cast<float>(config.v_segments);
    for (int u = 0; u <= config.u_segments; ++u) {
      const float fu =
          static_cast<float>(u) / static_cast<float>(config.u_segments);
      QVector3D position =
          config.origin + config.axis_u * fu + config.axis_v * fv;

      const float primary = fbm_noise(position.x() * 0.050F,
                                      position.z() * 0.050F, config.seed, 4);
      const float detail = fbm_noise(
          position.x() * 0.125F, position.z() * 0.125F, config.seed + 31U, 2);
      const float ridge = 1.0F - std::abs(primary * 2.0F - 1.0F);
      const float shape = std::clamp(
          primary * 0.58F + detail * 0.17F + ridge * 0.25F, 0.0F, 1.0F);

      float relief_mask = 0.0F;
      if (config.corner) {
        const float diagonal = 1.0F - std::abs(fu - fv);
        relief_mask = std::pow(std::sin(fu * std::numbers::pi_v<float>) *
                                   std::sin(fv * std::numbers::pi_v<float>),
                               0.82F) *
                      std::clamp(0.72F + diagonal * 0.28F, 0.0F, 1.0F);
      } else {
        const float along_fade =
            smoothstep(0.0F, 0.12F, fu) * smoothstep(0.0F, 0.12F, 1.0F - fu);
        const float depth_fade =
            std::pow(std::sin(fv * std::numbers::pi_v<float>), 1.10F);
        relief_mask = along_fade * depth_fade;
      }

      float edge_fade = 1.0F - fv;
      if (config.corner) {
        edge_fade = std::clamp(std::max(fu, fv), 0.0F, 1.0F);
      }
      edge_fade = std::pow(edge_fade, 2.25F);

      const float boundary_height =
          boundary_height_at(position.x(), position.z()) * edge_fade;
      position.setY(boundary_height + tile_size * k_mountain_base_lift +
                    max_relief_height * relief_mask * (0.34F + 0.66F * shape));

      Vertex vertex{};
      vertex.position = {position.x(), position.y(), position.z()};
      vertex.normal = {flat_normal.x(), flat_normal.y(), flat_normal.z()};
      vertex.tex_coord = {position.x() * tex_scale, position.z() * tex_scale};

      const std::size_t vertex_index =
          base_index + static_cast<std::size_t>(v * columns + u);
      vertices[vertex_index] = vertex;
    }
  }

  const bool flip_winding =
      QVector3D::crossProduct(config.axis_v, config.axis_u).y() < 0.0F;

  for (int v = 0; v < config.v_segments; ++v) {
    for (int u = 0; u < config.u_segments; ++u) {
      const unsigned int a = static_cast<unsigned int>(
          base_index + static_cast<std::size_t>(v * columns + u));
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
      renderer.mesh(m_mountain_mesh.get(), k_identity_matrix,
                    QVector3D(k_mountain_r, k_mountain_g, k_mountain_b));
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

  if (gas_shader != nullptr) {
    renderer.set_current_shader(previous_shader);
  } else {
    renderer.set_current_shader(previous_shader);
  }
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

  const int side_x_segments = std::clamp(
      static_cast<int>(std::ceil(width_world / (m_tile_size * 2.5F))), 12, 96);
  const int side_z_segments = std::clamp(
      static_cast<int>(std::ceil(height_world / (m_tile_size * 2.5F))), 12, 96);
  const int corner_segments = std::clamp(k_mountain_depth_segments + 1, 6, 16);
  const std::uint32_t base_seed =
      static_cast<std::uint32_t>((m_width * 92821) ^ (m_height * 68917)) ^
      0x7F4A7C15U;

  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{
                        QVector3D(-half_w, 0.0F, -half_h - inner_offset),
                        QVector3D(width_world, 0.0F, 0.0F),
                        QVector3D(0.0F, 0.0F, -mountain_band), side_x_segments,
                        k_mountain_depth_segments, base_seed + 11U, false},
                    m_tile_size, max_relief_height);
  append_patch_mesh(
      m_mountain_vertices, m_mountain_indices,
      MountainPatchConfig{QVector3D(-half_w, 0.0F, half_h + inner_offset),
                          QVector3D(width_world, 0.0F, 0.0F),
                          QVector3D(0.0F, 0.0F, mountain_band), side_x_segments,
                          k_mountain_depth_segments, base_seed + 29U, false},
      m_tile_size, max_relief_height);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{
                        QVector3D(-half_w - inner_offset, 0.0F, -half_h),
                        QVector3D(0.0F, 0.0F, height_world),
                        QVector3D(-mountain_band, 0.0F, 0.0F), side_z_segments,
                        k_mountain_depth_segments, base_seed + 47U, false},
                    m_tile_size, max_relief_height);
  append_patch_mesh(
      m_mountain_vertices, m_mountain_indices,
      MountainPatchConfig{QVector3D(half_w + inner_offset, 0.0F, -half_h),
                          QVector3D(0.0F, 0.0F, height_world),
                          QVector3D(mountain_band, 0.0F, 0.0F), side_z_segments,
                          k_mountain_depth_segments, base_seed + 71U, false},
      m_tile_size, max_relief_height);

  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(-half_w - outer_offset, 0.0F,
                                                  -half_h - outer_offset),
                                        QVector3D(mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, mountain_band),
                                        corner_segments, corner_segments,
                                        base_seed + 101U, true},
                    m_tile_size, max_relief_height);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(half_w + outer_offset, 0.0F,
                                                  -half_h - outer_offset),
                                        QVector3D(-mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, mountain_band),
                                        corner_segments, corner_segments,
                                        base_seed + 131U, true},
                    m_tile_size, max_relief_height);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(-half_w - outer_offset, 0.0F,
                                                  half_h + outer_offset),
                                        QVector3D(mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, -mountain_band),
                                        corner_segments, corner_segments,
                                        base_seed + 167U, true},
                    m_tile_size, max_relief_height);
  append_patch_mesh(m_mountain_vertices, m_mountain_indices,
                    MountainPatchConfig{QVector3D(half_w + outer_offset, 0.0F,
                                                  half_h + outer_offset),
                                        QVector3D(-mountain_band, 0.0F, 0.0F),
                                        QVector3D(0.0F, 0.0F, -mountain_band),
                                        corner_segments, corner_segments,
                                        base_seed + 197U, true},
                    m_tile_size, max_relief_height);

  compute_vertex_normals(m_mountain_vertices, m_mountain_indices);
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
