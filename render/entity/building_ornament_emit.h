#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "building_archetype_desc.h"
#include "building_decay.h"
#include "building_state.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

namespace Render::GL {

inline void emit_building_ornament(const BuildingArchetypeDesc& desc,
                                   const QMatrix4x4& model,
                                   ISubmitter& out,
                                   Mesh* unit_cube,
                                   Texture* white,
                                   bool detailed,
                                   const QVector3D* palette = nullptr,
                                   std::size_t palette_size = 0,
                                   BuildingState filter_state = BuildingState::Normal) {
  Mesh* const cube = unit_cube != nullptr ? unit_cube : get_unit_cube();
  if (cube == nullptr) {
    return;
  }

  auto const resolve_color = [&](const BuildingPartDesc& part) {
    const bool palette_part = part.kind == BuildingPartKind::PaletteBox ||
                              part.kind == BuildingPartKind::PaletteRotatedBox ||
                              part.kind == BuildingPartKind::PaletteCylinder;
    if (!palette_part) {
      return part.color;
    }
    if (palette == nullptr || part.palette_slot >= palette_size) {
      return part.color;
    }
    return palette[part.palette_slot];
  };

  for (const auto& part : desc.parts()) {
    if (!part_supports_state(part.states, filter_state)) {
      continue;
    }

    const QVector3D color = resolve_color(part);
    switch (part.kind) {
    case BuildingPartKind::Box:
    case BuildingPartKind::PaletteBox: {
      QMatrix4x4 local;
      local.translate(part.point_a);
      local.scale(part.point_b);
      out.mesh(cube, model * local, color, white, part.alpha, part.material_id);
      break;
    }
    case BuildingPartKind::RotatedBox:
    case BuildingPartKind::PaletteRotatedBox: {
      QMatrix4x4 local;
      local.translate(part.point_a);
      local.rotate(part.euler_deg.z(), 0.0F, 0.0F, 1.0F);
      local.rotate(part.euler_deg.y(), 0.0F, 1.0F, 0.0F);
      local.rotate(part.euler_deg.x(), 1.0F, 0.0F, 0.0F);
      local.scale(part.point_b);
      out.mesh(cube, model * local, color, white, part.alpha, part.material_id);
      break;
    }
    case BuildingPartKind::Cylinder:
    case BuildingPartKind::PaletteCylinder:
      out.mesh(get_unit_cylinder(),
               model * Render::Geom::cylinder_between(
                           part.point_a, part.point_b, part.radius),
               color,
               white,
               part.alpha,
               part.material_id);
      break;
    case BuildingPartKind::Cone:
      out.mesh(get_unit_cone(),
               model *
                   Render::Geom::cone_from_to(part.point_a, part.point_b, part.radius),
               color,
               white,
               part.alpha,
               part.material_id);
      break;
    }
  }
}

} // namespace Render::GL
