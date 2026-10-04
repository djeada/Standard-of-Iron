#include "mounted_swordsman_renderer_base.h"

#include <QVector3D>

#include <array>
#include <span>
#include <string>
#include <utility>

#include "animation/rig/humanoid_proportions.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "mounted_swordsman_pose.h"
#include "nations/equipment_loadout_catalog.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/horse_equipment_archetype.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/runtime/style_palette.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"
#include "render/palette.h"
#include "renderer_constants.h"

namespace Render::GL {

namespace {

constexpr auto k_profile = Render::GL::Humanoid::k_mounted_rider_proportion_profile;

constexpr float k_rider_team_mix_weight = 0.6F;
constexpr float k_rider_style_mix_weight = 0.4F;
constexpr float k_rider_leather_team_mix_weight = 0.15F;

} // namespace

MountedSwordsmanRendererBase::MountedSwordsmanRendererBase(
    MountedSwordsmanRendererConfig config)
    : m_config(std::move(config))
    , m_sword_handle(m_config.sword_handle) {
  auto& equipment_registry = EquipmentRegistry::instance();

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

  m_greaves_handle = m_config.greaves_handle;
  if (m_greaves_handle == k_invalid_equipment_handle &&
      !m_config.greaves_equipment_id.empty()) {
    m_greaves_handle = equipment_registry.resolve_handle(EquipmentCategory::Armor,
                                                         m_config.greaves_equipment_id);
  }

  m_horse_handles = resolve_mounted_horse_handles(m_config);

  build_visual_spec();
}

auto MountedSwordsmanRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void MountedSwordsmanRendererBase::adjust_variation(const DrawContext&,
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

void MountedSwordsmanRendererBase::get_variant(const DrawContext& ctx,
                                               uint32_t seed,
                                               HumanoidVariant& v) const {
  HumanoidRendererBase::get_variant(ctx, seed, v);
  if (m_config.rider_style.has_value()) {
    const SwordsmanStyleConfig& style = *m_config.rider_style;
    QVector3D const team_tint = resolve_team_tint(ctx);
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target,
                           float team_weight = k_rider_team_mix_weight,
                           float style_weight = k_rider_style_mix_weight) {
      target = Humanoid::mix_palette_color(
          target, override_color, team_tint, team_weight, style_weight);
    };
    apply_color(style.cloth_color,
                v.palette.cloth,
                style.cloth_team_weight.value_or(k_rider_team_mix_weight),
                style.cloth_style_weight.value_or(k_rider_style_mix_weight));
    apply_color(
        style.leather_color, v.palette.leather, k_rider_leather_team_mix_weight);
    apply_color(style.leather_dark_color,
                v.palette.leather_dark,
                k_rider_leather_team_mix_weight);
    apply_color(style.metal_color, v.palette.metal);
  }
  if (m_config.facial_hair.has_value()) {
    v.facial_hair = *m_config.facial_hair;
  }
}

void MountedSwordsmanRendererBase::build_visual_spec() {
  using namespace Render::Creature::Pipeline;

  const Render::Creature::ArchetypeId base_rider_id =
      (m_config.rider_archetype_id != Render::Creature::k_invalid_archetype)
          ? m_config.rider_archetype_id
          : Render::Creature::ArchetypeRegistry::k_rider_base;
  const std::array<EquipmentHandle, 6> handles{
      m_helmet_handle,
      m_config.has_shoulder ? m_shoulder_handle : k_invalid_equipment_handle,
      m_config.has_cavalry_shield ? m_shield_handle : k_invalid_equipment_handle,
      m_armor_handle,
      m_config.has_sword ? m_sword_handle : k_invalid_equipment_handle,
      m_greaves_handle,
  };
  std::size_t const handle_count = m_greaves_handle != k_invalid_equipment_handle
                                       ? handles.size()
                                       : handles.size() - 1U;

  UnitVisualSpec spec{};
  spec.kind = CreatureKind::Humanoid;
  spec.debug_name = m_config.rider_debug_name;
  spec.scaling = k_profile.as_pipeline_scaling();
  spec.archetype_id = resolve_humanoid_equipment_archetype(
      m_config.rider_debug_name,
      base_rider_id,
      std::span<const EquipmentHandle>(handles.data(), handle_count));
  spec.creature_asset_id = m_config.rider_creature_asset_id;

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

auto make_mounted_swordsman_config_from_loadout(std::string_view renderer_key)
    -> MountedSwordsmanRendererConfig {
  MountedSwordsmanRendererConfig config;
  const auto loadout = Nation::resolve_equipment_loadout(renderer_key);
  config.sword_equipment_id = loadout.ids.sword;
  config.shield_equipment_id = loadout.ids.shield;
  config.helmet_equipment_id = loadout.ids.helmet;
  config.armor_equipment_id = loadout.ids.armor;
  config.shoulder_equipment_id = loadout.ids.shoulder;
  config.greaves_equipment_id = loadout.ids.greaves;
  config.horse_saddle_equipment_id = loadout.ids.horse_saddle;
  config.horse_bridle_equipment_id = loadout.ids.horse_bridle;
  config.horse_reins_equipment_id = loadout.ids.horse_reins;
  config.horse_blanket_equipment_id = loadout.ids.horse_blanket;
  config.horse_barding_equipment_id = loadout.ids.horse_barding;
  config.horse_crupper_equipment_id = loadout.ids.horse_crupper;
  config.horse_decoration_equipment_id = loadout.ids.horse_decoration;
  config.sword_handle = loadout.sword_handle;
  config.shield_handle = loadout.shield_handle;
  config.helmet_handle = loadout.helmet_handle;
  config.armor_handle = loadout.armor_handle;
  config.shoulder_handle = loadout.shoulder_handle;
  config.greaves_handle = loadout.greaves_handle;
  config.horse_saddle_handle = loadout.horse_saddle_handle;
  config.horse_bridle_handle = loadout.horse_bridle_handle;
  config.horse_reins_handle = loadout.horse_reins_handle;
  config.horse_blanket_handle = loadout.horse_blanket_handle;
  config.horse_barding_handle = loadout.horse_barding_handle;
  config.horse_crupper_handle = loadout.horse_crupper_handle;
  config.horse_decoration_handle = loadout.horse_decoration_handle;
  config.has_shoulder = loadout.shoulder_handle != k_invalid_equipment_handle;
  config.facial_hair = loadout.ids.facial_hair;
  config.rider_debug_name = std::string(renderer_key) + "/rider";
  config.mount_debug_name = std::string(renderer_key) + "/mount";
  config.rider_creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  return config;
}

} // namespace Render::GL
