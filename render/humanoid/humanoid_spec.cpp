#include "humanoid_spec.h"

#include "../bone_palette_arena.h"
#include "../creature/spec.h"
#include "../gl/humanoid/humanoid_types.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "humanoid_full_builder.h"
#include "humanoid_renderer_base.h"
#include "humanoid_specs.h"
#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <mutex>
#include <span>
#include <vector>

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
};

void fill_humanoid_role_colors_impl(
    const Render::GL::HumanoidVariant &v,
    std::array<QVector3D, k_humanoid_role_count> &out) noexcept {
  out[Cloth - 1] = v.palette.cloth;
  out[Skin - 1] = v.palette.skin;
  out[Leather - 1] = v.palette.leather;
  out[LeatherDark - 1] = v.palette.leather_dark;
  out[Wood - 1] = v.palette.wood;
  out[Metal - 1] = v.palette.metal;
  out[ClothDark - 1] = v.palette.cloth * 0.92F;
}

constexpr auto make_minimal_capsule() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_minimal_body";
  p.shape = Creature::PrimitiveShape::Capsule;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Head);
  p.params.head_offset = QVector3D(0.0F, HP::HEAD_RADIUS, 0.0F);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(HumanoidBone::FootL);
  p.params.tail_offset = QVector3D(0.0F, 0.0F, 0.0F);
  p.params.radius = HP::TORSO_TOP_R;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodMinimal;
  return p;
}

