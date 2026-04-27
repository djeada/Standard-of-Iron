

#pragma once

#include "rig_interpreter.h"

#include <QVector3D>

#include <array>
#include <cstddef>

namespace Render::RigDSL {

template <std::size_t N>
class StaticAnchorResolver final : public AnchorResolver {
public:
  StaticAnchorResolver() { m_positions.fill(QVector3D(0.0F, 0.0F, 0.0F)); }

  void set(AnchorId id, const QVector3D &pos) {
    if (id < N) {
      m_positions[id] = pos;
    }
  }

  [[nodiscard]] auto resolve(AnchorId id) const -> QVector3D override {
    if (id >= N) {
      return {0.0F, 0.0F, 0.0F};
    }
    return m_positions[id];
  }

private:
  std::array<QVector3D, N> m_positions{};
};

class StaticPaletteResolver final : public PaletteResolver {
public:
  StaticPaletteResolver() { m_slots.fill(QVector3D(1.0F, 1.0F, 1.0F)); }

  void set(PaletteSlot slot, const QVector3D &color) {
    auto const idx = static_cast<std::size_t>(slot);
    if (idx < m_slots.size()) {
      m_slots[idx] = color;
    }
  }

  [[nodiscard]] auto resolve(PaletteSlot slot) const -> QVector3D override {
    auto const idx = static_cast<std::size_t>(slot);
    if (idx >= m_slots.size()) {
      return {1.0F, 1.0F, 1.0F};
    }
    return m_slots[idx];
  }

private:
  std::array<QVector3D, static_cast<std::size_t>(PaletteSlot::_Count)>
      m_slots{};
};

} // namespace Render::RigDSL
