#include "render/humanoid/asset/humanoid_spec.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/bone_palette_arena.h"
#include "render/creature/spec.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/gl/mesh.h"
#include "render/humanoid/asset/humanoid_beard_mesh.h"
#include "render/humanoid/runtime/body_frame_resolver.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/rigged_mesh_cache.h"
#include "render/submitter.h"

namespace Render::Humanoid {

namespace {

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

static_assert(Hair == k_humanoid_hair_role);
static_assert(LeatherDark == k_humanoid_leather_dark_role);
static_assert(Wood == k_humanoid_wood_role);
static_assert(Metal == k_humanoid_metal_role);

void fill_humanoid_role_colors_impl(
    const Render::GL::HumanoidVariant& v,
    std::array<QVector3D, k_humanoid_role_count>& out) noexcept {
  out[Cloth - 1] = v.palette.cloth;
  out[Skin - 1] = v.palette.skin;
  out[Leather - 1] = v.palette.leather;
  out[LeatherDark - 1] = v.palette.leather_dark;
  out[Wood - 1] = v.palette.wood;
  out[Metal - 1] = v.palette.metal;
  out[ClothDark - 1] = v.palette.cloth * 0.84F;
  auto const& hair = v.facial_hair;
  QVector3D const grey(0.72F, 0.71F, 0.69F);
  out[Hair - 1] = (hair.color * (1.0F - hair.greyness)) + (grey * hair.greyness);
}

constexpr auto bone(HumanoidBone b) noexcept -> Creature::BoneIndex {
  return static_cast<Creature::BoneIndex>(b);
}

constexpr auto make_full_chest() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_chest";
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  p.params.tail_offset = QVector3D(0.0F, -0.17F, 0.0F);
  p.params.radius = HP::TORSO_TOP_R * 0.86F;

  p.params.depth_radius = HP::TORSO_TOP_R * 0.54F;
  p.color_role = Cloth;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_pectoral(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_pectoral_l" : "humanoid_full_pectoral_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  float const x = left ? -HP::TORSO_TOP_R * 0.42F : HP::TORSO_TOP_R * 0.42F;
  p.params.head_offset = QVector3D(x, -0.04F, HP::TORSO_TOP_R * 0.26F);
  p.params.half_extents = QVector3D(
      HP::TORSO_TOP_R * 0.28F, HP::TORSO_TOP_R * 0.20F, HP::TORSO_TOP_R * 0.12F);
  p.color_role = Cloth;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_upper_back() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_upper_back";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Chest);

  p.params.head_offset = QVector3D(0.0F, 0.16F, -HP::TORSO_TOP_R * 0.40F);
  p.params.half_extents = QVector3D(
      HP::TORSO_TOP_R * 0.64F, HP::TORSO_TOP_R * 0.34F, HP::TORSO_TOP_R * 0.16F);
  p.color_role = Cloth;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr auto make_full_abdomen() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_abdomen";
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Spine);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  p.params.radius = HP::TORSO_BOT_R * 0.78F;
  p.params.depth_radius = HP::TORSO_BOT_R * 0.55F;
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
  // OrientedSphere scales the radius-1 unit sphere by twice the half
  // extents, so these are half the rendered radii. The cap is sized to sit
  // just proud of the upper arm (about 0.067 m at the shoulder): at the old
  // UPPER_ARM_R * 1.45 it rendered 0.29 m across, a ball wider than the neck
  // gap that swallowed every shoulder guard authored to the arm.
  float const r = HP::UPPER_ARM_R * 0.80F;
  p.params.half_extents = QVector3D(r, r * 0.84F, r * 0.96F);
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
                           HP::UPPER_ARM_R * 1.18F,
                           HP::UPPER_ARM_R * 1.00F,
                           Cloth);
}

constexpr auto
make_full_upper_arm_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_end_segment(left ? "humanoid_full_upper_arm_l_bot"
                                    : "humanoid_full_upper_arm_r_bot",
                               left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR,
                               left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
                               k_upper_arm_half,
                               HP::UPPER_ARM_R * 1.00F,
                               HP::UPPER_ARM_R * 0.86F,
                               Cloth);
}

