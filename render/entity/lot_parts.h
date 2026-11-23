#pragma once

#include <QVector3D>

#include <cstdint>

#include "render/entity/building_archetype_desc.h"

namespace Render::GL {

// Axis-aligned parts by their extents instead of centre and half size, which is
// how a building standing on a lot is easiest to author: this wall runs from
// here to there. Degenerate boxes are dropped.
class LotParts {
public:
  explicit LotParts(BuildingArchetypeDesc& desc)
      : m_desc(desc) {}

  [[nodiscard]] auto desc() -> BuildingArchetypeDesc& { return m_desc; }

  void box(float x0,
           float x1,
           float y0,
           float y1,
           float z0,
           float z1,
           const QVector3D& color) {
    if (x1 - x0 < 1.0e-4F || y1 - y0 < 1.0e-4F || z1 - z0 < 1.0e-4F) {
      return;
    }
    m_desc.add_box(QVector3D((x0 + x1) * 0.5F, (y0 + y1) * 0.5F, (z0 + z1) * 0.5F),
                   QVector3D((x1 - x0) * 0.5F, (y1 - y0) * 0.5F, (z1 - z0) * 0.5F),
                   color);
  }

  // A box that takes its colour from the instance palette. `material` names
  // what the paint is on; a palette colour left unclassified is sorted into
  // cloth, leather or metal by its brightness.
  void slot(float x0,
            float x1,
            float y0,
            float y1,
            float z0,
            float z1,
            std::uint8_t palette_slot,
            int material = k_building_material_stone) {
    if (x1 - x0 < 1.0e-4F || y1 - y0 < 1.0e-4F || z1 - z0 < 1.0e-4F) {
      return;
    }
    BuildingPartMaterial const scope(m_desc, material);
    m_desc.add_palette_box(
        QVector3D((x0 + x1) * 0.5F, (y0 + y1) * 0.5F, (z0 + z1) * 0.5F),
        QVector3D((x1 - x0) * 0.5F, (y1 - y0) * 0.5F, (z1 - z0) * 0.5F),
        palette_slot);
  }

  void cylinder(const QVector3D& from,
                const QVector3D& to,
                float radius,
                const QVector3D& color) {
    m_desc.add_cylinder(from, to, radius, color);
  }

  void
  post(float x, float z, float y0, float y1, float radius, const QVector3D& color) {
    m_desc.add_cylinder(QVector3D(x, y0, z), QVector3D(x, y1, z), radius, color);
  }

private:
  BuildingArchetypeDesc& m_desc;
};

} // namespace Render::GL
