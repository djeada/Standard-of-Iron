

#pragma once

#include "../creature/skeleton.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Render::Humanoid {

enum class HumanoidBone : std::uint8_t {
  Root = 0,
  Pelvis,
  Spine,
  Chest,
  Neck,
  Head,
  ShoulderL,
  UpperArmL,
  ForearmL,
  HandL,
  ShoulderR,
  UpperArmR,
  ForearmR,
  HandR,
  HipL,
  KneeL,
  FootL,
  HipR,
  KneeR,
  FootR,
  Count
};

inline constexpr std::size_t k_bone_count =
    static_cast<std::size_t>(HumanoidBone::Count);

inline constexpr std::uint8_t k_invalid_bone = 0xFF;

inline constexpr std::array<std::uint8_t, k_bone_count> k_bone_parents = {
    k_invalid_bone,
    static_cast<std::uint8_t>(HumanoidBone::Root),
    static_cast<std::uint8_t>(HumanoidBone::Pelvis),
    static_cast<std::uint8_t>(HumanoidBone::Spine),
    static_cast<std::uint8_t>(HumanoidBone::Chest),
    static_cast<std::uint8_t>(HumanoidBone::Neck),
    static_cast<std::uint8_t>(HumanoidBone::Chest),
    static_cast<std::uint8_t>(HumanoidBone::ShoulderL),
    static_cast<std::uint8_t>(HumanoidBone::UpperArmL),
    static_cast<std::uint8_t>(HumanoidBone::ForearmL),
    static_cast<std::uint8_t>(HumanoidBone::Chest),
    static_cast<std::uint8_t>(HumanoidBone::ShoulderR),
    static_cast<std::uint8_t>(HumanoidBone::UpperArmR),
    static_cast<std::uint8_t>(HumanoidBone::ForearmR),
    static_cast<std::uint8_t>(HumanoidBone::Pelvis),
    static_cast<std::uint8_t>(HumanoidBone::HipL),
    static_cast<std::uint8_t>(HumanoidBone::KneeL),
    static_cast<std::uint8_t>(HumanoidBone::Pelvis),
    static_cast<std::uint8_t>(HumanoidBone::HipR),
    static_cast<std::uint8_t>(HumanoidBone::KneeR),
};

[[nodiscard]] auto bone_name(HumanoidBone bone) noexcept -> std::string_view;
[[nodiscard]] auto parent_of(HumanoidBone bone) noexcept -> std::uint8_t;

[[nodiscard]] auto
humanoid_topology() noexcept -> const Render::Creature::SkeletonTopology &;

enum class HumanoidSocket : std::uint8_t {
  Head = 0,
  HandR,
  HandL,
  GripR,
  GripL,
  Back,
  HipL,
  HipR,
  ChestFront,
  ChestBack,
  FootL,
  FootR,
  Count
};

inline constexpr std::size_t k_socket_count =
    static_cast<std::size_t>(HumanoidSocket::Count);

using SocketDef = Render::Creature::SocketDef;

[[nodiscard]] auto
socket_def(HumanoidSocket socket) noexcept -> const SocketDef &;

[[nodiscard]] auto socket_bone(HumanoidSocket socket) noexcept -> HumanoidBone;

using BonePalette = std::array<QMatrix4x4, k_bone_count>;

void evaluate_skeleton(const Render::GL::HumanoidPose &pose,
                       const QVector3D &right_axis,
                       BonePalette &out_palette) noexcept;

[[nodiscard]] auto
socket_transform(const BonePalette &palette,
                 HumanoidSocket socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto
socket_transform(const Render::GL::AttachmentFrame &bone_frame,
                 HumanoidSocket socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto socket_position(const BonePalette &palette,
                                   HumanoidSocket socket) noexcept -> QVector3D;

[[nodiscard]] auto socket_attachment_frame(const BonePalette &palette,
                                           HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame;

[[nodiscard]] auto socket_attachment_frame(
    const Render::GL::AttachmentFrame &bone_frame,
    HumanoidSocket socket) noexcept -> Render::GL::AttachmentFrame;

[[nodiscard]] auto
bind_socket_transform(HumanoidSocket socket) noexcept -> QMatrix4x4;

[[nodiscard]] auto bind_socket_attachment_frame(HumanoidSocket socket) noexcept
    -> Render::GL::AttachmentFrame;

} // namespace Render::Humanoid
