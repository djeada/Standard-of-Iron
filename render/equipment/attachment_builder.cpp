

#include "attachment_builder.h"

#include "../render_archetype.h"
#include "../static_attachment_spec.h"

#include <QVector4D>
#include <algorithm>
#include <cmath>

namespace Render::Equipment {

namespace {

constexpr float kAxisEpsilonSq = 1.0e-6F;

auto safe_normalize(const QVector3D &v,
                    const QVector3D &fallback) -> QVector3D {
  if (v.lengthSquared() <= kAxisEpsilonSq) {
    return fallback;
  }
  QVector3D n = v;
  n.normalize();
  return n;
}

void apply_palette_roles(Render::Creature::StaticAttachmentSpec &spec,
                         std::span<const std::uint8_t> palette_roles) {
  const std::size_t copy_count =
      std::min(palette_roles.size(),
               Render::Creature::StaticAttachmentSpec::kPaletteSlotCount);
  for (std::size_t i = 0; i < copy_count; ++i) {
    spec.palette_role_remap[i] = palette_roles[i];
  }
}

} // namespace

auto build_static_attachment(const AttachmentBuildInput &in)
    -> Render::Creature::StaticAttachmentSpec {
  Render::Creature::StaticAttachmentSpec spec;
  spec.archetype = in.archetype;
  spec.socket_bone_index = in.socket_bone_index;
  spec.uniform_scale = in.uniform_scale;
  QMatrix4x4 local_pose = in.unit_local_pose_at_bind;
  if (in.bind_radius > 0.0F) {
    QMatrix4x4 t;
    t.translate(in.authored_local_offset * in.bind_radius);
    QMatrix4x4 s;
    if (in.bind_radius != 1.0F) {
      s.scale(in.bind_radius);
    }
    local_pose = in.bind_socket_transform * t * s;
  }
  spec.local_offset = local_pose;
  spec.override_color_role = in.override_color_role;
  spec.material_id = in.material_id;
  apply_palette_roles(spec, in.palette_role_remap);
  return spec;
}

auto build_socket_static_attachment(const SocketAttachmentBuildInput &in)
    -> Render::Creature::StaticAttachmentSpec {
  Render::Creature::StaticAttachmentSpec spec;
  spec.archetype = in.archetype;
  spec.socket_bone_index = in.socket_bone_index;
  spec.uniform_scale = in.uniform_scale;
  spec.local_offset = in.bind_socket_transform * in.mesh_from_socket;
  spec.override_color_role = in.override_color_role;
  spec.material_id = in.material_id;
  apply_palette_roles(spec, in.palette_role_remap);
  return spec;
}

auto build_prepared_static_attachment(const PreparedAttachmentBuildInput &in)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = build_static_attachment(in.attachment);
  apply_palette_roles(spec, in.palette_roles);
  return spec;
}

auto build_prepared_socket_static_attachment(
    const PreparedSocketAttachmentBuildInput &in)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = build_socket_static_attachment(in.attachment);
  apply_palette_roles(spec, in.palette_roles);
  return spec;
}

auto compose_basis_unit_local(const QVector3D &origin, const QVector3D &right,
                              const QVector3D &up,
                              const QVector3D &forward) -> QMatrix4x4 {
  const QVector3D r = safe_normalize(right, QVector3D(1.0F, 0.0F, 0.0F));
  const QVector3D u = safe_normalize(up, QVector3D(0.0F, 1.0F, 0.0F));
  const QVector3D f = safe_normalize(forward, QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 m;
  m.setColumn(0, QVector4D(r, 0.0F));
  m.setColumn(1, QVector4D(u, 0.0F));
  m.setColumn(2, QVector4D(f, 0.0F));
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

auto compose_axis_aligned_unit_local(const QVector3D &origin,
                                     const QVector3D &y_axis) -> QMatrix4x4 {
  const QVector3D y = safe_normalize(y_axis, QVector3D(0.0F, 1.0F, 0.0F));

  QVector3D x = QVector3D::crossProduct(QVector3D(0.0F, 1.0F, 0.0F), y);
  if (x.lengthSquared() <= kAxisEpsilonSq) {
    x = QVector3D::crossProduct(QVector3D(1.0F, 0.0F, 0.0F), y);
  }
  x = safe_normalize(x, QVector3D(1.0F, 0.0F, 0.0F));

  QVector3D z = QVector3D::crossProduct(x, y);
  z = safe_normalize(z, QVector3D(0.0F, 0.0F, 1.0F));

  return compose_basis_unit_local(origin, x, y, z);
}

} // namespace Render::Equipment
