#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace Game::Map {

enum class HillShape : std::uint8_t {
  Blob,
  Corridor,
  Arc,
  Elbow,
  Ring,
  Path,
  Mask,
};

inline constexpr float k_hill_shape_min_half_thickness = 1.25F;
inline constexpr float k_hill_shape_default_thickness_ratio = 0.34F;
inline constexpr float k_hill_shape_min_taper_scale = 0.18F;
inline constexpr float k_hill_shape_taper_span = 0.55F;
inline constexpr float k_hill_shape_closed_sweep_deg = 359.0F;
inline constexpr float k_hill_shape_arc_segment_deg = 9.0F;
inline constexpr float k_hill_shape_default_sweep_deg = 120.0F;
inline constexpr float k_hill_shape_default_elbow_sweep_deg = 90.0F;

[[nodiscard]] inline auto hill_shape_name(HillShape shape) -> std::string_view {
  switch (shape) {
  case HillShape::Corridor:
    return "corridor";
  case HillShape::Arc:
    return "arc";
  case HillShape::Elbow:
    return "elbow";
  case HillShape::Ring:
    return "ring";
  case HillShape::Path:
    return "path";
  case HillShape::Mask:
    return "mask";
  case HillShape::Blob:
    break;
  }
  return "blob";
}

[[nodiscard]] inline auto parse_hill_shape(std::string_view name,
                                           HillShape& out) -> bool {
  std::string lowered;
  lowered.reserve(name.size());
  for (const char character : name) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  if (lowered.empty() || lowered == "blob" || lowered == "ellipse" ||
      lowered == "round" || lowered == "mound") {
    out = HillShape::Blob;
    return true;
  }
  if (lowered == "corridor" || lowered == "ridge" || lowered == "wall" ||
      lowered == "capsule") {
    out = HillShape::Corridor;
    return true;
  }
  if (lowered == "arc" || lowered == "boomerang" || lowered == "crescent" ||
      lowered == "banana" || lowered == "horseshoe") {
    out = HillShape::Arc;
    return true;
  }
  if (lowered == "elbow" || lowered == "corner" || lowered == "l" ||
      lowered == "chevron") {
    out = HillShape::Elbow;
    return true;
  }
  if (lowered == "ring" || lowered == "crater" || lowered == "atoll") {
    out = HillShape::Ring;
    return true;
  }
  if (lowered == "path" || lowered == "polyline" || lowered == "spline" ||
      lowered == "custom") {
    out = HillShape::Path;
    return true;
  }
  if (lowered == "mask" || lowered == "cells" || lowered == "painted" ||
      lowered == "freeform") {
    out = HillShape::Mask;
    return true;
  }
  return false;
}

[[nodiscard]] inline auto hill_shape_is_spine(HillShape shape) -> bool {
  return shape != HillShape::Blob && shape != HillShape::Mask;
}

[[nodiscard]] inline auto hill_shape_is_mask(HillShape shape) -> bool {
  return shape == HillShape::Mask;
}

struct HillShapePoint {
  float x = 0.0F;
  float z = 0.0F;
};

struct HillShapeCell {
  int x = 0;
  int z = 0;
};

struct HillShapeParams {
  HillShape shape = HillShape::Blob;
  float half_width_cells = 8.0F;
  float half_depth_cells = 8.0F;
  float half_thickness_cells = 0.0F;
  float sweep_degrees = 0.0F;
  float sweep_start_degrees = 0.0F;
  float taper = 0.0F;
  bool has_sweep = false;
  bool has_sweep_start = false;
  std::vector<HillShapePoint> points;
  std::vector<HillShapeCell> mask_cells;
  float mask_center_x = 0.0F;
  float mask_center_z = 0.0F;
  float mask_inset_cells = 0.0F;
};

struct HillShapeGeometry {
  HillShape shape = HillShape::Blob;
  float half_width = 8.0F;
  float half_depth = 8.0F;
  float half_thickness = 4.0F;
  float taper = 0.0F;
  bool closed = false;
  std::vector<HillShapePoint> spine;
  std::vector<float> arclength;
  float total_length = 0.0F;
  float bound_half_x = 8.0F;
  float bound_half_z = 8.0F;

  int mask_width = 0;
  int mask_height = 0;
  float mask_origin_x = 0.0F;
  float mask_origin_z = 0.0F;
  float mask_inset = 2.0F;
  std::vector<float> mask_distance;

