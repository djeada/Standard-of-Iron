#include "terrain_mesh_builder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/map/terrain_surface.h"
#include "render/gl/render_constants.h"
#include "terrain_quad_tessellation.h"
#include "terrain_vertex_masks.h"

namespace {

using namespace Render::GL::Geometry;

inline auto linstep(float a, float b, float x) -> float {
  return std::clamp((x - a) / std::max(1e-6F, (b - a)), 0.0F, 1.0F);
}
inline auto smooth(float a, float b, float x) -> float {
  float const t = linstep(a, b, x);
  return t * t * (3.0F - 2.0F * t);
}

} // namespace

namespace Render::Ground {

using Render::GL::Vertex;

TerrainMeshBuilder::TerrainMeshBuilder(const TerrainMeshInput& input)
    : m_width(input.width)
    , m_height(input.height)
    , m_tile_size(input.tile_size)
    , m_height_data(input.heights)
    , m_terrain_types(input.terrain_types)
    , m_hill_entrances(input.hill_entrances) {
}

auto TerrainMeshBuilder::section_for(Game::Map::TerrainType type) -> int {
  switch (type) {
  case Game::Map::TerrainType::Mountain:
    return 2;
  case Game::Map::TerrainType::Hill:
    return 1;
  case Game::Map::TerrainType::Forest:
  case Game::Map::TerrainType::Flat:
  default:
    return 0;
  }
}

auto TerrainMeshBuilder::compute_hill_entry_weights(
    const std::vector<float>& height_data) const -> std::vector<float> {
  std::vector<float> entry_weight;
  if (!m_hill_entrances.empty() && m_hill_entrances.size() == height_data.size()) {
    constexpr int k_entry_radius = 4;
    entry_weight.assign(height_data.size(), 0.0F);
    for (int z = 0; z < m_height; ++z) {
      for (int x = 0; x < m_width; ++x) {
        int const idx = z * m_width + x;
        if (m_terrain_types[idx] != Game::Map::TerrainType::Hill) {
          continue;
        }
        auto min_dist = float(k_entry_radius + 1);
        for (int dz = -k_entry_radius; dz <= k_entry_radius; ++dz) {
          int const nz = z + dz;
          if (nz < 0 || nz >= m_height) {
            continue;
          }
          for (int dx = -k_entry_radius; dx <= k_entry_radius; ++dx) {
            int const nx = x + dx;
            if (nx < 0 || nx >= m_width) {
              continue;
            }
            int const n_idx = nz * m_width + nx;
            if (!m_hill_entrances[n_idx]) {
              continue;
            }
            float const dist = std::sqrt(float(dx * dx + dz * dz));
            min_dist = std::min(min_dist, dist);
          }
        }
        if (min_dist <= k_entry_radius) {
          float const t = 1.0F - (min_dist / float(k_entry_radius));
          entry_weight[idx] = t * t;
        }
      }
    }
  }
  return entry_weight;
}

auto TerrainMeshBuilder::compute_feature_foot_weights(
    const std::vector<float>& height_data) const -> std::vector<float> {
  const int vertex_count = m_width * m_height;
  std::vector<float> feature_foot_weight(vertex_count, 0.0F);

  auto is_elevated_feature = [&](int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_terrain_types.size())) {
      return false;
    }
    return m_terrain_types[idx] == Game::Map::TerrainType::Hill ||
           m_terrain_types[idx] == Game::Map::TerrainType::Mountain;
  };

  constexpr int k_feature_foot_radius = 5;
  constexpr float k_height_edge_threshold = 0.035F;
  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int const idx = z * m_width + x;
      bool const center_feature = is_elevated_feature(idx);
      auto min_dist = float(k_feature_foot_radius + 1);
      float max_height_delta = 0.0F;

      for (int dz = -k_feature_foot_radius; dz <= k_feature_foot_radius; ++dz) {
        int const nz = z + dz;
        if (nz < 0 || nz >= m_height) {
          continue;
        }
        for (int dx = -k_feature_foot_radius; dx <= k_feature_foot_radius; ++dx) {
          int const nx = x + dx;
          if (nx < 0 || nx >= m_width) {
            continue;
          }
          float const dist = std::sqrt(float(dx * dx + dz * dz));
          if (dist > float(k_feature_foot_radius)) {
            continue;
          }

          int const n_idx = nz * m_width + nx;
          if (is_elevated_feature(n_idx) != center_feature) {
            min_dist = std::min(min_dist, dist);
          }
          max_height_delta = std::max(max_height_delta,
                                      std::abs(height_data[n_idx] - height_data[idx]));
        }
      }

      float const boundary_t = (min_dist <= float(k_feature_foot_radius))
                                   ? 1.0F - (min_dist / float(k_feature_foot_radius))
                                   : 0.0F;
      float const height_edge_t =
          smooth(0.0F,
                 k_height_edge_threshold * float(k_feature_foot_radius),
                 max_height_delta);
      float const foot = boundary_t * boundary_t * (3.0F - 2.0F * boundary_t);
      feature_foot_weight[idx] =
          std::clamp(foot * (0.35F + 0.65F * height_edge_t), 0.0F, 1.0F);
    }
  }

  return feature_foot_weight;
}

