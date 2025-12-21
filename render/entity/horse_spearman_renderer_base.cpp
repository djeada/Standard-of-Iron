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

constexpr QVector3D k_default_proportion_scale{0.80F, 0.88F, 0.88F};

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

  m_horseRenderer.set_attachments(m_config.horse_attachments);

  cache_equipment();
}

auto HorseSpearmanRendererBase::get_proportion_scaling() const -> QVector3D {

  return QVector3D{0.78F, 0.84F, 0.84F};
}

auto HorseSpearmanRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void HorseSpearmanRendererBase::adjust_variation(
    const DrawContext &, uint32_t, VariationParams &variation) const {
  variation.height_scale = 0.90F;
  variation.bulk_scale = 0.70F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.40F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseSpearmanRendererBase::get_variant(const DrawContext &ctx,
                                            uint32_t seed,
                                            HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void HorseSpearmanRendererBase::apply_riding_animation(
    MountedPoseController &mounted_controller, MountedAttachmentFrame &mount,
    const HumanoidAnimationContext &anim_ctx, HumanoidPose &pose,
    const HorseDimensions &dims, const ReinState &reins) const {
  (void)dims;
  (void)reins;
  const AnimationInputs &anim = anim_ctx.inputs;
  float const speed_norm = anim_ctx.locomotion_normalized_speed();
  bool const is_charging = speed_norm > 0.65F;

  if (anim.is_attacking && anim.is_melee) {
    if (is_charging) {
      mounted_controller.ridingCharging(mount, 1.0F);
      mounted_controller.holdSpearMounted(mount, SpearGrip::COUCHED);

      pose.neck_base -= mount.seat_forward * 0.03F;
    } else {
      float const attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
      mounted_controller.ridingSpearThrust(mount, attack_phase);
    }
  } else {
    mounted_controller.ridingIdle(mount);
  }
}

void HorseSpearmanRendererBase::draw_equipment(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  float spear_length = 1.15F + (hash_01(horse_seed ^ 0xABCDU) - 0.5F) * 0.10F;
  float spear_shaft_radius =
      0.018F + (hash_01(horse_seed ^ 0x7777U) - 0.5F) * 0.003F;

  if (m_config.has_spear && m_cached_spear) {
    SpearRenderConfig spear_config;
    spear_config.shaft_color =
        v.palette.leather * QVector3D(0.85F, 0.75F, 0.65F);
    spear_config.spearhead_color = m_config.metal_color;
    spear_config.spear_length = spear_length;
    spear_config.shaft_radius = spear_shaft_radius;
    spear_config.spearhead_length = 0.18F;

    if (auto *spear_renderer =
            dynamic_cast<SpearRenderer *>(m_cached_spear.get())) {
      spear_renderer->set_config(spear_config);
    }
    m_cached_spear->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
  }

  if (m_config.has_shield && m_cached_shield) {
    m_cached_shield->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
  }

  if (m_config.has_shoulder && m_cached_shoulder) {
    m_cached_shoulder->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
  }
}

void HorseSpearmanRendererBase::draw_helmet(const DrawContext &ctx,
                                            const HumanoidVariant &v,
                                            const HumanoidPose &pose,
                                            ISubmitter &out) const {
  if (m_config.helmet_equipment_id.empty()) {
    return;
  }

  if (m_cached_helmet) {
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
    m_cached_helmet->render(ctx, frames, v.palette, anim_ctx, out);
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

  if (m_cached_armor) {
    m_cached_armor->render(ctx, pose.body_frames, v.palette, anim, out);
  }
}

void HorseSpearmanRendererBase::cache_equipment() {
  auto &registry = EquipmentRegistry::instance();

  if (!m_config.spear_equipment_id.empty()) {
    m_cached_spear =
        registry.get(EquipmentCategory::Weapon, m_config.spear_equipment_id);
  }

  if (!m_config.shield_equipment_id.empty()) {
    m_cached_shield =
        registry.get(EquipmentCategory::Weapon, m_config.shield_equipment_id);
  }

  if (!m_config.shoulder_equipment_id.empty()) {
    m_cached_shoulder =
        registry.get(EquipmentCategory::Armor, m_config.shoulder_equipment_id);
  }

  if (!m_config.helmet_equipment_id.empty()) {
    m_cached_helmet =
        registry.get(EquipmentCategory::Helmet, m_config.helmet_equipment_id);
  }

  if (!m_config.armor_equipment_id.empty()) {
    m_cached_armor =
        registry.get(EquipmentCategory::Armor, m_config.armor_equipment_id);
  }
}

auto HorseSpearmanRendererBase::resolve_shader_key(const DrawContext &ctx) const
    -> QString {
  std::string nation;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_spearman_") + nation);
  }
  return QStringLiteral("horse_spearman");
}

} // namespace Render::GL
