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

constexpr QVector3D k_default_proportion_scale{0.80F, 0.88F, 0.88F};

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

  m_horseRenderer.set_attachments(m_config.horse_attachments);
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
  variation.bulk_scale = 0.76F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void MountedKnightRendererBase::get_variant(const DrawContext &ctx,
                                            uint32_t seed,
                                            HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void MountedKnightRendererBase::apply_riding_animation(
    MountedPoseController &mounted_controller, MountedAttachmentFrame &mount,
    const HumanoidAnimationContext &anim_ctx, HumanoidPose &pose,
    const HorseDimensions &dims, const ReinState &reins) const {
  (void)pose;
  const AnimationInputs &anim = anim_ctx.inputs;
  float const speed_norm = anim_ctx.locomotion_normalized_speed();
  float const speed_lean = 0.0F;
  float const forward_lean =
      (dims.seat_forward_offset * 0.08F + speed_lean) / 0.15F;

  MountedPoseController::MountedRiderPoseRequest pose_request;
  pose_request.dims = dims;
  pose_request.forward_bias = forward_lean;
  pose_request.rein_slack_left = reins.slack;
  pose_request.rein_slack_right = reins.slack;
  pose_request.rein_tension_left = reins.tension;
  pose_request.rein_tension_right = reins.tension;
  pose_request.left_hand_on_reins = !m_config.has_cavalry_shield;
  pose_request.right_hand_on_reins = true;
  pose_request.clearance_forward = 1.15F;
  pose_request.clearance_up = 1.05F;
  pose_request.seat_pose =
      (speed_norm > 0.55F) ? MountedPoseController::MountedSeatPose::Forward
                           : MountedPoseController::MountedSeatPose::Neutral;
  pose_request.torso_compression =
      std::clamp(0.18F + anim_ctx.variation.posture_slump * 0.9F, 0.0F, 0.55F);
  pose_request.torso_twist = anim_ctx.variation.shoulder_tilt * 3.0F;
  pose_request.shoulder_dip =
      std::clamp(anim_ctx.variation.shoulder_tilt * 0.6F +
                     (m_config.has_cavalry_shield ? 0.18F : 0.08F),
                 -0.4F, 0.4F);

  if (m_config.has_cavalry_shield) {
    pose_request.shield_pose = MountedPoseController::MountedShieldPose::Guard;
  }

  if (anim.is_attacking && anim.is_melee) {
    pose_request.weapon_pose =
        MountedPoseController::MountedWeaponPose::SwordStrike;
    pose_request.action_phase =
        std::fmod(anim.time * MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
    pose_request.right_hand_on_reins = false;
    if (m_config.has_cavalry_shield) {
      pose_request.shield_pose =
          MountedPoseController::MountedShieldPose::Stowed;
    }
  } else {
    pose_request.weapon_pose =
        m_config.has_sword ? MountedPoseController::MountedWeaponPose::SwordIdle
                           : MountedPoseController::MountedWeaponPose::None;
    pose_request.right_hand_on_reins = !m_config.has_sword;
  }

  mounted_controller.apply_pose(mount, pose_request);
}

void MountedKnightRendererBase::draw_equipment(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
  auto &registry = EquipmentRegistry::instance();

  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  float const sword_length =
      0.82F + (hash_01(horse_seed ^ 0xABCDU) - 0.5F) * 0.12F;
  float const sword_width =
      0.042F + (hash_01(horse_seed ^ 0x7777U) - 0.5F) * 0.008F;

  if (m_config.has_sword && !m_config.sword_equipment_id.empty()) {
    auto sword =
        registry.get(EquipmentCategory::Weapon, m_config.sword_equipment_id);
    if (sword) {
      SwordRenderConfig sword_config;
      sword_config.metal_color = m_config.metal_color;
      sword_config.sword_length = sword_length;
      sword_config.sword_width = sword_width;

      if (auto *sword_renderer = dynamic_cast<SwordRenderer *>(sword.get())) {
        sword_renderer->set_config(sword_config);
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

  if (m_config.has_shoulder && !m_config.shoulder_equipment_id.empty()) {
    auto shoulder_cover =
        registry.get(EquipmentCategory::Armor, m_config.shoulder_equipment_id);
    if (shoulder_cover) {
      shoulder_cover->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
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
    BodyFrames frames = pose.body_frames;
    if (ctx.entity != nullptr) {
      auto *move = ctx.entity->get_component<Engine::Core::MovementComponent>();
      if (move != nullptr) {
        float speed_sq = move->vx * move->vx + move->vz * move->vz;
        if (speed_sq > 0.0001F && m_config.helmet_offset_moving > 0.0F) {
          frames.head.origin +=
              frames.head.forward * m_config.helmet_offset_moving;
        }
      }
    }
    helmet->render(ctx, frames, v.palette, anim_ctx, out);
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
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_swordsman_") + nation);
  }
  return QStringLiteral("horse_swordsman");
}

} // namespace Render::GL
