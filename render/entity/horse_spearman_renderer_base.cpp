#include "horse_spearman_renderer_base.h"

#include <QVector3D>

#include <array>
#include <utility>

#include "animation/rig/humanoid_proportions.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "mounted_knight_pose.h"
#include "render/creature/archetype_registry.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/horse_equipment_archetype.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"
#include "render/palette.h"
#include "renderer_constants.h"

namespace Render::GL {

namespace {

constexpr auto k_profile =
    Render::GL::Humanoid::k_mounted_rider_proportion_profile.with_offset(
        {.x = -0.02F, .y = -0.01F, .z = -0.02F});

}

HorseSpearmanRendererBase::HorseSpearmanRendererBase(HorseSpearmanRendererConfig config)
    : m_config(std::move(config))
    , m_spear_handle(m_config.spear_handle) {
  auto& equipment_registry = EquipmentRegistry::instance();

  if (m_spear_handle == k_invalid_equipment_handle) {
    m_spear_handle = equipment_registry.resolve_handle(EquipmentCategory::Weapon,
                                                       m_config.spear_equipment_id);
  }
  m_config.has_spear =
      m_config.has_spear && m_spear_handle != k_invalid_equipment_handle;
  if (!m_config.has_spear) {
    m_config.spear_equipment_id.clear();
  }

  m_shield_handle = m_config.shield_handle;
  if (m_shield_handle == k_invalid_equipment_handle) {
    m_shield_handle = equipment_registry.resolve_handle(EquipmentCategory::Weapon,
                                                        m_config.shield_equipment_id);
  }
  m_config.has_shield =
      m_config.has_shield && m_shield_handle != k_invalid_equipment_handle;
  if (!m_config.has_shield) {
    m_config.shield_equipment_id.clear();
  }

  m_helmet_handle = m_config.helmet_handle;
  if (m_helmet_handle == k_invalid_equipment_handle) {
    m_helmet_handle = equipment_registry.resolve_handle(EquipmentCategory::Helmet,
                                                        m_config.helmet_equipment_id);
  }

  m_armor_handle = m_config.armor_handle;
  if (m_armor_handle == k_invalid_equipment_handle) {
    m_armor_handle = equipment_registry.resolve_handle(EquipmentCategory::Armor,
                                                       m_config.armor_equipment_id);
  }

  m_shoulder_handle = m_config.shoulder_handle;
  if (m_shoulder_handle == k_invalid_equipment_handle) {
    m_shoulder_handle = equipment_registry.resolve_handle(
        EquipmentCategory::Armor, m_config.shoulder_equipment_id);
  }
  m_config.has_shoulder =
      m_config.has_shoulder && m_shoulder_handle != k_invalid_equipment_handle;
  if (!m_config.has_shoulder) {
    m_config.shoulder_equipment_id.clear();
  }

  m_horse_handles = resolve_mounted_horse_handles(m_config);

  build_visual_spec();
}

auto HorseSpearmanRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void HorseSpearmanRendererBase::adjust_variation(const DrawContext&,
                                                 uint32_t,
                                                 VariationParams& variation) const {
  variation.height_scale = 0.90F;
  variation.bulk_scale = 0.70F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.40F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseSpearmanRendererBase::get_variant(const DrawContext& ctx,
                                            uint32_t seed,
                                            HumanoidVariant& v) const {
  HumanoidRendererBase::get_variant(ctx, seed, v);
}

void HorseSpearmanRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  const Render::Creature::ArchetypeId base_rider_id =
      (m_config.rider_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::k_rider_base;
  const std::array<EquipmentHandle, 5> handles{
      m_helmet_handle,
      m_config.has_shoulder ? m_shoulder_handle : k_invalid_equipment_handle,
      m_config.has_shield ? m_shield_handle : k_invalid_equipment_handle,
      m_config.has_spear ? m_spear_handle : k_invalid_equipment_handle,
      m_armor_handle,
  };

  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;
  spec.debug_name = m_config.rider_debug_name;
  spec.scaling = k_profile.as_pipeline_scaling();
  spec.archetype_id = resolve_humanoid_equipment_archetype(
      m_config.rider_debug_name, base_rider_id, handles);

  set_visual_spec(spec);

  const Render::Creature::ArchetypeId base_mount_id =
      (m_config.mount_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.mount_archetype_id
          : Render::Creature::ArchetypeRegistry::k_horse_base;
  const auto mount_handles = m_horse_handles.as_array();
  set_mount_visual(resolve_horse_equipment_archetype(
                       m_config.mount_debug_name, base_mount_id, mount_handles),
                   m_config.mount_debug_name);
}

} // namespace Render::GL
