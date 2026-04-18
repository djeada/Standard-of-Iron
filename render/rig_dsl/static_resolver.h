// Stage 11 — simple in-memory resolvers for rig DSL consumers.
//
// These cover the common case of building/structural rigs where anchor
// positions are computed per-render from C++ code and passed to the
// interpreter as a flat table keyed by AnchorId.
//
// Rationale:
//   * Unit rigs (humanoid head, horse harness) need bone-frame-driven
//     resolvers — they can't use these.
//   * Buildings and props have no bone frames: their anchors are fixed
//     offsets in unit-local space. A flat table lookup is both correct
//     and zero-cost compared to a virtual-dispatching bone frame.
//   * Palette is the same: most buildings have 1–2 team-tinted slots and
//     the rest are literal colours. A 16-entry array lookup is ample.

#pragma once

#include "rig_interpreter.h"

#include <QVector3D>

#include <array>
#include <cstddef>

namespace Render::RigDSL {

// Anchor resolver backed by a contiguous array indexed by AnchorId.
// Unset entries default to (0,0,0). `N` is the number of anchors the
// caller will populate — typically the enum's implicit count.
template <std::size_t N> class StaticAnchorResolver final : public AnchorResolver {
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

// Palette resolver backed by a 16-slot array indexed by PaletteSlot.
// PaletteSlot::Literal is never looked up (the interpreter handles it
// inline) but we keep the slot for index symmetry.
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
  std::array<QVector3D, static_cast<std::size_t>(PaletteSlot::_Count)> m_slots{};
};

} // namespace Render::RigDSL