void TerrainMeshBuilder::smooth_terrain_normals(
    const std::vector<float>& height_data,
    const std::vector<QVector3D>& face_normals,
    std::vector<QVector3D>& normals,
    float min_height,
    float height_range) const {
  std::vector<QVector3D> filtered = normals;
  auto get_n = [&](int x, int z) -> QVector3D& {
    return normals[z * m_width + x];
  };

  for (int z = 1; z < m_height - 1; ++z) {
    for (int x = 1; x < m_width - 1; ++x) {
      const int idx = z * m_width + x;
      const float h0 = height_data[idx];
      const float nh = (h0 - min_height) / height_range;

      const float h_l = height_data[z * m_width + (x - 1)];
      const float h_r = height_data[z * m_width + (x + 1)];
      const float h_d = height_data[(z - 1) * m_width + x];
      const float h_u = height_data[(z + 1) * m_width + x];
      const float avg_nbr = 0.25F * (h_l + h_r + h_d + h_u);
      const float convexity = h0 - avg_nbr;

      const QVector3D n0 = normals[idx];
      const float slope = 1.0F - std::clamp(n0.y(), 0.0F, 1.0F);

      const float ridge_s = smooth(0.35F, 0.70F, slope);
      const float ridge_c = smooth(-0.02F, 0.18F, convexity);
      const float ridge_factor =
          std::clamp(0.5F * ridge_s + 0.5F * ridge_c, 0.0F, 1.0F);
      const float base_boost = 0.6F * (1.0F - nh);

      QVector3D acc(0, 0, 0);
      float wsum = 0.0F;
      for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx = x + dx;
          const int nz = z + dz;
          const int n_idx = nz * m_width + nx;

          const float dh = std::abs(height_data[n_idx] - h0);
          const QVector3D nn = get_n(nx, nz);
          const float ndot = std::max(0.0F, QVector3D::dotProduct(n0, nn));

          const float w_h = 1.0F / (1.0F + 2.0F * dh);
          const float w_n = std::pow(ndot, 8.0F);
          const float w_b = 1.0F + base_boost;
          const float w_r = 1.0F - ridge_factor * 0.85F;

          const float w = w_h * w_n * w_b * w_r;
          acc += nn * w;
          wsum += w;
        }
      }

      QVector3D n_filtered = (wsum > 0.0F) ? (acc / wsum) : n0;
      n_filtered.normalize();

      const QVector3D n_orig = face_normals[idx];
      const QVector3D n_final =
          (ridge_factor > 0.0F)
              ? (n_filtered * (1.0F - ridge_factor) + n_orig * ridge_factor)
              : n_filtered;

      filtered[idx] = n_final.normalized();
    }
  }
  normals.swap(filtered);
}

