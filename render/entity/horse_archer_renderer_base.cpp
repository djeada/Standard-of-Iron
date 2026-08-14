#include "horse_archer_renderer_base.h"

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
#include "render/humanoid/humanoid_math.h"
#include "render/humanoid/humanoid_proportion_profiles.h"
#include "render/palette.h"
#include "renderer_constants.h"

namespace Render::GL {

namespace {

constexpr auto k_profile = Render::GL::Humanoid::k_mounted_rider_proportion_profile;

}

HorseArcherRendererBase::HorseArcherRendererBase(HorseArcherRendererConfig config)
    : m_config(std::move(config))
    , m_bow_handle(m_config.bow_handle) {
  auto& equipment_registry = EquipmentRegistry::instance();

  if (m_bow_handle == k_invalid_equipment_handle) {
    m_bow_handle = equipment_registry.resolve_handle(EquipmentCategory::Weapon,
                                                     m_config.bow_equipment_id);
  }
  m_config.has_bow = m_config.has_bow && m_bow_handle != k_invalid_equipment_handle;
  if (!m_config.has_bow) {
    m_config.bow_equipment_id.clear();
  }

  m_quiver_handle = m_config.quiver_handle;
  if (m_quiver_handle == k_invalid_equipment_handle) {
    m_quiver_handle = equipment_registry.resolve_handle(EquipmentCategory::Weapon,
                                                        m_config.quiver_equipment_id);
  }
  m_config.has_quiver =
      m_config.has_quiver && m_quiver_handle != k_invalid_equipment_handle;
  if (!m_config.has_quiver) {
    m_config.quiver_equipment_id.clear();
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

  m_cloak_handle = m_config.cloak_handle;
  if (m_cloak_handle == k_invalid_equipment_handle) {
    m_cloak_handle = equipment_registry.resolve_handle(EquipmentCategory::Armor,
                                                       m_config.cloak_equipment_id);
  }
  m_config.has_cloak =
      m_config.has_cloak && m_cloak_handle != k_invalid_equipment_handle;
  if (!m_config.has_cloak) {
    m_config.cloak_equipment_id.clear();
  }

  m_horse_handles = resolve_mounted_horse_handles(m_config);

  build_visual_spec();
}

auto HorseArcherRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_profile.as_vector();
}

auto HorseArcherRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec& {
  return m_spec;
}

auto HorseArcherRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void HorseArcherRendererBase::adjust_variation(const DrawContext&,
                                               uint32_t,
                                               VariationParams& variation) const {
  variation.height_scale = 0.88F;
  variation.bulk_scale = 0.72F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseArcherRendererBase::get_variant(const DrawContext& ctx,
                                          uint32_t seed,
                                          HumanoidVariant& v) const {
  HumanoidRendererBase::get_variant(ctx, seed, v);
}

void HorseArcherRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  const Render::Creature::ArchetypeId base_rider_id =
      (m_config.rider_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::k_rider_base;
  const std::array<EquipmentHandle, 5> handles{
      m_helmet_handle,
      m_armor_handle,
      m_config.has_cloak ? m_cloak_handle : k_invalid_equipment_handle,
      m_config.has_bow ? m_bow_handle : k_invalid_equipment_handle,
      m_config.has_quiver ? m_quiver_handle : k_invalid_equipment_handle,
  };

  m_spec = UnitVisualSpec{};
  m_spec.kind = CreatureKind::Humanoid;
  m_spec.debug_name = m_config.rider_debug_name;
  m_spec.scaling = k_profile.as_pipeline_scaling();
  m_spec.archetype_id = resolve_humanoid_equipment_archetype(
      m_config.rider_debug_name, base_rider_id, handles);

  const Render::Creature::ArchetypeId base_mount_id =
      (m_config.mount_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.mount_archetype_id
          : Render::Creature::ArchetypeRegistry::k_horse_base;
  const auto mount_handles = m_horse_handles.as_array();
  set_mount_visual(resolve_horse_equipment_archetype(
                       m_config.mount_debug_name, base_mount_id, mount_handles),
                   m_config.mount_debug_name);
}

void HorseArcherRendererBase::append_companion_preparation(
    const DrawContext& ctx,
    const HumanoidVariant& variant,
    const HumanoidPose& pose,
    const HumanoidAnimationContext& anim_ctx,
    std::uint32_t seed,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::CreaturePreparationResult& out) const {
  MountedHumanoidRendererBase::append_companion_preparation(
      ctx, variant, pose, anim_ctx, seed, lod, out);
}

} // namespace Render::GL