constexpr auto make_full_elbow(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_elbow_l" : "humanoid_full_elbow_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.radius = HP::UPPER_ARM_R * 0.90F;
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
                           HP::FORE_ARM_R * 1.10F,
                           HP::FORE_ARM_R * 1.22F,
                           Skin);
}

constexpr auto
make_full_forearm_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  return make_limb_end_segment(left ? "humanoid_full_forearm_l_bot"
                                    : "humanoid_full_forearm_r_bot",
                               left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
                               left ? HumanoidBone::HandL : HumanoidBone::HandR,
                               HP::FORE_ARM_LEN * 0.28F,
                               HP::FORE_ARM_R * 1.22F,
                               HP::FORE_ARM_R * 0.80F,
                               Skin);
}

constexpr auto make_full_hand(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_hand_l" : "humanoid_full_hand_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.head_offset = QVector3D(0.0F, HP::HAND_RADIUS * 0.65F, 0.0F);
  p.params.half_extents = QVector3D(
      HP::HAND_RADIUS * 1.18F, HP::HAND_RADIUS * 1.38F, HP::HAND_RADIUS * 0.56F);
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
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.40F, HP::HEAD_RADIUS * 0.22F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.56F, HP::HEAD_RADIUS * 0.30F, HP::HEAD_RADIUS * 0.38F);
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
      QVector3D(0.0F, HP::HEAD_RADIUS * 0.30F, HP::HEAD_RADIUS * 0.60F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.60F, HP::HEAD_RADIUS * 0.12F, HP::HEAD_RADIUS * 0.22F);
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
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.01F, HP::HEAD_RADIUS * 0.80F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.09F, HP::HEAD_RADIUS * 0.18F, HP::HEAD_RADIUS * 0.18F);
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
      HP::UPPER_ARM_R * 1.18F,
      HP::UPPER_ARM_R * 0.86F,
      Cloth));
}

constexpr auto make_minimal_forearm(bool left) noexcept -> Creature::PrimitiveInstance {
  return as_minimal(make_limb_end_segment(
      left ? "humanoid_minimal_forearm_l" : "humanoid_minimal_forearm_r",
      left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR,
      left ? HumanoidBone::HandL : HumanoidBone::HandR,
      0.0F,
      HP::FORE_ARM_R * 1.10F,
      HP::FORE_ARM_R * 0.80F,
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

constexpr std::array<Creature::PrimitiveInstance, 17> k_minimal_parts = {
    as_minimal(make_full_chest()),
    as_minimal(make_full_abdomen()),
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
    make_full_pectoral(true),
    make_full_pectoral(false),
    make_full_upper_back(),
    make_full_abdomen(),
    make_full_pelvis_block(),
    make_full_buttock(true),
    make_full_buttock(false),

    make_full_deltoid(true),
    make_full_deltoid(false),

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

constexpr auto
make_skeleton_minimal_bone(const char* name,
                           HumanoidBone anchor,
                           HumanoidBone tail,
                           float radius) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = name;
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(anchor);
  p.params.tail_bone = bone(tail);
  p.params.radius = radius;
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
      HP::HEAD_RADIUS * 0.62F, HP::HEAD_RADIUS * 0.76F, HP::HEAD_RADIUS * 0.68F);
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
                               HP::UPPER_ARM_R * 0.48F),
    make_skeleton_minimal_bone("skeleton_minimal_upper_arm_r",
                               HumanoidBone::UpperArmR,
                               HumanoidBone::ForearmR,
                               HP::UPPER_ARM_R * 0.48F),
    make_skeleton_minimal_bone("skeleton_minimal_forearm_l",
                               HumanoidBone::ForearmL,
                               HumanoidBone::HandL,
                               HP::FORE_ARM_R * 0.44F),
    make_skeleton_minimal_bone("skeleton_minimal_forearm_r",
                               HumanoidBone::ForearmR,
                               HumanoidBone::HandR,
                               HP::FORE_ARM_R * 0.44F),
    make_skeleton_minimal_bone("skeleton_minimal_thigh_l",
                               HumanoidBone::HipL,
                               HumanoidBone::KneeL,
                               HP::UPPER_LEG_R * 0.46F),
    make_skeleton_minimal_bone("skeleton_minimal_thigh_r",
                               HumanoidBone::HipR,
                               HumanoidBone::KneeR,
                               HP::UPPER_LEG_R * 0.46F),
    make_skeleton_minimal_bone("skeleton_minimal_calf_l",
                               HumanoidBone::KneeL,
                               HumanoidBone::FootL,
                               HP::LOWER_LEG_R * 0.44F),
    make_skeleton_minimal_bone("skeleton_minimal_calf_r",
                               HumanoidBone::KneeR,
                               HumanoidBone::FootR,
                               HP::LOWER_LEG_R * 0.44F),
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
      HP::HEAD_RADIUS * 0.63F, HP::HEAD_RADIUS * 0.80F, HP::HEAD_RADIUS * 0.72F);
  p.color_role = Skin;
  return p;
}

