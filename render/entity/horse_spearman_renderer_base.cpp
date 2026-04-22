#include "horse_spearman_renderer_base.h"

#include "../creature/pipeline/equipment_registry.h"
#include "../equipment/equipment_registry.h"
#include "../equipment/equipment_submit.h"
#include "../equipment/weapons/shield_renderer.h"
#include "../equipment/weapons/spear_renderer.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../humanoid/mounted_pose_controller.h"
#include "../palette.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"

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
  build_visual_spec();
}

auto HorseSpearmanRendererBase::get_proportion_scaling() const -> QVector3D {
  return QVector3D{0.72F, 0.80F, 0.80F};
}

auto HorseSpearmanRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  static thread_local Render::Creature::Pipeline::MountedSpec spec;
  spec = MountedHumanoidRendererBase::mounted_visual_spec();
  spec.rider = visual_spec();
  spec.rider.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.rider.debug_name = "troops/horse_spearman/rider";
  spec.mount.debug_name = "troops/horse_spearman/horse";
  return spec;
}

auto HorseSpearmanRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  return m_spec;
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
      mounted_controller.riding_charging(mount, 1.0F);
      mounted_controller.hold_spear_mounted(mount, SpearGrip::COUCHED);

      pose.neck_base -= mount.seat_forward * 0.03F;
    } else {
      float const attack_phase =
          std::fmod(anim.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
      mounted_controller.riding_spear_thrust(mount, attack_phase);
    }
  } else {
    mounted_controller.riding_idle(mount);
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

void HorseSpearmanRendererBase::build_visual_spec() {
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

  if (m_config.has_spear) {
    QVector3D const metal_color = m_config.metal_color;
    EquipmentRecord rec{};
    rec.dispatch = [metal_color](const EquipmentSubmitContext &sub,
                                 Render::GL::EquipmentBatch &batch) {
      if (sub.ctx == nullptr || sub.frames == nullptr ||
          sub.palette == nullptr || sub.anim == nullptr) {
        return;
      }
      uint32_t const seed = sub.seed;
      float const spear_length =
          1.15F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.10F;
      float const spear_shaft_radius =
          0.018F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.003F;
      SpearRenderConfig spear_config;
      spear_config.shaft_color =
          sub.palette->leather * QVector3D(0.85F, 0.75F, 0.65F);
      spear_config.spearhead_color = metal_color;
      spear_config.spear_length = spear_length;
      spear_config.shaft_radius = spear_shaft_radius;
      spear_config.spearhead_length = 0.18F;
      SpearRenderer::submit(spear_config, *sub.ctx, *sub.frames, *sub.palette,
                            *sub.anim, batch);
    };
    m_loadout.push_back(std::move(rec));
  }

  if (m_config.has_shield && m_cached_shield) {
    m_loadout.push_back(make_legacy_equipment_record(*m_cached_shield));
  }

  if (m_config.has_shoulder && m_cached_shoulder) {
    m_loadout.push_back(make_legacy_equipment_record(*m_cached_shoulder));
  }

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/horse_spearman/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.equipment =
      std::span<const EquipmentRecord>{m_loadout.data(), m_loadout.size()};
  m_spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
}

} // namespace Render::GL
