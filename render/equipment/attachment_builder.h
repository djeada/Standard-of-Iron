

#pragma once

#include "../static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <span>

namespace Render::Creature {
struct StaticAttachmentSpec;
}

namespace Render::GL {
struct RenderArchetype;
}

namespace Render::Equipment {

struct AttachmentBuildInput {

  const Render::GL::RenderArchetype *archetype{nullptr};

  std::uint16_t socket_bone_index{0};

  QMatrix4x4 unit_local_pose_at_bind{};

  float uniform_scale{1.0F};

  std::span<const std::uint8_t> palette_role_remap{};

  std::uint8_t override_color_role{0};

  std::uint32_t material_id{0};

  QVector3D authored_local_offset{0.0F, 0.0F, 0.0F};
  float bind_radius{0.0F};
  QMatrix4x4 bind_socket_transform{};
};

struct SocketAttachmentBuildInput {

  const Render::GL::RenderArchetype *archetype{nullptr};

  std::uint16_t socket_bone_index{0};

  QMatrix4x4 bind_bone_transform{};
  QMatrix4x4 bind_socket_transform{};
  QMatrix4x4 mesh_from_socket{};

  float uniform_scale{1.0F};

  std::span<const std::uint8_t> palette_role_remap{};

  std::uint8_t override_color_role{0};

  std::uint32_t material_id{0};
};

[[nodiscard]] auto build_static_attachment(const AttachmentBuildInput &in)
    -> Render::Creature::StaticAttachmentSpec;

[[nodiscard]] auto
build_socket_static_attachment(const SocketAttachmentBuildInput &in)
    -> Render::Creature::StaticAttachmentSpec;

[[nodiscard]] auto
compose_basis_unit_local(const QVector3D &origin, const QVector3D &right,
                         const QVector3D &up,
                         const QVector3D &forward) -> QMatrix4x4;

[[nodiscard]] auto
compose_axis_aligned_unit_local(const QVector3D &origin,
                                const QVector3D &y_axis) -> QMatrix4x4;

} // namespace Render::Equipment
