#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Game::Systems {

struct RouteSpan {
  float begin_s{0.0F};
  float end_s{0.0F};
  float half_width{0.0F};
  std::uint32_t portal_id{0};
};

class MovementRoute {
public:
  struct Projection {
    float s{0.0F};
    float lateral{0.0F};
    std::size_t segment{0};
  };

  auto build(std::uint64_t route_revision,
             std::uint64_t topology_revision,
             float origin_x,
             float origin_z,
             const std::vector<std::pair<float, float>>& waypoints,
             std::size_t first_waypoint,
             float fallback_x,
             float fallback_z) -> bool;

  void clear();

  void update_final_point(float x, float z);

  [[nodiscard]] auto valid() const -> bool { return m_points.size() >= 2U; }
  [[nodiscard]] auto route_revision() const -> std::uint64_t {
    return m_route_revision;
  }
  [[nodiscard]] auto topology_revision() const -> std::uint64_t {
    return m_topology_revision;
  }
  [[nodiscard]] auto length() const -> float { return m_length; }
  [[nodiscard]] auto travelled() const -> float { return m_travelled; }
  [[nodiscard]] auto remaining() const -> float { return m_length - m_travelled; }
  [[nodiscard]] auto point_count() const -> std::size_t { return m_points.size(); }

  [[nodiscard]] auto project(float x, float z, float window) const -> Projection;

  void advance_to(float s);

  [[nodiscard]] auto point_at(float s) const -> std::pair<float, float>;
  [[nodiscard]] auto tangent_at(float s) const -> std::pair<float, float>;

  [[nodiscard]] auto next_vertex_s(float s) const -> float;

  template <typename SegmentClear>
  [[nodiscard]] auto steering_aim_s(float from_x,
                                    float from_z,
                                    float lookahead,
                                    float min_aim_distance,
                                    const SegmentClear& segment_clear) const -> float {

    constexpr float k_fallback_lookahead = 0.45F;
    float const reach =
        std::isfinite(lookahead) && lookahead > 0.0F ? lookahead : k_fallback_lookahead;
    if (!std::isfinite(min_aim_distance)) {
      min_aim_distance = reach * 0.75F;
    }
    float const target = std::min(m_length, m_travelled + reach);
    float aim_s = std::min(target, next_vertex_s(m_travelled));
    constexpr int k_max_extensions = 16;
    constexpr int k_clear_bisections = 6;
    for (int extension = 0; extension < k_max_extensions && aim_s < target;
         ++extension) {
      auto const aim = point_at(aim_s);
      if (std::hypot(aim.first - from_x, aim.second - from_z) >= min_aim_distance) {
        break;
      }
      float const next = std::min(target, next_vertex_s(aim_s));
      auto const beyond = point_at(next);
      if (segment_clear(from_x, from_z, beyond.first, beyond.second)) {
        aim_s = next;
        continue;
      }
      float clear_s = aim_s;
      float blocked_s = next;
      for (int bisection = 0; bisection < k_clear_bisections; ++bisection) {
        float const middle = (clear_s + blocked_s) * 0.5F;
        auto const point = point_at(middle);
        if (segment_clear(from_x, from_z, point.first, point.second)) {
          clear_s = middle;
        } else {
          blocked_s = middle;
        }
      }
      aim_s = clear_s;
      break;
    }
    return aim_s;
  }

  [[nodiscard]] auto final_point() const -> std::pair<float, float>;

  [[nodiscard]] auto waypoint_index_at(float s) const -> std::size_t;

  void set_spans(std::vector<RouteSpan> spans) { m_spans = std::move(spans); }
  [[nodiscard]] auto span_at(float s) const -> const RouteSpan*;
  [[nodiscard]] auto spans() const -> const std::vector<RouteSpan>& { return m_spans; }

private:
  struct Vertex {
    float x{0.0F};
    float z{0.0F};
    float cumulative{0.0F};
  };

  std::vector<Vertex> m_points;
  std::vector<RouteSpan> m_spans;
  std::uint64_t m_route_revision{0};
  std::uint64_t m_topology_revision{0};
  std::size_t m_first_waypoint{0};
  float m_length{0.0F};
  float m_travelled{0.0F};
};

} // namespace Game::Systems
