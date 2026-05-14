#include "mounted_knight_renderer_base.h"

#include <QVector3D>

#include <array>
#include <utility>

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../creature/archetype_registry.h"
#include "../equipment/equipment_registry.h"
#include "../equipment/horse_equipment_archetype.h"
#include "../equipment/humanoid_equipment_archetype.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../palette.h"
#include "mounted_knight_pose.h"
#include "renderer_constants.h"

namespace Render::GL {

namespace {

constexpr QVector3D k_default_proportion_scale{0.82F, 0.90F, 0.90F};

}

MountedKnightRendererBase::MountedKnightRendererBase(MountedKnightRendererConfig config)
    : m_config(std::move(config)) {
  auto& equipment_registry = EquipmentRegistry::instance();

  m_sword_handle = m_config.sword_handle;
  if (m_sword_handle == k_invalid_equipment_handle) {
    m_sword_handle = equipment_registry.resolve_handle(EquipmentCategory::Weapon,
                                                       m_config.sword_equipment_id);
  }
  m_config.has_sword =
      m_config.has_sword && m_sword_handle != k_invalid_equipment_handle;
  if (!m_config.has_sword) {
    m_config.sword_equipment_id.clear();
  }

  m_shield_handle = m_config.shield_handle;
  if (m_shield_handle == k_invalid_equipment_handle) {
    m_shield_handle = equipment_registry.resolve_handle(EquipmentCategory::Weapon,
                                                        m_config.shield_equipment_id);
  }
  m_config.has_cavalry_shield =
      m_config.has_cavalry_shield && m_shield_handle != k_invalid_equipment_handle;
  if (!m_config.has_cavalry_shield) {
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

  auto resolve_mount_handle = [&](EquipmentHandle& handle,
                                  EquipmentCategory category,
                                  std::string& equipment_id) {
    if (handle == k_invalid_equipment_handle && !equipment_id.empty()) {
      handle = equipment_registry.resolve_handle(category, equipment_id);
    }
  };

  m_horse_saddle_handle = m_config.horse_saddle_handle;
  resolve_mount_handle(m_horse_saddle_handle,
                       EquipmentCategory::HorseTack,
                       m_config.horse_saddle_equipment_id);
  m_horse_bridle_handle = m_config.horse_bridle_handle;
  resolve_mount_handle(m_horse_bridle_handle,
                       EquipmentCategory::HorseTack,
                       m_config.horse_bridle_equipment_id);
  m_horse_reins_handle = m_config.horse_reins_handle;
  resolve_mount_handle(m_horse_reins_handle,
                       EquipmentCategory::HorseTack,
                       m_config.horse_reins_equipment_id);
  m_horse_blanket_handle = m_config.horse_blanket_handle;
  resolve_mount_handle(m_horse_blanket_handle,
                       EquipmentCategory::HorseTack,
                       m_config.horse_blanket_equipment_id);
  m_horse_barding_handle = m_config.horse_barding_handle;
  resolve_mount_handle(m_horse_barding_handle,
                       EquipmentCategory::HorseArmor,
                       m_config.horse_barding_equipment_id);
  m_horse_crupper_handle = m_config.horse_crupper_handle;
  resolve_mount_handle(m_horse_crupper_handle,
                       EquipmentCategory::HorseArmor,
                       m_config.horse_crupper_equipment_id);
  m_horse_decoration_handle = m_config.horse_decoration_handle;
  resolve_mount_handle(m_horse_decoration_handle,
                       EquipmentCategory::HorseDecoration,
                       m_config.horse_decoration_equipment_id);

  build_visual_spec();
}

auto MountedKnightRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto MountedKnightRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec& {
  if (!m_mounted_visual_spec_baked) {
    m_mounted_visual_spec_cache = MountedHumanoidRendererBase::mounted_visual_spec();
    m_mounted_visual_spec_cache.rider = visual_spec();
    m_mounted_visual_spec_cache.rider.kind =
        Render::Creature::Pipeline::CreatureKind::Humanoid;
    m_mounted_visual_spec_cache.rider.debug_name = m_config.rider_debug_name;
    m_mounted_visual_spec_cache.mount.debug_name = m_config.mount_debug_name;
    m_mounted_visual_spec_baked = true;
  }
  return m_mounted_visual_spec_cache;
}

auto MountedKnightRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec& {
  return m_spec;
}

auto MountedKnightRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void MountedKnightRendererBase::adjust_variation(const DrawContext&,
                                                 uint32_t,
                                                 VariationParams& variation) const {
  variation.height_scale = 0.88F;
  variation.bulk_scale = 0.76F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void MountedKnightRendererBase::get_variant(const DrawContext& ctx,
                                            uint32_t seed,
                                            HumanoidVariant& v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void MountedKnightRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  const Render::Creature::ArchetypeId base_rider_id =
      (m_config.rider_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::k_rider_base;
  const std::array<EquipmentHandle, 5> handles{
      m_helmet_handle,
      m_config.has_shoulder ? m_shoulder_handle : k_invalid_equipment_handle,
      m_config.has_cavalry_shield ? m_shield_handle : k_invalid_equipment_handle,
      m_armor_handle,
      m_config.has_sword ? m_sword_handle : k_invalid_equipment_handle,
  };

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = m_config.rider_debug_name;
  const QVector3D scale = get_proportion_scaling();
  m_spec.scaling = ProportionScaling{scale.x(), scale.y(), scale.z()};
  m_spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
  m_spec.archetype_id = resolve_humanoid_equipment_archetype(
      m_config.rider_debug_name, base_rider_id, handles);
  m_spec.creature_asset_id = m_config.rider_creature_asset_id;

  const Render::Creature::ArchetypeId base_mount_id =
      (m_config.mount_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.mount_archetype_id
          : Render::Creature::ArchetypeRegistry::k_horse_base;
  const std::array<EquipmentHandle, 7> mount_handles{
      m_horse_saddle_handle,
      m_horse_bridle_handle,
      m_horse_reins_handle,
      m_horse_blanket_handle,
      m_horse_barding_handle,
      m_horse_crupper_handle,
      m_horse_decoration_handle,
  };
  set_mount_archetype_id(resolve_horse_equipment_archetype(
      m_config.mount_debug_name, base_mount_id, mount_handles));
}

} // namespace Render::GL
