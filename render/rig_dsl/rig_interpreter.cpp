#include "rig_interpreter.h"

#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../material.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cstdint>

namespace Render::RigDSL {

namespace {

[[nodiscard]] auto unpack(const PackedColor &c) -> QVector3D {
  constexpr float inv = 1.0F / 255.0F;
  return {static_cast<float>(c.r) * inv, static_cast<float>(c.g) * inv,
          static_cast<float>(c.b) * inv};
}

[[nodiscard]] auto pick_color(const PartDef &part,
                              const PaletteResolver *palette) -> QVector3D {
  QVector3D base;
  if (part.color_slot == PaletteSlot::Literal || palette == nullptr) {
    base = unpack(part.literal_color);
  } else {
    base = palette->resolve(part.color_slot);
  }
  return base * part.color_scale;
}

[[nodiscard]] auto mesh_for(PartKind kind) -> Render::GL::Mesh * {
  switch (kind) {
  case PartKind::Cylinder:
    return Render::GL::get_unit_cylinder();
  case PartKind::Cone:
    return Render::GL::get_unit_cone();
  case PartKind::Sphere:
    return Render::GL::get_unit_sphere();
  case PartKind::Box:
    return Render::GL::get_unit_cube();
  }
  return nullptr;
}

} // namespace

void render_part(const PartDef &part, const InterpretContext &ctx,
                 Render::GL::ISubmitter &submitter) {
  if (ctx.anchors == nullptr) {
    return;
  }
  if ((part.lod_mask & (1U << ctx.lod)) == 0U) {
    return;
  }

  Render::GL::Mesh *mesh = mesh_for(part.kind);
  if (mesh == nullptr) {
    return;
  }

  QVector3D const a = ctx.anchors->resolve(part.anchor_a);
  QVector3D const b = (part.kind == PartKind::Sphere)
                          ? a
                          : ctx.anchors->resolve(part.anchor_b);

  float const scale = (part.scale_id != kInvalidScalar && ctx.scalars != nullptr)
                          ? ctx.scalars->resolve(part.scale_id)
                          : 1.0F;
  float const sx = part.size_x * scale;
  float const sy = part.size_y * scale;
  float const sz = part.size_z * scale;

  QMatrix4x4 model;
  switch (part.kind) {
  case PartKind::Cylinder:
    model = Render::Geom::cylinder_between(ctx.model, a, b, sx);
    break;
  case PartKind::Cone:
    model = Render::Geom::cone_from_to(ctx.model, a, b, sx);
    break;
  case PartKind::Sphere:
    model = Render::Geom::sphere_at(ctx.model, a, sx);
    break;
  case PartKind::Box: {
    QVector3D const centre = (a + b) * 0.5F;
    QVector3D const span = b - a;
    model = ctx.model;
    model.translate(centre);
    model.scale(std::abs(span.x()) * 0.5F * sx,
                std::abs(span.y()) * 0.5F * sy,
                std::abs(span.z()) * 0.5F * sz);
    break;
  }
  }

  QVector3D const color = pick_color(part, ctx.palette);
  float const alpha = std::clamp(ctx.global_alpha * part.alpha, 0.0F, 1.0F);

  if (ctx.material != nullptr) {
    submitter.part(mesh, ctx.material, model, color, nullptr, alpha,
                   static_cast<int>(part.material_id));
  } else {
    submitter.mesh(mesh, model, color, nullptr, alpha,
                   static_cast<int>(part.material_id));
  }
}

void render_rig(const RigDef &def, const InterpretContext &ctx,
                Render::GL::ISubmitter &submitter) {
  for (std::size_t i = 0; i < def.part_count; ++i) {
    render_part(def.parts[i], ctx, submitter);
  }
}

} // namespace Render::RigDSL