constexpr std::array<Creature::PrimitiveInstance, 1> k_minimal_parts = {
    make_minimal_capsule(),
};

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
  p.params.radius = HP::TORSO_TOP_R * 0.98F;
  p.params.depth_radius = HP::TORSO_TOP_R * 0.74F;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_pectoral(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_pectoral_l" : "humanoid_full_pectoral_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  float const x = left ? -HP::TORSO_TOP_R * 0.56F : HP::TORSO_TOP_R * 0.56F;
  p.params.head_offset = QVector3D(x, -0.05F, HP::TORSO_TOP_R * 0.40F);
  p.params.half_extents =
      QVector3D(HP::TORSO_TOP_R * 0.48F, HP::TORSO_TOP_R * 0.32F,
                HP::TORSO_TOP_R * 0.22F);
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_upper_back() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_upper_back";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Chest);
  p.params.head_offset = QVector3D(0.0F, -0.05F, -HP::TORSO_TOP_R * 0.48F);
  p.params.half_extents =
      QVector3D(HP::TORSO_TOP_R * 0.84F, HP::TORSO_TOP_R * 0.44F,
                HP::TORSO_TOP_R * 0.22F);
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_abdomen() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_abdomen";
  p.shape = Creature::PrimitiveShape::OrientedCylinder;
  p.params.anchor_bone = bone(HumanoidBone::Spine);
  p.params.tail_bone = bone(HumanoidBone::Chest);
  p.params.radius = HP::TORSO_BOT_R * 0.72F;
  p.params.depth_radius = HP::TORSO_BOT_R * 0.50F;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_pelvis_block() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_pelvis_block";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  p.params.head_offset = QVector3D(0.0F, 0.00F, -HP::TORSO_BOT_R * 0.02F);
  p.params.half_extents =
      QVector3D(HP::TORSO_BOT_R * 0.96F, HP::TORSO_BOT_R * 0.34F,
                HP::TORSO_BOT_R * 0.56F);
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_buttock(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_buttock_l" : "humanoid_full_buttock_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Pelvis);
  float const x = left ? -HP::TORSO_BOT_R * 0.42F : HP::TORSO_BOT_R * 0.42F;
  p.params.head_offset = QVector3D(x, -0.02F, -HP::TORSO_BOT_R * 0.34F);
  p.params.half_extents =
      QVector3D(HP::TORSO_BOT_R * 0.30F, HP::TORSO_BOT_R * 0.27F,
                HP::TORSO_BOT_R * 0.24F);
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_deltoid(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_deltoid_l" : "humanoid_full_deltoid_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone =
      bone(left ? HumanoidBone::ShoulderL : HumanoidBone::ShoulderR);
  p.params.head_offset = QVector3D(0.0F, 0.0F, 0.0F);
  float const r = HP::UPPER_ARM_R * 1.75F;
  p.params.half_extents = QVector3D(r, r * 0.84F, r * 0.96F);
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr float k_upper_arm_half = HP::UPPER_ARM_LEN * 0.5F;
constexpr float k_fore_arm_half = HP::FORE_ARM_LEN * 0.5F;

constexpr auto make_full_upper_arm_proximal(bool left) noexcept
    -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_upper_arm_l_top" : "humanoid_full_upper_arm_r_top";
  p.shape = Creature::PrimitiveShape::Cylinder;
  auto const b = bone(left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR);
  p.params.anchor_bone = b;
  p.params.tail_bone = b;
  p.params.tail_offset = QVector3D(0.0F, k_upper_arm_half, 0.0F);
  p.params.radius = HP::UPPER_ARM_R * 1.28F;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_upper_arm_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_upper_arm_l_bot" : "humanoid_full_upper_arm_r_bot";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone =
      bone(left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR);
  p.params.head_offset = QVector3D(0.0F, k_upper_arm_half, 0.0F);
  p.params.tail_bone =
      bone(left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.radius = HP::UPPER_ARM_R * 0.98F;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_elbow(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_elbow_l" : "humanoid_full_elbow_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone =
      bone(left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.radius = HP::UPPER_ARM_R * 1.00F;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_forearm_proximal(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_forearm_l_top" : "humanoid_full_forearm_r_top";
  p.shape = Creature::PrimitiveShape::Cylinder;
  auto const b = bone(left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.anchor_bone = b;
  p.params.tail_bone = b;
  p.params.tail_offset = QVector3D(0.0F, k_fore_arm_half, 0.0F);
  p.params.radius = HP::FORE_ARM_R * 1.28F;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_forearm_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_forearm_l_bot" : "humanoid_full_forearm_r_bot";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone =
      bone(left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.head_offset = QVector3D(0.0F, k_fore_arm_half, 0.0F);
  p.params.tail_bone = bone(left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.radius = HP::FORE_ARM_R * 0.86F;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_hand(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_hand_l" : "humanoid_full_hand_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.head_offset = QVector3D(0.0F, HP::HAND_RADIUS * 0.65F, 0.0F);
  p.params.half_extents =
      QVector3D(HP::HAND_RADIUS * 1.18F, HP::HAND_RADIUS * 1.38F,
                HP::HAND_RADIUS * 0.56F);
  p.color_role = LeatherDark;
  p.lod_mask = Creature::kLodFull;
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
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_cranium() noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_cranium";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset = QVector3D(0.0F, HP::HEAD_RADIUS * 0.06F, 0.0F);
  p.params.half_extents =
      QVector3D(HP::HEAD_RADIUS * 0.80F, HP::HEAD_RADIUS * 0.98F,
                HP::HEAD_RADIUS * 0.88F);
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_jaw() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_jaw";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.40F, HP::HEAD_RADIUS * 0.22F);
  p.params.half_extents =
      QVector3D(HP::HEAD_RADIUS * 0.42F, HP::HEAD_RADIUS * 0.24F,
                HP::HEAD_RADIUS * 0.34F);
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_brow() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_brow";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset =
      QVector3D(0.0F, HP::HEAD_RADIUS * 0.30F, HP::HEAD_RADIUS * 0.60F);
  p.params.half_extents =
      QVector3D(HP::HEAD_RADIUS * 0.60F, HP::HEAD_RADIUS * 0.12F,
                HP::HEAD_RADIUS * 0.22F);
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_nose() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_nose";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.head_offset =
      QVector3D(0.0F, -HP::HEAD_RADIUS * 0.01F, HP::HEAD_RADIUS * 0.80F);
  p.params.half_extents =
      QVector3D(HP::HEAD_RADIUS * 0.09F, HP::HEAD_RADIUS * 0.18F,
                HP::HEAD_RADIUS * 0.18F);
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr float k_upper_leg_half = HP::UPPER_LEG_LEN * 0.5F;
constexpr float k_lower_leg_half = HP::LOWER_LEG_LEN * 0.5F;

constexpr auto
make_full_thigh_proximal(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_thigh_l_top" : "humanoid_full_thigh_r_top";
  p.shape = Creature::PrimitiveShape::Cylinder;
  auto const b = bone(left ? HumanoidBone::HipL : HumanoidBone::HipR);
  p.params.anchor_bone = b;
  p.params.tail_bone = b;
  p.params.tail_offset = QVector3D(0.0F, k_upper_leg_half, 0.0F);
  p.params.radius = HP::UPPER_LEG_R * 1.50F;
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_thigh_distal(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_thigh_l_bot" : "humanoid_full_thigh_r_bot";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(left ? HumanoidBone::HipL : HumanoidBone::HipR);
  p.params.head_offset = QVector3D(0.0F, k_upper_leg_half, 0.0F);
  p.params.tail_bone = bone(left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.radius = HP::UPPER_LEG_R * 1.02F;
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_knee(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_knee_l" : "humanoid_full_knee_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.radius = HP::LOWER_LEG_R * 1.18F;
  p.color_role = Leather;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_calf_proximal(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_calf_l_top" : "humanoid_full_calf_r_top";
  p.shape = Creature::PrimitiveShape::Cylinder;
  auto const b = bone(left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.anchor_bone = b;
  p.params.tail_bone = b;
  p.params.tail_offset = QVector3D(0.0F, k_lower_leg_half, 0.0F);
  p.params.radius = HP::LOWER_LEG_R * 1.48F;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_calf_distal(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_calf_l_bot" : "humanoid_full_calf_r_bot";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = bone(left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.head_offset = QVector3D(0.0F, k_lower_leg_half, 0.0F);
  p.params.tail_bone = bone(left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.radius = HP::LOWER_LEG_R * 0.88F;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_ankle(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_ankle_l" : "humanoid_full_ankle_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.radius = HP::LOWER_LEG_R * 0.74F;
  p.color_role = Leather;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_foot(bool left) noexcept -> Creature::PrimitiveInstance {

  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_foot_l" : "humanoid_full_foot_r";
  p.shape = Creature::PrimitiveShape::OrientedSphere;
  p.params.anchor_bone = bone(left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.head_offset =
      QVector3D(0.0F, HP::LOWER_LEG_R * 0.18F, HP::LOWER_LEG_R * 1.20F);
  p.params.half_extents =
      QVector3D(HP::LOWER_LEG_R * 0.78F, HP::LOWER_LEG_R * 0.28F,
                HP::LOWER_LEG_R * 1.55F);
  p.color_role = LeatherDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

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

} // namespace

auto humanoid_creature_spec() noexcept -> const Creature::CreatureSpec & {
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
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(0, 0.0F, false,
                                                            variation, pose);

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
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

auto humanoid_bind_body_frames() noexcept -> const Render::GL::BodyFrames & {
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
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(0, 0.0F, false,
                                                              variation, pose);

    HumanoidBodyMetrics metrics{};
    compute_humanoid_body_metrics(pose, QVector3D(1.0F, 1.0F, 1.0F), 1.0F,
                                  metrics);
    compute_humanoid_head_frame(pose, metrics);
    compute_humanoid_body_frames(pose, metrics);
    return pose.body_frames;
  }();
  return frames;
}

void fill_humanoid_role_colors(
    const Render::GL::HumanoidVariant &variant,
    std::array<QVector3D, k_humanoid_role_count> &out_roles) noexcept {
  fill_humanoid_role_colors_impl(variant, out_roles);
}

} // namespace Render::Humanoid
