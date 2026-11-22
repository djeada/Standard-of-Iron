#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

namespace Render::GL {

struct ShadowRangeFit {
  float near_distance = 0.0F;
  float far_distance = 0.0F;
};

struct ShadowCascadeSphere {
  QVector3D center;
  float radius = 1.0F;
};

inline constexpr float k_shadow_split_lambda = 0.68F;
inline constexpr float k_shadow_range_near_margin = 0.94F;
inline constexpr float k_shadow_range_far_margin = 1.05F;
inline constexpr float k_shadow_sphere_padding = 1.03F;

[[nodiscard]] inline auto ray_plane_y_distance(const QVector3D& origin,
                                               const QVector3D& direction,
                                               float height) noexcept -> float {
  const float dy = direction.y();
  if (std::abs(dy) < 1e-5F) {
    return -1.0F;
  }
  return (height - origin.y()) / dy;
}

[[nodiscard]] inline auto
fit_shadow_distance_range(const QVector3D& camera_position,
                          std::span<const QVector3D> ray_directions,
                          float slab_min_y,
                          float slab_max_y,
                          float min_distance,
                          float max_distance) noexcept -> ShadowRangeFit {
  min_distance = std::max(min_distance, 0.05F);
  max_distance = std::max(max_distance, min_distance + 1.0F);
  if (slab_max_y < slab_min_y) {
    std::swap(slab_min_y, slab_max_y);
  }

  const bool camera_inside_slab =
      camera_position.y() >= slab_min_y && camera_position.y() <= slab_max_y;

  float nearest = std::numeric_limits<float>::max();
  float farthest = 0.0F;
  bool any_escape = false;
  bool leaves_through_top = false;
  bool leaves_through_bottom = false;

  for (const QVector3D& direction : ray_directions) {
    const float to_top = ray_plane_y_distance(camera_position, direction, slab_max_y);
    const float to_bottom =
        ray_plane_y_distance(camera_position, direction, slab_min_y);

    float enter = 0.0F;
    float exit = 0.0F;
    if (camera_inside_slab) {
      enter = 0.0F;

      float leave = -1.0F;
      if (to_top > 0.0F) {
        leave = to_top;
      }
      if (to_bottom > 0.0F && (leave < 0.0F || to_bottom < leave)) {
        leave = to_bottom;
      }
      if (leave < 0.0F) {
        any_escape = true;
        exit = max_distance;
      } else {
        exit = leave;
        if (leave == to_top) {
          leaves_through_top = true;
        } else {
          leaves_through_bottom = true;
        }
      }
    } else {

      const bool above = camera_position.y() > slab_max_y;
      const float first = above ? to_top : to_bottom;
      const float second = above ? to_bottom : to_top;
      if (first <= 0.0F) {
        any_escape = true;
        continue;
      }
      enter = first;
      exit = second > 0.0F ? second : max_distance;
    }

    nearest = std::min(nearest, enter);
    farthest = std::max(farthest, exit);
  }

  ShadowRangeFit fit;
  if (nearest == std::numeric_limits<float>::max()) {

    fit.near_distance = min_distance;
    fit.far_distance = max_distance;
    return fit;
  }

  if (any_escape || (leaves_through_top && leaves_through_bottom)) {
    farthest = max_distance;
  }

  fit.near_distance = std::clamp(
      nearest * k_shadow_range_near_margin, min_distance, max_distance - 1.0F);
  fit.far_distance = std::clamp(
      farthest * k_shadow_range_far_margin, fit.near_distance + 1.0F, max_distance);
  return fit;
}

[[nodiscard]] inline auto compute_shadow_cascade_splits(
    float near_distance,
    float far_distance,
    int cascade_count,
    float lambda = k_shadow_split_lambda) noexcept -> std::array<float, 4> {
  std::array<float, 4> splits{};
  cascade_count = std::clamp(cascade_count, 1, 4);
  near_distance = std::max(near_distance, 0.05F);
  far_distance = std::max(far_distance, near_distance + 1.0F);
  for (int cascade = 0; cascade < cascade_count; ++cascade) {
    const float p = static_cast<float>(cascade + 1) / static_cast<float>(cascade_count);
    const float logarithmic = near_distance * std::pow(far_distance / near_distance, p);
    const float uniform = near_distance + (far_distance - near_distance) * p;
    splits[static_cast<std::size_t>(cascade)] =
        logarithmic * lambda + uniform * (1.0F - lambda);
  }
  for (int cascade = cascade_count; cascade < 4; ++cascade) {
    splits[static_cast<std::size_t>(cascade)] = far_distance;
  }
  return splits;
}

[[nodiscard]] inline auto
fit_shadow_cascade_sphere(const QVector3D& camera_position,
                          std::span<const QVector3D, 4> corner_directions,
                          const QVector3D& forward,
                          float near_distance,
                          float far_distance) noexcept -> ShadowCascadeSphere {
  std::array<QVector3D, 8> corners{};
  QVector3D center;
  for (std::size_t i = 0; i < 4; ++i) {
    corners[i] = camera_position + corner_directions[i] * near_distance;
    corners[i + 4] = camera_position + corner_directions[i] * far_distance;
    center += corners[i] + corners[i + 4];
  }
  center /= 8.0F;

  float radius = 0.0F;
  for (const QVector3D& corner : corners) {
    radius = std::max(radius, (corner - center).length());
  }

  const std::array<std::pair<std::size_t, std::size_t>, 4> edges{
      {{0, 1}, {1, 3}, {3, 2}, {2, 0}}};
  for (const auto& [a, b] : edges) {
    const QVector3D mid = (corner_directions[a] + corner_directions[b]).normalized();
    radius = std::max(radius, (camera_position + mid * far_distance - center).length());
  }
  radius = std::max(
      radius,
      (camera_position + forward.normalized() * far_distance - center).length());

  ShadowCascadeSphere sphere;
  sphere.center = center;
  sphere.radius = std::max(radius * k_shadow_sphere_padding, 1.0F);
  return sphere;
}

inline constexpr float k_shadow_light_follow_cos = 0.99999848F;
inline constexpr float k_shadow_caster_height_allowance = 10.0F;
inline constexpr float k_shadow_default_slab_min = -2.0F;
inline constexpr float k_shadow_default_slab_max = 6.0F;

struct ShadowLightBasis {
  QVector3D direction{0.0F, 1.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 0.0F, 1.0F};
};

[[nodiscard]] inline auto
follow_shadow_light_direction(const QVector3D& held,
                              const QVector3D& sun_direction) noexcept -> QVector3D {
  if (held.isNull() ||
      QVector3D::dotProduct(sun_direction, held) < k_shadow_light_follow_cos) {
    return sun_direction;
  }
  return held;
}

[[nodiscard]] inline auto
shadow_light_basis(const QVector3D& light_direction) noexcept -> ShadowLightBasis {
  const QVector3D reference =
      std::abs(QVector3D::dotProduct(light_direction, QVector3D(0, 1, 0))) > 0.96F
          ? QVector3D(0, 0, 1)
          : QVector3D(0, 1, 0);
  ShadowLightBasis basis;
  basis.direction = light_direction;
  basis.right = QVector3D::crossProduct(reference, light_direction).normalized();
  basis.up = QVector3D::crossProduct(light_direction, basis.right).normalized();
  return basis;
}

struct ShadowSunThrow {
  float stretch = 0.0F;
  float rigged_slack = 6.0F;
  float throw_scale = 1.0F;
};

[[nodiscard]] inline auto
shadow_sun_throw(const QVector3D& light_direction) noexcept -> ShadowSunThrow {
  ShadowSunThrow sun;
  sun.stretch = std::hypot(light_direction.x(), light_direction.z()) /
                std::max(light_direction.y(), 0.15F);
  sun.rigged_slack = 6.0F + 4.0F * std::min(sun.stretch, 4.0F);
  sun.throw_scale = std::sqrt(1.0F + sun.stretch * sun.stretch);
  return sun;
}

[[nodiscard]] inline auto
shadow_view_rays(const QMatrix4x4& inverse_view_projection,
                 const QVector3D& forward) -> std::array<QVector3D, 5> {
  std::array<QVector3D, 5> view_rays{};
  std::size_t corner = 0;
  for (int y : {-1, 1}) {
    for (int x : {-1, 1}) {
      const QVector4D near_h =
          inverse_view_projection *
          QVector4D(static_cast<float>(x), static_cast<float>(y), -1.0F, 1.0F);
      const QVector4D far_h =
          inverse_view_projection *
          QVector4D(static_cast<float>(x), static_cast<float>(y), 1.0F, 1.0F);
      view_rays[corner] =
          (far_h.toVector3DAffine() - near_h.toVector3DAffine()).normalized();
      ++corner;
    }
  }
  view_rays[4] = forward.normalized();
  return view_rays;
}

struct ShadowReceiverSlab {
  float min_y = k_shadow_default_slab_min;
  float max_y = k_shadow_default_slab_max;
};

[[nodiscard]] inline auto
finish_shadow_receiver_slab(float terrain_min_y,
                            float terrain_max_y) noexcept -> ShadowReceiverSlab {
  ShadowReceiverSlab slab;
  if (terrain_min_y <= terrain_max_y) {
    slab.min_y = terrain_min_y;
    slab.max_y = terrain_max_y;
  }
  slab.max_y += k_shadow_caster_height_allowance;
  return slab;
}

struct ShadowCascadeFrame {
  QVector3D center;
  float radius = 1.0F;
  float world_texel = 0.0F;
  float depth_span = 0.0F;
  QMatrix4x4 light_view;
  QMatrix4x4 light_projection;
  QMatrix4x4 light_view_projection;
};

[[nodiscard]] inline auto
fit_shadow_cascade_frame(const ShadowCascadeSphere& sphere,
                         const ShadowLightBasis& light,
                         int resolution) -> ShadowCascadeFrame {
  ShadowCascadeFrame frame;
  frame.radius = std::ceil(sphere.radius * 16.0F) / 16.0F;
  frame.world_texel =
      (frame.radius * 2.0F) / static_cast<float>(std::max(resolution, 1));
  QVector3D center = sphere.center;
  center -= light.right *
            std::fmod(QVector3D::dotProduct(center, light.right), frame.world_texel);
  center -=
      light.up * std::fmod(QVector3D::dotProduct(center, light.up), frame.world_texel);
  frame.center = center;
  frame.depth_span = frame.radius * 6.0F - 0.1F;
  frame.light_view.lookAt(
      center + light.direction * (frame.radius * 2.5F), center, light.up);
  frame.light_projection.ortho(-frame.radius,
                               frame.radius,
                               -frame.radius,
                               frame.radius,
                               0.1F,
                               frame.radius * 6.0F);
  frame.light_view_projection = frame.light_projection * frame.light_view;
  return frame;
}

struct ShadowCascadeCull {
  QVector3D center;
  float radius = 0.0F;
  QVector3D light_right;
  QVector3D light_up;
  QVector3D camera_position;
  float receiver_near = 0.0F;
  float receiver_far = 0.0F;
  float receiver_margin = 0.0F;
  float ground_height = 0.0F;
  float shadow_throw = 1.0F;
  float min_caster_radius = 0.0F;

  [[nodiscard]] auto accepts(const QVector3D& caster_center,
                             float caster_radius) const noexcept -> bool {
    const QVector3D relative = caster_center - center;
    const float reach = radius + caster_radius;
    if (std::abs(QVector3D::dotProduct(relative, light_right)) > reach ||
        std::abs(QVector3D::dotProduct(relative, light_up)) > reach) {
      return false;
    }
    const float caster_height =
        std::max(0.0F, caster_center.y() + caster_radius - ground_height);
    const float receiver_reach =
        caster_radius + receiver_margin + caster_height * shadow_throw;
    const float distance = (caster_center - camera_position).length();
    return distance + receiver_reach >= receiver_near &&
           distance - receiver_reach <= receiver_far;
  }
};

} // namespace Render::GL
