#include "mounted_knight_renderer_base.h"

#include "../creature/pipeline/equipment_registry.h"
#include "../equipment/equipment_registry.h"
#include "../equipment/equipment_submit.h"
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

  cache_equipment();
  build_visual_spec();
}

auto MountedKnightRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto MountedKnightRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  static thread_local Render::Creature::Pipeline::MountedSpec spec;
  spec = MountedHumanoidRendererBase::mounted_visual_spec();
  spec.rider = visual_spec();
  spec.rider.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.rider.debug_name = "troops/mounted_knight/rider";
  spec.mount.debug_name = "troops/mounted_knight/horse";
  return spec;
}

auto MountedKnightRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  return m_spec;
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

void MountedKnightRendererBase::cache_equipment() {
  auto &registry = EquipmentRegistry::instance();
  if (!m_config.helmet_equipment_id.empty()) {
    m_cached_helmet =
        registry.get(EquipmentCategory::Helmet, m_config.helmet_equipment_id);
  }
  if (!m_config.armor_equipment_id.empty()) {
    m_cached_armor =
        registry.get(EquipmentCategory::Armor, m_config.armor_equipment_id);
  }
  if (!m_config.shoulder_equipment_id.empty()) {
    m_cached_shoulder =
        registry.get(EquipmentCategory::Armor, m_config.shoulder_equipment_id);
  }
  if (!m_config.shield_equipment_id.empty()) {
    m_cached_shield =
        registry.get(EquipmentCategory::Weapon, m_config.shield_equipment_id);
  }
}

void MountedKnightRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  m_loadout.clear();

  if (m_cached_helmet) {
    auto *helmet_ptr = m_cached_helmet.get();
    float const helmet_offset = m_config.helmet_offset_moving;
    EquipmentRecord rec{};
    rec.dispatch = [helmet_ptr,
                    helmet_offset](const EquipmentSubmitContext &sub,
                                   Render::GL::EquipmentBatch &batch) {
      if (helmet_ptr == nullptr || sub.ctx == nullptr ||
          sub.frames == nullptr || sub.palette == nullptr) {
        return;
      }
      BodyFrames frames = *sub.frames;
      if (sub.ctx->entity != nullptr && helmet_offset > 0.0F) {
        if (auto *move = sub.ctx->entity
                             ->get_component<Engine::Core::MovementComponent>();
            move != nullptr) {
          float const speed_sq = move->vx * move->vx + move->vz * move->vz;
          if (speed_sq > 0.0001F) {
            frames.head.origin += frames.head.forward * helmet_offset;
          }
        }
      }
      HumanoidAnimationContext anim_ctx{};
      helmet_ptr->render(*sub.ctx, frames, *sub.palette, anim_ctx, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  if (m_cached_armor) {
    m_loadout.push_back(make_legacy_equipment_record(*m_cached_armor));
  }

  if (m_config.has_shoulder && m_cached_shoulder) {
    m_loadout.push_back(make_legacy_equipment_record(*m_cached_shoulder));
  }

  if (m_config.has_sword) {
    QVector3D const metal_color = m_config.metal_color;
    EquipmentRecord rec{};
    rec.dispatch = [metal_color](const EquipmentSubmitContext &sub,
                                 Render::GL::EquipmentBatch &batch) {
      if (sub.ctx == nullptr || sub.frames == nullptr ||
          sub.palette == nullptr || sub.anim == nullptr) {
        return;
      }
      uint32_t const seed = sub.seed;
      SwordRenderConfig sword_config;
      sword_config.metal_color = metal_color;
      sword_config.sword_length =
          0.82F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.12F;
      sword_config.sword_width =
          0.042F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.008F;
      SwordRenderer::submit(sword_config, *sub.ctx, *sub.frames, *sub.palette,
                            *sub.anim, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  if (m_config.has_cavalry_shield && m_cached_shield) {
    m_loadout.push_back(make_legacy_equipment_record(*m_cached_shield));
  }

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/mounted_knight/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.equipment =
      std::span<const EquipmentRecord>{m_loadout.data(), m_loadout.size()};

  m_spec.owned_legacy_slots = LegacySlotMask::Helmet | LegacySlotMask::Armor;
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