  [[nodiscard]] auto is_spine() const -> bool {
    return hill_shape_is_spine(shape) && spine.size() >= 2;
  }

  [[nodiscard]] auto is_mask() const -> bool {
    return hill_shape_is_mask(shape) && mask_width > 0 && mask_height > 0;
  }

  [[nodiscard]] auto is_shaped() const -> bool { return is_spine() || is_mask(); }
};

struct HillSpineSample {
  float distance = 0.0F;
  float nearest_x = 0.0F;
  float nearest_z = 0.0F;
  float along = 0.0F;
  float side = 1.0F;
  float half_thickness = 1.0F;
};

[[nodiscard]] inline auto hill_shape_default_half_thickness(float half_width,
                                                            float half_depth) -> float {
  return std::max(k_hill_shape_min_half_thickness,
                  std::min(half_width, half_depth) *
                      k_hill_shape_default_thickness_ratio);
}

namespace detail {

inline auto hill_shape_smoothstep(float value) -> float {
  value = std::clamp(value, 0.0F, 1.0F);
  return value * value * (3.0F - 2.0F * value);
}

inline void hill_shape_append_arc(std::vector<HillShapePoint>& out,
                                  float radius_x,
                                  float radius_z,
                                  float start_deg,
                                  float sweep_deg) {
  constexpr float k_deg_to_rad = std::numbers::pi_v<float> / 180.0F;
  const int segments = std::max(
      6,
      static_cast<int>(std::ceil(std::abs(sweep_deg) / k_hill_shape_arc_segment_deg)));
  for (int step = 0; step <= segments; ++step) {
    const float t = static_cast<float>(step) / static_cast<float>(segments);
    const float angle = (start_deg + sweep_deg * t) * k_deg_to_rad;
    out.push_back({radius_x * std::cos(angle), radius_z * std::sin(angle)});
  }
}

inline void hill_shape_finalize(HillShapeGeometry& geometry) {
  geometry.arclength.assign(geometry.spine.size(), 0.0F);
  geometry.total_length = 0.0F;
  for (std::size_t i = 1; i < geometry.spine.size(); ++i) {
    geometry.total_length += std::hypot(geometry.spine[i].x - geometry.spine[i - 1].x,
                                        geometry.spine[i].z - geometry.spine[i - 1].z);
    geometry.arclength[i] = geometry.total_length;
  }

  float max_x = 0.0F;
  float max_z = 0.0F;
  for (const HillShapePoint& point : geometry.spine) {
    max_x = std::max(max_x, std::abs(point.x));
    max_z = std::max(max_z, std::abs(point.z));
  }
  geometry.bound_half_x = max_x + geometry.half_thickness;
  geometry.bound_half_z = max_z + geometry.half_thickness;
}

inline void hill_shape_build_mask(HillShapeGeometry& geometry,
                                  const HillShapeParams& params) {
  if (params.mask_cells.empty()) {
    return;
  }

  int min_x = params.mask_cells.front().x;
  int max_x = min_x;
  int min_z = params.mask_cells.front().z;
  int max_z = min_z;
  for (const HillShapeCell& cell : params.mask_cells) {
    min_x = std::min(min_x, cell.x);
    max_x = std::max(max_x, cell.x);
    min_z = std::min(min_z, cell.z);
    max_z = std::max(max_z, cell.z);
  }

  const float span_x = static_cast<float>(max_x - min_x + 1) * 0.5F;
  const float span_z = static_cast<float>(max_z - min_z + 1) * 0.5F;
  const float reference = std::max(std::min(span_x, span_z), 1.0F);
  const float requested_inset = std::max(
      params.mask_inset_cells > 0.0F ? params.mask_inset_cells : reference * 0.34F,
      1.0F);
  geometry.mask_inset = requested_inset;

  const int margin = static_cast<int>(std::ceil(requested_inset)) + 2;
  geometry.mask_width = (max_x - min_x + 1) + margin * 2;
  geometry.mask_height = (max_z - min_z + 1) + margin * 2;
  geometry.mask_origin_x = static_cast<float>(min_x - margin) - params.mask_center_x;
  geometry.mask_origin_z = static_cast<float>(min_z - margin) - params.mask_center_z;

  const int cell_count = geometry.mask_width * geometry.mask_height;
  std::vector<std::uint8_t> inside(static_cast<std::size_t>(cell_count), 0U);
  for (const HillShapeCell& cell : params.mask_cells) {
    const int local_x = cell.x - min_x + margin;
    const int local_z = cell.z - min_z + margin;
    inside[static_cast<std::size_t>(local_z * geometry.mask_width + local_x)] = 1U;
  }

  const int reach = margin;
  const float unreached = static_cast<float>(reach) + 1.0F;
  geometry.mask_distance.assign(static_cast<std::size_t>(cell_count), unreached);
  for (int z = 0; z < geometry.mask_height; ++z) {
    for (int x = 0; x < geometry.mask_width; ++x) {
      const std::size_t index = static_cast<std::size_t>(z * geometry.mask_width + x);
      const bool is_inside = inside[index] != 0U;
      float best = unreached;
      for (int dz = -reach; dz <= reach; ++dz) {
        const int nz = z + dz;
        if (nz < 0 || nz >= geometry.mask_height) {
          continue;
        }
        for (int dx = -reach; dx <= reach; ++dx) {
          const int nx = x + dx;
          if (nx < 0 || nx >= geometry.mask_width) {
            continue;
          }
          const bool neighbour_inside =
              inside[static_cast<std::size_t>(nz * geometry.mask_width + nx)] != 0U;
          if (neighbour_inside == is_inside) {
            continue;
          }
          best = std::min(best, std::sqrt(static_cast<float>(dx * dx + dz * dz)));
        }
      }
      geometry.mask_distance[index] = is_inside ? -best + 0.5F : best - 0.5F;
    }
  }

  float deepest = 0.0F;
  for (const float distance : geometry.mask_distance) {
    deepest = std::max(deepest, -distance);
  }
  geometry.mask_inset =
      std::clamp(requested_inset, 1.0F, std::max(deepest * 0.62F, 1.0F));

  geometry.half_thickness = geometry.mask_inset;
  geometry.bound_half_x =
      std::max(std::abs(static_cast<float>(min_x) - params.mask_center_x),
               std::abs(static_cast<float>(max_x) - params.mask_center_x)) +
      1.0F;
  geometry.bound_half_z =
      std::max(std::abs(static_cast<float>(min_z) - params.mask_center_z),
               std::abs(static_cast<float>(max_z) - params.mask_center_z)) +
      1.0F;
}

} // namespace detail

