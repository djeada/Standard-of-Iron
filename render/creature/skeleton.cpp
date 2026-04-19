#include "skeleton.h"

#include <QVector3D>
#include <cassert>
#include <cmath>

namespace Render::Creature {

namespace {

constexpr float kAxisEpsilonSq = 1.0e-6F;
constexpr float kLengthEpsilon = 1.0e-5F;

auto normalised_or(const QVector3D &v,
                   const QVector3D &fallback) noexcept -> QVector3D {
  float const len = v.length();
  if (len < kLengthEpsilon) {
    return fallback;
  }
  return v / len;
}

auto orthonormalise_x(const QVector3D &right_hint, const QVector3D &y_axis,
                      const QVector3D &world_fallback) noexcept -> QVector3D {
  QVector3D x = right_hint - y_axis * QVector3D::dotProduct(right_hint, y_axis);
  if (x.lengthSquared() < kAxisEpsilonSq) {
    x = world_fallback - y_axis * QVector3D::dotProduct(world_fallback, y_axis);
    if (x.lengthSquared() < kAxisEpsilonSq) {
      QVector3D const alt(0.0F, 0.0F, 1.0F);
      x = alt - y_axis * QVector3D::dotProduct(alt, y_axis);
    }
  }
  x.normalize();
  return x;
}

} // namespace

auto make_bone_basis(const QVector3D &head, const QVector3D &tail,
                     const QVector3D &right_hint) noexcept -> QMatrix4x4 {
  QVector3D const y_axis =
      normalised_or(tail - head, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const x_axis =
      orthonormalise_x(right_hint, y_axis, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const z_axis = QVector3D::crossProduct(x_axis, y_axis).normalized();

  QMatrix4x4 m;
  m.setColumn(0, QVector4D(x_axis, 0.0F));
  m.setColumn(1, QVector4D(y_axis, 0.0F));
  m.setColumn(2, QVector4D(z_axis, 0.0F));
  m.setColumn(3, QVector4D(head, 1.0F));
  return m;
}

auto basis_from_parent(const QMatrix4x4 &parent,
                       const QVector3D &origin) noexcept -> QMatrix4x4 {
  QMatrix4x4 m = parent;
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

auto basis_from_root_up(const QVector3D &origin,
                        const QVector3D &right_hint) noexcept -> QMatrix4x4 {
  QVector3D const y_axis(0.0F, 1.0F, 0.0F);
  QVector3D const x_axis =
      orthonormalise_x(right_hint, y_axis, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const z_axis = QVector3D::crossProduct(x_axis, y_axis).normalized();

  QMatrix4x4 m;
  m.setColumn(0, QVector4D(x_axis, 0.0F));
  m.setColumn(1, QVector4D(y_axis, 0.0F));
  m.setColumn(2, QVector4D(z_axis, 0.0F));
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

void evaluate_skeleton(const SkeletonTopology &topo, JointProviderFn provider,
                       void *user, const QVector3D &right_axis,
                       std::span<QMatrix4x4> out_palette) noexcept {
  assert(provider != nullptr);
  assert(out_palette.size() >= topo.bones.size());

  QVector3D right = right_axis;
  if (right.lengthSquared() < kAxisEpsilonSq) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right.normalize();
  }

  std::size_t const n = topo.bones.size();
  for (std::size_t i = 0; i < n; ++i) {
    BoneResolution const r = provider(user, static_cast<BoneIndex>(i));

    switch (r.kind) {
    case BoneBasisKind::FromRootUp:
      out_palette[i] = basis_from_root_up(r.head, right);
      break;

    case BoneBasisKind::FromHeadTail: {

      if ((r.tail - r.head).lengthSquared() < kAxisEpsilonSq) {
        BoneIndex const p = topo.bones[i].parent;
        if (p != kInvalidBone && p < i) {
          out_palette[i] = basis_from_parent(out_palette[p], r.head);
        } else {
          out_palette[i] = basis_from_root_up(r.head, right);
        }
      } else {
        out_palette[i] = make_bone_basis(r.head, r.tail, right);
      }
      break;
    }

    case BoneBasisKind::FromParent:
    default: {
      BoneIndex const p = topo.bones[i].parent;
      if (p != kInvalidBone && p < i) {
        out_palette[i] = basis_from_parent(out_palette[p], r.head);
      } else {
        out_palette[i] = basis_from_root_up(r.head, right);
      }
      break;
    }
    }
  }
}

auto socket_transform(const SkeletonTopology &topo,
                      std::span<const QMatrix4x4> palette,
                      SocketIndex socket) noexcept -> QMatrix4x4 {
  if (socket >= topo.sockets.size()) {
    return QMatrix4x4{};
  }
  SocketDef const &def = topo.sockets[socket];
  if (def.bone == kInvalidBone || def.bone >= palette.size()) {
    return QMatrix4x4{};
  }
  QMatrix4x4 const &bone = palette[def.bone];
  if (def.local_offset.isNull()) {
    return bone;
  }
  QMatrix4x4 result = bone;
  QVector3D const world_offset =
      bone.column(0).toVector3D() * def.local_offset.x() +
      bone.column(1).toVector3D() * def.local_offset.y() +
      bone.column(2).toVector3D() * def.local_offset.z();
  QVector3D const origin = bone.column(3).toVector3D() + world_offset;
  result.setColumn(3, QVector4D(origin, 1.0F));
  return result;
}

auto socket_position(const SkeletonTopology &topo,
                     std::span<const QMatrix4x4> palette,
                     SocketIndex socket) noexcept -> QVector3D {
  return socket_transform(topo, palette, socket).column(3).toVector3D();
}

auto socket_attachment_frame(
    const SkeletonTopology &topo, std::span<const QMatrix4x4> palette,
    SocketIndex socket) noexcept -> Render::GL::AttachmentFrame {
  QMatrix4x4 const m = socket_transform(topo, palette, socket);
  Render::GL::AttachmentFrame f;
  f.origin = m.column(3).toVector3D();
  f.right = m.column(0).toVector3D();
  f.up = m.column(1).toVector3D();
  f.forward = m.column(2).toVector3D();
  return f;
}

auto find_bone(const SkeletonTopology &topo,
               std::string_view name) noexcept -> BoneIndex {
  for (std::size_t i = 0; i < topo.bones.size(); ++i) {
    if (topo.bones[i].name == name) {
      return static_cast<BoneIndex>(i);
    }
  }
  return kInvalidBone;
}

auto find_socket(const SkeletonTopology &topo,
                 std::string_view name) noexcept -> SocketIndex {
  for (std::size_t i = 0; i < topo.sockets.size(); ++i) {
    if (topo.sockets[i].name == name) {
      return static_cast<SocketIndex>(i);
    }
  }
  return kInvalidSocket;
}

auto validate_topology(const SkeletonTopology &topo) noexcept -> bool {
  bool has_root = false;
  for (std::size_t i = 0; i < topo.bones.size(); ++i) {
    BoneDef const &b = topo.bones[i];
    if (b.parent == kInvalidBone) {
      if (has_root) {
        return false;
      }
      has_root = true;
    } else if (b.parent >= i) {
      return false;
    }
    for (std::size_t j = i + 1; j < topo.bones.size(); ++j) {
      if (topo.bones[j].name == b.name) {
        return false;
      }
    }
  }
  if (!has_root && !topo.bones.empty()) {
    return false;
  }
  for (SocketDef const &s : topo.sockets) {
    if (s.bone == kInvalidBone || s.bone >= topo.bones.size()) {
      return false;
    }
  }
  return true;
}

} // namespace Render::Creature