constexpr auto make_skeleton_jaw() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p = make_full_jaw();
  p.debug_name = "skeleton_jaw";
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.46F, HP::HEAD_RADIUS * 0.20F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.36F, HP::HEAD_RADIUS * 0.15F, HP::HEAD_RADIUS * 0.28F);
  p.color_role = Skin;
  return p;
}

constexpr auto make_skeleton_eye(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "skeleton_eye_socket_l" : "skeleton_eye_socket_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  float const side = left ? -1.0F : 1.0F;
  p.params.head_offset = QVector3D(
      side * HP::HEAD_RADIUS * 0.27F, HP::HEAD_RADIUS * 0.12F, HP::HEAD_RADIUS * 0.74F);
  p.params.half_extents = QVector3D(
      HP::HEAD_RADIUS * 0.18F, HP::HEAD_RADIUS * 0.16F, HP::HEAD_RADIUS * 0.07F);
  p.color = QVector3D(0.045F, 0.050F, 0.055F);
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
  p.params.head_offset = QVector3D(side * HP::HEAD_RADIUS * 0.16F,
                                   -HP::HEAD_RADIUS * 0.08F,
                                   HP::HEAD_RADIUS * 0.70F);
  p.params.tail_offset = QVector3D(side * HP::HEAD_RADIUS * 0.38F,
                                   -HP::HEAD_RADIUS * 0.24F,
                                   HP::HEAD_RADIUS * 0.58F);
  p.params.radius = HP::HEAD_RADIUS * 0.065F;
  p.params.depth_radius = HP::HEAD_RADIUS * 0.045F;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

constexpr std::array<Creature::PrimitiveInstance, 60> k_skeleton_full_parts = {
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
                       HP::UPPER_ARM_R * 0.46F),
    make_skeleton_limb("skeleton_upper_arm_r",
                       HumanoidBone::UpperArmR,
                       HumanoidBone::ForearmR,
                       HP::UPPER_ARM_R * 0.46F),
    make_skeleton_limb("skeleton_forearm_l",
                       HumanoidBone::ForearmL,
                       HumanoidBone::HandL,
                       HP::FORE_ARM_R * 0.42F),
    make_skeleton_limb("skeleton_forearm_r",
                       HumanoidBone::ForearmR,
                       HumanoidBone::HandR,
                       HP::FORE_ARM_R * 0.42F),
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
                       HP::UPPER_LEG_R * 0.42F),
    make_skeleton_limb("skeleton_thigh_r",
                       HumanoidBone::HipR,
                       HumanoidBone::KneeR,
                       HP::UPPER_LEG_R * 0.42F),
    make_skeleton_limb("skeleton_calf_l",
                       HumanoidBone::KneeL,
                       HumanoidBone::FootL,
                       HP::LOWER_LEG_R * 0.40F),
    make_skeleton_limb("skeleton_calf_r",
                       HumanoidBone::KneeR,
                       HumanoidBone::FootR,
                       HP::LOWER_LEG_R * 0.40F),
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
        QVector3D(0.0F, -HP::HEAD_RADIUS * 0.08F, HP::HEAD_RADIUS * 0.76F),
        QVector3D(HP::HEAD_RADIUS * 0.10F,
                  HP::HEAD_RADIUS * 0.16F,
                  HP::HEAD_RADIUS * 0.055F)),
    make_skeleton_face_cavity(
        "skeleton_mouth_cavity",
        QVector3D(0.0F, -HP::HEAD_RADIUS * 0.37F, HP::HEAD_RADIUS * 0.70F),
        QVector3D(HP::HEAD_RADIUS * 0.27F,
                  HP::HEAD_RADIUS * 0.075F,
                  HP::HEAD_RADIUS * 0.050F)),
    make_skeleton_cheek(true),
    make_skeleton_cheek(false),
};

} // namespace