void TerrainMeshBuilder::emit_terrain_chunk(
    const TerrainMeshBuild& build,
    int chunk_x,
    int chunk_z,
    int chunk_max_x,
    int chunk_max_z,
    std::vector<TerrainChunkGeometry>& out) const {
  const auto& positions = build.positions;
  const auto& normals = build.normals;
  const auto& height_data = build.height_data;
  const auto& feature_foot_weight = build.feature_foot_weight;
  const auto& entry_weight = build.entry_weight;
  const auto& sample_height_at = build.sample_height_at;
  const auto sample_entry_at = [&](float gx, float gz) {
    return Render::Ground::sample_grid_bilinear(
        entry_weight, m_width, m_height, gx, gz);
  };
  const auto& normal_from_heights_at = build.normal_from_heights_at;
  const auto& quad_section = build.quad_section;
  const float half_width = build.half_width;
  const float half_height = build.half_height;
  auto expand_bounds = [](SectionData& section, const QVector3D& position) {
    section.bounds_min.setX(std::min(section.bounds_min.x(), position.x()));
    section.bounds_min.setY(std::min(section.bounds_min.y(), position.y()));
    section.bounds_min.setZ(std::min(section.bounds_min.z(), position.z()));
    section.bounds_max.setX(std::max(section.bounds_max.x(), position.x()));
    section.bounds_max.setY(std::max(section.bounds_max.y(), position.y()));
    section.bounds_max.setZ(std::max(section.bounds_max.z(), position.z()));
  };

  SectionData sections[3];

  auto ensure_vertex = [&](SectionData& section, int global_index) -> unsigned int {
    auto it = section.remap.find(global_index);
    if (it != section.remap.end()) {
      return it->second;
    }
    Vertex v{};
    const QVector3D& pos = positions[global_index];
    const QVector3D& normal = normals[global_index];
    v.position[0] = pos.x();
    v.position[1] = pos.y();
    v.position[2] = pos.z();
    v.normal[0] = normal.x();
    v.normal[1] = normal.y();
    v.normal[2] = normal.z();

    v.tex_coord[0] = feature_foot_weight[global_index];
    v.tex_coord[1] = Render::Ground::pack_entry_and_landform(
        entry_weight.empty() ? 0.0F : entry_weight[global_index],
        build.hill_weight[global_index],
        build.mountain_weight[global_index]);

    section.vertices.push_back(v);
    auto const local_index = static_cast<unsigned int>(section.vertices.size() - 1);
    section.remap.emplace(global_index, local_index);
    expand_bounds(section, pos);
    return local_index;
  };

  auto add_vertex = [&](SectionData& section,
                        const QVector3D& pos,
                        const QVector3D& normal,
                        float entry_mask) -> unsigned int {
    Vertex v{};
    v.position[0] = pos.x();
    v.position[1] = pos.y();
    v.position[2] = pos.z();
    v.normal[0] = normal.x();
    v.normal[1] = normal.y();
    v.normal[2] = normal.z();

    const float mask_gx = (pos.x() / m_tile_size) + half_width;
    const float mask_gz = (pos.z() / m_tile_size) + half_height;
    v.tex_coord[0] = Render::Ground::sample_grid_bilinear(
        feature_foot_weight, m_width, m_height, mask_gx, mask_gz);
    v.tex_coord[1] = Render::Ground::pack_entry_and_landform(
        entry_weight.empty() ? 0.0F : entry_mask,
        Render::Ground::sample_grid_bilinear(
            build.hill_weight, m_width, m_height, mask_gx, mask_gz),
        Render::Ground::sample_grid_bilinear(
            build.mountain_weight, m_width, m_height, mask_gx, mask_gz));

    section.vertices.push_back(v);
    auto const local_index = static_cast<unsigned int>(section.vertices.size() - 1);
    expand_bounds(section, pos);
    return local_index;
  };

  auto add_vertex_at_grid =
      [&](SectionData& section, float gx, float gz, float entry_mask) -> unsigned int {
    float const world_x = (gx - half_width) * m_tile_size;
    float const world_z = (gz - half_height) * m_tile_size;
    float const h = sample_height_at(gx, gz);
    QVector3D const pos(world_x, h, world_z);
    QVector3D const normal = normal_from_heights_at(gx, gz);
    return add_vertex(section, pos, normal, entry_mask);
  };

  for (int z = chunk_z; z < chunk_max_z; ++z) {
    for (int x = chunk_x; x < chunk_max_x; ++x) {
      int const idx0 = z * m_width + x;
      int const idx1 = idx0 + 1;
      int const idx2 = (z + 1) * m_width + x;
      int const idx3 = idx2 + 1;

      int const section_index = quad_section(m_terrain_types[idx0],
                                             m_terrain_types[idx1],
                                             m_terrain_types[idx2],
                                             m_terrain_types[idx3]);

      SectionData& section = sections[section_index];
      unsigned int const v0 = ensure_vertex(section, idx0);
      unsigned int const v1 = ensure_vertex(section, idx1);
      unsigned int const v2 = ensure_vertex(section, idx2);
      unsigned int const v3 = ensure_vertex(section, idx3);

      float entry_factor = 0.0F;
      if (!entry_weight.empty()) {
        entry_factor = 0.25F * (entry_weight[idx0] + entry_weight[idx1] +
                                entry_weight[idx2] + entry_weight[idx3]);
      }
      float const foot_factor =
          0.25F * (feature_foot_weight[idx0] + feature_foot_weight[idx1] +
                   feature_foot_weight[idx2] + feature_foot_weight[idx3]);
      bool const rising_diagonal = Game::Map::quad_splits_along_rising_diagonal(
          height_data[idx0], height_data[idx1], height_data[idx2], height_data[idx3]);
      bool const subdivide =
          section_index == 2 ||
          (section_index > 0 && (entry_factor > 0.20F || foot_factor > 0.16F));

      if (subdivide) {
        auto const gx = float(x);
        auto const gz = float(z);
        unsigned int const v00 = v0;
        unsigned int const v20 = v1;
        unsigned int const v02 = v2;
        unsigned int const v22 = v3;

        unsigned int const v10 =
            add_vertex_at_grid(section, gx + 0.5F, gz, sample_entry_at(gx + 0.5F, gz));
        unsigned int const v01 =
            add_vertex_at_grid(section, gx, gz + 0.5F, sample_entry_at(gx, gz + 0.5F));
        unsigned int const v21 = add_vertex_at_grid(
            section, gx + 1.0F, gz + 0.5F, sample_entry_at(gx + 1.0F, gz + 0.5F));
        unsigned int const v12 = add_vertex_at_grid(
            section, gx + 0.5F, gz + 1.0F, sample_entry_at(gx + 0.5F, gz + 1.0F));
        unsigned int const v11 = add_vertex_at_grid(
            section, gx + 0.5F, gz + 0.5F, sample_entry_at(gx + 0.5F, gz + 0.5F));

        Render::Ground::append_subdivided_quad_triangles(
            section.indices,
            {v00, v10, v20, v01, v11, v21, v02, v12, v22},
            rising_diagonal);
      } else {
        Render::Ground::append_quad_triangles(
            section.indices, v0, v1, v2, v3, rising_diagonal);
      }
    }
  }

  for (int i = 0; i < 3; ++i) {
    if (sections[i].indices.empty()) {
      continue;
    }
    TerrainChunkGeometry geometry;
    geometry.vertices = std::move(sections[i].vertices);
    geometry.indices = std::move(sections[i].indices);
    geometry.type = (i == 0)   ? Game::Map::TerrainType::Flat
                    : (i == 1) ? Game::Map::TerrainType::Hill
                               : Game::Map::TerrainType::Mountain;
    geometry.bounds_min = sections[i].bounds_min;
    geometry.bounds_max = sections[i].bounds_max;
    out.push_back(std::move(geometry));
  }
}

