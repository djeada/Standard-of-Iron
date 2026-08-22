#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <span>

namespace Render::Creature::Quadruped {

class BoneDelta {
public:
  BoneDelta() = default;
  explicit BoneDelta(const QMatrix4x4& delta) noexcept
      : m_delta(delta) {}

  [[nodiscard]] auto point(const QVector3D& rest) const noexcept -> QVector3D {
    return m_delta.map(rest);
  }

  [[nodiscard]] auto axis(const QVector3D& rest) const noexcept -> QVector3D {
    return m_delta.mapVector(rest).normalized();
  }

  [[nodiscard]] auto matrix() const noexcept -> const QMatrix4x4& { return m_delta; }

private:
  QMatrix4x4 m_delta;
};

[[nodiscard]] auto bone_delta(std::span<const QMatrix4x4> pose,
                              std::span<const QMatrix4x4> bind,
                              std::size_t bone) noexcept -> BoneDelta;

} // namespace Render::Creature::Quadruped
