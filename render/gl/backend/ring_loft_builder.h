#pragma once

#include <QVector2D>
#include <QVector3D>

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace Render::GL::BackendPipelines {

// A vertex on a lofted ring. `u` runs around the ring, `v` along the profile,
// and `weight` is a free per-ring channel the tree shaders use for the amount
// of bough sway; callers that do not need it leave it at zero.
struct RingVertex {
  QVector3D position;
  QVector3D normal;
  float u{0.0F};
  float v{0.0F};
  float weight{0.0F};
};

// Builds a surface of revolution from horizontal rings: trunks, boughs, jars,
// columns. Every ring has the same segment count, so any two of them can be
// stitched into a band, and a ring can be closed off with a triangle fan.
//
// Callers convert the finished RingVertex list into whatever vertex layout
// their shader expects; this class only knows about geometry.
class RingLoftBuilder {
public:
  struct Ring {
    float radius{1.0F};
    float y{0.0F};
    // Y component of the ring's normal before normalisation: negative flares
    // the surface outward-down, positive tilts it up toward the axis.
    float normal_up{0.0F};
    float v{0.0F};
    float weight{0.0F};
    QVector2D center{0.0F, 0.0F};
  };

  explicit RingLoftBuilder(int segments);

  // Appends one ring and returns the index of its first vertex, which is what
  // connect() and cap() take.
  auto add_ring(const Ring& ring) -> int;

  // Stitches two rings into a band of quads.
  void connect(int lower, int upper);

  // Stitches a chain of rings bottom to top: connect_chain({a, b, c}) is
  // connect(a, b) followed by connect(b, c).
  void connect_chain(std::initializer_list<int> rings);

  // Closes a ring with a triangle fan around a single apex vertex. `facing_up`
  // picks both the apex normal and the winding, so a base cap and a tip cap
  // are the same call with opposite orientation.
  void cap(int ring,
           float apex_y,
           const QVector2D& apex_center,
           float v,
           float apex_weight = 0.0F,
           bool facing_up = true);

  void reserve(std::size_t rings);

  [[nodiscard]] auto segments() const noexcept -> int { return m_segments; }
  [[nodiscard]] auto vertices() const noexcept -> const std::vector<RingVertex>& {
    return m_vertices;
  }
  [[nodiscard]] auto indices() const noexcept -> const std::vector<std::uint16_t>& {
    return m_indices;
  }

private:
  int m_segments;
  std::vector<RingVertex> m_vertices;
  std::vector<std::uint16_t> m_indices;
};

} // namespace Render::GL::BackendPipelines
