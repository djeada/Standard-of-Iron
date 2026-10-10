#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "campaign_map_render_utils.h"

namespace CampaignMapFilm {

struct CameraPose {
  QVector2D target{0.5F, 0.5F};
  float target_height = 0.0F;
  float distance = 1.35F;
  float yaw = 185.0F;
  float pitch = 52.0F;
  float fov = 45.0F;
};

inline auto world_point(const QVector2D& uv, float height) -> QVector3D {
  return {1.0F - uv.x(), height, uv.y()};
}

inline auto camera_eye(const CameraPose& pose) -> QVector3D {
  const QVector3D center = world_point(pose.target, pose.target_height);
  const float yaw_rad = qDegreesToRadians(pose.yaw);
  const float pitch_rad = qDegreesToRadians(std::clamp(pose.pitch, 1.0F, 90.0F));
  const float distance = std::max(0.01F, pose.distance);
  return {center.x() + distance * std::sin(yaw_rad) * std::cos(pitch_rad),
          center.y() + distance * std::sin(pitch_rad),
          center.z() + distance * std::cos(yaw_rad) * std::cos(pitch_rad)};
}

inline auto
view_projection(float width, float height, const CameraPose& pose) -> QMatrix4x4 {
  const float aspect = std::max(1.0F, width) / std::max(1.0F, height);
  const float distance = std::max(0.01F, pose.distance);
  const float near_plane = std::clamp(distance * 0.05F, 0.002F, 0.1F);
  const float far_plane = std::max(10.0F, distance * 8.0F);

  QMatrix4x4 projection;
  projection.perspective(
      std::clamp(pose.fov, 5.0F, 120.0F), aspect, near_plane, far_plane);

  QMatrix4x4 view;
  view.lookAt(camera_eye(pose),
              world_point(pose.target, pose.target_height),
              QVector3D(0.0F, 0.0F, 1.0F));
  return projection * view;
}

struct Projected {
  QPointF pixel;
  float depth = 0.0F;
  bool in_front = false;
};

inline auto project(const QMatrix4x4& view_proj,
                    const QVector3D& world,
                    float width,
                    float height) -> Projected {
  const QVector4D clip = view_proj * QVector4D(world, 1.0F);
  Projected out;
  if (clip.w() <= 1e-6F) {
    return out;
  }
  const float ndc_x = clip.x() / clip.w();
  const float ndc_y = clip.y() / clip.w();
  out.pixel = QPointF((ndc_x + 1.0F) * 0.5F * width, (1.0F - ndc_y) * 0.5F * height);
  out.depth = clip.z() / clip.w();
  out.in_front = true;
  return out;
}

class RoutePath {
public:
  static constexpr int k_samples_per_segment = 8;

  RoutePath() = default;

  explicit RoutePath(const std::vector<QVector2D>& raw,
                     int samples_per_segment = k_samples_per_segment)
      : m_samples(std::max(1, samples_per_segment))
      , m_raw_count(raw.size()) {
    if (raw.size() < 2) {
      m_points = raw;
    } else {
      m_points = CampaignMapRender::smooth_catmull_rom(raw, m_samples);
    }
    m_cumulative.assign(m_points.size(), 0.0F);
    for (std::size_t i = 1; i < m_points.size(); ++i) {
      m_cumulative[i] = m_cumulative[i - 1] + (m_points[i] - m_points[i - 1]).length();
    }
  }

  [[nodiscard]] auto empty() const -> bool { return m_points.size() < 2; }
  [[nodiscard]] auto length() const -> float {
    return m_cumulative.empty() ? 0.0F : m_cumulative.back();
  }
  [[nodiscard]] auto points() const -> const std::vector<QVector2D>& {
    return m_points;
  }
  [[nodiscard]] auto raw_count() const -> std::size_t { return m_raw_count; }

  [[nodiscard]] auto smoothed_index_of_raw(std::size_t raw_index) const -> std::size_t {
    if (m_points.empty()) {
      return 0;
    }
    if (m_raw_count < 2) {
      return std::min(raw_index, m_points.size() - 1);
    }
    return std::min(raw_index * static_cast<std::size_t>(m_samples),
                    m_points.size() - 1);
  }

