#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/creature/gpu/rigged_mesh_cache.h"
#include "render/creature/humanoid/asset/humanoid_beard_mesh.h"
#include "render/creature/humanoid/asset/humanoid_face_mesh.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/runtime/body_frame_resolver.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/skeleton_evaluator.h"
#include "render/creature/spec.h"
#include "render/frame/bone_palette_arena.h"
#include "render/frame/submitter.h"
#include "render/geom/garment_shell.h"
#include "render/geom/mesh.h"
#include "render/geom/primitives.h"

namespace Render::Humanoid::humanoid_spec_detail {

namespace Creature = Render::Creature;
using HP = Render::GL::HumanProportions;

enum HumanoidColorRole : std::uint8_t {
  Cloth = 1,
  Skin = 2,
  Leather = 3,
  LeatherDark = 4,
  Wood = 5,
  Metal = 6,
  ClothDark = 7,
  Hair = 8,
};

static_assert(ClothDark == k_humanoid_cloth_dark_role);
static_assert(Hair == k_humanoid_hair_role);
static_assert(LeatherDark == k_humanoid_leather_dark_role);
static_assert(Wood == k_humanoid_wood_role);
static_assert(Metal == k_humanoid_metal_role);

constexpr auto bone(HumanoidBone b) noexcept -> Creature::BoneIndex {
  return static_cast<Creature::BoneIndex>(b);
}

constexpr auto make_full_chest() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_chest";
  p.shape = Creature::PrimitiveShape::Mesh;
  p.params.anchor_bone = bone(HumanoidBone::Spine);
  p.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
  p.mesh_skinning = Creature::MeshSkinning::Authored;
  p.color_role = Cloth;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_pelvis_block() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_pelvis_block";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  p.params.head_offset = QVector3D(0.0F, 0.00F, -HP::TORSO_BOT_R * 0.02F);
  p.params.half_extents = QVector3D(
      HP::TORSO_BOT_R * 0.96F, HP::TORSO_BOT_R * 0.34F, HP::TORSO_BOT_R * 0.56F);
  p.color_role = ClothDark;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_buttock(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_buttock_l" : "humanoid_full_buttock_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  float const x = left ? -HP::TORSO_BOT_R * 0.42F : HP::TORSO_BOT_R * 0.42F;
  p.params.head_offset = QVector3D(x, -0.02F, -HP::TORSO_BOT_R * 0.34F);
  p.params.half_extents = QVector3D(
      HP::TORSO_BOT_R * 0.30F, HP::TORSO_BOT_R * 0.27F, HP::TORSO_BOT_R * 0.24F);
  p.color_role = ClothDark;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_deltoid(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_deltoid_l" : "humanoid_full_deltoid_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::ShoulderL : HumanoidBone::ShoulderR);
  p.params.head_offset = QVector3D(0.0F, 0.0F, 0.0F);

  float const r = HP::UPPER_ARM_R * 1.04F;
  p.params.half_extents = QVector3D(r, r * 0.84F, r * 0.96F);
  p.color_role = Cloth;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_full_deltoid_cap(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_deltoid_cap_l" : "humanoid_full_deltoid_cap_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR);
  p.params.head_offset = QVector3D(0.0F, HP::UPPER_ARM_LEN * 0.17F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::UPPER_ARM_R * 1.02F, HP::UPPER_ARM_R * 1.20F, HP::UPPER_ARM_R * 0.96F);
  p.color_role = Cloth;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr float k_upper_arm_half = HP::UPPER_ARM_LEN * 0.5F;
constexpr auto
make_limb_segment(std::string_view name,
                  HumanoidBone segment_bone,
                  float from_along,
                  float to_along,
                  float head_radius,
                  float tail_radius,
                  std::uint8_t role) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::TaperedCylinder;
  auto const b = bone(segment_bone);
  p.params.anchor_bone = b;
  p.params.tail_bone = b;
  p.params.head_offset = QVector3D(0.0F, from_along, 0.0F);
  p.params.tail_offset = QVector3D(0.0F, to_along, 0.0F);
  p.span_skinning = Creature::SpanSkinning::AnchorRigid;
  p.params.radius = head_radius;
  p.params.tail_radius = tail_radius;
  p.color_role = role;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_limb_end_segment(std::string_view name,
                      HumanoidBone segment_bone,
                      HumanoidBone joint_bone,
                      float from_along,
                      float head_radius,
                      float tail_radius,
                      std::uint8_t role) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::TaperedCylinder;
  p.params.anchor_bone = bone(segment_bone);
  p.params.tail_bone = bone(joint_bone);
  p.params.head_offset = QVector3D(0.0F, from_along, 0.0F);

  p.span_skinning = Creature::SpanSkinning::AnchorRigid;
  p.params.radius = head_radius;
  p.params.tail_radius = tail_radius;
  p.color_role = role;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_full_upper_arm_proximal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_segment(left ? "humanoid_full_upper_arm_l_top"
                                : "humanoid_full_upper_arm_r_top",
                           left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR,
                           0.0F,
                           k_upper_arm_half,
                           HP::UPPER_ARM_R * 1.28F,
                           HP::UPPER_ARM_R * 1.06F,
                           Cloth);
}

constexpr auto
make_full_upper_arm_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_end_segment(left ? "humanoid_full_upper_arm_l_bot"
                                    : "humanoid_full_upper_arm_r_bot",
                               left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR,
                               left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
                               k_upper_arm_half,
                               HP::UPPER_ARM_R * 1.06F,
                               HP::UPPER_ARM_R * 0.90F,
                               Cloth);
}

