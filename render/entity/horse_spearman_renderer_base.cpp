#include "horse_spearman_renderer_base.h"

#include "../creature/archetype_registry.h"
#include "../equipment/equipment_registry.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
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
  auto &equipment_registry = EquipmentRegistry::instance();
  m_spear_handle = m_config.spear_handle;
  if (m_spear_handle == k_invalid_equipment_handle) {
    m_spear_handle = equipment_registry.resolve_handle(
        EquipmentCategory::Weapon, m_config.spear_equipment_id);
  }
  m_config.has_spear =
      m_config.has_spear && m_spear_handle != k_invalid_equipment_handle;
  if (!m_config.has_spear) {
    m_config.spear_equipment_id.clear();
  }

  m_shield_handle = m_config.shield_handle;
  if (m_shield_handle == k_invalid_equipment_handle) {
    m_shield_handle = equipment_registry.resolve_handle(
        EquipmentCategory::Weapon, m_config.shield_equipment_id);
  }
  m_config.has_shield =
      m_config.has_shield && m_shield_handle != k_invalid_equipment_handle;
  if (!m_config.has_shield) {
    m_config.shield_equipment_id.clear();
  }

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

void HorseSpearmanRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = "troops/horse_spearman/rider";
  QVector3D const ps = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{ps.x(), ps.y(), ps.z()};
  m_spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
  m_spec.archetype_id =
      (m_config.rider_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::k_rider_base;
}

} // namespace Render::GL