  [[nodiscard]] auto progress_at_raw_index(std::size_t raw_index) const -> float {
    if (empty() || length() <= 0.0F) {
      return 0.0F;
    }
    return m_cumulative[smoothed_index_of_raw(raw_index)] / length();
  }

  [[nodiscard]] auto point_at(float progress) const -> QVector2D {
    if (m_points.empty()) {
      return {};
    }
    if (empty()) {
      return m_points.front();
    }
    const auto [index, blend] = locate(progress);
    return m_points[index] + (m_points[index + 1] - m_points[index]) * blend;
  }

  [[nodiscard]] auto direction_at(float progress) const -> QVector2D {
    if (empty()) {
      return {1.0F, 0.0F};
    }
    const auto [index, blend] = locate(progress);
    (void)blend;
    const QVector2D delta = m_points[index + 1] - m_points[index];
    const float len = delta.length();
    return len > 1e-9F ? delta / len : QVector2D(1.0F, 0.0F);
  }

  [[nodiscard]] auto slice(float from, float to) const -> std::vector<QVector2D> {
    std::vector<QVector2D> out;
    if (empty()) {
      return out;
    }
    float a = std::clamp(from, 0.0F, 1.0F);
    float b = std::clamp(to, 0.0F, 1.0F);
    if (b <= a) {
      return out;
    }
    const float total = length();
    const float start_len = a * total;
    const float end_len = b * total;
    out.push_back(point_at(a));
    for (std::size_t i = 0; i < m_points.size(); ++i) {
      if (m_cumulative[i] > start_len && m_cumulative[i] < end_len) {
        out.push_back(m_points[i]);
      }
    }
    out.push_back(point_at(b));
    return out;
  }

private:
  [[nodiscard]] auto locate(float progress) const -> std::pair<std::size_t, float> {
    const float target = std::clamp(progress, 0.0F, 1.0F) * length();
    const auto it = std::upper_bound(m_cumulative.begin(), m_cumulative.end(), target);
    std::size_t upper = static_cast<std::size_t>(it - m_cumulative.begin());
    upper = std::clamp<std::size_t>(upper, 1, m_points.size() - 1);
    const std::size_t index = upper - 1;
    const float span = m_cumulative[upper] - m_cumulative[index];
    const float blend =
        span > 1e-12F ? std::clamp((target - m_cumulative[index]) / span, 0.0F, 1.0F)
                      : 0.0F;
    return {index, blend};
  }

  int m_samples = k_samples_per_segment;
  std::size_t m_raw_count = 0;
  std::vector<QVector2D> m_points;
  std::vector<float> m_cumulative;
};

struct RouteStyle {
  float width_px = 5.0F;
  QVector4D casing{0.10F, 0.07F, 0.05F, 1.0F};
  QVector4D gold{0.86F, 0.70F, 0.36F, 1.0F};
  QVector4D core{0.66F, 0.14F, 0.10F, 1.0F};
  float shadow_alpha = 0.35F;
  bool head = true;
  float head_radius_px = 9.0F;
  float ghost_alpha = 0.0F;
};

struct RegionHighlight {
  std::vector<QString> provinces;
  std::vector<QVector2D> triangles;
  QVector4D fill{0.93F, 0.74F, 0.38F, 0.30F};
  QVector4D rim{0.98F, 0.83F, 0.48F, 0.95F};
  float rim_px = 2.0F;
  float dim_outside = 0.0F;
  float amount = 0.0F;
};

struct FrameState {
  bool active = false;
  CameraPose camera;
  float reference_height = 1080.0F;
  float terrain_height_scale = 0.085F;
  bool province_fills = false;
  float province_fill_alpha = 1.0F;
  bool show_borders = false;
  float drape_radius = 0.008F;
  bool show_game_route = false;
  bool show_symbols = true;
  bool route_visible = false;
  float route_from = 0.0F;
  float route_to = 0.0F;
  float route_window_end = 1.0F;
  RouteStyle route_style;
  float time = 0.0F;
  std::vector<RegionHighlight> highlights;
};

} // namespace CampaignMapFilm
