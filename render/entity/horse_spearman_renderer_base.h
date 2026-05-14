#pragma once

#include <QVector3D>

#include <string>

#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/render_request.h"
#include "../equipment/equipment_registry.h"
#include "mounted_humanoid_renderer_base.h"

namespace Render::GL {

struct HorseSpearmanRendererConfig {
  std::string spear_equipment_id;
  std::string shield_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  std::string shoulder_equipment_id;
  std::string horse_saddle_equipment_id;
  std::string horse_bridle_equipment_id;
  std::string horse_reins_equipment_id;
  std::string horse_blanket_equipment_id;
  std::string horse_barding_equipment_id;
  std::string horse_crupper_equipment_id;
  std::string horse_decoration_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float mount_scale = 0.95F;
  bool has_spear = true;
  bool has_shield = false;
  bool has_shoulder = false;
  float helmet_offset_moving = 0.0F;

  Render::Creature::ArchetypeId rider_archetype_id{
      Render::Creature::k_invalid_archetype};

  Render::Creature::ArchetypeId mount_archetype_id{
      Render::Creature::k_invalid_archetype};

  std::string rider_debug_name{"troops/horse_spearman/rider"};
  std::string mount_debug_name{"troops/horse_spearman/horse"};

  EquipmentHandle spear_handle{k_invalid_equipment_handle};
  EquipmentHandle shield_handle{k_invalid_equipment_handle};
  EquipmentHandle helmet_handle{k_invalid_equipment_handle};
  EquipmentHandle armor_handle{k_invalid_equipment_handle};
  EquipmentHandle shoulder_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_saddle_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_bridle_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_reins_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_blanket_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_barding_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_crupper_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_decoration_handle{k_invalid_equipment_handle};
};

class HorseSpearmanRendererBase : public MountedHumanoidRendererBase {
public:
  explicit HorseSpearmanRendererBase(HorseSpearmanRendererConfig config);
  HorseSpearmanRendererBase(const HorseSpearmanRendererBase&) = delete;
  HorseSpearmanRendererBase& operator=(const HorseSpearmanRendererBase&) = delete;
  HorseSpearmanRendererBase(HorseSpearmanRendererBase&&) = delete;
  HorseSpearmanRendererBase& operator=(HorseSpearmanRendererBase&&) = delete;
  ~HorseSpearmanRendererBase() override = default;

  auto mounted_visual_spec() const
      -> const Render::Creature::Pipeline::MountedSpec& override;

  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override;

  auto get_proportion_scaling() const -> QVector3D override;
  auto get_mount_scale() const -> float override;
  void adjust_variation(const DrawContext&,
                        uint32_t,
                        VariationParams& variation) const override;
  void
  get_variant(const DrawContext& ctx, uint32_t seed, HumanoidVariant& v) const override;

protected:
  const HorseSpearmanRendererConfig& config() const { return m_config; }

private:
  void build_visual_spec();

  HorseSpearmanRendererConfig m_config;
  EquipmentHandle m_spear_handle{k_invalid_equipment_handle};
  EquipmentHandle m_shield_handle{k_invalid_equipment_handle};
  EquipmentHandle m_helmet_handle{k_invalid_equipment_handle};
  EquipmentHandle m_armor_handle{k_invalid_equipment_handle};
  EquipmentHandle m_shoulder_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_saddle_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_bridle_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_reins_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_blanket_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_barding_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_crupper_handle{k_invalid_equipment_handle};
  EquipmentHandle m_horse_decoration_handle{k_invalid_equipment_handle};
  Render::Creature::Pipeline::UnitVisualSpec m_spec{};
};

} // namespace Render::GL
