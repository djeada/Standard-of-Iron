#pragma once

#include "render/creature/humanoid/asset/humanoid_body_primitives.h"

namespace Render::Humanoid::humanoid_spec_detail {

constexpr auto make_skeleton_minimal_bone(
    const char* name,
    HumanoidBone anchor,
    HumanoidBone tail,
    float radius,
    Creature::SpanSkinning skinning = Creature::SpanSkinning::LinearBlend) noexcept
    -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(anchor);
  p.params.tail_bone = bone(tail);
  p.params.radius = radius;
  p.span_skinning = skinning;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_minimal;
  return p;
}

constexpr auto make_skeleton_minimal_rib(bool left, bool lower) noexcept
    -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? (lower ? "skeleton_minimal_rib_l_low" : "skeleton_minimal_rib_l_high")
           : (lower ? "skeleton_minimal_rib_r_low" : "skeleton_minimal_rib_r_high");
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  float const side = left ? -1.0F : 1.0F;
  float const height = lower ? -0.10F : 0.12F;
  float const outer = lower ? 0.175F : 0.205F;
  p.params.head_offset = QVector3D(side * 0.022F, height, 0.032F);
  p.params.tail_offset = QVector3D(side * outer, height - 0.014F, 0.008F);
  p.params.radius = HP::NECK_RADIUS * (lower ? 0.29F : 0.33F);
  p.params.depth_radius = HP::NECK_RADIUS * 0.20F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_minimal;
  return p;
}

