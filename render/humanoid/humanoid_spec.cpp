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
  kRoleCount = 7,
};

auto fill_role_colors(const Render::GL::HumanoidVariant &v,
                      std::array<QVector3D, kRoleCount> &out) noexcept -> void {
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

constexpr auto make_reduced_torso() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_reduced_torso";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Chest);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Pelvis);
  p.params.radius = HP::TORSO_TOP_R;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodReduced;
  return p;
}

constexpr auto make_reduced_head() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_reduced_head";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Head);
  p.params.radius = HP::HEAD_RADIUS;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodReduced;
  return p;
}

constexpr auto
make_reduced_arm(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_reduced_arm_l" : "humanoid_reduced_arm_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::ShoulderL : HumanoidBone::ShoulderR);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.radius = (HP::UPPER_ARM_R + HP::FORE_ARM_R) * 0.5F;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodReduced;
  return p;
}

constexpr auto
make_reduced_leg(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_reduced_leg_l" : "humanoid_reduced_leg_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::HipL : HumanoidBone::HipR);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.radius = (HP::UPPER_LEG_R + HP::LOWER_LEG_R) * 0.5F;
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodReduced;
  return p;
}

constexpr std::array<Creature::PrimitiveInstance, 6> k_reduced_parts = {
    make_reduced_torso(),    make_reduced_head(),    make_reduced_arm(true),
    make_reduced_arm(false), make_reduced_leg(true), make_reduced_leg(false),
};

constexpr auto make_full_torso() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_torso";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Chest);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Pelvis);
  p.params.radius = HP::TORSO_TOP_R;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_neck() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_neck";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Neck);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Head);
  p.params.radius = HP::NECK_RADIUS;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto make_full_head() noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_head";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(HumanoidBone::Head);
  p.params.radius = HP::HEAD_RADIUS;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_upper_arm(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name =
      left ? "humanoid_full_upper_arm_l" : "humanoid_full_upper_arm_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::UpperArmL : HumanoidBone::UpperArmR);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.radius = HP::UPPER_ARM_R;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_elbow(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_elbow_l" : "humanoid_full_elbow_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.radius = HP::HAND_RADIUS * 1.05F;
  p.color_role = Cloth;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_forearm(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_forearm_l" : "humanoid_full_forearm_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::ForearmL : HumanoidBone::ForearmR);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.radius = HP::FORE_ARM_R;
  p.color_role = Skin;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_hand(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_hand_l" : "humanoid_full_hand_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::HandL : HumanoidBone::HandR);
  p.params.radius = HP::HAND_RADIUS * 0.95F;
  p.color_role = LeatherDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_thigh(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_thigh_l" : "humanoid_full_thigh_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::HipL : HumanoidBone::HipR);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.radius = HP::UPPER_LEG_R;
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_knee(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_knee_l" : "humanoid_full_knee_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.radius = HP::LOWER_LEG_R * 0.95F;
  p.color_role = ClothDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_shin(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_shin_l" : "humanoid_full_shin_r";
  p.shape = Creature::PrimitiveShape::Cylinder;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::KneeL : HumanoidBone::KneeR);
  p.params.tail_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.radius = HP::LOWER_LEG_R;
  p.color_role = Leather;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr auto
make_full_foot(bool left) noexcept -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = left ? "humanoid_full_foot_l" : "humanoid_full_foot_r";
  p.shape = Creature::PrimitiveShape::Sphere;
  p.params.anchor_bone = static_cast<Creature::BoneIndex>(
      left ? HumanoidBone::FootL : HumanoidBone::FootR);
  p.params.radius = HP::LOWER_LEG_R * 1.10F;
  p.color_role = LeatherDark;
  p.lod_mask = Creature::kLodFull;
  return p;
}

constexpr std::array<Creature::PrimitiveInstance, 19> k_full_parts = {
    make_full_torso(),          make_full_neck(),
    make_full_head(),           make_full_upper_arm(true),
    make_full_upper_arm(false), make_full_elbow(true),
    make_full_elbow(false),     make_full_forearm(true),
    make_full_forearm(false),   make_full_hand(true),
    make_full_hand(false),      make_full_thigh(true),
    make_full_thigh(false),     make_full_knee(true),
    make_full_knee(false),      make_full_shin(true),
    make_full_shin(false),      make_full_foot(true),
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
    s.lod_reduced = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_reduced_parts.data(),
                                                     k_reduced_parts.size()),
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

auto resolve_renderer(Render::GL::ISubmitter &out) noexcept
    -> Render::GL::Renderer * {
  if (auto *renderer = dynamic_cast<Render::GL::Renderer *>(&out)) {
    return renderer;
  }
  if (auto *batch = dynamic_cast<Render::GL::BatchingSubmitter *>(&out)) {
    return dynamic_cast<Render::GL::Renderer *>(batch->fallback_submitter());
  }
  return nullptr;
}

auto build_humanoid_bind_palette() -> std::array<QMatrix4x4, kBoneCount> {
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

  std::array<QMatrix4x4, kBoneCount> palette{};
  BonePalette tmp{};
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), tmp);
  for (std::size_t i = 0; i < kBoneCount; ++i) {
    palette[i] = tmp[i];
  }
  return palette;
}

} // namespace

