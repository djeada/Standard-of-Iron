#pragma once

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