[[nodiscard]] inline auto
build_hill_shape(const HillShapeParams& params) -> HillShapeGeometry {
  HillShapeGeometry geometry;
  geometry.shape = params.shape;
  geometry.half_width = std::max(params.half_width_cells, 0.5F);
  geometry.half_depth = std::max(params.half_depth_cells, 0.5F);
  geometry.taper = std::clamp(params.taper, 0.0F, 1.0F);
  geometry.bound_half_x = geometry.half_width;
  geometry.bound_half_z = geometry.half_depth;

  if (params.shape == HillShape::Blob) {
    geometry.half_thickness = std::min(geometry.half_width, geometry.half_depth);
    return geometry;
  }

  if (params.shape == HillShape::Mask) {
    detail::hill_shape_build_mask(geometry, params);
    if (!geometry.is_mask()) {
      geometry.shape = HillShape::Blob;
      geometry.half_thickness = std::min(geometry.half_width, geometry.half_depth);
    }
    return geometry;
  }

  const float authored_thickness = params.half_thickness_cells;
  const float default_thickness =
      params.shape == HillShape::Corridor
          ? std::min(geometry.half_width, geometry.half_depth)
          : hill_shape_default_half_thickness(geometry.half_width, geometry.half_depth);
  float thickness = authored_thickness > 0.0F ? authored_thickness : default_thickness;

  if (params.shape != HillShape::Path) {
    thickness = std::min(thickness, std::min(geometry.half_width, geometry.half_depth));
  }
  geometry.half_thickness = std::max(thickness, k_hill_shape_min_half_thickness);

  const float inset_x = std::max(geometry.half_width - geometry.half_thickness, 0.0F);
  const float inset_z = std::max(geometry.half_depth - geometry.half_thickness, 0.0F);

  switch (params.shape) {
  case HillShape::Corridor: {
    if (geometry.half_width >= geometry.half_depth) {
      geometry.spine.push_back({-inset_x, 0.0F});
      geometry.spine.push_back({inset_x, 0.0F});
    } else {
      geometry.spine.push_back({0.0F, -inset_z});
      geometry.spine.push_back({0.0F, inset_z});
    }
    break;
  }
  case HillShape::Arc:
  case HillShape::Ring: {
    const float sweep = params.has_sweep ? params.sweep_degrees
                                         : (params.shape == HillShape::Ring
                                                ? 360.0F
                                                : k_hill_shape_default_sweep_deg);
    const float clamped_sweep = std::clamp(sweep, 5.0F, 360.0F);
    geometry.closed = clamped_sweep >= k_hill_shape_closed_sweep_deg;
    const float start =
        params.has_sweep_start ? params.sweep_start_degrees : -clamped_sweep * 0.5F;
    detail::hill_shape_append_arc(
        geometry.spine, inset_x, inset_z, start, clamped_sweep);
    if (geometry.closed && geometry.spine.size() >= 2) {
      geometry.spine.back() = geometry.spine.front();
    }
    break;
  }
  case HillShape::Elbow: {
    constexpr float k_deg_to_rad = std::numbers::pi_v<float> / 180.0F;
    const float sweep = std::clamp(
        params.has_sweep ? params.sweep_degrees : k_hill_shape_default_elbow_sweep_deg,
        15.0F,
        345.0F);
    const HillShapePoint corner{-inset_x, -inset_z};
    const float angle = sweep * k_deg_to_rad;
    geometry.spine.push_back({corner.x + inset_x * 2.0F, corner.z});
    geometry.spine.push_back(corner);
    geometry.spine.push_back({corner.x + std::cos(angle) * inset_z * 2.0F,
                              corner.z + std::sin(angle) * inset_z * 2.0F});
    break;
  }
  case HillShape::Path: {
    geometry.spine = params.points;
    if (geometry.spine.size() >= 3) {
      const HillShapePoint& first = geometry.spine.front();
      const HillShapePoint& last = geometry.spine.back();
      geometry.closed = std::hypot(first.x - last.x, first.z - last.z) < 0.5F;
    }
    break;
  }
  case HillShape::Blob:
  case HillShape::Mask:
    break;
  }

  if (geometry.spine.size() < 2) {
    geometry.shape = HillShape::Blob;
    geometry.spine.clear();
    geometry.closed = false;
    geometry.half_thickness = std::min(geometry.half_width, geometry.half_depth);
    return geometry;
  }

  detail::hill_shape_finalize(geometry);
  return geometry;
}

