#include "movement_route.h"

#include <algorithm>
#include <cmath>

namespace Game::Systems {

namespace {

// Squared distance from (px,pz) to the segment a->b, plus the parameter along
// it. One helper so projection, lateral error and tangent all agree.
struct SegmentHit {
  float t{0.0F};
  float distance_sq{0.0F};
};

auto closest_on_segment(float ax, float az, float bx, float bz, float px, float pz)
    -> SegmentHit {
  float const dx = bx - ax;
  float const dz = bz - az;
  float const length_sq = dx * dx + dz * dz;
  float t = 0.0F;
  if (length_sq > 1.0e-9F) {
    t = std::clamp(((px - ax) * dx + (pz - az) * dz) / length_sq, 0.0F, 1.0F);
  }
  float const cx = ax + dx * t;
  float const cz = az + dz * t;
  float const ex = px - cx;
  float const ez = pz - cz;
  return {t, ex * ex + ez * ez};
}

} // namespace

auto MovementRoute::build(std::uint64_t route_revision,
                          std::uint64_t topology_revision,
                          float origin_x,
                          float origin_z,
                          const std::vector<std::pair<float, float>>& waypoints,
                          std::size_t first_waypoint,
                          float fallback_x,
                          float fallback_z) -> bool {
  m_points.clear();
  m_spans.clear();
  m_route_revision = route_revision;
  m_topology_revision = topology_revision;
  m_first_waypoint = first_waypoint;
  m_length = 0.0F;
  m_travelled = 0.0F;

  m_points.push_back({origin_x, origin_z, 0.0F});
  for (std::size_t index = first_waypoint; index < waypoints.size(); ++index) {
    float const x = waypoints[index].first;
    float const z = waypoints[index].second;
    auto const& previous = m_points.back();
    float const step = std::hypot(x - previous.x, z - previous.z);
    if (step <= 1.0e-4F) {
      continue;
    }
    m_length += step;
    m_points.push_back({x, z, m_length});
  }

  if (m_points.size() < 2U) {
    float const step = std::hypot(fallback_x - origin_x, fallback_z - origin_z);
    if (step <= 1.0e-4F) {
      m_points.clear();
      m_length = 0.0F;
      return false;
    }
    m_length = step;
    m_points.push_back({fallback_x, fallback_z, m_length});
    m_first_waypoint = waypoints.size();
  }
  return true;
}

void MovementRoute::clear() {
  m_points.clear();
  m_spans.clear();
  m_length = 0.0F;
  m_travelled = 0.0F;
  m_route_revision = 0;
  m_topology_revision = 0;
}

void MovementRoute::update_final_point(float x, float z) {
  if (m_points.size() < 2U) {
    return;
  }
  auto const& previous = m_points[m_points.size() - 2U];
  float const step = std::hypot(x - previous.x, z - previous.z);
  m_points.back() = {x, z, previous.cumulative + step};
  m_length = m_points.back().cumulative;
  m_travelled = std::min(m_travelled, m_length);
}

auto MovementRoute::project(float x, float z, float window) const -> Projection {
  Projection best;
  if (!valid()) {
    return best;
  }

  float const low = m_travelled;
  float const high = std::min(m_length, m_travelled + std::max(0.0F, window));

  float best_distance_sq = -1.0F;
  for (std::size_t index = 0; index + 1U < m_points.size(); ++index) {
    auto const& a = m_points[index];
    auto const& b = m_points[index + 1U];
    if (b.cumulative < low || a.cumulative > high) {
      continue;
    }
    auto const hit = closest_on_segment(a.x, a.z, b.x, b.z, x, z);
    float const s =
        std::clamp(a.cumulative + (b.cumulative - a.cumulative) * hit.t, low, high);
    if (best_distance_sq < 0.0F || hit.distance_sq < best_distance_sq) {
      best_distance_sq = hit.distance_sq;
      best.s = s;
      best.segment = index;
    }
  }

  if (best_distance_sq < 0.0F) {
    best.s = low;
    best.segment = waypoint_index_at(low);
    auto const [px, pz] = point_at(low);
    best.lateral = std::hypot(x - px, z - pz);
    return best;
  }

  best.lateral = std::sqrt(std::max(0.0F, best_distance_sq));
  return best;
}

void MovementRoute::advance_to(float s) {
  m_travelled = std::clamp(std::max(m_travelled, s), 0.0F, m_length);
}

auto MovementRoute::point_at(float s) const -> std::pair<float, float> {
  if (m_points.empty()) {
    return {0.0F, 0.0F};
  }
  float const clamped = std::clamp(s, 0.0F, m_length);
  for (std::size_t index = 0; index + 1U < m_points.size(); ++index) {
    auto const& a = m_points[index];
    auto const& b = m_points[index + 1U];
    if (clamped > b.cumulative) {
      continue;
    }
    float const span = b.cumulative - a.cumulative;
    float const t = span > 1.0e-6F ? (clamped - a.cumulative) / span : 0.0F;
    return {a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t};
  }
  return {m_points.back().x, m_points.back().z};
}

auto MovementRoute::tangent_at(float s) const -> std::pair<float, float> {
  if (!valid()) {
    return {0.0F, 1.0F};
  }
  float const clamped = std::clamp(s, 0.0F, m_length);
  for (std::size_t index = 0; index + 1U < m_points.size(); ++index) {
    auto const& a = m_points[index];
    auto const& b = m_points[index + 1U];
    if (clamped > b.cumulative && index + 2U < m_points.size()) {
      continue;
    }
    float const dx = b.x - a.x;
    float const dz = b.z - a.z;
    float const length = std::hypot(dx, dz);
    if (length <= 1.0e-6F) {
      continue;
    }
    return {dx / length, dz / length};
  }
  return {0.0F, 1.0F};
}

auto MovementRoute::next_vertex_s(float s) const -> float {
  for (auto const& point : m_points) {
    if (point.cumulative > s + 1.0e-4F) {
      return point.cumulative;
    }
  }
  return m_length;
}

auto MovementRoute::final_point() const -> std::pair<float, float> {
  if (m_points.empty()) {
    return {0.0F, 0.0F};
  }
  return {m_points.back().x, m_points.back().z};
}

auto MovementRoute::waypoint_index_at(float s) const -> std::size_t {
  // Vertex 0 is the origin the route was built from, so vertex n maps to the
  // caller's waypoint first_waypoint + n - 1.
  std::size_t vertex = 0;
  for (std::size_t index = 1; index < m_points.size(); ++index) {
    if (s + 1.0e-4F >= m_points[index].cumulative) {
      vertex = index;
    } else {
      break;
    }
  }
  return m_first_waypoint + vertex;
}

auto MovementRoute::span_at(float s) const -> const RouteSpan* {
  for (auto const& span : m_spans) {
    if (s >= span.begin_s && s <= span.end_s) {
      return &span;
    }
  }
  return nullptr;
}

} // namespace Game::Systems
