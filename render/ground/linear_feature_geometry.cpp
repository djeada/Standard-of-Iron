#include "linear_feature_geometry.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
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

auto sample_water_surface_height_clamped(
    const Game::Map::TerrainHeightMap& height_map,
    float world_x,
    float world_z) -> float {
  // Lake cells store submerged bed height for collision and shoreline blending.
  // A feeding river instead needs the lake's surface elevation at their join.
  const float shoreline_blend = height_map.get_tile_size() * 1.25F;
  for (const auto& lake : height_map.get_lakes()) {
    if (Game::Map::point_in_lake(lake, world_x, world_z, shoreline_blend)) {
      return lake.center.y();
    }
  }
  return sample_height_clamped(height_map, world_x, world_z);
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

auto make_river_ribbon_settings() -> LinearFeatureRibbonSettings {
  LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.5F;
  settings.min_length_steps = 8;
  settings.cross_section_segments = 4;
  settings.edge_noise_frequencies = {0.08F, 0.22F, 0.65F};
  settings.edge_noise_weights = {0.62F, 0.28F, 0.10F};
  settings.width_scale = 0.92F;
  settings.width_variation_scale = 0.18F;
  settings.meander_frequency = 2.8F;
  // For rivers this value is a fraction of authored width. The endpoint fade
  // keeps independently authored segments watertight at junctions.
  settings.meander_amplitude = 0.11F;
  settings.y_offset = 0.12F;
  return settings;
}

auto sample_linear_feature_cross_section(
    const LinearFeatureRibbonSegment& segment,
    float t,
    const LinearFeatureRibbonSettings& settings) -> LinearFeatureCrossSection {
  QVector3D direction = segment.end - segment.start;
  const float length = direction.length();
  if (length < 0.01F) {
    return {segment.start, std::max(segment.width * 0.5F, 0.01F)};
  }
  direction.normalize();
  QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
  QVector3D center = segment.start + direction * (length * std::clamp(t, 0.0F, 1.0F));

  float combined_noise = 0.0F;
  float weight_sum = 0.0F;
  for (std::size_t index = 0; index < settings.edge_noise_frequencies.size();
       ++index) {
    const float frequency = settings.edge_noise_frequencies[index];
    const float weight = settings.edge_noise_weights[index];
    if (frequency <= 0.0F || weight <= 0.0F) {
      continue;
    }
    combined_noise += value_noise(center.x() * frequency, center.z() * frequency) *
                      weight;
    weight_sum += weight;
  }
  if (weight_sum > 0.0F) {
    combined_noise = combined_noise / weight_sum * 2.0F - 1.0F;
  }

  if (settings.meander_amplitude > 0.0F && settings.meander_frequency > 0.0F) {
    const float meander =
        (value_noise(t * settings.meander_frequency,
                     length * settings.meander_length_scale) *
             2.0F -
         1.0F) *
        segment.width * settings.meander_amplitude *
        std::sin(std::numbers::pi_v<float> * std::clamp(t, 0.0F, 1.0F));
    center += perpendicular * meander;
  }

  // Leave a narrow authored bank inside the blocked gameplay footprint.
  const float half_width = segment.width * 0.5F * settings.width_scale;
  return {center,
          std::max(half_width +
                       combined_noise * half_width * settings.width_variation_scale,
                   0.01F)};
}

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
    const auto cross_section =
        sample_linear_feature_cross_section(segment, t, settings);
    QVector3D const center_pos = cross_section.center;
    const float centerline_height =
        settings.height_map != nullptr && settings.follow_terrain_centerline
            ? sample_water_surface_height_clamped(
                  *settings.height_map, center_pos.x(), center_pos.z())
            : center_pos.y();

    float const normal[3] = {0.0F, 1.0F, 0.0F};

    float const row_half_width = cross_section.half_width;
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
        if (settings.follow_terrain_centerline) {
          // Water stays level across its width but follows the already-carved
          // riverbed along the flow direction.
          vertex_y = centerline_height;
        } else if (settings.sample_terrain_envelope) {
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

auto build_linear_feature_junction_meshes(
    const std::vector<LinearFeatureRibbonSegment>& segments,
    float tile_size,
    const LinearFeatureRibbonSettings& settings)
    -> std::vector<LinearFeatureJunctionMesh> {
  struct Junction {
    QVector3D center;
    float radius = 0.0F;
    int samples = 0;
  };

  std::vector<Junction> junctions;
  const float join_distance = std::max(tile_size * 0.30F, 0.05F);
  const float join_distance_sq = join_distance * join_distance;
  auto add_endpoint = [&](const LinearFeatureCrossSection& endpoint) {
    auto match = std::find_if(junctions.begin(), junctions.end(), [&](const auto& item) {
      const float dx = item.center.x() - endpoint.center.x();
      const float dz = item.center.z() - endpoint.center.z();
      return dx * dx + dz * dz <= join_distance_sq;
    });
    if (match == junctions.end()) {
      junctions.push_back({endpoint.center, endpoint.half_width, 1});
      return;
    }
    match->center =
        (match->center * static_cast<float>(match->samples) + endpoint.center) /
        static_cast<float>(match->samples + 1);
    match->radius = std::max(match->radius, endpoint.half_width);
    ++match->samples;
  };

  for (const auto& segment : segments) {
    if ((segment.end - segment.start).lengthSquared() < 0.0001F) {
      continue;
    }
    add_endpoint(sample_linear_feature_cross_section(segment, 0.0F, settings));
    add_endpoint(sample_linear_feature_cross_section(segment, 1.0F, settings));
  }

  constexpr float two_pi = 2.0F * std::numbers::pi_v<float>;
  std::vector<LinearFeatureJunctionMesh> result;
  result.reserve(junctions.size());
  for (const auto& junction : junctions) {
    const int ring_segments = std::clamp(
        static_cast<int>(std::ceil(two_pi * junction.radius /
                                   std::max(tile_size * 0.35F, 0.15F))),
        16,
        48);
    std::vector<Render::GL::Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(static_cast<std::size_t>(ring_segments + 1));
    indices.reserve(static_cast<std::size_t>(ring_segments * 3));

    auto append_vertex = [&](const QVector3D& position, float radial_t, float angle_t) {
      Render::GL::Vertex vertex{};
      vertex.position = {position.x(), position.y() + settings.y_offset, position.z()};
      vertex.normal = {0.0F, 1.0F, 0.0F};
      vertex.tex_coord = {0.5F * (1.0F - radial_t), angle_t};
      vertices.push_back(vertex);
    };
    append_vertex(junction.center, 0.0F, 0.5F);
    for (int index = 0; index < ring_segments; ++index) {
      const float angle_t = static_cast<float>(index) /
                            static_cast<float>(ring_segments);
      const float angle = angle_t * two_pi;
      QVector3D position = junction.center;
      position.setX(position.x() + std::cos(angle) * junction.radius);
      position.setZ(position.z() + std::sin(angle) * junction.radius);
      append_vertex(position, 1.0F, angle_t);
    }
    for (int index = 0; index < ring_segments; ++index) {
      const auto current = 1U + static_cast<unsigned int>(index);
      const auto next = 1U +
                        static_cast<unsigned int>((index + 1) % ring_segments);
      indices.insert(indices.end(), {0U, current, next});
    }
    result.push_back(
        {std::make_unique<Render::GL::Mesh>(vertices, indices), junction.center});
  }
  return result;
}

auto build_lake_surface_mesh(const Game::Map::Lake& lake,
                             float tile_size,
                             float y_offset) -> std::unique_ptr<Render::GL::Mesh> {
  constexpr float two_pi = 6.28318530717958647692F;
  constexpr float deg_to_rad = 0.01745329251994329577F;
  const float half_width = std::max(lake.width * 0.5F, tile_size * 0.5F);
  const float half_depth = std::max(lake.depth * 0.5F, tile_size * 0.5F);
  const int angular_segments = std::clamp(
      static_cast<int>(std::ceil(two_pi * std::max(half_width, half_depth) /
                                 std::max(tile_size * 0.65F, 0.25F))),
      32,
      160);
  const int radial_rings = std::clamp(
      static_cast<int>(std::ceil(std::min(half_width, half_depth) /
                                 std::max(tile_size * 0.8F, 0.35F))),
      3,
      28);
  const float rotation = lake.rotation_deg * deg_to_rad;
  const float cos_rotation = std::cos(rotation);
  const float sin_rotation = std::sin(rotation);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(1U + static_cast<std::size_t>(radial_rings * angular_segments));
  indices.reserve(static_cast<std::size_t>(angular_segments) *
                  static_cast<std::size_t>(radial_rings * 6));

  auto append_vertex = [&](float local_x, float local_z, float radial_t) {
    const float rotated_x = local_x * cos_rotation - local_z * sin_rotation;
    const float rotated_z = local_x * sin_rotation + local_z * cos_rotation;
    Render::GL::Vertex vertex{};
    vertex.position = {lake.center.x() + rotated_x,
                       lake.center.y() + y_offset,
                       lake.center.z() + rotated_z};
    vertex.normal = {0.0F, 1.0F, 0.0F};
    // The shared water shader treats tex_coord.y == 0 as shoreline.
    vertex.tex_coord = {0.5F + local_x / (half_width * 2.2F),
                        std::clamp(0.5F * (1.0F - radial_t), 0.0F, 0.5F)};
    vertices.push_back(vertex);
  };

  append_vertex(0.0F, 0.0F, 0.0F);
  for (int ring = 1; ring <= radial_rings; ++ring) {
    const float radial_t = static_cast<float>(ring) / static_cast<float>(radial_rings);
    for (int segment = 0; segment < angular_segments; ++segment) {
      const float angle = two_pi * static_cast<float>(segment) /
                          static_cast<float>(angular_segments);
      const float boundary = Game::Map::lake_boundary_scale(lake, angle);
      append_vertex(std::cos(angle) * half_width * radial_t * boundary,
                    std::sin(angle) * half_depth * radial_t * boundary,
                    radial_t);
    }
  }

  for (int segment = 0; segment < angular_segments; ++segment) {
    const unsigned int current = 1U + static_cast<unsigned int>(segment);
    const unsigned int next =
        1U + static_cast<unsigned int>((segment + 1) % angular_segments);
    indices.insert(indices.end(), {0U, next, current});
  }
  for (int ring = 1; ring < radial_rings; ++ring) {
    const unsigned int inner =
        1U + static_cast<unsigned int>((ring - 1) * angular_segments);
    const unsigned int outer = 1U + static_cast<unsigned int>(ring * angular_segments);
    for (int segment = 0; segment < angular_segments; ++segment) {
      const unsigned int next =
          static_cast<unsigned int>((segment + 1) % angular_segments);
      const unsigned int a = inner + static_cast<unsigned int>(segment);
      const unsigned int b = outer + static_cast<unsigned int>(segment);
      const unsigned int c = outer + next;
      const unsigned int d = inner + next;
      indices.insert(indices.end(), {a, d, b, d, c, b});
    }
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
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
  const float approach_length =
      std::min(bridge.width * 0.70F, length * 0.14F);
  const float visual_length = length + approach_length * 2.0F;
  float const segment_step = std::max(tile_size * 0.28F, 0.16F);
  float const end_fade_length =
      std::min(std::clamp(bridge.width * 0.70F, tile_size * 0.45F, tile_size * 1.20F),
               visual_length * 0.45F);

  int length_segments = static_cast<int>(std::ceil(visual_length / segment_step));
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
    float const span_distance = visual_length * mesh_t;
    const float authored_distance =
        std::clamp(span_distance - approach_length, 0.0F, length);
    const float authored_t = authored_distance / length;
    QVector3D const center_pos =
        bridge.start + dir * (span_distance - approach_length);

    float const start_blend =
        smoothstep01(span_distance / std::max(end_fade_length, 0.001F));
    float const end_blend =
        smoothstep01((visual_length - span_distance) /
                     std::max(end_fade_length, 0.001F));
    float const profile_blend = start_blend * end_blend;

    float const arch_curve =
        Game::Map::bridge_arch_curve(authored_t) * (0.85F + 0.15F * profile_blend);
    float const deck_height = Game::Map::bridge_deck_world_y(bridge, authored_t);

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
    float const tex_v = mesh_t * visual_length * 0.4F;

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

auto build_riverbank_mesh(
    const std::vector<Game::Map::RiverSegment>& river_network,
    std::size_t segment_index,
    const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult {
  RiverbankMeshBuildResult result;

  if (segment_index >= river_network.size()) {
    return result;
  }
  const auto& segment = river_network[segment_index];

  QVector3D dir = segment.end - segment.start;
  float const length = dir.length();
  if (length < 0.01F) {
    return result;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const tile_size = height_map.get_tile_size();
  const auto ribbon_settings = make_river_ribbon_settings();
  const LinearFeatureRibbonSegment ribbon_segment{
      segment.start, segment.end, segment.width};

  constexpr int k_rings_per_side = 5;
  constexpr int k_total_rings = k_rings_per_side * 2;

  int length_steps = static_cast<int>(std::ceil(length / (tile_size * 0.5F))) + 1;
  length_steps = std::max(length_steps, 8);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  const float rendered_water_width_scale =
      make_river_ribbon_settings().width_scale;

  auto bank_point_inside_other_channel = [&](const QVector3D& center) {
    for (std::size_t other_index = 0; other_index < river_network.size();
         ++other_index) {
      if (other_index == segment_index) {
        continue;
      }
      const auto& other = river_network[other_index];
      const float river_dx = other.end.x() - other.start.x();
      const float river_dz = other.end.z() - other.start.z();
      const float length_squared = river_dx * river_dx + river_dz * river_dz;
      const float projection =
          length_squared > 0.0001F
              ? std::clamp(((center.x() - other.start.x()) * river_dx +
                            (center.z() - other.start.z()) * river_dz) /
                               length_squared,
                           0.0F,
                           1.0F)
              : 0.0F;
      const float nearest_x = other.start.x() + river_dx * projection;
      const float nearest_z = other.start.z() + river_dz * projection;
      const float dx = center.x() - nearest_x;
      const float dz = center.z() - nearest_z;
      const float water_radius =
          other.width * rendered_water_width_scale * 0.5F;
      if (dx * dx + dz * dz <= water_radius * water_radius) {
        return true;
      }
    }
    return false;
  };

  for (int i = 0; i < length_steps; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(length_steps - 1);
    const auto cross_section =
        sample_linear_feature_cross_section(ribbon_segment, t, ribbon_settings);
    QVector3D center_pos = cross_section.center;
    const float bank_half_width = cross_section.half_width;
    float const center_height =
        sample_height_clamped(height_map, center_pos.x(), center_pos.z());

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
          center_pos - perpendicular * (bank_half_width + ring_dist);
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
            perpendicular * (bank_half_width +
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
            center_pos - perpendicular * (bank_half_width +
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
          center_pos + perpendicular * (bank_half_width + ring_dist);
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
            perpendicular * (bank_half_width +
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
            center_pos + perpendicular * (bank_half_width +
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
      const float midpoint_t =
          (static_cast<float>(i) + 0.5F) /
          static_cast<float>(length_steps - 1);
      const auto midpoint = sample_linear_feature_cross_section(
          ribbon_segment, midpoint_t, ribbon_settings);
      const bool clip_left_bank = bank_point_inside_other_channel(
          midpoint.center - perpendicular * midpoint.half_width);
      const bool clip_right_bank = bank_point_inside_other_channel(
          midpoint.center + perpendicular * midpoint.half_width);

      for (int ring = 0; ring < k_rings_per_side - 1; ++ring) {
        unsigned int const idx0 = base_idx + ring;
        unsigned int const idx1 = base_idx + ring + 1;
        unsigned int const idx2 = next_base_idx + ring;
        unsigned int const idx3 = next_base_idx + ring + 1;

        if (!clip_left_bank) {
          indices.push_back(idx0);
          indices.push_back(idx2);
          indices.push_back(idx1);
          indices.push_back(idx1);
          indices.push_back(idx2);
          indices.push_back(idx3);
        }
      }

      for (int ring = 0; ring < k_rings_per_side - 1; ++ring) {
        unsigned int const idx0 = base_idx + k_rings_per_side + ring;
        unsigned int const idx1 = base_idx + k_rings_per_side + ring + 1;
        unsigned int const idx2 = next_base_idx + k_rings_per_side + ring;
        unsigned int const idx3 = next_base_idx + k_rings_per_side + ring + 1;

        if (!clip_right_bank) {
          indices.push_back(idx0);
          indices.push_back(idx1);
          indices.push_back(idx2);
          indices.push_back(idx1);
          indices.push_back(idx3);
          indices.push_back(idx2);
        }
      }

      unsigned int const left_top = base_idx;
      unsigned int const left_bottom = base_idx + k_total_rings;
      unsigned int const left_top_next = next_base_idx;
      unsigned int const left_bottom_next = next_base_idx + k_total_rings;

      if (!clip_left_bank) {
        indices.push_back(left_top);
        indices.push_back(left_bottom);
        indices.push_back(left_top_next);
        indices.push_back(left_bottom);
        indices.push_back(left_bottom_next);
        indices.push_back(left_top_next);
      }

      unsigned int const right_top = base_idx + k_rings_per_side;
      unsigned int const right_bottom = base_idx + k_total_rings + 1;
      unsigned int const right_top_next = next_base_idx + k_rings_per_side;
      unsigned int const right_bottom_next = next_base_idx + k_total_rings + 1;

      if (!clip_right_bank) {
        indices.push_back(right_top);
        indices.push_back(right_top_next);
        indices.push_back(right_bottom);
        indices.push_back(right_bottom);
        indices.push_back(right_top_next);
        indices.push_back(right_bottom_next);
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    result.visibility_samples.clear();
    return result;
  }

  result.mesh = std::make_unique<Render::GL::Mesh>(vertices, indices);
  return result;
}

auto build_riverbank_mesh(const Game::Map::RiverSegment& segment,
                          const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult {
  return build_riverbank_mesh(
      std::vector<Game::Map::RiverSegment>{segment}, 0U, height_map);
}

auto build_lake_shore_mesh(const Game::Map::Lake& lake,
                           const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult {
  RiverbankMeshBuildResult result;
  constexpr float two_pi = 6.28318530717958647692F;
  constexpr float deg_to_rad = 0.01745329251994329577F;
  constexpr int ring_count = 3;
  const float tile_size = height_map.get_tile_size();
  const float half_width = std::max(lake.width * 0.5F, tile_size * 0.5F);
  const float half_depth = std::max(lake.depth * 0.5F, tile_size * 0.5F);
  const float shore_width = std::clamp(tile_size * 0.7F, 0.45F, 1.1F);
  const int angular_segments = std::clamp(
      static_cast<int>(std::ceil(two_pi * std::max(half_width, half_depth) /
                                 std::max(tile_size * 0.75F, 0.3F))),
      32,
      160);
  const float rotation = lake.rotation_deg * deg_to_rad;
  const float cos_rotation = std::cos(rotation);
  const float sin_rotation = std::sin(rotation);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(static_cast<std::size_t>((ring_count + 1) * angular_segments));
  indices.reserve(static_cast<std::size_t>(ring_count * angular_segments * 6));

  for (int ring = 0; ring <= ring_count; ++ring) {
    const float shore_t = static_cast<float>(ring) / static_cast<float>(ring_count);
    const float blend = smoothstep01(shore_t);
    for (int segment = 0; segment < angular_segments; ++segment) {
      const float angle = two_pi * static_cast<float>(segment) /
                          static_cast<float>(angular_segments);
      const float boundary = Game::Map::lake_boundary_scale(lake, angle);
      const float local_x =
          std::cos(angle) * (half_width * boundary + shore_width * shore_t);
      const float local_z =
          std::sin(angle) * (half_depth * boundary + shore_width * shore_t);
      const float rotated_x = local_x * cos_rotation - local_z * sin_rotation;
      const float rotated_z = local_x * sin_rotation + local_z * cos_rotation;
      const float world_x = lake.center.x() + rotated_x;
      const float world_z = lake.center.z() + rotated_z;
      const float terrain_y = sample_height_clamped(height_map, world_x, world_z);
      const float water_edge_y = lake.center.y() + 0.045F;

      Render::GL::Vertex vertex{};
      vertex.position = {
          world_x, mixf(water_edge_y, terrain_y + 0.015F, blend), world_z};
      vertex.normal = {0.0F, 1.0F, 0.0F};
      vertex.tex_coord = {shore_t,
                          static_cast<float>(segment) /
                              static_cast<float>(angular_segments)};
      vertices.push_back(vertex);
      if (ring == ring_count && segment % 3 == 0) {
        result.visibility_samples.emplace_back(world_x, terrain_y, world_z);
      }
    }
  }

  for (int ring = 0; ring < ring_count; ++ring) {
    const unsigned int inner = static_cast<unsigned int>(ring * angular_segments);
    const unsigned int outer =
        static_cast<unsigned int>((ring + 1) * angular_segments);
    for (int segment = 0; segment < angular_segments; ++segment) {
      const unsigned int next =
          static_cast<unsigned int>((segment + 1) % angular_segments);
      const unsigned int a = inner + static_cast<unsigned int>(segment);
      const unsigned int b = outer + static_cast<unsigned int>(segment);
      const unsigned int c = outer + next;
      const unsigned int d = inner + next;
      indices.insert(indices.end(), {a, d, b, d, c, b});
    }
  }

  result.mesh = std::make_unique<Render::GL::Mesh>(vertices, indices);
  return result;
}

} // namespace Render::Ground