void apply_skeleton_proportion_pose(Render::GL::HumanoidPose& io_pose) noexcept {
  QVector3D const cervical_axis = io_pose.head_pos - io_pose.neck_base;

  io_pose.head_pos = io_pose.neck_base + cervical_axis * 0.70F;
}

auto humanoid_creature_spec() noexcept -> const Creature::CreatureSpec& {
  static const Creature::CreatureSpec spec = [] {
    Creature::CreatureSpec s;
    s.species_name = "humanoid";
    s.topology = humanoid_topology();
    s.lod_minimal = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_minimal_parts.data(),
                                                     k_minimal_parts.size()),
    };
    s.lod_full = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_full_parts.data(),
                                                     k_full_parts.size()),
    };

    return s;
  }();
  return spec;
}

namespace {

struct BodyVariantSpec {
  std::string species_name;
  std::unique_ptr<Render::GL::Mesh> full_beard;
  std::unique_ptr<Render::GL::Mesh> minimal_beard;
  std::vector<Creature::PrimitiveInstance> full_parts;
  std::vector<Creature::PrimitiveInstance> minimal_parts;
  Creature::CreatureSpec spec;
};

auto beard_primitive(Render::GL::Mesh* mesh,
                     std::uint8_t lod_mask) -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_beard";
  p.shape = Creature::PrimitiveShape::Mesh;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.half_extents = QVector3D(k_beard_head_silhouette_radius,
                                    k_beard_head_silhouette_radius,
                                    k_beard_head_silhouette_radius);
  p.custom_mesh = mesh;
  p.mesh_skinning = Creature::MeshSkinning::Authored;
  p.color_role = Hair;
  p.lod_mask = lod_mask;
  return p;
}

auto build_body_variant_spec(HumanoidBodyVariant variant)
    -> std::unique_ptr<BodyVariantSpec> {
  auto out = std::make_unique<BodyVariantSpec>();
  auto const& base = humanoid_creature_spec();
  out->species_name =
      std::string(base.species_name) + std::string(body_variant_name_suffix(variant));
  out->full_beard = build_humanoid_beard_mesh(variant, Creature::CreatureLOD::Full);
  out->minimal_beard =
      build_humanoid_beard_mesh(variant, Creature::CreatureLOD::Minimal);
  out->full_parts.assign(base.lod_full.primitives.begin(),
                         base.lod_full.primitives.end());
  out->minimal_parts.assign(base.lod_minimal.primitives.begin(),
                            base.lod_minimal.primitives.end());
  if (out->full_beard != nullptr) {
    out->full_parts.push_back(
        beard_primitive(out->full_beard.get(), Creature::k_lod_full));
  }
  if (out->minimal_beard != nullptr) {
    out->minimal_parts.push_back(
        beard_primitive(out->minimal_beard.get(), Creature::k_lod_minimal));
  }
  out->spec = base;
  out->spec.species_name = out->species_name;
  out->spec.body_variant = static_cast<std::uint8_t>(variant);
  out->spec.lod_full = Creature::PartGraph{std::span<const Creature::PrimitiveInstance>(
      out->full_parts.data(), out->full_parts.size())};
  out->spec.lod_minimal =
      Creature::PartGraph{std::span<const Creature::PrimitiveInstance>(
          out->minimal_parts.data(), out->minimal_parts.size())};
  return out;
}

} // namespace