auto compute_bone_palette(const Render::GL::HumanoidPose &pose,
                          std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < kBoneCount) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), tmp);
  for (std::size_t i = 0; i < kBoneCount; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(kBoneCount);
}

auto humanoid_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, kBoneCount> palette =
      build_humanoid_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

namespace {

void submit_humanoid_rigged_impl(const Render::GL::HumanoidPose &pose,
                                 const Render::GL::HumanoidVariant &variant,
                                 Creature::CreatureLOD lod,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept {

  BonePalette tmp{};
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), tmp);

  auto *renderer = resolve_renderer(out);
  if (renderer == nullptr) {
    if (lod == Creature::CreatureLOD::Billboard) {
      return;
    }
    std::array<QVector3D, kRoleCount> roles{};
    fill_role_colors(variant, roles);
    Creature::submit_creature(
        humanoid_creature_spec(),
        std::span<const QMatrix4x4>(tmp.data(), tmp.size()), lod,
        world_from_unit, out,
        std::span<const QVector3D>(roles.data(), roles.size()));
    return;
  }

  auto const &spec = humanoid_creature_spec();
  auto bind = humanoid_bind_palette();

  auto *entry = renderer->rigged_mesh_cache().get_or_bake(spec, lod, bind, 0);
  if (entry == nullptr || entry->mesh == nullptr ||
      entry->mesh->index_count() == 0U) {
    return;
  }

  auto &arena = renderer->bone_palette_arena();
  Render::GL::BonePaletteSlot palette_slot_h = arena.allocate_palette();
  QMatrix4x4 *palette_slot = palette_slot_h.cpu;

  std::size_t const n =
      std::min<std::size_t>(entry->inverse_bind.size(), kBoneCount);
  for (std::size_t i = 0; i < n; ++i) {
    palette_slot[i] = tmp[i] * entry->inverse_bind[i];
  }

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.material = nullptr;
  cmd.world = world_from_unit;
  cmd.bone_palette = palette_slot;
  cmd.palette_ubo = palette_slot_h.ubo;
  cmd.palette_offset = static_cast<std::uint32_t>(palette_slot_h.offset);
  cmd.bone_count = static_cast<std::uint32_t>(n);
  cmd.color = variant.palette.cloth;
  cmd.alpha = 1.0F;
  cmd.texture = nullptr;
  cmd.material_id = 0;
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);

  out.rigged(cmd);
}

} // namespace

void submit_humanoid_reduced_rigged(const Render::GL::HumanoidPose &pose,
                                    const Render::GL::HumanoidVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept {
  submit_humanoid_rigged_impl(pose, variant, Creature::CreatureLOD::Reduced,
                              world_from_unit, out);
}

void submit_humanoid_full_rigged(const Render::GL::HumanoidPose &pose,
                                 const Render::GL::HumanoidVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept {
  submit_humanoid_rigged_impl(pose, variant, Creature::CreatureLOD::Full,
                              world_from_unit, out);
}

void submit_humanoid_minimal_rigged(const Render::GL::HumanoidPose &pose,
                                    const Render::GL::HumanoidVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept {
  submit_humanoid_rigged_impl(pose, variant, Creature::CreatureLOD::Minimal,
                              world_from_unit, out);
}

} // namespace Render::Humanoid