constexpr auto
make_skeleton_minimal_pelvis(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_minimal_pelvis_l" : "skeleton_minimal_pelvis_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(side * HP::TORSO_BOT_R * 0.34F, -0.02F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::TORSO_BOT_R * 0.32F, HP::TORSO_BOT_R * 0.22F, HP::TORSO_BOT_R * 0.28F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_minimal;
  return p;
}

constexpr auto make_skeleton_minimal_skull() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "skeleton_minimal_skull";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset = QVector3D(0.0F, HP::HEAD_RADIUS * 0.06F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.74F, HP::HEAD_RADIUS * 0.90F, HP::HEAD_RADIUS * 0.82F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_minimal;
  return p;
}

constexpr std::array<Creature::PrimitiveInstance, 18> k_skeleton_minimal_parts = {
    make_skeleton_minimal_bone("skeleton_minimal_spine",
                               HumanoidBone::Pelvis,
                               HumanoidBone::Chest,
                               HP::NECK_RADIUS * 0.48F),
    make_skeleton_minimal_rib(true, false),
    make_skeleton_minimal_rib(false, false),
    make_skeleton_minimal_rib(true, true),
    make_skeleton_minimal_rib(false, true),
    make_skeleton_minimal_pelvis(true),
    make_skeleton_minimal_pelvis(false),
    make_skeleton_minimal_bone("skeleton_minimal_upper_arm_l",
                               HumanoidBone::UpperArmL,
                               HumanoidBone::ForearmL,
                               HP::UPPER_ARM_R * 0.62F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_upper_arm_r",
                               HumanoidBone::UpperArmR,
                               HumanoidBone::ForearmR,
                               HP::UPPER_ARM_R * 0.62F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_forearm_l",
                               HumanoidBone::ForearmL,
                               HumanoidBone::HandL,
                               HP::FORE_ARM_R * 0.58F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_forearm_r",
                               HumanoidBone::ForearmR,
                               HumanoidBone::HandR,
                               HP::FORE_ARM_R * 0.58F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_thigh_l",
                               HumanoidBone::HipL,
                               HumanoidBone::KneeL,
                               HP::UPPER_LEG_R * 0.58F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_thigh_r",
                               HumanoidBone::HipR,
                               HumanoidBone::KneeR,
                               HP::UPPER_LEG_R * 0.58F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_calf_l",
                               HumanoidBone::KneeL,
                               HumanoidBone::FootL,
                               HP::LOWER_LEG_R * 0.58F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_calf_r",
                               HumanoidBone::KneeR,
                               HumanoidBone::FootR,
                               HP::LOWER_LEG_R * 0.58F,
                               Creature::SpanSkinning::AnchorRigid),
    make_skeleton_minimal_bone("skeleton_minimal_neck",
                               HumanoidBone::Neck,
                               HumanoidBone::Head,
                               HP::NECK_RADIUS * 0.24F),
    make_skeleton_minimal_skull(),
    make_skeleton_minimal_bone("skeleton_minimal_shoulder",
                               HumanoidBone::ShoulderL,
                               HumanoidBone::ShoulderR,
                               HP::NECK_RADIUS * 0.34F),
};

constexpr auto make_skeleton_spine() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "skeleton_spine";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  p.params.head_offset = QVector3D(0.0F, 0.07F, -0.015F);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  p.params.tail_offset = QVector3D(0.0F, 0.03F, -0.015F);
  p.params.radius = HP::NECK_RADIUS * 0.42F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_upper_spine() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "skeleton_upper_spine";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  p.params.head_offset = QVector3D(0.0F, -0.12F, -0.03F);
  p.params.tail_bone = bone(HumanoidBone::Neck);
  p.params.tail_offset = QVector3D(0.0F, -0.015F, -0.01F);
  p.params.radius = HP::NECK_RADIUS * 0.28F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_neck() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "skeleton_neck";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Neck);
  p.params.head_offset = QVector3D(0.0F, 0.0F, -0.005F);
  p.params.tail_bone = bone(HumanoidBone::Head);
  p.params.tail_offset = QVector3D(0.0F, -HP::HEAD_RADIUS * 0.48F, -0.005F);
  p.params.radius = HP::NECK_RADIUS * 0.18F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_neck_base() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "skeleton_neck_base";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  p.params.head_offset = QVector3D(0.0F, 0.10F, 0.01F);
  p.params.tail_bone = bone(HumanoidBone::Neck);
  p.params.tail_offset = QVector3D(0.0F, -0.035F, 0.01F);
  p.params.radius = HP::NECK_RADIUS * 0.23F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_collar(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_collar_l" : "skeleton_collar_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(
      side * HP::TORSO_TOP_R * 0.12F, HP::TORSO_TOP_R * 0.28F, HP::TORSO_TOP_R * 0.10F);
  p.params.tail_bone = bone(left ? HumanoidBone::ShoulderL : HumanoidBone::ShoulderR);
  p.params.radius = HP::NECK_RADIUS * 0.26F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_abdomen_bar(const char* name, float y, float z) noexcept
    -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Spine);
  p.params.head_offset = QVector3D(-HP::TORSO_BOT_R * 0.42F, y, z);
  p.params.tail_bone = bone(HumanoidBone::Spine);
  p.params.tail_offset = QVector3D(HP::TORSO_BOT_R * 0.42F, y, z);
  p.params.radius = HP::NECK_RADIUS * 0.20F;
  p.params.depth_radius = HP::NECK_RADIUS * 0.14F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_sternum() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "skeleton_sternum";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  p.params.head_offset = QVector3D(0.0F, 0.19F, 0.055F);
  p.params.tail_bone = bone(HumanoidBone::Spine);
  p.params.tail_offset = QVector3D(0.0F, -0.18F, 0.022F);
  p.params.radius = HP::NECK_RADIUS * 0.24F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_rib(bool left,
                                 int index) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? (index == 0   ? "skeleton_rib_l_high"
                         : index == 1 ? "skeleton_rib_l_upper"
                         : index == 2 ? "skeleton_rib_l_mid"
                         : index == 3 ? "skeleton_rib_l_low"
                                      : "skeleton_rib_l_floating")
                      : (index == 0   ? "skeleton_rib_r_high"
                         : index == 1 ? "skeleton_rib_r_upper"
                         : index == 2 ? "skeleton_rib_r_mid"
                         : index == 3 ? "skeleton_rib_r_low"
                                      : "skeleton_rib_r_floating");
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  float const side = left ? -1.0F : 1.0F;
  auto const idx = static_cast<float>(index);
  float const y = 0.19F - idx * 0.075F;
  float const outer = 0.205F - idx * 0.010F;
  p.params.head_offset = QVector3D(side * 0.028F, y, 0.055F);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  p.params.tail_offset = QVector3D(side * outer, y - 0.012F, 0.020F);
  p.params.radius = HP::NECK_RADIUS * 0.24F;
  p.params.depth_radius = HP::NECK_RADIUS * 0.16F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_skeleton_back_rib(bool left, int index) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? (index == 0   ? "skeleton_back_rib_l_high"
                         : index == 1 ? "skeleton_back_rib_l_upper"
                         : index == 2 ? "skeleton_back_rib_l_mid"
                         : index == 3 ? "skeleton_back_rib_l_low"
                                      : "skeleton_back_rib_l_floating")
                      : (index == 0   ? "skeleton_back_rib_r_high"
                         : index == 1 ? "skeleton_back_rib_r_upper"
                         : index == 2 ? "skeleton_back_rib_r_mid"
                         : index == 3 ? "skeleton_back_rib_r_low"
                                      : "skeleton_back_rib_r_floating");
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  float const side = left ? -1.0F : 1.0F;
  auto const idx = static_cast<float>(index);
  float const y = 0.18F - idx * 0.075F;
  float const outer = 0.195F - idx * 0.010F;
  p.params.head_offset = QVector3D(side * 0.026F, y, -0.050F);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  p.params.tail_offset = QVector3D(side * outer, y - 0.010F, -0.018F);
  p.params.radius = HP::NECK_RADIUS * 0.22F;
  p.params.depth_radius = HP::NECK_RADIUS * 0.15F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_pelvis(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_pelvis_l" : "skeleton_pelvis_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset =
      QVector3D(side * HP::TORSO_BOT_R * 0.38F, -0.02F, -HP::TORSO_BOT_R * 0.04F);
  p.params.half_extents = QVector3D(
      HP::TORSO_BOT_R * 0.28F, HP::TORSO_BOT_R * 0.20F, HP::TORSO_BOT_R * 0.32F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_skeleton_limb(const char* name,
                   HumanoidBone anchor,
                   HumanoidBone tail,
                   float radius) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(anchor);
  p.params.tail_bone = bone(tail);
  p.params.radius = radius;
  p.span_skinning = Creature::SpanSkinning::AnchorRigid;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_skeleton_joint(const char* name,
                    HumanoidBone anchor,
                    float radius) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(anchor);
  p.params.radius = radius;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_hand(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p = make_full_hand(left);
  p.debug_name = left ? "skeleton_hand_l" : "skeleton_hand_r";
  p.params.half_extents = QVector3D(
      HP::HAND_RADIUS * 0.80F, HP::HAND_RADIUS * 1.05F, HP::HAND_RADIUS * 0.34F);
  p.color_role = Skin;
  return p;
}

constexpr auto make_skeleton_foot(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p = make_full_foot(left);
  p.debug_name = left ? "skeleton_foot_l" : "skeleton_foot_r";
  p.params.half_extents = QVector3D(
      HP::LOWER_LEG_R * 0.50F, HP::LOWER_LEG_R * 0.18F, HP::LOWER_LEG_R * 1.20F);
  p.color_role = Skin;
  return p;
}

constexpr auto make_skeleton_skull() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p = make_full_cranium();
  p.debug_name = "skeleton_skull";
  p.params.head_offset = QVector3D(0.0F, HP::HEAD_RADIUS * 0.10F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.74F, HP::HEAD_RADIUS * 0.90F, HP::HEAD_RADIUS * 0.82F);
  p.color_role = Skin;
  return p;
}

constexpr auto make_skeleton_jaw() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p = make_full_jaw();
  p.debug_name = "skeleton_jaw";
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 1.42F, HP::HEAD_RADIUS * 0.50F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.48F, HP::HEAD_RADIUS * 0.25F, HP::HEAD_RADIUS * 0.48F);
  p.color_role = Skin;
  return p;
}

