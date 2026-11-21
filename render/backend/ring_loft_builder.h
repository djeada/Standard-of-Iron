#pragma once

#include <QVector2D>
#include <QVector3D>

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace Render::GL::BackendPipelines {

struct RingVertex {
  QVector3D position;
  QVector3D normal;
  float u{0.0F};
  float v{0.0F};
  float weight{0.0F};
};

class RingLoftBuilder {
public:
  struct Ring {
    float radius{1.0F};
    float y{0.0F};

    float normal_up{0.0F};
    float v{0.0F};
    float weight{0.0F};
    QVector2D center{0.0F, 0.0F};
  };

  explicit RingLoftBuilder(int segments);

  auto add_ring(const Ring& ring) -> int;

  void connect(int lower, int upper);

  void connect_chain(std::initializer_list<int> rings);

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