auto humanoid_creature_spec_for_body_variant(std::uint8_t body_variant) noexcept
    -> const Creature::CreatureSpec* {
  if (body_variant == 0U || body_variant >= k_humanoid_body_variant_count) {
    return &humanoid_creature_spec();
  }
  static const auto variants = [] {
    std::array<std::unique_ptr<BodyVariantSpec>, k_humanoid_body_variant_count> built{};
    for (std::size_t i = 1; i < built.size(); ++i) {
      built[i] = build_body_variant_spec(static_cast<HumanoidBodyVariant>(i));
    }
    return built;
  }();
  return &variants[body_variant]->spec;
}

auto skeleton_humanoid_creature_spec() noexcept -> const Creature::CreatureSpec& {
  static const Creature::CreatureSpec spec = [] {
    Creature::CreatureSpec s;
    s.species_name = "skeleton_humanoid";
    s.topology = humanoid_topology();
    s.lod_minimal = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_skeleton_minimal_parts.data(),
                                                     k_skeleton_minimal_parts.size()),
    };
    s.lod_full = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_skeleton_full_parts.data(),
                                                     k_skeleton_full_parts.size()),
    };
    return s;
  }();
  return spec;
}

} // namespace Render::Humanoid

namespace Render::Humanoid {

namespace {

auto build_humanoid_bind_palette() -> std::array<QMatrix4x4, k_bone_count> {
  Render::GL::VariationParams variation{};
  variation.height_scale = 1.0F;
  variation.bulk_scale = 1.0F;
  variation.stance_width = 1.0F;
  variation.arm_swing_amp = 1.0F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;

  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      0, 0.0F, false, variation, pose);

  std::array<QMatrix4x4, k_bone_count> palette{};
  BonePalette tmp{};
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), tmp);
  for (std::size_t i = 0; i < k_bone_count; ++i) {
    palette[i] = tmp[i];
  }
  return palette;
}

} // namespace

auto humanoid_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, k_bone_count> palette =
      build_humanoid_bind_palette();
  return {palette.data(), palette.size()};
}

auto humanoid_inverse_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, k_bone_count> inverse = []() {
    auto const bind = humanoid_bind_palette();
    std::array<QMatrix4x4, k_bone_count> result{};
    for (std::size_t i = 0; i < k_bone_count && i < bind.size(); ++i) {
      result[i] = bind[i].inverted();
    }
    return result;
  }();
  return {inverse.data(), inverse.size()};
}

auto humanoid_bind_body_frames() noexcept -> const Render::GL::BodyFrames& {
  static const Render::GL::BodyFrames frames = []() -> Render::GL::BodyFrames {
    Render::GL::VariationParams variation{};
    variation.height_scale = 1.0F;
    variation.bulk_scale = 1.0F;
    variation.stance_width = 1.0F;
    variation.arm_swing_amp = 1.0F;
    variation.walk_speed_mult = 1.0F;
    variation.posture_slump = 0.0F;
    variation.shoulder_tilt = 0.0F;

    Render::GL::HumanoidPose pose{};
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0, 0.0F, false, variation, pose);

    HumanoidBodyMetrics metrics{};
    compute_humanoid_body_metrics(pose, QVector3D(1.0F, 1.0F, 1.0F), 1.0F, metrics);
    compute_humanoid_head_frame(pose, metrics);
    compute_humanoid_body_frames(pose, metrics);
    return pose.body_frames;
  }();
  return frames;
}

void fill_humanoid_role_colors(
    const Render::GL::HumanoidVariant& variant,
    std::array<QVector3D, k_humanoid_role_count>& out_roles) noexcept {
  fill_humanoid_role_colors_impl(variant, out_roles);
}

} // namespace Render::Humanoid