[[nodiscard]] inline auto hill_shape_taper_scale(const HillShapeGeometry& geometry,
                                                 float along) -> float {
  if (geometry.closed || geometry.taper <= 0.0F) {
    return 1.0F;
  }
  const float edge = std::min(along, 1.0F - along) * 2.0F;
  const float eased = detail::hill_shape_smoothstep(edge / k_hill_shape_taper_span);
  return std::max(1.0F - geometry.taper * (1.0F - eased), k_hill_shape_min_taper_scale);
}

[[nodiscard]] inline auto
sample_hill_spine(float local_x,
                  float local_z,
                  const HillShapeGeometry& geometry) -> HillSpineSample {
  HillSpineSample sample;
  sample.half_thickness = std::max(geometry.half_thickness, 0.001F);
  if (!geometry.is_spine()) {
    sample.distance = std::hypot(local_x, local_z);
    return sample;
  }

  float best_distance_sq = std::numeric_limits<float>::max();
  float best_x = geometry.spine.front().x;
  float best_z = geometry.spine.front().z;
  float best_length = 0.0F;
  float best_side = 1.0F;

  for (std::size_t i = 1; i < geometry.spine.size(); ++i) {
    const HillShapePoint& a = geometry.spine[i - 1];
    const HillShapePoint& b = geometry.spine[i];
    const float seg_x = b.x - a.x;
    const float seg_z = b.z - a.z;
    const float seg_len_sq = seg_x * seg_x + seg_z * seg_z;
    float t = 0.0F;
    if (seg_len_sq > 1e-8F) {
      t = std::clamp(
          ((local_x - a.x) * seg_x + (local_z - a.z) * seg_z) / seg_len_sq, 0.0F, 1.0F);
    }
    const float px = a.x + seg_x * t;
    const float pz = a.z + seg_z * t;
    const float dx = local_x - px;
    const float dz = local_z - pz;
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_x = px;
      best_z = pz;
      best_length = geometry.arclength[i - 1] + std::sqrt(seg_len_sq) * t;
      best_side = (seg_x * dz - seg_z * dx) >= 0.0F ? 1.0F : -1.0F;
    }
  }

  sample.distance = std::sqrt(best_distance_sq);
  sample.nearest_x = best_x;
  sample.nearest_z = best_z;
  sample.along = geometry.total_length > 0.0001F
                     ? std::clamp(best_length / geometry.total_length, 0.0F, 1.0F)
                     : 0.0F;
  sample.side = best_side;
  sample.half_thickness = std::max(
      geometry.half_thickness * hill_shape_taper_scale(geometry, sample.along), 0.001F);
  return sample;
}