constexpr auto make_full_elbow(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_elbow_l" : "humanoid_full_elbow_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.radius = HP::UPPER_ARM_R * 0.98F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_full_forearm_proximal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_segment(left ? "humanoid_full_forearm_l_top"
                                : "humanoid_full_forearm_r_top",
                           left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
                           0.0F,
                           HP::FORE_ARM_LEN * 0.28F,
                           HP::FORE_ARM_R * 1.18F,
                           HP::FORE_ARM_R * 1.34F,
                           Skin);
}

constexpr auto
make_full_forearm_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_end_segment(left ? "humanoid_full_forearm_l_bot"
                                    : "humanoid_full_forearm_r_bot",
                               left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
                               left ? HumanoidBone::HandL : HumanoidBone::HandR,
                               HP::FORE_ARM_LEN * 0.28F,
                               HP::FORE_ARM_R * 1.34F,
                               HP::FORE_ARM_R * 0.86F,
                               Skin);
}

constexpr auto make_full_hand(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_hand_l" : "humanoid_full_hand_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.head_offset = QVector3D(0.0F, HP::HAND_RADIUS * 0.86F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::HAND_RADIUS * 1.22F, HP::HAND_RADIUS * 1.65F, HP::HAND_RADIUS * 0.83F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_neck() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_neck";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(HumanoidBone::Neck);
  p.params.tail_bone = bone(HumanoidBone::Head);
  p.params.radius = HP::NECK_RADIUS * 0.88F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_cranium() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_cranium";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset = QVector3D(0.0F, HP::HEAD_RADIUS * 0.06F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.80F, HP::HEAD_RADIUS * 0.98F, HP::HEAD_RADIUS * 0.88F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_jaw() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_jaw";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 1.22F, HP::HEAD_RADIUS * 0.42F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.53F, HP::HEAD_RADIUS * 0.40F, HP::HEAD_RADIUS * 0.51F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_nose() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_nose";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.93F, HP::HEAD_RADIUS * 1.50F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.085F, HP::HEAD_RADIUS * 0.115F, HP::HEAD_RADIUS * 0.12F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_ear(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_ear_l" : "humanoid_full_ear_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(side * HP::HEAD_RADIUS * 1.50F,
                                   -HP::HEAD_RADIUS * 0.74F,
                                   -HP::HEAD_RADIUS * 0.06F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.07F, HP::HEAD_RADIUS * 0.15F, HP::HEAD_RADIUS * 0.10F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_brow() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_brow";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.24F, HP::HEAD_RADIUS * 1.37F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.52F, HP::HEAD_RADIUS * 0.11F, HP::HEAD_RADIUS * 0.21F);
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr float k_upper_leg_half = HP::UPPER_LEG_LEN * 0.5F;
constexpr auto
make_full_thigh_proximal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_segment(left ? "humanoid_full_thigh_l_top"
                                : "humanoid_full_thigh_r_top",
                           left ? HumanoidBone::HipL : HumanoidBone::HipR,
                           0.0F,
                           k_upper_leg_half,
                           HP::UPPER_LEG_R * 1.34F,
                           HP::UPPER_LEG_R * 1.12F,
                           ClothDark);
}

constexpr auto
make_full_thigh_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_end_segment(left ? "humanoid_full_thigh_l_bot"
                                    : "humanoid_full_thigh_r_bot",
                               left ? HumanoidBone::HipL : HumanoidBone::HipR,
                               left ? HumanoidBone::KneeL : HumanoidBone::KneeR,
                               k_upper_leg_half,
                               HP::UPPER_LEG_R * 1.12F,
                               HP::UPPER_LEG_R * 0.96F,
                               ClothDark);
}

