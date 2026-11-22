#include "render/ground/linear_feature_geometry.h"

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
#include "render/ground/linear_feature_geometry_internal.h"
#include "render/math/render_constants.h"

namespace Render::Ground {

using namespace linear_feature_detail;

auto make_river_ribbon_settings() -> LinearFeatureRibbonSettings {
  constexpr auto& shape = Game::Map::k_river_ribbon_shape;
  static_assert(shape.width_scale * (1.0F + shape.width_variation_scale) <=
                    Game::Map::k_river_drawn_edge_scale,
                "river ribbon draws wider than the worst-case drawn river edge");
  static_assert(shape.meander_amplitude <= Game::Map::k_river_drawn_meander_reach,
                "river ribbon meanders further than the worst-case drawn river edge");

  LinearFeatureRibbonSettings settings;
  settings.sample_step = 0.24F;
  settings.min_length_steps = 8;
  settings.cross_section_segments = 4;
  settings.edge_noise_frequencies = shape.edge_noise_frequencies;
  settings.edge_noise_weights = shape.edge_noise_weights;
  settings.width_scale = shape.width_scale;
  settings.width_variation_scale = shape.width_variation_scale;
  settings.meander_frequency = shape.meander_frequency;
  settings.meander_length_scale = shape.meander_length_scale;
  settings.meander_amplitude = shape.meander_amplitude;
  settings.y_offset = 0.12F;
  settings.junction_sink = 0.03F;
  settings.segment_layer_step = 0.004F;
  return settings;
}

auto make_water_surface_ribbon_settings(const Game::Map::TerrainHeightMap& height_map)
    -> LinearFeatureRibbonSettings {
  LinearFeatureRibbonSettings settings = make_river_ribbon_settings();
  settings.height_map = &height_map;
  settings.use_segment_elevation_profile = true;
  settings.y_offset = 0.02F;
  return settings;
}

auto sample_linear_feature_cross_section(const LinearFeatureRibbonSegment& segment,
                                         float t,
                                         const LinearFeatureRibbonSettings& settings)
    -> LinearFeatureCrossSection {
  const Game::Map::RibbonShape shape{
      .edge_noise_frequencies = settings.edge_noise_frequencies,
      .edge_noise_weights = settings.edge_noise_weights,
      .width_scale = settings.width_scale,
      .width_variation_scale = settings.width_variation_scale,
      .meander_frequency = settings.meander_frequency,
      .meander_length_scale = settings.meander_length_scale,
      .meander_amplitude = settings.meander_amplitude,
  };
  const auto section = Game::Map::sample_ribbon_cross_section(
      segment.start, segment.end, segment.width, t, shape);
  return {section.center, section.half_width};
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
    float centerline_height = center_pos.y();
    if (!settings.use_segment_elevation_profile && settings.height_map != nullptr &&
        settings.follow_terrain_centerline) {
      centerline_height = sample_water_surface_height_clamped(
          *settings.height_map, center_pos.x(), center_pos.z());
    }

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
      if (settings.use_segment_elevation_profile) {
        vertex_y = centerline_height;
      } else if (settings.height_map != nullptr) {
        if (settings.follow_terrain_centerline) {

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
  constexpr std::size_t k_segment_layers = 4;
  LinearFeatureRibbonSettings layered = settings;
  for (std::size_t index = 0; index < segments.size(); ++index) {
    layered.y_offset =
        settings.y_offset +
        settings.segment_layer_step * static_cast<float>(index % k_segment_layers);
    meshes.push_back(build_linear_ribbon_mesh(segments[index], tile_size, layered));
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
    auto match =
        std::find_if(junctions.begin(), junctions.end(), [&](const auto& item) {
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
    const int ring_segments =
        std::clamp(static_cast<int>(std::ceil(two_pi * junction.radius /
                                              std::max(tile_size * 0.35F, 0.15F))),
                   16,
                   48);
    std::vector<Render::GL::Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(static_cast<std::size_t>(ring_segments + 1));
    indices.reserve(static_cast<std::size_t>(ring_segments * 3));

    auto append_vertex = [&](const QVector3D& position, float radial_t, float angle_t) {
      QVector3D surface_position = position;
      if (!settings.use_segment_elevation_profile && settings.height_map != nullptr) {
        surface_position.setY(
            settings.follow_terrain_centerline
                ? sample_water_surface_height_clamped(
                      *settings.height_map, position.x(), position.z())
                : sample_height_clamped(
                      *settings.height_map, position.x(), position.z()));
      }
      Render::GL::Vertex vertex{};
      vertex.position = {surface_position.x(),
                         surface_position.y() + settings.y_offset -
                             settings.junction_sink,
                         surface_position.z()};
      vertex.normal = {0.0F, 1.0F, 0.0F};
      vertex.tex_coord = {
          settings.junction_uses_center_uv ? 0.5F : 0.5F * (1.0F - radial_t), angle_t};
      vertices.push_back(vertex);
    };
    append_vertex(junction.center, 0.0F, 0.5F);
    for (int index = 0; index < ring_segments; ++index) {
      const float angle_t =
          static_cast<float>(index) / static_cast<float>(ring_segments);
      const float angle = angle_t * two_pi;
      QVector3D position = junction.center;
      position.setX(position.x() + std::cos(angle) * junction.radius);
      position.setZ(position.z() + std::sin(angle) * junction.radius);
      append_vertex(position, 1.0F, angle_t);
    }
    for (int index = 0; index < ring_segments; ++index) {
      const auto current = 1U + static_cast<unsigned int>(index);
      const auto next = 1U + static_cast<unsigned int>((index + 1) % ring_segments);
      indices.insert(indices.end(), {0U, current, next});
    }
    result.push_back({std::make_unique<Render::GL::Mesh>(vertices, indices),
                      junction.center,
                      junction.samples});
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

  const float water_render_margin = std::max(tile_size * 0.70F, 0.10F);
  const int angular_segments =
      std::clamp(static_cast<int>(std::ceil(two_pi * std::max(half_width, half_depth) /
                                            std::max(tile_size * 0.65F, 0.25F))),
                 32,
                 160);
  const int radial_rings =
      std::clamp(static_cast<int>(std::ceil(std::min(half_width, half_depth) /
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

    vertex.tex_coord = {0.5F + local_x / (half_width * 2.2F),
                        std::clamp(0.5F * (1.0F - radial_t), 0.0F, 0.5F)};
    vertices.push_back(vertex);
  };

  append_vertex(0.0F, 0.0F, 0.0F);
  for (int ring = 1; ring <= radial_rings; ++ring) {
    const float radial_t = static_cast<float>(ring) / static_cast<float>(radial_rings);
    for (int segment = 0; segment < angular_segments; ++segment) {
      const float angle =
          two_pi * static_cast<float>(segment) / static_cast<float>(angular_segments);
      const float boundary = Game::Map::lake_boundary_scale(lake, angle);
      append_vertex(
          std::cos(angle) * (half_width * boundary + water_render_margin) * radial_t,
          std::sin(angle) * (half_depth * boundary + water_render_margin) * radial_t,
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
      const auto next = static_cast<unsigned int>((segment + 1) % angular_segments);
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
                       float tile_size,
                       const Game::Map::TerrainHeightMap& height_map)
    -> std::unique_ptr<Render::GL::Mesh> {
  QVector3D dir = bridge.end - bridge.start;
  dir.setY(0.0F);
  float const length = dir.length();
  if (length < 0.01F) {
    return nullptr;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const bridge_width = std::max(bridge.width, Game::Map::k_min_bridge_width);
  float const half_width = bridge_width * 0.5F;

  float const landing_run = Game::Map::bridge_visual_landing_run(bridge_width);
  const float visual_length = length + landing_run * 2.0F;
  QVector3D const visual_start = bridge.start - dir * landing_run;
  float const segment_step = std::max(tile_size * 0.28F, 0.16F);

  float const end_fade_length =
      std::min(std::clamp(bridge_width * 0.70F, tile_size * 1.60F, tile_size * 4.00F),
               visual_length * 0.45F);

  int length_segments = static_cast<int>(std::ceil(visual_length / segment_step));
  length_segments =
      std::max(length_segments, Render::GL::Geometry::min_length_segments + 2);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int k_vertices_per_bridge_segment = 20;
  float const deck_thickness = std::clamp(bridge_width * 0.26F, 0.55F, 1.05F);

  float const parapet_height = std::clamp(bridge_width * 0.13F, 0.55F, 0.92F);
  float const parapet_half_width =
      Game::Map::bridge_parapet_half_thickness(bridge_width);
  float const side_bevel = std::clamp(bridge_width * 0.12F, 0.10F, 0.30F);

  float const lake_padding = Game::Map::drawn_lake_padding(tile_size);
  auto const landing_floor =
      [&](float along_from, float along_to, float abutment_y) -> std::optional<float> {
    if (!Game::Map::drawn_water_level_under_bridge(bridge,
                                                   height_map.get_river_segments(),
                                                   height_map.get_lakes(),
                                                   lake_padding,
                                                   along_from,
                                                   along_to)
             .has_value()) {
      return std::nullopt;
    }
    return abutment_y;
  };
  std::optional<float> const start_landing_floor =
      landing_floor(-landing_run, 0.0F, bridge.start.y());
  std::optional<float> const end_landing_floor =
      landing_floor(length, length + landing_run, bridge.end.y());

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

  auto vertex_position = [&](unsigned int index) {
    const auto& p = vertices[index].position;
    return QVector3D(p[0], p[1], p[2]);
  };

  auto push_quad = [&](unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
    const auto& n = vertices[a].normal;
    if (QVector3D::dotProduct(
            QVector3D::crossProduct(vertex_position(b) - vertex_position(a),
                                    vertex_position(c) - vertex_position(a)),
            QVector3D(n[0], n[1], n[2])) < 0.0F) {
      std::swap(b, d);
    }
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
    const float authored_distance = span_distance - landing_run;
    const float authored_t = std::clamp(authored_distance / length, 0.0F, 1.0F);
    QVector3D const center_pos = visual_start + dir * span_distance;

    float const start_blend =
        smoothstep01(span_distance / std::max(end_fade_length, 0.001F));
    float const end_blend = smoothstep01((visual_length - span_distance) /
                                         std::max(end_fade_length, 0.001F));
    float const profile_blend = start_blend * end_blend;

    float const arch_curve = Game::Map::bridge_arch_curve(authored_t);

    float landing = 1.0F;
    if (span_distance < landing_run) {
      landing = smoothstep01(span_distance / landing_run);
    } else if (span_distance > visual_length - landing_run) {
      landing = smoothstep01((visual_length - span_distance) / landing_run);
    }
    float ground_y = height_map.get_base_height_at(center_pos.x(), center_pos.z());
    const std::optional<float>& landing_floor_y =
        span_distance < visual_length * 0.5F ? start_landing_floor : end_landing_floor;
    if (landing_floor_y.has_value()) {
      ground_y = std::max(ground_y, *landing_floor_y);
    }
    float const deck_height = mixf(ground_y + Game::Map::k_bridge_deck_visual_lift,
                                   Game::Map::bridge_deck_world_y(bridge, authored_t),
                                   landing);

    float const stone_noise =
        std::sin(center_pos.x() * 3.0F) * std::cos(center_pos.z() * 2.5F) * 0.02F;

    float const ring_half_width = half_width * (1.0F + 0.22F * (1.0F - profile_blend));
    float const bottom_half_width =
        std::max(ring_half_width - side_bevel * (0.55F + 0.45F * profile_blend),
                 ring_half_width * 0.68F);

    float const ring_thickness =
        mixf(Game::Map::k_bridge_landing_thickness,
             mixf(deck_thickness * 1.35F, deck_thickness * 0.55F, arch_curve),
             landing);

    float const ring_parapet_height =
        parapet_height * (0.46F + 0.54F * profile_blend) * landing;
    float const ring_parapet_offset =
        Game::Map::bridge_parapet_centre_offset(ring_half_width, bridge_width);

    float const deck_y = deck_height + stone_noise * (0.55F + 0.45F * profile_blend);
    float const underside_y = deck_y - ring_thickness;
    float const rail_top_y = deck_y + ring_parapet_height;

    QVector3D top_left = center_pos + perpendicular * (-ring_half_width);
    top_left.setY(deck_y);
    QVector3D top_right = center_pos + perpendicular * ring_half_width;
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

    auto rail_point = [&](float offset, float height) {
      QVector3D point = center_pos + perpendicular * offset;
      point.setY(height);
      return point;
    };
    const QVector3D left_outer_bottom =
        rail_point(-ring_parapet_offset - parapet_half_width, deck_y);
    const QVector3D left_outer_top =
        rail_point(-ring_parapet_offset - parapet_half_width, rail_top_y);
    const QVector3D left_inner_bottom =
        rail_point(-ring_parapet_offset + parapet_half_width, deck_y);
    const QVector3D left_inner_top =
        rail_point(-ring_parapet_offset + parapet_half_width, rail_top_y);
    const QVector3D right_inner_bottom =
        rail_point(ring_parapet_offset - parapet_half_width, deck_y);
    const QVector3D right_inner_top =
        rail_point(ring_parapet_offset - parapet_half_width, rail_top_y);
    const QVector3D right_outer_bottom =
        rail_point(ring_parapet_offset + parapet_half_width, deck_y);
    const QVector3D right_outer_top =
        rail_point(ring_parapet_offset + parapet_half_width, rail_top_y);

    const float profile_step = 0.02F;
    const float t0 = std::max(0.0F, authored_t - profile_step / length);
    const float t1 = std::min(1.0F, authored_t + profile_step / length);
    const float grade = (Game::Map::bridge_deck_world_y(bridge, t1) -
                         Game::Map::bridge_deck_world_y(bridge, t0)) /
                        std::max((t1 - t0) * length, 0.001F);
    const QVector3D deck_normal =
        (QVector3D(0.0F, 1.0F, 0.0F) - dir * grade).normalized();

    add_vertex(top_left, deck_normal, 0.0F, span_distance);
    add_vertex(top_right, deck_normal, ring_half_width * 2.0F, span_distance);
    add_vertex(bottom_left, -deck_normal, 0.0F, span_distance);
    add_vertex(bottom_right, -deck_normal, bottom_half_width * 2.0F, span_distance);
    add_vertex(side_left_top, left_normal, span_distance, deck_y);
    add_vertex(side_left_bottom, left_normal, span_distance, underside_y);
    add_vertex(side_right_top, right_normal, span_distance, deck_y);
    add_vertex(side_right_bottom, right_normal, span_distance, underside_y);
    add_vertex(left_outer_bottom, -perpendicular, span_distance, deck_y);
    add_vertex(left_outer_top, -perpendicular, span_distance, rail_top_y);
    add_vertex(left_inner_bottom, perpendicular, span_distance, deck_y);
    add_vertex(left_inner_top, perpendicular, span_distance, rail_top_y);
    add_vertex(left_outer_top, deck_normal, 0.0F, span_distance);
    add_vertex(left_inner_top, deck_normal, parapet_half_width * 2.0F, span_distance);
    add_vertex(right_inner_bottom, -perpendicular, span_distance, deck_y);
    add_vertex(right_inner_top, -perpendicular, span_distance, rail_top_y);
    add_vertex(right_outer_bottom, perpendicular, span_distance, deck_y);
    add_vertex(right_outer_top, perpendicular, span_distance, rail_top_y);
    add_vertex(right_inner_top, deck_normal, 0.0F, span_distance);
    add_vertex(right_outer_top, deck_normal, parapet_half_width * 2.0F, span_distance);
  }

  for (int i = 0; i < length_segments; ++i) {
    auto const base_idx = static_cast<unsigned int>(i * k_vertices_per_bridge_segment);
    unsigned int const next_idx = base_idx + k_vertices_per_bridge_segment;

    push_quad(base_idx + 0, base_idx + 1, next_idx + 1, next_idx + 0);
    push_quad(next_idx + 3, next_idx + 2, base_idx + 2, base_idx + 3);
    push_quad(base_idx + 4, base_idx + 5, next_idx + 5, next_idx + 4);
    push_quad(base_idx + 6, base_idx + 7, next_idx + 7, next_idx + 6);

    push_quad(base_idx + 8, base_idx + 9, next_idx + 9, next_idx + 8);
    push_quad(base_idx + 11, base_idx + 10, next_idx + 10, next_idx + 11);
    push_quad(base_idx + 12, base_idx + 13, next_idx + 13, next_idx + 12);
    push_quad(base_idx + 14, base_idx + 15, next_idx + 15, next_idx + 14);
    push_quad(base_idx + 17, base_idx + 16, next_idx + 16, next_idx + 17);
    push_quad(base_idx + 18, base_idx + 19, next_idx + 19, next_idx + 18);
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
        const QVector3D position(vtx.position[0], vtx.position[1], vtx.position[2]);
        vtx.tex_coord[0] =
            QVector3D::dotProduct(position - visual_start, perpendicular) +
            bridge_width;
        vtx.tex_coord[1] = position.y();
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
    add_cap(
        start_idx + 8, start_idx + 10, start_idx + 11, start_idx + 9, -forward_normal);
    add_cap(end_idx + 8, end_idx + 9, end_idx + 11, end_idx + 10, forward_normal);
    add_cap(start_idx + 14,
            start_idx + 16,
            start_idx + 17,
            start_idx + 15,
            -forward_normal);
    add_cap(end_idx + 14, end_idx + 15, end_idx + 17, end_idx + 16, forward_normal);
  }

  auto strip_point = [&](float distance, unsigned int column) {
    const float row = std::clamp(distance / visual_length, 0.0F, 1.0F) *
                      static_cast<float>(length_segments);
    const auto lo = static_cast<unsigned int>(std::floor(row));
    const auto hi = std::min(lo + 1U, static_cast<unsigned int>(length_segments));
    return vertex_position(lo * k_vertices_per_bridge_segment + column) *
               (1.0F - (row - static_cast<float>(lo))) +
           vertex_position(hi * k_vertices_per_bridge_segment + column) *
               (row - static_cast<float>(lo));
  };
  auto lateral_offset = [&](const QVector3D& point) {
    return QVector3D::dotProduct(point - visual_start, perpendicular);
  };

  auto add_dressed_block = [&](float start,
                               float end,
                               float offset_start,
                               float offset_end,
                               float block_half_width,
                               float bottom_start,
                               float bottom_end,
                               float top_start,
                               float top_end,
                               float bevel) {
    const float chamfer = std::min({bevel,
                                    (end - start) * 0.20F,
                                    block_half_width * 0.35F,
                                    (top_start - bottom_start) * 0.24F,
                                    (top_end - bottom_end) * 0.24F});
    std::array<std::array<QVector3D, 4>, 4> rings;
    for (int ring = 0; ring < 4; ++ring) {
      const float inset = (ring == 0 || ring == 3) ? chamfer : 0.0F;
      for (int corner = 0; corner < 4; ++corner) {
        const bool far_end = corner == 1 || corner == 2;
        const float distance = far_end ? end - inset : start + inset;
        const float t = (distance - start) / (end - start);
        const float side = corner < 2 ? -1.0F : 1.0F;
        QVector3D point = visual_start + dir * distance +
                          perpendicular * (mixf(offset_start, offset_end, t) +
                                           side * (block_half_width - inset));
        const float bottom = mixf(bottom_start, bottom_end, t);
        const float top = mixf(top_start, top_end, t);
        point.setY(ring == 0   ? bottom
                   : ring == 1 ? bottom + chamfer
                   : ring == 2 ? top - chamfer
                               : top);
        rings[ring][corner] = point;
      }
    }
    auto face = [&](const QVector3D& a,
                    const QVector3D& b,
                    const QVector3D& c,
                    const QVector3D& d) {
      const QVector3D normal = QVector3D::crossProduct(b - a, c - a).normalized();
      const auto first = static_cast<unsigned int>(vertices.size());
      const float width = (b - a).length();
      const float height = (d - a).length();
      add_vertex(a, normal, -1.0F, 0.0F);
      add_vertex(b, normal, -1.0F - width, 0.0F);
      add_vertex(c, normal, -1.0F - width, height);
      add_vertex(d, normal, -1.0F, height);
      push_quad(first, first + 1, first + 2, first + 3);
    };
    for (int ring = 0; ring < 3; ++ring) {
      for (int corner = 0; corner < 4; ++corner) {
        const int next = (corner + 1) % 4;
        face(rings[ring][corner],
             rings[ring + 1][corner],
             rings[ring + 1][next],
             rings[ring][next]);
      }
    }
    face(rings[0][0], rings[0][1], rings[0][2], rings[0][3]);
    face(rings[3][3], rings[3][2], rings[3][1], rings[3][0]);
  };

  const float detail_start = landing_run * 0.65F;
  const float detail_length = visual_length - detail_start * 2.0F;

  const int stone_count =
      std::clamp(static_cast<int>(std::ceil(detail_length / 0.95F)) | 1, 3, 191);
  const float stone_length = detail_length / static_cast<float>(stone_count);
  for (int stone = 0; stone < stone_count; ++stone) {
    const float start =
        detail_start + stone_length * static_cast<float>(stone) + 0.014F;
    const float end = start + stone_length - 0.028F;
    for (int side = 0; side < 2; ++side) {
      const unsigned int outer = side == 0 ? 12U : 18U;
      const QVector3D rail_start =
          (strip_point(start, outer) + strip_point(start, outer + 1)) * 0.5F;
      const QVector3D rail_end =
          (strip_point(end, outer) + strip_point(end, outer + 1)) * 0.5F;
      const float variation = 0.012F * std::sin(static_cast<float>(stone) * 2.37F);
      add_dressed_block(start,
                        end,
                        lateral_offset(rail_start),
                        lateral_offset(rail_end),
                        parapet_half_width + 0.095F,
                        rail_start.y() - 0.035F,
                        rail_end.y() - 0.035F,
                        rail_start.y() + 0.17F + variation,
                        rail_end.y() + 0.17F + variation,
                        0.045F);

      const unsigned int edge = side == 0 ? 0U : 1U;
      const QVector3D deck_start = strip_point(start, edge);
      const QVector3D deck_end = strip_point(end, edge);
      const float soffit_start = strip_point(start, edge + 2).y();
      const float soffit_end = strip_point(end, edge + 2).y();
      const float ring_height = stone == stone_count / 2 ? 0.44F : 0.34F;
      add_dressed_block(start,
                        end,
                        lateral_offset(deck_start),
                        lateral_offset(deck_end),
                        side_bevel + 0.035F,
                        soffit_start - 0.055F,
                        soffit_end - 0.055F,
                        std::min(soffit_start + ring_height, deck_start.y() - 0.03F),
                        std::min(soffit_end + ring_height, deck_end.y() - 0.03F),
                        0.028F);
    }
  }

  const float post_inset = std::min(length * 0.15F, 1.25F);
  const float post_half_length = std::min(length * 0.08F, 0.46F);
  for (const float distance :
       {landing_run + post_inset, landing_run + length - post_inset}) {
    for (int side = 0; side < 2; ++side) {
      const unsigned int outer = side == 0 ? 12U : 18U;
      const QVector3D rail =
          (strip_point(distance, outer) + strip_point(distance, outer + 1)) * 0.5F;
      const float offset = lateral_offset(rail);
      const float footing =
          std::min(sample_height_clamped(height_map, rail.x(), rail.z()) - 0.12F,
                   strip_point(distance, side == 0 ? 2U : 3U).y() - 0.18F);
      const float top = rail.y() + 0.12F;
      add_dressed_block(distance - post_half_length,
                        distance + post_half_length,
                        offset,
                        offset,
                        parapet_half_width + 0.10F,
                        footing,
                        footing,
                        top,
                        top,
                        0.055F);
      add_dressed_block(distance - post_half_length - 0.06F,
                        distance + post_half_length + 0.06F,
                        offset,
                        offset,
                        parapet_half_width + 0.18F,
                        top,
                        top,
                        top + 0.18F,
                        top + 0.18F,
                        0.045F);
    }
  }

  if (vertices.empty() || indices.empty()) {
    return nullptr;
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_lake_shore_mesh(const Game::Map::Lake& lake,
                           const Game::Map::TerrainHeightMap& height_map)
    -> RiverbankMeshBuildResult {
  RiverbankMeshBuildResult result;
  constexpr float two_pi = 6.28318530717958647692F;
  constexpr float deg_to_rad = 0.01745329251994329577F;
  constexpr int ring_count = 5;
  const float tile_size = height_map.get_tile_size();
  const float half_width = std::max(lake.width * 0.5F, tile_size * 0.5F);
  const float half_depth = std::max(lake.depth * 0.5F, tile_size * 0.5F);
  const float water_render_margin = std::max(tile_size * 0.70F, 0.10F);
  const float shore_width = std::clamp(tile_size * 1.85F, 1.25F, 2.75F);
  const int angular_segments =
      std::clamp(static_cast<int>(std::ceil(two_pi * std::max(half_width, half_depth) /
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
      const float angle =
          two_pi * static_cast<float>(segment) / static_cast<float>(angular_segments);
      const float boundary = Game::Map::lake_boundary_scale(lake, angle);

      const float edge_local_x =
          std::cos(angle) * (half_width * boundary + water_render_margin);
      const float edge_local_z =
          std::sin(angle) * (half_depth * boundary + water_render_margin);
      const float edge_world_x =
          lake.center.x() + edge_local_x * cos_rotation - edge_local_z * sin_rotation;
      const float edge_world_z =
          lake.center.z() + edge_local_x * sin_rotation + edge_local_z * cos_rotation;
      const float local_shore_width =
          shore_width *
          (0.55F + 0.95F * value_noise(edge_world_x * 0.18F, edge_world_z * 0.18F));

      const float local_x =
          std::cos(angle) *
          (half_width * boundary + water_render_margin + local_shore_width * shore_t);
      const float local_z =
          std::sin(angle) *
          (half_depth * boundary + water_render_margin + local_shore_width * shore_t);
      const float rotated_x = local_x * cos_rotation - local_z * sin_rotation;
      const float rotated_z = local_x * sin_rotation + local_z * cos_rotation;
      const float world_x = lake.center.x() + rotated_x;
      const float world_z = lake.center.z() + rotated_z;
      const float terrain_y = sample_height_clamped(height_map, world_x, world_z);

      const float water_edge_y = lake.center.y() + 0.018F;

      Render::GL::Vertex vertex{};
      const float blended_height = mixf(water_edge_y, terrain_y + 0.004F, blend);
      const float overlay_lift = ring == ring_count ? 0.0F : 0.007F * (1.0F - shore_t);
      const float shore_height =
          ring == 0 ? water_edge_y : std::max(blended_height, terrain_y + overlay_lift);
      vertex.position = {world_x, shore_height, world_z};
      QVector3D outward(rotated_x, 0.0F, rotated_z);
      outward.normalize();
      const float radial_slope = std::clamp((terrain_y + 0.004F - water_edge_y) /
                                                std::max(local_shore_width, 0.01F),
                                            -0.65F,
                                            0.65F);
      const QVector3D bank_normal =
          QVector3D(-outward.x() * radial_slope, 1.0F, -outward.z() * radial_slope)
              .normalized();
      vertex.normal = {bank_normal.x(), bank_normal.y(), bank_normal.z()};
      vertex.tex_coord = {
          shore_t, static_cast<float>(segment) / static_cast<float>(angular_segments)};
      vertices.push_back(vertex);
      if (ring == ring_count && segment % 3 == 0) {
        result.visibility_samples.emplace_back(world_x, terrain_y, world_z);
      }
    }
  }

  for (int ring = 0; ring < ring_count; ++ring) {
    const auto inner = static_cast<unsigned int>(ring * angular_segments);
    const auto outer = static_cast<unsigned int>((ring + 1) * angular_segments);
    for (int segment = 0; segment < angular_segments; ++segment) {
      const auto next = static_cast<unsigned int>((segment + 1) % angular_segments);
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
