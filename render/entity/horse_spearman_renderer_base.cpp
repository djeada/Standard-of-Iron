#include "horse_spearman_renderer_base.h"

#include "../equipment/equipment_registry.h"
#include "../equipment/weapons/shield_renderer.h"
#include "../equipment/weapons/spear_renderer.h"
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

constexpr QVector3D k_default_proportion_scale{0.96F, 0.90F, 0.98F};

}

HorseSpearmanRendererBase::HorseSpearmanRendererBase(
    HorseSpearmanRendererConfig config)
    : m_config(std::move(config)) {
  m_config.has_spear =
      m_config.has_spear && !m_config.spear_equipment_id.empty();
  if (!m_config.has_spear) {
    m_config.spear_equipment_id.clear();
  }

  m_config.has_shield =
      m_config.has_shield && !m_config.shield_equipment_id.empty();
  if (!m_config.has_shield) {
    m_config.shield_equipment_id.clear();
  }

  m_horseRenderer.setAttachments(m_config.horse_attachments);
}

auto HorseSpearmanRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto HorseSpearmanRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void HorseSpearmanRendererBase::adjust_variation(
    const DrawContext &, uint32_t, VariationParams &variation) const {
  variation.height_scale = 0.90F;
  variation.bulk_scale = 0.85F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseSpearmanRendererBase::get_variant(const DrawContext &ctx,
                                            uint32_t seed,
                                            HumanoidVariant &v) const {
  QVector3D const team_tint = resolveTeamTint(ctx);
  v.palette = makeHumanoidPalette(team_tint, seed);
}

auto HorseSpearmanRendererBase::get_scaled_horse_dimensions(uint32_t seed) const
    -> HorseDimensions {
  HorseDimensions dims = makeHorseDimensions(seed);
  scaleHorseDimensions(dims, get_mount_scale());
  return dims;
}

void HorseSpearmanRendererBase::customize_pose(
    const DrawContext &ctx, const HumanoidAnimationContext &anim_ctx,
    uint32_t seed, HumanoidPose &pose) const {
  const AnimationInputs &anim = anim_ctx.inputs;

  uint32_t horse_seed = seed;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseDimensions dims = get_scaled_horse_dimensions(horse_seed);
  HorseProfile mount_profile{};
  mount_profile.dims = dims;
  MountedAttachmentFrame mount = compute_mount_frame(mount_profile);
  tuneMountedKnightFrame(dims, mount);
  HorseMotionSample const motion =
      evaluate_horse_motion(mount_profile, anim, anim_ctx);
  apply_mount_vertical_offset(mount, motion.bob);

  m_last_pose = &pose;
  m_last_mount = mount;

  ReinState const reins = compute_rein_state(horse_seed, anim_ctx);
  m_last_rein_state = reins;
  m_has_last_reins = true;

  MountedPoseController mounted_controller(pose, anim_ctx);

  mounted_controller.mountOnHorse(mount);

  float const speed_norm = anim_ctx.locomotion_normalized_speed();
  bool const is_charging = speed_norm > 0.65F;

  if (anim.is_attacking && anim.is_melee) {
    if (is_charging) {
      mounted_controller.ridingCharging(mount, 1.0F);
      mounted_controller.holdSpearMounted(mount, SpearGrip::COUCHED);
    } else {
      float const attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
      mounted_controller.ridingSpearThrust(mount, attack_phase);
    }
  } else {
    mounted_controller.ridingIdle(mount);
  }

  applyMountedKnightLowerBody(dims, mount, anim_ctx, pose);
}

auto HorseSpearmanRendererBase::compute_horse_spearman_extras(
    uint32_t seed, const HumanoidVariant &v,
    const HorseDimensions &dims) const -> HorseSpearmanExtras {
  HorseSpearmanExtras extras;
  extras.horse_profile =
      makeHorseProfile(seed, v.palette.leather, v.palette.cloth);
  extras.horse_profile.dims = dims;
  extras.spear_length = 1.15F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.10F;
  extras.spear_shaft_radius = 0.018F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.003F;

  return extras;
}

void HorseSpearmanRendererBase::addAttachments(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseSpearmanExtras extras;
  auto it = m_extras_cache.find(horse_seed);
  if (it != m_extras_cache.end()) {
    extras = it->second;
  } else {
    HorseDimensions dims = get_scaled_horse_dimensions(horse_seed);
    extras = compute_horse_spearman_extras(horse_seed, v, dims);
    m_extras_cache[horse_seed] = extras;

    if (m_extras_cache.size() > MAX_EXTRAS_CACHE_SIZE) {
      m_extras_cache.clear();
    }
  }

  const bool is_current_pose = (m_last_pose == &pose);
  const MountedAttachmentFrame *mount_ptr =
      (is_current_pose) ? &m_last_mount : nullptr;
  const ReinState *rein_ptr =
      (is_current_pose && m_has_last_reins) ? &m_last_rein_state : nullptr;
  const AnimationInputs &anim = anim_ctx.inputs;

  m_horseRenderer.render(ctx, anim, anim_ctx, extras.horse_profile, mount_ptr,
                         rein_ptr, out);
  m_last_pose = nullptr;
  m_has_last_reins = false;

  auto &registry = EquipmentRegistry::instance();

  if (m_config.has_spear && !m_config.spear_equipment_id.empty()) {
    auto spear =
        registry.get(EquipmentCategory::Weapon, m_config.spear_equipment_id);
    if (spear) {
      SpearRenderConfig spear_config;
      spear_config.shaft_color =
          v.palette.leather * QVector3D(0.85F, 0.75F, 0.65F);
      spear_config.spearhead_color = m_config.metal_color;
      spear_config.spear_length = extras.spear_length;
      spear_config.shaft_radius = extras.spear_shaft_radius;
      spear_config.spearhead_length = 0.18F;

      if (auto *spear_renderer = dynamic_cast<SpearRenderer *>(spear.get())) {
        spear_renderer->setConfig(spear_config);
      }
      spear->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  if (m_config.has_shield && !m_config.shield_equipment_id.empty()) {
    auto shield =
        registry.get(EquipmentCategory::Weapon, m_config.shield_equipment_id);
    if (shield) {
      shield->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }
}

void HorseSpearmanRendererBase::draw_helmet(const DrawContext &ctx,
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

void HorseSpearmanRendererBase::draw_armor(const DrawContext &ctx,
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

auto HorseSpearmanRendererBase::resolve_shader_key(const DrawContext &ctx) const
    -> QString {
  std::string nation;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nationIDToString(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_spearman_") + nation);
  }
  return QStringLiteral("horse_spearman");
}

} // namespace Render::GL
