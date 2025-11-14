#include "mounted_knight_renderer_base.h"

#include "../equipment/equipment_registry.h"
#include "../equipment/weapons/shield_renderer.h"
#include "../equipment/weapons/sword_renderer.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../humanoid/mounted_pose_controller.h"
#include "../palette.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/systems/nation_id.h"

#include "mounted_knight_pose.h"
#include "renderer_constants.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Render::GL {

namespace {

constexpr QVector3D k_default_proportion_scale{0.92F, 0.88F, 0.96F};

}

MountedKnightRendererBase::MountedKnightRendererBase(
    MountedKnightRendererConfig config)
    : m_config(std::move(config)) {
  m_config.has_sword =
      m_config.has_sword && !m_config.sword_equipment_id.empty();
  if (!m_config.has_sword) {
    m_config.sword_equipment_id.clear();
  }

  m_config.has_cavalry_shield =
      m_config.has_cavalry_shield && !m_config.shield_equipment_id.empty();
  if (!m_config.has_cavalry_shield) {
    m_config.shield_equipment_id.clear();
  }

  m_horseRenderer.setAttachments(m_config.horse_attachments);
}

auto MountedKnightRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto MountedKnightRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void MountedKnightRendererBase::adjust_variation(
    const DrawContext &, uint32_t, VariationParams &variation) const {
  variation.height_scale = 0.88F;
  variation.bulk_scale = 0.82F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void MountedKnightRendererBase::get_variant(const DrawContext &ctx,
                                            uint32_t seed,
                                            HumanoidVariant &v) const {
  QVector3D const team_tint = resolveTeamTint(ctx);
  v.palette = makeHumanoidPalette(team_tint, seed);
}

auto MountedKnightRendererBase::getScaledHorseDimensions(uint32_t seed) const
    -> HorseDimensions {
  HorseDimensions dims = makeHorseDimensions(seed);
  scaleHorseDimensions(dims, get_mount_scale());
  return dims;
}

void MountedKnightRendererBase::customize_pose(
    const DrawContext &ctx, const HumanoidAnimationContext &anim_ctx,
    uint32_t seed, HumanoidPose &pose) const {
  const AnimationInputs &anim = anim_ctx.inputs;

  uint32_t horse_seed = seed;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseDimensions dims = getScaledHorseDimensions(horse_seed);
  HorseProfile mount_profile{};
  mount_profile.dims = dims;
  MountedAttachmentFrame mount = compute_mount_frame(mount_profile);
  tuneMountedKnightFrame(dims, mount);
  HorseMotionSample const motion =
      evaluate_horse_motion(mount_profile, anim, anim_ctx);
  apply_mount_vertical_offset(mount, motion.bob);

  m_lastPose = &pose;
  m_lastMount = mount;

  ReinState const reins = compute_rein_state(horse_seed, anim_ctx);
  m_lastReinState = reins;
  m_hasLastReins = true;

  MountedPoseController mounted_controller(pose, anim_ctx);

  float const speed_norm = anim_ctx.locomotion_normalized_speed();
  float const speed_lean = std::clamp(
      anim_ctx.locomotion_speed() * 0.10F + speed_norm * 0.05F, 0.0F, 0.22F);
  float const forward_lean =
      (dims.seatForwardOffset * 0.08F + speed_lean) / 0.15F;

  MountedPoseController::MountedRiderPoseRequest pose_request;
  pose_request.dims = dims;
  pose_request.forwardBias = forward_lean;
  pose_request.reinSlackLeft = reins.slack;
  pose_request.reinSlackRight = reins.slack;
  pose_request.reinTensionLeft = reins.tension;
  pose_request.reinTensionRight = reins.tension;
  pose_request.leftHandOnReins = !m_config.has_cavalry_shield;
  pose_request.rightHandOnReins = true;
  pose_request.clearanceForward = 1.15F;
  pose_request.clearanceUp = 1.05F;
  pose_request.seatPose = (speed_norm > 0.55F)
                              ? MountedPoseController::MountedSeatPose::Forward
                              : MountedPoseController::MountedSeatPose::Neutral;
  pose_request.torsoCompression = std::clamp(
      0.18F + speed_norm * 0.28F + anim_ctx.variation.posture_slump * 0.9F,
      0.0F, 0.55F);
  pose_request.torsoTwist = anim_ctx.variation.shoulder_tilt * 3.0F;
  pose_request.shoulderDip =
      std::clamp(anim_ctx.variation.shoulder_tilt * 0.6F +
                     (m_config.has_cavalry_shield ? 0.18F : 0.08F),
                 -0.4F, 0.4F);

  if (m_config.has_cavalry_shield) {
    pose_request.shieldPose = MountedPoseController::MountedShieldPose::Guard;
  }

  if (anim.is_attacking && anim.is_melee) {
    pose_request.weaponPose =
        MountedPoseController::MountedWeaponPose::SwordStrike;
    pose_request.shieldPose =
        m_config.has_cavalry_shield
            ? MountedPoseController::MountedShieldPose::Stowed
            : pose_request.shieldPose;
    pose_request.actionPhase =
        std::fmod(anim.time * MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
    pose_request.rightHandOnReins = false;
  } else {
    pose_request.weaponPose =
        m_config.has_sword ? MountedPoseController::MountedWeaponPose::SwordIdle
                           : MountedPoseController::MountedWeaponPose::None;
    pose_request.rightHandOnReins = !m_config.has_sword;
  }

  mounted_controller.applyPose(mount, pose_request);
  applyMountedKnightLowerBody(dims, mount, anim_ctx, pose);
}

auto MountedKnightRendererBase::computeMountedKnightExtras(
    uint32_t seed, const HumanoidVariant &v,
    const HorseDimensions &dims) const -> MountedKnightExtras {
  MountedKnightExtras extras;
  extras.horseProfile =
      makeHorseProfile(seed, v.palette.leather, v.palette.cloth);
  extras.horseProfile.dims = dims;
  extras.swordLength = 0.82F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.12F;
  extras.swordWidth = 0.042F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.008F;

  return extras;
}

void MountedKnightRendererBase::addAttachments(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  MountedKnightExtras extras;
  auto it = m_extrasCache.find(horse_seed);
  if (it != m_extrasCache.end()) {
    extras = it->second;
  } else {
    HorseDimensions dims = getScaledHorseDimensions(horse_seed);
    extras = computeMountedKnightExtras(horse_seed, v, dims);
    m_extrasCache[horse_seed] = extras;

    if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
      m_extrasCache.clear();
    }
  }

  const bool is_current_pose = (m_lastPose == &pose);
  const MountedAttachmentFrame *mount_ptr =
      (is_current_pose) ? &m_lastMount : nullptr;
  const ReinState *rein_ptr =
      (is_current_pose && m_hasLastReins) ? &m_lastReinState : nullptr;
  const AnimationInputs &anim = anim_ctx.inputs;

  m_horseRenderer.render(ctx, anim, anim_ctx, extras.horseProfile, mount_ptr,
                         rein_ptr, out);
  m_lastPose = nullptr;
  m_hasLastReins = false;

  auto &registry = EquipmentRegistry::instance();

  if (m_config.has_sword && !m_config.sword_equipment_id.empty()) {
    auto sword =
        registry.get(EquipmentCategory::Weapon, m_config.sword_equipment_id);
    if (sword) {
      SwordRenderConfig sword_config;
      sword_config.metal_color = m_config.metal_color;
      sword_config.sword_length = extras.swordLength;
      sword_config.sword_width = extras.swordWidth;

      if (auto *sword_renderer = dynamic_cast<SwordRenderer *>(sword.get())) {
        sword_renderer->setConfig(sword_config);
      }
      sword->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  if (m_config.has_cavalry_shield && !m_config.shield_equipment_id.empty()) {
    auto shield =
        registry.get(EquipmentCategory::Weapon, m_config.shield_equipment_id);
    if (shield) {
      shield->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }
}

void MountedKnightRendererBase::draw_helmet(const DrawContext &ctx,
                                            const HumanoidVariant &v,
                                            const HumanoidPose &pose,
                                            ISubmitter &out) const {
  if (m_config.helmet_equipment_id.empty()) {
    return;
  }

  auto &registry = EquipmentRegistry::instance();
  auto helmet =
      registry.get(EquipmentCategory::Helmet, m_config.helmet_equipment_id);
  if (helmet) {
    HumanoidAnimationContext anim_ctx{};
    helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
  }
}

void MountedKnightRendererBase::draw_armor(const DrawContext &ctx,
                                           const HumanoidVariant &v,
                                           const HumanoidPose &pose,
                                           const HumanoidAnimationContext &anim,
                                           ISubmitter &out) const {
  if (m_config.armor_equipment_id.empty()) {
    return;
  }

  auto &registry = EquipmentRegistry::instance();
  auto armor =
      registry.get(EquipmentCategory::Armor, m_config.armor_equipment_id);
  if (armor) {
    armor->render(ctx, pose.body_frames, v.palette, anim, out);
  }
}

auto MountedKnightRendererBase::resolve_shader_key(const DrawContext &ctx) const
    -> QString {
  std::string nation;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nationIDToString(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_swordsman_") + nation);
  }
  return QStringLiteral("horse_swordsman");
}

} // namespace Render::GL
