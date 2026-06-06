#include "linear_feature_geometry.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../gl/render_constants.h"
#include "ground_utils.h"

namespace {

auto value_noise(float x, float y) -> float {
  float const ix = std::floor(x);
  float const iy = std::floor(y);
  float fx = x - ix;
  float fy = y - iy;

  fx = fx * fx * (3.0F - 2.0F * fx);
  fy = fy * fy * (3.0F - 2.0F * fy);

  float const a = Render::Ground::noise_hash(ix, iy);
  float const b = Render::Ground::noise_hash(ix + 1.0F, iy);
  float const c = Render::Ground::noise_hash(ix, iy + 1.0F);
  float const d = Render::Ground::noise_hash(ix + 1.0F, iy + 1.0F);

  return a * (1.0F - fx) * (1.0F - fy) + b * fx * (1.0F - fy) + c * (1.0F - fx) * fy +
         d * fx * fy;
}

auto sample_height_clamped(const Game::Map::TerrainHeightMap& height_map,
                           float world_x,
                           float world_z) -> float {
  const auto& heights = height_map.get_height_data();
  const int grid_width = height_map.get_width();
  const int grid_height = height_map.get_height();
  const float tile_size = height_map.get_tile_size();

  if (heights.empty() || grid_width <= 0 || grid_height <= 0) {
    return 0.0F;
  }

  float const half_width = static_cast<float>(grid_width) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(grid_height) * 0.5F - 0.5F;
  float gx = (world_x / tile_size) + half_width;
  float gz = (world_z / tile_size) + half_height;

  gx = std::clamp(gx, 0.0F, static_cast<float>(grid_width - 1));
  gz = std::clamp(gz, 0.0F, static_cast<float>(grid_height - 1));

  int const x0 = static_cast<int>(std::floor(gx));
  int const z0 = static_cast<int>(std::floor(gz));
  int const x1 = std::min(x0 + 1, grid_width - 1);
  int const z1 = std::min(z0 + 1, grid_height - 1);

  float const tx = gx - static_cast<float>(x0);
  float const tz = gz - static_cast<float>(z0);

  float const h00 = heights[static_cast<size_t>(z0 * grid_width + x0)];
  float const h10 = heights[static_cast<size_t>(z0 * grid_width + x1)];
  float const h01 = heights[static_cast<size_t>(z1 * grid_width + x0)];
  float const h11 = heights[static_cast<size_t>(z1 * grid_width + x1)];

  float const h0 = h00 * (1.0F - tx) + h10 * tx;
  float const h1 = h01 * (1.0F - tx) + h11 * tx;
  return h0 * (1.0F - tz) + h1 * tz;
}

auto sample_height_envelope(const Game::Map::TerrainHeightMap& height_map,
                            const QVector3D& center,
                            const QVector3D& dir,
                            const QVector3D& perpendicular,
                            float longitudinal_radius,
                            float lateral_radius,
                            float tile_size) -> float {
  float const clamped_longitudinal = std::max(longitudinal_radius, 0.0F);
  float const clamped_lateral = std::max(lateral_radius, 0.0F);

  float const sample_spacing = std::max(tile_size * 0.25F, 0.1F);
  int const longitudinal_samples =
      clamped_longitudinal > 1e-4F
          ? std::max(2,
                     static_cast<int>(
                         std::ceil((clamped_longitudinal * 2.0F) / sample_spacing)) +
                         1)
          : 1;
  int const lateral_samples =
      clamped_lateral > 1e-4F
          ? std::max(
                2,
                static_cast<int>(std::ceil((clamped_lateral * 2.0F) / sample_spacing)) +
                    1)
          : 1;

  float max_height = sample_height_clamped(height_map, center.x(), center.z());
  for (int longitudinal_index = 0; longitudinal_index < longitudinal_samples;
       ++longitudinal_index) {
    float const longitudinal_t = longitudinal_samples > 1
                                     ? static_cast<float>(longitudinal_index) /
                                           static_cast<float>(longitudinal_samples - 1)
                                     : 0.5F;
    float const longitudinal_offset =
        -clamped_longitudinal + longitudinal_t * (clamped_longitudinal * 2.0F);

    for (int lateral_index = 0; lateral_index < lateral_samples; ++lateral_index) {
      float const lateral_t = lateral_samples > 1
                                  ? static_cast<float>(lateral_index) /
                                        static_cast<float>(lateral_samples - 1)
                                  : 0.5F;
      float const lateral_offset =
          -clamped_lateral + lateral_t * (clamped_lateral * 2.0F);

      QVector3D const sample_pos =
          center + dir * longitudinal_offset + perpendicular * lateral_offset;
      max_height =
          std::max(max_height,
                   sample_height_clamped(height_map, sample_pos.x(), sample_pos.z()));
    }
  }
  return max_height;
}

auto smoothstep01(float value) -> float {
  float const t = std::clamp(value, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto mixf(float a, float b, float t) -> float {
  return a + (b - a) * t;
}

} // namespace

namespace Render::Ground {

auto build_linear_ribbon_mesh(const LinearFeatureRibbonSegment& segment,
                              float tile_size,
                              const LinearFeatureRibbonSettings& settings)
    -> std::unique_ptr<Render::GL::Mesh> {
  QVector3D dir = segment.end - segment.start;
  float const length = dir.length();
  if (length < 0.01F) {
    return nullptr;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const half_width = segment.width * 0.5F;
  float const step = std::max(tile_size * settings.sample_step, 0.001F);
  int const cross_section_segments = std::max(settings.cross_section_segments, 1);
  int const vertices_per_row = cross_section_segments + 1;

  int length_steps = static_cast<int>(std::ceil(length / step)) + 1;
  length_steps = std::max(length_steps, settings.min_length_steps);
  float const row_step =
      length_steps > 1 ? (length / static_cast<float>(length_steps - 1)) : 0.0F;

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  vertices.reserve(static_cast<size_t>(length_steps * vertices_per_row));
  indices.reserve(static_cast<size_t>(length_steps - 1) *
                  static_cast<size_t>(cross_section_segments) * 6U);

  for (int i = 0; i < length_steps; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(length_steps - 1);
    QVector3D center_pos = segment.start + dir * (length * t);

    float combined_noise = 0.0F;
    float weight_sum = 0.0F;
    for (size_t noise_index = 0; noise_index < settings.edge_noise_frequencies.size();
         ++noise_index) {
      float const frequency = settings.edge_noise_frequencies[noise_index];
      float const weight = settings.edge_noise_weights[noise_index];
      if (frequency <= 0.0F || weight <= 0.0F) {
        continue;
      }

      combined_noise +=
          value_noise(center_pos.x() * frequency, center_pos.z() * frequency) * weight;
      weight_sum += weight;
    }
    if (weight_sum > 0.0F) {
      combined_noise /= weight_sum;
    }
    combined_noise = (combined_noise - 0.5F) * 2.0F;

    float const width_variation =
        combined_noise * half_width * settings.width_variation_scale;

    if (settings.meander_amplitude > 0.0F && settings.meander_frequency > 0.0F) {
      float const meander = value_noise(t * settings.meander_frequency,
                                        length * settings.meander_length_scale) *
                            settings.meander_amplitude;
      center_pos += perpendicular * meander;
    }

    float const normal[3] = {0.0F, 1.0F, 0.0F};

    float const row_half_width = std::max(half_width + width_variation, 0.01F);
    float const longitudinal_radius = row_step * 0.5F;
    float const lateral_radius =
        row_half_width / static_cast<float>(cross_section_segments);

    for (int lateral_index = 0; lateral_index < vertices_per_row; ++lateral_index) {
      float const cross_t = static_cast<float>(lateral_index) /
                            static_cast<float>(cross_section_segments);
      float const lateral_offset = (cross_t * 2.0F - 1.0F) * row_half_width;
      QVector3D const vertex_pos = center_pos + perpendicular * lateral_offset;

      float vertex_y = vertex_pos.y();
      if (settings.height_map != nullptr) {
        if (settings.sample_terrain_envelope) {
          vertex_y = sample_height_envelope(*settings.height_map,
                                            vertex_pos,
                                            dir,
                                            perpendicular,
                                            longitudinal_radius,
                                            lateral_radius,
                                            tile_size);
        } else {
          vertex_y = sample_height_clamped(
              *settings.height_map, vertex_pos.x(), vertex_pos.z());
        }
      }

      Render::GL::Vertex vertex{};
      vertex.position[0] = vertex_pos.x();
      vertex.position[1] = vertex_y + settings.y_offset;
      vertex.position[2] = vertex_pos.z();
      vertex.normal[0] = normal[0];
      vertex.normal[1] = normal[1];
      vertex.normal[2] = normal[2];
      vertex.tex_coord[0] = cross_t;
      vertex.tex_coord[1] = t;
      vertices.push_back(vertex);
    }

    if (i < length_steps - 1) {
      auto const row_offset = static_cast<unsigned int>(i * vertices_per_row);
      auto const next_row_offset =
          static_cast<unsigned int>((i + 1) * vertices_per_row);
      for (int segment_index = 0; segment_index < cross_section_segments;
           ++segment_index) {
        unsigned int const idx0 = row_offset + static_cast<unsigned int>(segment_index);
        unsigned int const idx1 =
            next_row_offset + static_cast<unsigned int>(segment_index);
        unsigned int const idx2 = idx0 + 1;
        unsigned int const idx3 = idx1 + 1;

        indices.push_back(idx0);
        indices.push_back(idx1);
        indices.push_back(idx2);

        indices.push_back(idx2);
        indices.push_back(idx1);
        indices.push_back(idx3);
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_linear_ribbon_meshes(const std::vector<LinearFeatureRibbonSegment>& segments,
                                float tile_size,
                                const LinearFeatureRibbonSettings& settings)
    -> std::vector<std::unique_ptr<Render::GL::Mesh>> {
  std::vector<std::unique_ptr<Render::GL::Mesh>> meshes;
  meshes.reserve(segments.size());
  for (const auto& segment : segments) {
    meshes.push_back(build_linear_ribbon_mesh(segment, tile_size, settings));
  }
  return meshes;
}

auto build_bridge_mesh(const Game::Map::Bridge& bridge,
                       float tile_size) -> std::unique_ptr<Render::GL::Mesh> {
  QVector3D dir = bridge.end - bridge.start;
  float const length = dir.length();
  if (length < 0.01F) {
    return nullptr;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const half_width = bridge.width * 0.5F;
  float const segment_step = std::max(tile_size * 0.28F, 0.16F);
  float const end_fade_length =
      std::min(std::clamp(bridge.width * 0.70F, tile_size * 0.45F, tile_size * 1.20F),
               length * 0.45F);

  int length_segments = static_cast<int>(std::ceil(length / segment_step));
  length_segments =
      std::max(length_segments, Render::GL::Geometry::min_length_segments + 2);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int k_vertices_per_bridge_segment = 12;
  float const deck_thickness = std::clamp(bridge.width * 0.24F, 0.32F, 0.72F);
  float const parapet_height = std::clamp(bridge.width * 0.18F, 0.18F, 0.42F);
  float const side_bevel = std::clamp(bridge.width * 0.12F, 0.10F, 0.30F);

  auto add_vertex =
      [&](const QVector3D& position, const QVector3D& normal, float u, float v) {
        Render::GL::Vertex vtx{};
        vtx.position[0] = position.x();
        vtx.position[1] = position.y();
        vtx.position[2] = position.z();
        QVector3D const n = normal.normalized();
        vtx.normal[0] = n.x();
        vtx.normal[1] = n.y();
        vtx.normal[2] = n.z();
        vtx.tex_coord[0] = u;
        vtx.tex_coord[1] = v;
        vertices.push_back(vtx);
      };

  auto push_quad = [&](unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
    indices.push_back(a);
    indices.push_back(c);
    indices.push_back(d);
  };

  for (int i = 0; i <= length_segments; ++i) {
    float const mesh_t = static_cast<float>(i) / static_cast<float>(length_segments);
    float const span_distance = length * mesh_t;
    QVector3D const center_pos = bridge.start + dir * span_distance;

    float const start_blend =
        smoothstep01(span_distance / std::max(end_fade_length, 0.001F));
    float const end_blend =
        smoothstep01((length - span_distance) / std::max(end_fade_length, 0.001F));
    float const profile_blend = start_blend * end_blend;

    float const arch_curve =
        Game::Map::bridge_arch_curve(mesh_t) * (0.85F + 0.15F * profile_blend);
    float const deck_height = Game::Map::bridge_deck_world_y(bridge, mesh_t);

    float const stone_noise =
        std::sin(center_pos.x() * 3.0F) * std::cos(center_pos.z() * 2.5F) * 0.02F;

    float const bottom_half_width = std::max(
        half_width - side_bevel * (0.55F + 0.45F * profile_blend), half_width * 0.68F);
    float const ring_thickness =
        mixf(deck_thickness * 0.72F, deck_thickness, profile_blend);
    float const ring_parapet_height = parapet_height * (0.35F + 0.65F * profile_blend);
    float const ring_parapet_offset =
        half_width + side_bevel * (0.20F + 0.35F * profile_blend);

    float const deck_y = deck_height + stone_noise * (0.55F + 0.45F * profile_blend);
    float const underside_y =
        deck_y - ring_thickness - arch_curve * bridge.height * 0.60F;
    float const rail_top_y = deck_y + ring_parapet_height;

    QVector3D top_left = center_pos + perpendicular * (-half_width);
    top_left.setY(deck_y);
    QVector3D top_right = center_pos + perpendicular * half_width;
    top_right.setY(deck_y);

    QVector3D bottom_left = center_pos + perpendicular * (-bottom_half_width);
    bottom_left.setY(underside_y);
    QVector3D bottom_right = center_pos + perpendicular * bottom_half_width;
    bottom_right.setY(underside_y);

    QVector3D const side_left_top = top_left;
    QVector3D const side_left_bottom = bottom_left;
    QVector3D const side_right_top = top_right;
    QVector3D const side_right_bottom = bottom_right;

    QVector3D left_normal =
        QVector3D::crossProduct(dir, side_left_bottom - side_left_top).normalized();
    QVector3D right_normal =
        QVector3D::crossProduct(side_right_bottom - side_right_top, dir).normalized();
    if (left_normal.lengthSquared() < 0.0001F) {
      left_normal = (-perpendicular).normalized();
    }
    if (right_normal.lengthSquared() < 0.0001F) {
      right_normal = perpendicular.normalized();
    }

    QVector3D parapet_left_bottom = center_pos + perpendicular * (-ring_parapet_offset);
    parapet_left_bottom.setY(deck_y);
    QVector3D parapet_left_top = parapet_left_bottom;
    parapet_left_top.setY(rail_top_y);

    QVector3D parapet_right_bottom = center_pos + perpendicular * ring_parapet_offset;
    parapet_right_bottom.setY(deck_y);
    QVector3D parapet_right_top = parapet_right_bottom;
    parapet_right_top.setY(rail_top_y);

    float const tex_u0 = 0.0F;
    float const tex_u1 = 1.0F;
    float const tex_v = mesh_t * length * 0.4F;

    add_vertex(top_left, QVector3D(0.0F, 1.0F, 0.0F), tex_u0, tex_v);
    add_vertex(top_right, QVector3D(0.0F, 1.0F, 0.0F), tex_u1, tex_v);
    add_vertex(bottom_left, QVector3D(0.0F, -1.0F, 0.0F), tex_u0, tex_v);
    add_vertex(bottom_right, QVector3D(0.0F, -1.0F, 0.0F), tex_u1, tex_v);
    add_vertex(side_left_top, left_normal, tex_u0, tex_v);
    add_vertex(side_left_bottom, left_normal, tex_u0, tex_v);
    add_vertex(side_right_top, right_normal, tex_u1, tex_v);
    add_vertex(side_right_bottom, right_normal, tex_u1, tex_v);
    add_vertex(parapet_left_top, left_normal, tex_u0, tex_v);
    add_vertex(parapet_left_bottom, left_normal, tex_u0, tex_v);
    add_vertex(parapet_right_top, right_normal, tex_u1, tex_v);
    add_vertex(parapet_right_bottom, right_normal, tex_u1, tex_v);

    if (i < length_segments) {
      auto const base_idx =
          static_cast<unsigned int>(i * k_vertices_per_bridge_segment);
      unsigned int const next_idx = base_idx + k_vertices_per_bridge_segment;

      push_quad(base_idx + 0, base_idx + 1, next_idx + 1, next_idx + 0);
      push_quad(next_idx + 3, next_idx + 2, base_idx + 2, base_idx + 3);
      push_quad(base_idx + 4, base_idx + 5, next_idx + 5, next_idx + 4);
      push_quad(base_idx + 6, base_idx + 7, next_idx + 7, next_idx + 6);
      push_quad(base_idx + 9, base_idx + 8, next_idx + 8, next_idx + 9);
      push_quad(base_idx + 11, base_idx + 10, next_idx + 10, next_idx + 11);
    }
  }

  if (!vertices.empty()) {
    unsigned int const start_idx = 0;
    auto const end_idx =
        static_cast<unsigned int>(length_segments * k_vertices_per_bridge_segment);
    QVector3D const forward_normal = dir;

    auto add_cap = [&](unsigned int top_l,
                       unsigned int top_r,
                       unsigned int bottom_r,
                       unsigned int bottom_l,
                       const QVector3D& normal) {
      auto const cap_start = static_cast<unsigned int>(vertices.size());
      auto copy_vertex = [&](unsigned int source, const QVector3D& norm) {
        const Render::GL::Vertex& src = vertices[source];
        Render::GL::Vertex vtx = src;
        QVector3D const n = norm.normalized();
        vtx.normal[0] = n.x();
        vtx.normal[1] = n.y();
        vtx.normal[2] = n.z();
        vertices.push_back(vtx);
      };
      copy_vertex(top_l, normal);
      copy_vertex(top_r, normal);
      copy_vertex(bottom_r, normal);
      copy_vertex(bottom_l, normal);
      push_quad(cap_start + 0, cap_start + 1, cap_start + 2, cap_start + 3);
    };

    add_cap(
        start_idx + 0, start_idx + 1, start_idx + 3, start_idx + 2, -forward_normal);
    add_cap(end_idx + 0, end_idx + 1, end_idx + 3, end_idx + 2, forward_normal);
  }

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_riverbank_mesh(const Game::Map::RiverSegment& segment,
                          const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult {
  RiverbankMeshBuildResult result;

  QVector3D dir = segment.end - segment.start;
  float const length = dir.length();
  if (length < 0.01F) {
    return result;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const half_width = segment.width * 0.5F;
  float const tile_size = height_map.get_tile_size();

  constexpr int k_rings_per_side = 5;
  constexpr int k_total_rings = k_rings_per_side * 2;

  int length_steps = static_cast<int>(std::ceil(length / (tile_size * 0.5F))) + 1;
  length_steps = std::max(length_steps, 8);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  for (int i = 0; i < length_steps; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(length_steps - 1);
    QVector3D center_pos = segment.start + dir * (length * t);
    float const center_height =
        sample_height_clamped(height_map, center_pos.x(), center_pos.z());

    constexpr float k_edge_noise_freq_1 = 2.0F;
    constexpr float k_edge_noise_freq_2 = 5.0F;
    constexpr float k_edge_noise_freq_3 = 10.0F;

    float const edge_noise1 = value_noise(center_pos.x() * k_edge_noise_freq_1,
                                          center_pos.z() * k_edge_noise_freq_1);
    float const edge_noise2 = value_noise(center_pos.x() * k_edge_noise_freq_2,
                                          center_pos.z() * k_edge_noise_freq_2);
    float const edge_noise3 = value_noise(center_pos.x() * k_edge_noise_freq_3,
                                          center_pos.z() * k_edge_noise_freq_3);

    float combined_noise = edge_noise1 * 0.5F + edge_noise2 * 0.3F + edge_noise3 * 0.2F;
    combined_noise = (combined_noise - 0.5F) * 2.0F;

    float const width_variation = combined_noise * half_width * 0.35F;

    float const meander = value_noise(t * 3.0F, length * 0.1F) * 0.3F;
    center_pos += perpendicular * meander;

    struct RingProfile {
      float distance_from_water;
      float height_offset;
    };

    constexpr RingProfile k_bank_rings[k_rings_per_side] = {
        {0.0F, 0.02F},
        {0.125F, 0.175F},
        {0.25F, 0.3F},
        {0.375F, 0.125F},
        {0.5F, -0.15F},
    };

    float const ring_noise =
        value_noise(center_pos.x() * 3.0F, center_pos.z() * 3.0F) * 0.075F;
    float const base_bank_width = 0.5F + ring_noise;

    auto const ring_start_idx = static_cast<unsigned int>(vertices.size());

    for (int ring = 0; ring < k_rings_per_side; ++ring) {
      float const ring_dist = k_bank_rings[ring].distance_from_water * base_bank_width;
      float const ring_height = k_bank_rings[ring].height_offset;

      QVector3D const ring_pos =
          center_pos - perpendicular * (half_width + width_variation + ring_dist);
      float const terrain_height =
          sample_height_clamped(height_map, ring_pos.x(), ring_pos.z());
      float const clamped_height = std::min(terrain_height, center_height + 0.05F);

      if (ring == 0) {
        result.visibility_samples.push_back(ring_pos);
      }

      Render::GL::Vertex vtx{};
      vtx.position[0] = ring_pos.x();
      vtx.position[1] = clamped_height + ring_height;
      vtx.position[2] = ring_pos.z();

      QVector3D normal;
      if (ring == 0) {
        QVector3D const next_ring_pos =
            center_pos -
            perpendicular * (half_width + width_variation +
                             k_bank_rings[1].distance_from_water * base_bank_width);
        float const next_terrain =
            sample_height_clamped(height_map, next_ring_pos.x(), next_ring_pos.z());
        float const next_clamped = std::min(next_terrain, center_height + 0.05F);
        QVector3D const slope_vec(next_ring_pos.x() - ring_pos.x(),
                                  (next_clamped + k_bank_rings[1].height_offset) -
                                      (clamped_height + ring_height),
                                  next_ring_pos.z() - ring_pos.z());
        normal = QVector3D::crossProduct(slope_vec, dir).normalized();
      } else if (ring == k_rings_per_side - 1) {
        unsigned int const prev_idx = ring_start_idx + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const slope_vec(ring_pos.x() - prev_pos.x(),
                                  (clamped_height + ring_height) - prev_pos.y(),
                                  ring_pos.z() - prev_pos.z());
        normal = QVector3D::crossProduct(slope_vec, dir).normalized();
      } else {
        unsigned int const prev_idx = ring_start_idx + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const next_ring_pos =
            center_pos - perpendicular * (half_width + width_variation +
                                          k_bank_rings[ring + 1].distance_from_water *
                                              base_bank_width);
        float const next_terrain =
            sample_height_clamped(height_map, next_ring_pos.x(), next_ring_pos.z());
        float const next_clamped = std::min(next_terrain, center_height + 0.05F);

        QVector3D const slope_from_prev(ring_pos.x() - prev_pos.x(),
                                        (terrain_height + ring_height) - prev_pos.y(),
                                        ring_pos.z() - prev_pos.z());
        QVector3D const slope_to_next(
            next_ring_pos.x() - ring_pos.x(),
            (next_clamped + k_bank_rings[ring + 1].height_offset) -
                (clamped_height + ring_height),
            next_ring_pos.z() - ring_pos.z());

        QVector3D const n1 = QVector3D::crossProduct(slope_from_prev, dir).normalized();
        QVector3D const n2 = QVector3D::crossProduct(slope_to_next, dir).normalized();
        normal = ((n1 + n2) * 0.5F).normalized();
      }

      vtx.normal[0] = normal.x();
      vtx.normal[1] = normal.y();
      vtx.normal[2] = normal.z();
      vtx.tex_coord[0] = static_cast<float>(ring) / (k_rings_per_side - 1);
      vtx.tex_coord[1] = t;
      vertices.push_back(vtx);
    }

    for (int ring = 0; ring < k_rings_per_side; ++ring) {
      float const ring_dist = k_bank_rings[ring].distance_from_water * base_bank_width;
      float const ring_height = k_bank_rings[ring].height_offset;

      QVector3D const ring_pos =
          center_pos + perpendicular * (half_width + width_variation + ring_dist);
      float const terrain_height =
          sample_height_clamped(height_map, ring_pos.x(), ring_pos.z());
      float const clamped_height = std::min(terrain_height, center_height + 0.05F);

      if (ring == 0) {
        result.visibility_samples.push_back(ring_pos);
      }

      Render::GL::Vertex vtx{};
      vtx.position[0] = ring_pos.x();
      vtx.position[1] = clamped_height + ring_height;
      vtx.position[2] = ring_pos.z();

      QVector3D normal;
      if (ring == 0) {
        QVector3D const next_ring_pos =
            center_pos +
            perpendicular * (half_width + width_variation +
                             k_bank_rings[1].distance_from_water * base_bank_width);
        float const next_terrain =
            sample_height_clamped(height_map, next_ring_pos.x(), next_ring_pos.z());
        float const next_clamped = std::min(next_terrain, center_height + 0.05F);
        QVector3D const slope_vec(next_ring_pos.x() - ring_pos.x(),
                                  (next_clamped + k_bank_rings[1].height_offset) -
                                      (clamped_height + ring_height),
                                  next_ring_pos.z() - ring_pos.z());
        normal = QVector3D::crossProduct(dir, slope_vec).normalized();
      } else if (ring == k_rings_per_side - 1) {
        unsigned int const prev_idx = ring_start_idx + k_rings_per_side + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const slope_vec(ring_pos.x() - prev_pos.x(),
                                  (clamped_height + ring_height) - prev_pos.y(),
                                  ring_pos.z() - prev_pos.z());
        normal = QVector3D::crossProduct(dir, slope_vec).normalized();
      } else {
        unsigned int const prev_idx = ring_start_idx + k_rings_per_side + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const next_ring_pos =
            center_pos + perpendicular * (half_width + width_variation +
                                          k_bank_rings[ring + 1].distance_from_water *
                                              base_bank_width);
        float const next_terrain =
            sample_height_clamped(height_map, next_ring_pos.x(), next_ring_pos.z());
        float const next_clamped = std::min(next_terrain, center_height + 0.05F);

        QVector3D const slope_from_prev(ring_pos.x() - prev_pos.x(),
                                        (clamped_height + ring_height) - prev_pos.y(),
                                        ring_pos.z() - prev_pos.z());
        QVector3D const slope_to_next(
            next_ring_pos.x() - ring_pos.x(),
            (next_clamped + k_bank_rings[ring + 1].height_offset) -
                (clamped_height + ring_height),
            next_ring_pos.z() - ring_pos.z());

        QVector3D const n1 = QVector3D::crossProduct(dir, slope_from_prev).normalized();
        QVector3D const n2 = QVector3D::crossProduct(dir, slope_to_next).normalized();
        normal = ((n1 + n2) * 0.5F).normalized();
      }

      vtx.normal[0] = normal.x();
      vtx.normal[1] = normal.y();
      vtx.normal[2] = normal.z();
      vtx.tex_coord[0] = static_cast<float>(ring) / (k_rings_per_side - 1);
      vtx.tex_coord[1] = t;
      vertices.push_back(vtx);
    }

    {
      Render::GL::Vertex const& water_edge_left = vertices[ring_start_idx];
      Render::GL::Vertex skirt_vtx = water_edge_left;
      skirt_vtx.position[1] = -0.05F;
      skirt_vtx.normal[0] = -perpendicular.x();
      skirt_vtx.normal[1] = 0.0F;
      skirt_vtx.normal[2] = -perpendicular.z();
      vertices.push_back(skirt_vtx);
    }

    {
      Render::GL::Vertex const& water_edge_right =
          vertices[ring_start_idx + k_rings_per_side];
      Render::GL::Vertex skirt_vtx = water_edge_right;
      skirt_vtx.position[1] = -0.05F;
      skirt_vtx.normal[0] = perpendicular.x();
      skirt_vtx.normal[1] = 0.0F;
      skirt_vtx.normal[2] = perpendicular.z();
      vertices.push_back(skirt_vtx);
    }

    if (i < length_steps - 1) {
      unsigned int const base_idx = ring_start_idx;
      unsigned int const next_base_idx = base_idx + k_total_rings + 2;

      for (int ring = 0; ring < k_rings_per_side - 1; ++ring) {
        unsigned int const idx0 = base_idx + ring;
        unsigned int const idx1 = base_idx + ring + 1;
        unsigned int const idx2 = next_base_idx + ring;
        unsigned int const idx3 = next_base_idx + ring + 1;

        indices.push_back(idx0);
        indices.push_back(idx2);
        indices.push_back(idx1);
        indices.push_back(idx1);
        indices.push_back(idx2);
        indices.push_back(idx3);
      }

      for (int ring = 0; ring < k_rings_per_side - 1; ++ring) {
        unsigned int const idx0 = base_idx + k_rings_per_side + ring;
        unsigned int const idx1 = base_idx + k_rings_per_side + ring + 1;
        unsigned int const idx2 = next_base_idx + k_rings_per_side + ring;
        unsigned int const idx3 = next_base_idx + k_rings_per_side + ring + 1;

        indices.push_back(idx0);
        indices.push_back(idx1);
        indices.push_back(idx2);
        indices.push_back(idx1);
        indices.push_back(idx3);
        indices.push_back(idx2);
      }

      unsigned int const left_top = base_idx;
      unsigned int const left_bottom = base_idx + k_total_rings;
      unsigned int const left_top_next = next_base_idx;
      unsigned int const left_bottom_next = next_base_idx + k_total_rings;

      indices.push_back(left_top);
      indices.push_back(left_bottom);
      indices.push_back(left_top_next);
      indices.push_back(left_bottom);
      indices.push_back(left_bottom_next);
      indices.push_back(left_top_next);

      unsigned int const right_top = base_idx + k_rings_per_side;
      unsigned int const right_bottom = base_idx + k_total_rings + 1;
      unsigned int const right_top_next = next_base_idx + k_rings_per_side;
      unsigned int const right_bottom_next = next_base_idx + k_total_rings + 1;

      indices.push_back(right_top);
      indices.push_back(right_top_next);
      indices.push_back(right_bottom);
      indices.push_back(right_bottom);
      indices.push_back(right_top_next);
      indices.push_back(right_bottom_next);
    }
  }

  if (vertices.empty() || indices.empty()) {
    result.visibility_samples.clear();
    return result;
  }

  result.mesh = std::make_unique<Render::GL::Mesh>(vertices, indices);
  return result;
}

} // namespace Render::Ground
