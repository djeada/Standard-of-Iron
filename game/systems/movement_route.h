#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Game::Systems {

// A span of the route whose usable width is known: an open stretch, an inferred
// narrow corridor, or an authored portal (gate, bridge, hill entrance).
struct RouteSpan {
  float begin_s{0.0F};
  float end_s{0.0F};
  float half_width{0.0F};
  std::uint32_t portal_id{0};
};

// One planned route as geometry rather than as a list of circles to enter.
//
// Waypoint circles can be re-entered from the far side, which is how a route
// loses progress and a heading flicks between two equally valid answers. This
// carries cumulative arclength instead: progress is a monotonically increasing
// distance along the polyline, and every question the follower asks -- where am
// I, which way does the route point, how much is left -- is answered from that
// one number.
class MovementRoute {
public:
  struct Projection {
    float s{0.0F};
    float lateral{0.0F};
    std::size_t segment{0};
  };

  // Builds the polyline from the body's position at assignment time through
  // every remaining waypoint, falling back to a straight segment to
  // (fallback_x, fallback_z) when no waypoints remain.
  //
  // A direct target is a route with one segment, not the absence of a route:
  // treating it as "no geometry" left it outside every progress measurement.
  // Returns false only when even the fallback is degenerate; the revision is
  // recorded either way so a failed build is not retried every tick.
  auto build(std::uint64_t route_revision,
             std::uint64_t topology_revision,
             float origin_x,
             float origin_z,
             const std::vector<std::pair<float, float>>& waypoints,
             std::size_t first_waypoint,
             float fallback_x,
             float fallback_z) -> bool;

  void clear();

  // Follows a moving goal without replanning: only the last vertex moves, so
  // the arclength already travelled stays valid.
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

  // Projects a world point onto the polyline, searching only forward from the
  // arclength already consumed and only as far as `window`. Progress can
  // therefore never run backwards because the body drifted sideways.
  [[nodiscard]] auto project(float x, float z, float window) const -> Projection;

  // Consumes progress. Never decreases.
  void advance_to(float s);

  [[nodiscard]] auto point_at(float s) const -> std::pair<float, float>;
  [[nodiscard]] auto tangent_at(float s) const -> std::pair<float, float>;

  // The arclength of the next vertex at or after `s`. Steering never aims past
  // it, so a taut route's corner is followed rather than cut.
  [[nodiscard]] auto next_vertex_s(float s) const -> float;

  [[nodiscard]] auto final_point() const -> std::pair<float, float>;

  // Index of the waypoint the body is heading for, in the original path's
  // coordinates, derived from arclength rather than from arrival circles.
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