constexpr auto make_skeleton_eye(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_eye_socket_l" : "skeleton_eye_socket_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(side * HP::HEAD_RADIUS * 0.52F,
                                   -HP::HEAD_RADIUS * 0.42F,
                                   HP::HEAD_RADIUS * 1.32F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.19F, HP::HEAD_RADIUS * 0.20F, HP::HEAD_RADIUS * 0.10F);
  p.color = QVector3D(0.030F, 0.034F, 0.040F);
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_skeleton_eye_ember(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_eye_ember_l" : "skeleton_eye_ember_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(side * HP::HEAD_RADIUS * 0.52F,
                                   -HP::HEAD_RADIUS * 0.45F,
                                   HP::HEAD_RADIUS * 1.47F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.045F, HP::HEAD_RADIUS * 0.045F, HP::HEAD_RADIUS * 0.035F);
  p.color = QVector3D(0.55F, 1.0F, 0.82F);
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_face_cavity(const char* name,
                                         const QVector3D& offset,
                                         const QVector3D& extents) noexcept
    -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset = offset;
  p.params.half_extents = extents;
  p.color = QVector3D(0.035F, 0.038F, 0.040F);
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_skeleton_cheek(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_cheek_l" : "skeleton_cheek_r";
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.tail_bone = bone(HumanoidBone::Head);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(side * HP::HEAD_RADIUS * 0.34F,
                                   -HP::HEAD_RADIUS * 0.84F,
                                   HP::HEAD_RADIUS * 1.36F);
  p.params.tail_offset = QVector3D(side * HP::HEAD_RADIUS * 1.02F,
                                   -HP::HEAD_RADIUS * 0.98F,
                                   HP::HEAD_RADIUS * 0.90F);
  p.params.radius = HP::HEAD_RADIUS * 0.10F;
  p.params.depth_radius = HP::HEAD_RADIUS * 0.08F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr std::array<Creature::PrimitiveInstance, 62> k_skeleton_full_parts = {
    make_skeleton_spine(),
    make_skeleton_upper_spine(),
    make_skeleton_neck_base(),
    make_skeleton_collar(true),
    make_skeleton_collar(false),
    make_skeleton_sternum(),
    make_skeleton_abdomen_bar(
        "skeleton_abdomen_bar_high", HP::TORSO_TOP_R * 0.02F, HP::TORSO_TOP_R * 0.22F),
    make_skeleton_abdomen_bar(
        "skeleton_abdomen_bar_low", -HP::TORSO_TOP_R * 0.20F, HP::TORSO_TOP_R * 0.18F),
    make_skeleton_rib(true, 0),
    make_skeleton_rib(false, 0),
    make_skeleton_rib(true, 1),
    make_skeleton_rib(false, 1),
    make_skeleton_rib(true, 2),
    make_skeleton_rib(false, 2),
    make_skeleton_rib(true, 3),
    make_skeleton_rib(false, 3),
    make_skeleton_rib(true, 4),
    make_skeleton_rib(false, 4),
    make_skeleton_back_rib(true, 0),
    make_skeleton_back_rib(false, 0),
    make_skeleton_back_rib(true, 1),
    make_skeleton_back_rib(false, 1),
    make_skeleton_back_rib(true, 2),
    make_skeleton_back_rib(false, 2),
    make_skeleton_back_rib(true, 3),
    make_skeleton_back_rib(false, 3),
    make_skeleton_back_rib(true, 4),
    make_skeleton_back_rib(false, 4),
    make_skeleton_pelvis(true),
    make_skeleton_pelvis(false),
    make_skeleton_limb("skeleton_upper_arm_l",
                       HumanoidBone::UpperArmL,
                       HumanoidBone::ForearmL,
                       HP::UPPER_ARM_R * 0.62F),
    make_skeleton_limb("skeleton_upper_arm_r",
                       HumanoidBone::UpperArmR,
                       HumanoidBone::ForearmR,
                       HP::UPPER_ARM_R * 0.62F),
    make_skeleton_limb("skeleton_forearm_l",
                       HumanoidBone::ForearmL,
                       HumanoidBone::HandL,
                       HP::FORE_ARM_R * 0.58F),
    make_skeleton_limb("skeleton_forearm_r",
                       HumanoidBone::ForearmR,
                       HumanoidBone::HandR,
                       HP::FORE_ARM_R * 0.58F),
    make_skeleton_joint(
        "skeleton_shoulder_l", HumanoidBone::ShoulderL, HP::UPPER_ARM_R * 0.68F),
    make_skeleton_joint(
        "skeleton_shoulder_r", HumanoidBone::ShoulderR, HP::UPPER_ARM_R * 0.68F),
    make_skeleton_joint(
        "skeleton_elbow_l", HumanoidBone::ForearmL, HP::FORE_ARM_R * 0.62F),
    make_skeleton_joint(
        "skeleton_elbow_r", HumanoidBone::ForearmR, HP::FORE_ARM_R * 0.62F),
    make_skeleton_hand(true),
    make_skeleton_hand(false),
    make_skeleton_limb("skeleton_thigh_l",
                       HumanoidBone::HipL,
                       HumanoidBone::KneeL,
                       HP::UPPER_LEG_R * 0.56F),
    make_skeleton_limb("skeleton_thigh_r",
                       HumanoidBone::HipR,
                       HumanoidBone::KneeR,
                       HP::UPPER_LEG_R * 0.56F),
    make_skeleton_limb("skeleton_calf_l",
                       HumanoidBone::KneeL,
                       HumanoidBone::FootL,
                       HP::LOWER_LEG_R * 0.56F),
    make_skeleton_limb("skeleton_calf_r",
                       HumanoidBone::KneeR,
                       HumanoidBone::FootR,
                       HP::LOWER_LEG_R * 0.56F),
    make_skeleton_joint("skeleton_hip_l", HumanoidBone::HipL, HP::UPPER_LEG_R * 0.58F),
    make_skeleton_joint("skeleton_hip_r", HumanoidBone::HipR, HP::UPPER_LEG_R * 0.58F),
    make_skeleton_joint(
        "skeleton_knee_l", HumanoidBone::KneeL, HP::LOWER_LEG_R * 0.56F),
    make_skeleton_joint(
        "skeleton_knee_r", HumanoidBone::KneeR, HP::LOWER_LEG_R * 0.56F),
    make_skeleton_foot(true),
    make_skeleton_foot(false),
    make_skeleton_neck(),
    make_skeleton_skull(),
    make_skeleton_jaw(),
    make_full_brow(),
    make_skeleton_eye(true),
    make_skeleton_eye(false),
    make_skeleton_face_cavity(
        "skeleton_nasal_cavity",
        QVector3D(0.0F, -HP::HEAD_RADIUS * 0.86F, HP::HEAD_RADIUS * 1.31F),
        QVector3D(
            HP::HEAD_RADIUS * 0.07F, HP::HEAD_RADIUS * 0.12F, HP::HEAD_RADIUS * 0.07F)),
    make_skeleton_face_cavity(
        "skeleton_mouth_cavity",
        QVector3D(0.0F, -HP::HEAD_RADIUS * 1.24F, HP::HEAD_RADIUS * 1.30F),
        QVector3D(HP::HEAD_RADIUS * 0.30F,
                  HP::HEAD_RADIUS * 0.035F,
                  HP::HEAD_RADIUS * 0.06F)),
    make_skeleton_cheek(true),
    make_skeleton_cheek(false),
    make_skeleton_eye_ember(true),
    make_skeleton_eye_ember(false),
};

} // namespace Render::Humanoid::humanoid_spec_detail
