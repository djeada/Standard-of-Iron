

#pragma once

#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Render::Creature {

using BoneIndex = std::uint16_t;
using SocketIndex = std::uint16_t;

inline constexpr BoneIndex kInvalidBone = static_cast<BoneIndex>(0xFFFFu);
inline constexpr SocketIndex kInvalidSocket = static_cast<SocketIndex>(0xFFFFu);

enum class BoneBasisKind : std::uint8_t {
  FromHeadTail = 0,
  FromParent = 1,
  FromRootUp = 2,
};

struct BoneResolution {
  BoneBasisKind kind{BoneBasisKind::FromParent};
  QVector3D head{};
  QVector3D tail{};
};

using JointProviderFn = BoneResolution (*)(void *user, BoneIndex bone);

struct BoneDef {
  std::string_view name;
  BoneIndex parent{kInvalidBone};
};

struct SocketDef {
  std::string_view name;
  BoneIndex bone{kInvalidBone};

  QVector3D local_offset{};
  QVector3D local_right{1.0F, 0.0F, 0.0F};
  QVector3D local_up{0.0F, 1.0F, 0.0F};
  QVector3D local_forward{0.0F, 0.0F, 1.0F};
};

struct SkeletonTopology {
  std::span<const BoneDef> bones;
  std::span<const SocketDef> sockets;
};

void evaluate_skeleton(const SkeletonTopology &topo, JointProviderFn provider,
                       void *user, const QVector3D &right_axis,
                       std::span<QMatrix4x4> out_palette) noexcept;

[[nodiscard]] auto socket_transform(const SkeletonTopology &topo,
                                    std::span<const QMatrix4x4> palette,
                                    SocketIndex socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto
socket_transform(const QMatrix4x4 &bone_transform,
                 const SocketDef &socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto socket_position(const SkeletonTopology &topo,
                                   std::span<const QMatrix4x4> palette,
                                   SocketIndex socket) noexcept -> QVector3D;

[[nodiscard]] auto socket_attachment_frame(
    const SkeletonTopology &topo, std::span<const QMatrix4x4> palette,
    SocketIndex socket) noexcept -> Render::GL::AttachmentFrame;

[[nodiscard]] auto socket_attachment_frame(
    const Render::GL::AttachmentFrame &bone_frame,
    const SocketDef &socket) noexcept -> Render::GL::AttachmentFrame;

[[nodiscard]] auto find_bone(const SkeletonTopology &topo,
                             std::string_view name) noexcept -> BoneIndex;
[[nodiscard]] auto find_socket(const SkeletonTopology &topo,
                               std::string_view name) noexcept -> SocketIndex;

[[nodiscard]] auto
make_bone_basis(const QVector3D &head, const QVector3D &tail,
                const QVector3D &right_hint) noexcept -> QMatrix4x4;

[[nodiscard]] auto
basis_from_parent(const QMatrix4x4 &parent,
                  const QVector3D &origin) noexcept -> QMatrix4x4;

[[nodiscard]] auto
basis_from_root_up(const QVector3D &origin,
                   const QVector3D &right_hint) noexcept -> QMatrix4x4;

[[nodiscard]] auto
validate_topology(const SkeletonTopology &topo) noexcept -> bool;

} // namespace Render::Creature
