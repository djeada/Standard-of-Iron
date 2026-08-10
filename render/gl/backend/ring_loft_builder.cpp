#include "render/gl/backend/ring_loft_builder.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL::BackendPipelines {

namespace {
constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
}

RingLoftBuilder::RingLoftBuilder(int segments)
    : m_segments(std::max(segments, 3)) {
}

void RingLoftBuilder::reserve(std::size_t rings) {
  m_vertices.reserve(rings * static_cast<std::size_t>(m_segments) + rings);
  m_indices.reserve(rings * static_cast<std::size_t>(m_segments) * 6U);
}

auto RingLoftBuilder::add_ring(const Ring& ring) -> int {
  const int start = static_cast<int>(m_vertices.size());
  for (int i = 0; i < m_segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(m_segments);
    const float angle = t * k_two_pi;
    const float nx = std::cos(angle);
    const float nz = std::sin(angle);
    QVector3D normal(nx, ring.normal_up, nz);
    normal.normalize();
    QVector3D const position(
        ring.radius * nx + ring.center.x(), ring.y, ring.radius * nz + ring.center.y());
    m_vertices.push_back({position, normal, t, ring.v, ring.weight});
  }
  return start;
}

void RingLoftBuilder::connect(int lower, int upper) {
  for (int i = 0; i < m_segments; ++i) {
    const int next = (i + 1) % m_segments;
    const auto lower0 = static_cast<std::uint16_t>(lower + i);
    const auto lower1 = static_cast<std::uint16_t>(lower + next);
    const auto upper0 = static_cast<std::uint16_t>(upper + i);
    const auto upper1 = static_cast<std::uint16_t>(upper + next);

    m_indices.push_back(lower0);
    m_indices.push_back(lower1);
    m_indices.push_back(upper1);
    m_indices.push_back(lower0);
    m_indices.push_back(upper1);
    m_indices.push_back(upper0);
  }
}

void RingLoftBuilder::connect_chain(std::initializer_list<int> rings) {
  const auto* previous = rings.begin();
  for (const auto* it = rings.begin() + (rings.size() > 0 ? 1 : 0); it != rings.end();
       ++it) {
    connect(*previous, *it);
    previous = it;
  }
}

void RingLoftBuilder::cap(int ring,
                          float apex_y,
                          const QVector2D& apex_center,
                          float v,
                          float apex_weight,
                          bool facing_up) {
  const auto apex = static_cast<std::uint16_t>(m_vertices.size());
  m_vertices.push_back({QVector3D(apex_center.x(), apex_y, apex_center.y()),
                        QVector3D(0.0F, facing_up ? 1.0F : -1.0F, 0.0F),
                        0.5F,
                        v,
                        apex_weight});
  for (int i = 0; i < m_segments; ++i) {
    const int next = (i + 1) % m_segments;
    const auto first = static_cast<std::uint16_t>(ring + (facing_up ? i : next));
    const auto second = static_cast<std::uint16_t>(ring + (facing_up ? next : i));
    m_indices.push_back(first);
    m_indices.push_back(second);
    m_indices.push_back(apex);
  }
}

} // namespace Render::GL::BackendPipelines