constexpr auto make_full_knee(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_knee_l" : "humanoid_full_knee_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.radius = HP::LOWER_LEG_R * 1.50F;
  p.color_role = Leather;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
make_full_calf_proximal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_segment(left ? "humanoid_full_calf_l_top"
                                : "humanoid_full_calf_r_top",
                           left ? HumanoidBone::KneeL : HumanoidBone::KneeR,
                           0.0F,
                           HP::LOWER_LEG_LEN * 0.30F,
                           HP::LOWER_LEG_R * 1.30F,
                           HP::LOWER_LEG_R * 1.46F,
                           Skin);
}

constexpr auto
make_full_calf_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_end_segment(left ? "humanoid_full_calf_l_bot"
                                    : "humanoid_full_calf_r_bot",
                               left ? HumanoidBone::KneeL : HumanoidBone::KneeR,
                               left ? HumanoidBone::FootL : HumanoidBone::FootR,
                               HP::LOWER_LEG_LEN * 0.30F,
                               HP::LOWER_LEG_R * 1.46F,
                               HP::LOWER_LEG_R * 0.78F,
                               Skin);
}

constexpr auto make_full_ankle(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_ankle_l" : "humanoid_full_ankle_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.radius = HP::LOWER_LEG_R * 0.80F;
  p.color_role = Leather;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_foot(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_foot_l" : "humanoid_full_foot_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.head_offset =
      QVector3D(0.0F, HP::LOWER_LEG_R * 0.18F, HP::LOWER_LEG_R * 1.20F);
  p.params.half_extents = QVector3D(
      HP::LOWER_LEG_R * 0.78F, HP::LOWER_LEG_R * 0.28F, HP::LOWER_LEG_R * 1.55F);
  p.color_role = LeatherDark;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto
as_minimal(Creature::PrimitiveInstance p) noexcept -> Creature::PrimitiveInstance {
  p.lod_mask = Creature::k_lod_minimal;
  return p;
}

constexpr auto
make_minimal_upper_arm(bool left) noexcept -> Creature::PrimitiveInstance {
  return as_minimal(make_limb_end_segment(
      left ? "humanoid_minimal_upper_arm_l" : "humanoid_minimal_upper_arm_r",
      left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR,
      left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
      0.0F,
      HP::UPPER_ARM_R * 1.28F,
      HP::UPPER_ARM_R * 0.90F,
      Cloth));
}

constexpr auto make_minimal_forearm(bool left) noexcept -> Creature::PrimitiveInstance {
  return as_minimal(make_limb_end_segment(
      left ? "humanoid_minimal_forearm_l" : "humanoid_minimal_forearm_r",
      left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
      left ? HumanoidBone::HandL : HumanoidBone::HandR,
      0.0F,
      HP::FORE_ARM_R * 1.18F,
      HP::FORE_ARM_R * 0.86F,
      Skin));
}

constexpr auto make_minimal_thigh(bool left) noexcept -> Creature::PrimitiveInstance {
  return as_minimal(make_limb_end_segment(
      left ? "humanoid_minimal_thigh_l" : "humanoid_minimal_thigh_r",
      left ? HumanoidBone::HipL : HumanoidBone::HipR,
      left ? HumanoidBone::KneeL : HumanoidBone::KneeR,
      0.0F,
      HP::UPPER_LEG_R * 1.34F,
      HP::UPPER_LEG_R * 0.96F,
      ClothDark));
}

constexpr auto make_minimal_calf(bool left) noexcept -> Creature::PrimitiveInstance {
  return as_minimal(make_limb_end_segment(
      left ? "humanoid_minimal_calf_l" : "humanoid_minimal_calf_r",
      left ? HumanoidBone::KneeL : HumanoidBone::KneeR,
      left ? HumanoidBone::FootL : HumanoidBone::FootR,
      0.0F,
      HP::LOWER_LEG_R * 1.34F,
      HP::LOWER_LEG_R * 0.78F,
      Skin));
}

constexpr std::array<Creature::PrimitiveInstance, 16> k_minimal_parts = {
    as_minimal(make_full_chest()),
    as_minimal(make_full_pelvis_block()),

    as_minimal(make_full_neck()),
    as_minimal(make_full_cranium()),

    make_minimal_upper_arm(true),
    make_minimal_upper_arm(false),
    make_minimal_forearm(true),
    make_minimal_forearm(false),
    as_minimal(make_full_hand(true)),
    as_minimal(make_full_hand(false)),

    make_minimal_thigh(true),
    make_minimal_thigh(false),
    make_minimal_calf(true),
    make_minimal_calf(false),
    as_minimal(make_full_foot(true)),
    as_minimal(make_full_foot(false)),
};

constexpr std::array<Creature::PrimitiveInstance, 41> k_full_parts = {

    make_full_chest(),
    make_full_pelvis_block(),
    make_full_buttock(true),
    make_full_buttock(false),

    make_full_deltoid(true),
    make_full_deltoid(false),
    make_full_deltoid_cap(true),
    make_full_deltoid_cap(false),

    make_full_upper_arm_proximal(true),
    make_full_upper_arm_proximal(false),
    make_full_upper_arm_distal(true),
    make_full_upper_arm_distal(false),
    make_full_elbow(true),
    make_full_elbow(false),
    make_full_forearm_proximal(true),
    make_full_forearm_proximal(false),
    make_full_forearm_distal(true),
    make_full_forearm_distal(false),
    make_full_hand(true),
    make_full_hand(false),

    make_full_neck(),
    make_full_cranium(),
    make_full_jaw(),
    make_full_brow(),
    make_full_nose(),
    make_full_ear(true),
    make_full_ear(false),

    make_full_thigh_proximal(true),
    make_full_thigh_proximal(false),
    make_full_thigh_distal(true),
    make_full_thigh_distal(false),
    make_full_knee(true),
    make_full_knee(false),
    make_full_calf_proximal(true),
    make_full_calf_proximal(false),
    make_full_calf_distal(true),
    make_full_calf_distal(false),
    make_full_ankle(true),
    make_full_ankle(false),
    make_full_foot(true),
    make_full_foot(false),
};

} // namespace Render::Humanoid::humanoid_spec_detail
