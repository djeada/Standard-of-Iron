#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "game/map/bridge_geometry.h"
#include "game/map/river_ribbon.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_surface.h"
#include "render/ground/linear_feature_geometry.h"
#include "render/ground/linear_feature_geometry_internal.h"
#include "render/math/render_constants.h"

namespace Render::Ground {

using namespace linear_feature_detail;

auto build_riverbank_mesh(const std::vector<Game::Map::RiverSegment>& river_network,
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
  float const tile_size = height_map.get_tile_size();
  const auto ribbon_settings = make_river_ribbon_settings();
  const LinearFeatureRibbonSegment ribbon_segment{
      segment.start, segment.end, segment.width};

  constexpr int k_rings_per_side = 7;
  constexpr int k_total_rings = k_rings_per_side * 2;

  int length_steps = static_cast<int>(std::ceil(length / (tile_size * 0.22F))) + 1;
  length_steps = std::max(length_steps, 8);

  auto endpoint_bank_direction = [&](const QVector3D& endpoint) {
    QVector3D tangent = dir;
    const float join_distance = std::max(tile_size * 0.30F, 0.05F);
    const float join_distance_sq = join_distance * join_distance;
    for (std::size_t other_index = 0; other_index < river_network.size();
         ++other_index) {
      if (other_index == segment_index) {
        continue;
      }
      const auto& other = river_network[other_index];
      const bool shares_start =
          (other.start - endpoint).lengthSquared() <= join_distance_sq;
      const bool shares_end =
          (other.end - endpoint).lengthSquared() <= join_distance_sq;
      if (!shares_start && !shares_end) {
        continue;
      }
      QVector3D neighbor_direction = other.end - other.start;
      if (neighbor_direction.lengthSquared() < 0.000001F) {
        continue;
      }
      neighbor_direction.normalize();
      float const alignment = QVector3D::dotProduct(neighbor_direction, dir);
      if (std::abs(alignment) < 0.50F) {

        continue;
      }
      if (alignment < 0.0F) {
        neighbor_direction = -neighbor_direction;
      }
      tangent += neighbor_direction;
    }
    return tangent.normalized();
  };

  auto local_bank_direction = [&](float t) {
    if (t <= 0.0001F) {
      return endpoint_bank_direction(segment.start);
    }
    if (t >= 0.9999F) {
      return endpoint_bank_direction(segment.end);
    }
    const float tangent_step = 1.0F / static_cast<float>(std::max(length_steps - 1, 1));
    const auto before = sample_linear_feature_cross_section(
        ribbon_segment, std::max(0.0F, t - tangent_step), ribbon_settings);
    const auto after = sample_linear_feature_cross_section(
        ribbon_segment, std::min(1.0F, t + tangent_step), ribbon_settings);
    QVector3D tangent = after.center - before.center;
    if (tangent.lengthSquared() < 0.000001F) {
      tangent = dir;
    } else {
      tangent.normalize();
    }
    return tangent;
  };

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  const float rendered_water_width_scale = make_river_ribbon_settings().width_scale;

  auto is_same_watercourse_continuation = [&](const Game::Map::RiverSegment& other) {
    const float join_distance = std::max(tile_size * 0.60F, 0.10F);
    const float join_distance_sq = join_distance * join_distance;
    const bool shares_endpoint =
        (other.start - segment.start).lengthSquared() <= join_distance_sq ||
        (other.start - segment.end).lengthSquared() <= join_distance_sq ||
        (other.end - segment.start).lengthSquared() <= join_distance_sq ||
        (other.end - segment.end).lengthSquared() <= join_distance_sq;
    if (!shares_endpoint) {
      return false;
    }
    QVector3D neighbor_direction = other.end - other.start;
    if (neighbor_direction.lengthSquared() < 0.000001F) {
      return false;
    }
    neighbor_direction.normalize();
    return std::abs(QVector3D::dotProduct(neighbor_direction, dir)) >= 0.50F;
  };

  auto bank_point_inside_other_channel = [&](const QVector3D& center) {
    for (std::size_t other_index = 0; other_index < river_network.size();
         ++other_index) {
      if (other_index == segment_index) {
        continue;
      }
      const auto& other = river_network[other_index];
      if (is_same_watercourse_continuation(other)) {
        continue;
      }
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
      const float water_radius = other.width * rendered_water_width_scale * 0.5F;
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
    QVector3D const center_pos = cross_section.center;

    const float bank_half_width =
        cross_section.half_width - std::min(0.22F, cross_section.half_width * 0.075F);
    const QVector3D bank_direction = local_bank_direction(t);
    const QVector3D bank_perpendicular(-bank_direction.z(), 0.0F, bank_direction.x());
    float const center_height =
        sample_height_clamped(height_map, center_pos.x(), center_pos.z());

    struct RingProfile {

      float distance_from_water;

      float height_offset;

      float terrain_follow;
    };

    constexpr RingProfile k_bank_rings[k_rings_per_side] = {
        {0.00F, -0.035F, 0.00F},
        {0.08F, -0.012F, 0.10F},
        {0.20F, 0.010F, 0.34F},
        {0.38F, 0.012F, 0.64F},
        {0.60F, 0.008F, 0.86F},
        {0.82F, 0.004F, 0.96F},
        {1.00F, 0.002F, 1.00F},
    };

    float const ring_noise =
        value_noise(center_pos.x() * 0.055F, center_pos.z() * 0.055F) * 0.34F - 0.17F;
    float const base_bank_width =
        std::clamp(segment.width * 0.24F, 0.65F, 2.40F) * (1.0F + ring_noise);

    float const side_bias =
        value_noise(center_pos.x() * 0.028F + 21.7F, center_pos.z() * 0.028F - 8.3F) -
        0.5F;
    float const left_bank_width = base_bank_width * (1.0F + side_bias * 0.70F);
    float const right_bank_width = base_bank_width * (1.0F - side_bias * 0.70F);

    auto bank_surface_height = [&](const QVector3D& position, int ring) {
      float const terrain_height =
          sample_height_clamped(height_map, position.x(), position.z());

      const float channel_height = std::min(terrain_height, center_height + 0.05F);
      const float follow = k_bank_rings[ring].terrain_follow;
      return channel_height + (terrain_height - channel_height) * follow +
             k_bank_rings[ring].height_offset;
    };

    auto const ring_start_idx = static_cast<unsigned int>(vertices.size());

    for (int ring = 0; ring < k_rings_per_side; ++ring) {
      float const ring_dist = k_bank_rings[ring].distance_from_water * left_bank_width;

      QVector3D const ring_pos =
          center_pos - bank_perpendicular * (bank_half_width + ring_dist);
      float const ring_height_y = bank_surface_height(ring_pos, ring);

      if (ring == 0) {
        result.visibility_samples.push_back(ring_pos);
      }

      Render::GL::Vertex vtx{};
      vtx.position[0] = ring_pos.x();
      vtx.position[1] = ring_height_y;
      vtx.position[2] = ring_pos.z();

      QVector3D normal;
      if (ring == 0) {
        QVector3D const next_ring_pos =
            center_pos - bank_perpendicular *
                             (bank_half_width +
                              k_bank_rings[1].distance_from_water * left_bank_width);
        float const next_height = bank_surface_height(next_ring_pos, ring + 1);
        QVector3D const slope_vec(next_ring_pos.x() - ring_pos.x(),
                                  next_height - ring_height_y,
                                  next_ring_pos.z() - ring_pos.z());
        normal = QVector3D::crossProduct(bank_direction, slope_vec).normalized();
      } else if (ring == k_rings_per_side - 1) {
        unsigned int const prev_idx = ring_start_idx + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const slope_vec(ring_pos.x() - prev_pos.x(),
                                  ring_height_y - prev_pos.y(),
                                  ring_pos.z() - prev_pos.z());
        normal = QVector3D::crossProduct(bank_direction, slope_vec).normalized();
      } else {
        unsigned int const prev_idx = ring_start_idx + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const next_ring_pos =
            center_pos -
            bank_perpendicular *
                (bank_half_width +
                 k_bank_rings[ring + 1].distance_from_water * left_bank_width);
        float const next_height = bank_surface_height(next_ring_pos, ring + 1);

        QVector3D const slope_from_prev(ring_pos.x() - prev_pos.x(),
                                        ring_height_y - prev_pos.y(),
                                        ring_pos.z() - prev_pos.z());
        QVector3D const slope_to_next(next_ring_pos.x() - ring_pos.x(),
                                      next_height - ring_height_y,
                                      next_ring_pos.z() - ring_pos.z());

        QVector3D const n1 =
            QVector3D::crossProduct(bank_direction, slope_from_prev).normalized();
        QVector3D const n2 =
            QVector3D::crossProduct(bank_direction, slope_to_next).normalized();
        normal = ((n1 + n2) * 0.5F).normalized();
      }

      vtx.normal[0] = normal.x();
      vtx.normal[1] = normal.y();
      vtx.normal[2] = normal.z();
      vtx.tex_coord[0] = k_bank_rings[ring].distance_from_water;
      vtx.tex_coord[1] = t;
      vertices.push_back(vtx);
    }

    for (int ring = 0; ring < k_rings_per_side; ++ring) {
      float const ring_dist = k_bank_rings[ring].distance_from_water * right_bank_width;

      QVector3D const ring_pos =
          center_pos + bank_perpendicular * (bank_half_width + ring_dist);
      float const ring_height_y = bank_surface_height(ring_pos, ring);

      if (ring == 0) {
        result.visibility_samples.push_back(ring_pos);
      }

      Render::GL::Vertex vtx{};
      vtx.position[0] = ring_pos.x();
      vtx.position[1] = ring_height_y;
      vtx.position[2] = ring_pos.z();

      QVector3D normal;
      if (ring == 0) {
        QVector3D const next_ring_pos =
            center_pos + bank_perpendicular *
                             (bank_half_width +
                              k_bank_rings[1].distance_from_water * right_bank_width);
        float const next_height = bank_surface_height(next_ring_pos, ring + 1);
        QVector3D const slope_vec(next_ring_pos.x() - ring_pos.x(),
                                  next_height - ring_height_y,
                                  next_ring_pos.z() - ring_pos.z());
        normal = QVector3D::crossProduct(slope_vec, bank_direction).normalized();
      } else if (ring == k_rings_per_side - 1) {
        unsigned int const prev_idx = ring_start_idx + k_rings_per_side + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const slope_vec(ring_pos.x() - prev_pos.x(),
                                  ring_height_y - prev_pos.y(),
                                  ring_pos.z() - prev_pos.z());
        normal = QVector3D::crossProduct(slope_vec, bank_direction).normalized();
      } else {
        unsigned int const prev_idx = ring_start_idx + k_rings_per_side + ring - 1;
        QVector3D const prev_pos(vertices[prev_idx].position[0],
                                 vertices[prev_idx].position[1],
                                 vertices[prev_idx].position[2]);
        QVector3D const next_ring_pos =
            center_pos +
            bank_perpendicular *
                (bank_half_width +
                 k_bank_rings[ring + 1].distance_from_water * right_bank_width);
        float const next_height = bank_surface_height(next_ring_pos, ring + 1);

        QVector3D const slope_from_prev(ring_pos.x() - prev_pos.x(),
                                        ring_height_y - prev_pos.y(),
                                        ring_pos.z() - prev_pos.z());
        QVector3D const slope_to_next(next_ring_pos.x() - ring_pos.x(),
                                      next_height - ring_height_y,
                                      next_ring_pos.z() - ring_pos.z());

        QVector3D const n1 =
            QVector3D::crossProduct(slope_from_prev, bank_direction).normalized();
        QVector3D const n2 =
            QVector3D::crossProduct(slope_to_next, bank_direction).normalized();
        normal = ((n1 + n2) * 0.5F).normalized();
      }

      vtx.normal[0] = normal.x();
      vtx.normal[1] = normal.y();
      vtx.normal[2] = normal.z();
      vtx.tex_coord[0] = k_bank_rings[ring].distance_from_water;
      vtx.tex_coord[1] = t;
      vertices.push_back(vtx);
    }

    if (i < length_steps - 1) {
      unsigned int const base_idx = ring_start_idx;
      unsigned int const next_base_idx = base_idx + k_total_rings;
      const float midpoint_t =
          (static_cast<float>(i) + 0.5F) / static_cast<float>(length_steps - 1);
      const auto midpoint = sample_linear_feature_cross_section(
          ribbon_segment, midpoint_t, ribbon_settings);
      const QVector3D midpoint_direction = local_bank_direction(midpoint_t);
      const QVector3D midpoint_perpendicular(
          -midpoint_direction.z(), 0.0F, midpoint_direction.x());
      const bool clip_left_bank = bank_point_inside_other_channel(
          midpoint.center - midpoint_perpendicular * midpoint.half_width);
      const bool clip_right_bank = bank_point_inside_other_channel(
          midpoint.center + midpoint_perpendicular * midpoint.half_width);

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

auto build_riverbank_junction_meshes(
    const std::vector<Game::Map::RiverSegment>& river_network,
    const Game::Map::TerrainHeightMap& height_map)
    -> std::vector<RiverbankMeshBuildResult> {
  struct Junction {
    QVector3D center;
    float water_radius = 0.0F;
    float bank_width = 0.0F;
    int samples = 0;
  };

  const float tile_size = height_map.get_tile_size();
  const auto settings = make_river_ribbon_settings();
  const float join_distance = std::max(tile_size * 0.30F, 0.05F);
  const float join_distance_sq = join_distance * join_distance;
  std::vector<Junction> junctions;

  auto add_endpoint = [&](const LinearFeatureCrossSection& endpoint,
                          float authored_width) {
    auto match =
        std::find_if(junctions.begin(), junctions.end(), [&](const auto& item) {
          const float dx = item.center.x() - endpoint.center.x();
          const float dz = item.center.z() - endpoint.center.z();
          return dx * dx + dz * dz <= join_distance_sq;
        });
    const float bank_width = 0.50F + std::min(authored_width * 0.0025F, 0.075F);
    if (match == junctions.end()) {
      junctions.push_back({endpoint.center, endpoint.half_width, bank_width, 1});
      return;
    }
    match->center =
        (match->center * static_cast<float>(match->samples) + endpoint.center) /
        static_cast<float>(match->samples + 1);
    match->water_radius = std::max(match->water_radius, endpoint.half_width);
    match->bank_width = std::max(match->bank_width, bank_width);
    ++match->samples;
  };

  for (const auto& segment : river_network) {
    const LinearFeatureRibbonSegment ribbon{segment.start, segment.end, segment.width};
    add_endpoint(sample_linear_feature_cross_section(ribbon, 0.0F, settings),
                 segment.width);
    add_endpoint(sample_linear_feature_cross_section(ribbon, 1.0F, settings),
                 segment.width);
  }

  for (auto& junction : junctions) {
    if (junction.samples >= 2) {
      continue;
    }
    int overlapping_channels = 0;
    for (const auto& segment : river_network) {
      const float dx = segment.end.x() - segment.start.x();
      const float dz = segment.end.z() - segment.start.z();
      const float length_squared = dx * dx + dz * dz;
      const float projection =
          length_squared > 0.0001F
              ? std::clamp(((junction.center.x() - segment.start.x()) * dx +
                            (junction.center.z() - segment.start.z()) * dz) /
                               length_squared,
                           0.0F,
                           1.0F)
              : 0.0F;
      const float nearest_x = segment.start.x() + dx * projection;
      const float nearest_z = segment.start.z() + dz * projection;
      const float distance_x = junction.center.x() - nearest_x;
      const float distance_z = junction.center.z() - nearest_z;
      const float channel_radius = segment.width * settings.width_scale * 0.5F;
      if (distance_x * distance_x + distance_z * distance_z <=
          channel_radius * channel_radius) {
        ++overlapping_channels;
        junction.water_radius = std::max(junction.water_radius, channel_radius);
        junction.bank_width = std::max(
            junction.bank_width, 0.50F + std::min(segment.width * 0.0025F, 0.075F));
      }
    }
    junction.samples = std::max(junction.samples, overlapping_channels);
  }

  constexpr int k_bank_rings = 6;
  constexpr float two_pi = 2.0F * std::numbers::pi_v<float>;
  std::vector<RiverbankMeshBuildResult> results;
  for (const auto& junction : junctions) {
    if (junction.samples < 2 || junction.water_radius <= 0.01F) {
      continue;
    }

    const int angular_segments =
        std::clamp(static_cast<int>(std::ceil(
                       two_pi * (junction.water_radius + junction.bank_width) /
                       std::max(tile_size * 0.28F, 0.18F))),
                   20,
                   64);
    std::vector<Render::GL::Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(1U + static_cast<std::size_t>(k_bank_rings * angular_segments));
    indices.reserve(static_cast<std::size_t>(angular_segments) *
                    (3U + static_cast<unsigned int>(k_bank_rings - 1) * 6U));

    const float bed_height = sample_water_surface_height_clamped(
        height_map, junction.center.x(), junction.center.z());

    const float waterline_height = bed_height + settings.y_offset - 0.320F;

    Render::GL::Vertex center_vertex{};
    center_vertex.position = {
        junction.center.x(), waterline_height, junction.center.z()};
    center_vertex.normal = {0.0F, 1.0F, 0.0F};
    center_vertex.tex_coord = {0.0F, 0.0F};
    vertices.push_back(center_vertex);

    constexpr float radius_profile[k_bank_rings] = {
        0.52F, 1.02F, 1.22F, 1.50F, 1.76F, 2.0F};
    for (int ring = 0; ring < k_bank_rings; ++ring) {
      const float shore_t = ring < 2 ? 0.0F
                                     : static_cast<float>(ring - 1) /
                                           static_cast<float>(k_bank_rings - 2);
      const float radius =
          ring < 2 ? junction.water_radius * radius_profile[ring]
                   : junction.water_radius +
                         junction.bank_width * (radius_profile[ring] - 1.0F);
      const float blend = smoothstep01(shore_t);
      for (int segment = 0; segment < angular_segments; ++segment) {
        const float angle_t =
            static_cast<float>(segment) / static_cast<float>(angular_segments);
        const float angle = angle_t * two_pi;
        const QVector3D outward(std::cos(angle), 0.0F, std::sin(angle));
        const float world_x = junction.center.x() + outward.x() * radius;
        const float world_z = junction.center.z() + outward.z() * radius;
        const float terrain_height =
            sample_height_clamped(height_map, world_x, world_z);
        const float blended = mixf(waterline_height, terrain_height + 0.055F, blend);
        constexpr float k_junction_lift[k_bank_rings] = {
            0.0F, 0.0F, 0.016F, 0.010F, 0.004F, 0.0F};
        const float height =
            ring < 2 ? waterline_height
                     : std::max(blended, terrain_height + k_junction_lift[ring]);
        const float radial_slope =
            std::clamp((terrain_height + 0.055F - waterline_height) /
                           std::max(junction.bank_width, 0.01F),
                       -0.65F,
                       0.65F);
        const QVector3D normal =
            QVector3D(-outward.x() * radial_slope, 1.0F, -outward.z() * radial_slope)
                .normalized();

        Render::GL::Vertex vertex{};
        vertex.position = {world_x, height, world_z};
        vertex.normal = {normal.x(), normal.y(), normal.z()};
        vertex.tex_coord = {shore_t, angle_t};
        vertices.push_back(vertex);
      }
    }

    for (int segment = 0; segment < angular_segments; ++segment) {
      const unsigned int current = 1U + static_cast<unsigned int>(segment);
      const unsigned int next =
          1U + static_cast<unsigned int>((segment + 1) % angular_segments);
      indices.insert(indices.end(), {0U, next, current});
    }
    for (int ring = 0; ring < k_bank_rings - 1; ++ring) {
      const unsigned int inner =
          1U + static_cast<unsigned int>(ring * angular_segments);
      const unsigned int outer =
          1U + static_cast<unsigned int>((ring + 1) * angular_segments);
      for (int segment = 0; segment < angular_segments; ++segment) {
        const auto next = static_cast<unsigned int>((segment + 1) % angular_segments);
        const unsigned int a = inner + static_cast<unsigned int>(segment);
        const unsigned int b = outer + static_cast<unsigned int>(segment);
        const unsigned int c = outer + next;
        const unsigned int d = inner + next;
        indices.insert(indices.end(), {a, d, b, d, c, b});
      }
    }

    RiverbankMeshBuildResult result;
    result.mesh = std::make_unique<Render::GL::Mesh>(vertices, indices);
    for (int segment = 0; segment < angular_segments; segment += 3) {
      const float angle =
          two_pi * static_cast<float>(segment) / static_cast<float>(angular_segments);
      result.visibility_samples.emplace_back(
          junction.center.x() +
              std::cos(angle) * (junction.water_radius + junction.bank_width),
          waterline_height,
          junction.center.z() +
              std::sin(angle) * (junction.water_radius + junction.bank_width));
    }
    results.push_back(std::move(result));
  }

  return results;
}

} // namespace Render::Ground