auto TerrainMeshBuilder::build() const -> std::vector<TerrainChunkGeometry> {
  std::vector<TerrainChunkGeometry> chunks;
  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return chunks;
  }

  const std::vector<float>& height_data = m_height_data;
  const std::vector<float> entry_weight = compute_hill_entry_weights(height_data);

  float min_h = std::numeric_limits<float>::infinity();
  float max_h = -std::numeric_limits<float>::infinity();
  for (float const h : height_data) {
    min_h = std::min(min_h, h);
    max_h = std::max(max_h, h);
  }
  const float height_range = std::max(1e-4F, max_h - min_h);

  const float half_width = m_width * 0.5F - 0.5F;
  const float half_height = m_height * 0.5F - 0.5F;
  const int vertex_count = m_width * m_height;

  std::vector<QVector3D> positions(vertex_count);
  std::vector<QVector3D> normals(vertex_count, QVector3D(0.0F, 0.0F, 0.0F));
  std::vector<QVector3D> face_accum(vertex_count, QVector3D(0, 0, 0));
  const std::vector<float> feature_foot_weight =
      compute_feature_foot_weights(height_data);

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int const idx = z * m_width + x;
      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;
      positions[idx] = QVector3D(world_x, height_data[idx], world_z);
    }
  }

  auto sample_height_at = [&](float gx, float gz) {
    return Game::Map::sample_triangulated_height(
        height_data.data(), m_width, m_height, gx, gz);
  };

  auto normal_from_heights_at = [&](float gx, float gz) {
    float const gx0 = std::clamp(gx - 1.0F, 0.0F, float(m_width - 1));
    float const gx1 = std::clamp(gx + 1.0F, 0.0F, float(m_width - 1));
    float const gz0 = std::clamp(gz - 1.0F, 0.0F, float(m_height - 1));
    float const gz1 = std::clamp(gz + 1.0F, 0.0F, float(m_height - 1));
    float const h_l = sample_height_at(gx0, gz);
    float const h_r = sample_height_at(gx1, gz);
    float const h_d = sample_height_at(gx, gz0);
    float const h_u = sample_height_at(gx, gz1);
    QVector3D const dx(2.0F * m_tile_size, h_r - h_l, 0.0F);
    QVector3D const dz(0.0F, h_u - h_d, 2.0F * m_tile_size);
    QVector3D n = QVector3D::crossProduct(dz, dx);
    if (n.lengthSquared() > 0.0F) {
      n.normalize();
    }
    return n.isNull() ? QVector3D(0, 1, 0) : n;
  };

  for (int i = 0; i < vertex_count; ++i) {
    int const x = i % m_width;
    int const z = i / m_width;
    normals[i] = normal_from_heights_at(float(x), float(z));
    face_accum[i] = normals[i];
  }

  smooth_terrain_normals(height_data, face_accum, normals, min_h, height_range);

  auto quad_section = [&](Game::Map::TerrainType a,
                          Game::Map::TerrainType b,
                          Game::Map::TerrainType c,
                          Game::Map::TerrainType d) {
    int const priority_a = section_for(a);
    int const priority_b = section_for(b);
    int const priority_c = section_for(c);
    int const priority_d = section_for(d);
    int result = priority_a;
    result = std::max(result, priority_b);
    result = std::max(result, priority_c);
    result = std::max(result, priority_d);
    return result;
  };

  const int chunk_size = default_chunk_size;

  const auto landform_weights =
      Render::Ground::compute_landform_weights(m_terrain_types, m_width, m_height);
  const TerrainMeshBuild build{.positions = positions,
                               .normals = normals,
                               .height_data = height_data,
                               .feature_foot_weight = feature_foot_weight,
                               .entry_weight = entry_weight,
                               .hill_weight = landform_weights.hill,
                               .mountain_weight = landform_weights.mountain,
                               .sample_height_at = sample_height_at,
                               .normal_from_heights_at = normal_from_heights_at,
                               .quad_section = quad_section,
                               .half_width = half_width,
                               .half_height = half_height};

  for (int chunk_z = 0; chunk_z < m_height - 1; chunk_z += chunk_size) {
    int const chunk_max_z = std::min(chunk_z + chunk_size, m_height - 1);
    for (int chunk_x = 0; chunk_x < m_width - 1; chunk_x += chunk_size) {
      int const chunk_max_x = std::min(chunk_x + chunk_size, m_width - 1);
      emit_terrain_chunk(build, chunk_x, chunk_z, chunk_max_x, chunk_max_z, chunks);
    }
  }

  return chunks;
}

} // namespace Render::Ground