struct HillSpinePose {
  HillShapePoint position;
  HillShapePoint tangent{1.0F, 0.0F};
};

[[nodiscard]] inline auto hill_shape_pose_at(const HillShapeGeometry& geometry,
                                             float along) -> HillSpinePose {
  HillSpinePose pose;
  if (!geometry.is_spine()) {
    return pose;
  }

  const float target =
      std::clamp(along, 0.0F, 1.0F) * std::max(geometry.total_length, 0.0001F);
  for (std::size_t i = 1; i < geometry.spine.size(); ++i) {
    const float segment_start = geometry.arclength[i - 1];
    const float segment_end = geometry.arclength[i];
    const float segment_length = segment_end - segment_start;
    if (target > segment_end && i + 1 < geometry.spine.size()) {
      continue;
    }
    const float t =
        segment_length > 1e-5F
            ? std::clamp((target - segment_start) / segment_length, 0.0F, 1.0F)
            : 0.0F;
    const HillShapePoint& a = geometry.spine[i - 1];
    const HillShapePoint& b = geometry.spine[i];
    pose.position = {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t};
    const float length = std::max(std::hypot(b.x - a.x, b.z - a.z), 1e-5F);
    pose.tangent = {(b.x - a.x) / length, (b.z - a.z) / length};
    break;
  }
  return pose;
}

[[nodiscard]] inline auto hill_shape_mask_distance(
    float local_x, float local_z, const HillShapeGeometry& geometry) -> float {
  const float grid_x = local_x - geometry.mask_origin_x;
  const float grid_z = local_z - geometry.mask_origin_z;
  const float outside = geometry.mask_inset + 2.0F;
  if (grid_x < 0.0F || grid_z < 0.0F ||
      grid_x > static_cast<float>(geometry.mask_width - 1) ||
      grid_z > static_cast<float>(geometry.mask_height - 1)) {
    return outside;
  }

  const int x0 = static_cast<int>(std::floor(grid_x));
  const int z0 = static_cast<int>(std::floor(grid_z));
  const int x1 = std::min(x0 + 1, geometry.mask_width - 1);
  const int z1 = std::min(z0 + 1, geometry.mask_height - 1);
  const float fx = grid_x - static_cast<float>(x0);
  const float fz = grid_z - static_cast<float>(z0);

  const auto at = [&](int x, int z) {
    return geometry
        .mask_distance[static_cast<std::size_t>(z * geometry.mask_width + x)];
  };
  const float top = at(x0, z0) * (1.0F - fx) + at(x1, z0) * fx;
  const float bottom = at(x0, z1) * (1.0F - fx) + at(x1, z1) * fx;
  return top * (1.0F - fz) + bottom * fz;
}

[[nodiscard]] inline auto hill_shape_ramp_target(
    float local_x, float local_z, const HillShapeGeometry& geometry) -> HillShapePoint {
  if (geometry.is_mask()) {
    const float step = 0.75F;
    float x = local_x;
    float z = local_z;
    for (int iteration = 0; iteration < 64; ++iteration) {
      const float distance = hill_shape_mask_distance(x, z, geometry);
      if (distance <= -geometry.mask_inset) {
        break;
      }
      const float gradient_x = hill_shape_mask_distance(x + 0.5F, z, geometry) -
                               hill_shape_mask_distance(x - 0.5F, z, geometry);
      const float gradient_z = hill_shape_mask_distance(x, z + 0.5F, geometry) -
                               hill_shape_mask_distance(x, z - 0.5F, geometry);
      const float length = std::hypot(gradient_x, gradient_z);
      if (length < 1e-4F) {
        break;
      }
      x -= gradient_x / length * step;
      z -= gradient_z / length * step;
    }
    return {x, z};
  }
  if (!geometry.is_spine()) {
    return {0.0F, 0.0F};
  }
  const HillSpineSample sample = sample_hill_spine(local_x, local_z, geometry);
  return {sample.nearest_x, sample.nearest_z};
}

} // namespace Game::Map
