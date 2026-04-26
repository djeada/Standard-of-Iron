#include "horse_spearman_renderer_base.h"

#include "../creature/archetype_registry.h"
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
  set_mount_archetype_id(m_config.mount_archetype_id);
  build_visual_spec();
}

auto HorseSpearmanRendererBase::get_proportion_scaling() const -> QVector3D {
  return QVector3D{0.72F, 0.80F, 0.80F};
}

auto HorseSpearmanRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  if (!m_mounted_visual_spec_baked) {
    m_mounted_visual_spec_cache =
        MountedHumanoidRendererBase::mounted_visual_spec();
    m_mounted_visual_spec_cache.rider = visual_spec();
    m_mounted_visual_spec_cache.rider.kind =
        Render::Creature::Pipeline::CreatureKind::Humanoid;
    m_mounted_visual_spec_cache.rider.archetype_id =
        Render::Creature::ArchetypeRegistry::kRiderBase;
    m_mounted_visual_spec_cache.rider.debug_name =
        "troops/horse_spearman/rider";
    m_mounted_visual_spec_cache.mount.debug_name =
        "troops/horse_spearman/horse";
    m_mounted_visual_spec_baked = true;
  }
  return m_mounted_visual_spec_cache;
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

void HorseSpearmanRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/horse_spearman/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
  m_spec.archetype_id =
      (m_config.rider_archetype_id != Render::Creature::kInvalidArchetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::kRiderBase;
}

} // namespace Render::GL
