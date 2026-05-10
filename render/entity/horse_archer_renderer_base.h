#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/render_request.h"
#include "../equipment/equipment_registry.h"
#include "mounted_humanoid_renderer_base.h"

#include <QVector3D>

#include <string>

namespace Render::GL {

struct HorseArcherRendererConfig {
  std::string bow_equipment_id;
  std::string quiver_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  std::string cloak_equipment_id;
  std::string horse_saddle_equipment_id;
  std::string horse_bridle_equipment_id;
  std::string horse_reins_equipment_id;
  std::string horse_blanket_equipment_id;
  std::string horse_barding_equipment_id;
  std::string horse_crupper_equipment_id;
  std::string horse_decoration_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  QVector3D fletching_color{0.85F, 0.40F, 0.40F};
  QVector3D cloak_color{0.14F, 0.38F, 0.54F};
  QVector3D cloak_trim_color{0.75F, 0.66F, 0.42F};
  int cloak_back_material_id = 5;
  int cloak_shoulder_material_id = 6;
  float mount_scale = 0.95F;
  bool has_bow = true;
  bool has_quiver = true;
  bool has_cloak = false;
  float helmet_offset_moving = 0.0F;

  Render::Creature::ArchetypeId rider_archetype_id{
      Render::Creature::k_invalid_archetype};

  Render::Creature::ArchetypeId mount_archetype_id{
      Render::Creature::k_invalid_archetype};

  std::string rider_debug_name{"troops/horse_archer/rider"};
  std::string mount_debug_name{"troops/horse_archer/horse"};

  EquipmentHandle bow_handle{k_invalid_equipment_handle};
  EquipmentHandle quiver_handle{k_invalid_equipment_handle};
  EquipmentHandle helmet_handle{k_invalid_equipment_handle};
  EquipmentHandle armor_handle{k_invalid_equipment_handle};
  EquipmentHandle cloak_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_saddle_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_bridle_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_reins_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_blanket_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_barding_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_crupper_handle{k_invalid_equipment_handle};
  EquipmentHandle horse_decoration_handle{k_invalid_equipment_handle};
};

class HorseArcherRendererBase : public MountedHumanoidRendererBase {
public:
  explicit HorseArcherRendererBase(HorseArcherRendererConfig config);
  HorseArcherRendererBase(const HorseArcherRendererBase &) = delete;
  HorseArcherRendererBase &operator=(const HorseArcherRendererBase &) = delete;
  HorseArcherRendererBase(HorseArcherRendererBase &&) = delete;
  HorseArcherRendererBase &operator=(HorseArcherRendererBase &&) = delete;
  ~HorseArcherRendererBase() override = default;

  auto mounted_visual_spec() const
      -> const Render::Creature::Pipeline::MountedSpec & override;

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override;

  auto get_proportion_scaling() const -> QVector3D override;
  auto get_mount_scale() const -> float override;
  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override;
  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override;
  void append_companion_preparation(
      const DrawContext &ctx, const HumanoidVariant &variant,
      const HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx,
      std::uint32_t seed, Render::Creature::CreatureLOD lod,
      Render::Creature::Pipeline::CreaturePreparationResult &out)
      const override;

protected:
  const HorseArcherRendererConfig &config() const { return m_config; }

private:
  void build_visual_spec();

  HorseArcherRendererConfig m_config;
  EquipmentHandle m_bow_handle{k_invalid_equipment_handle};
  EquipmentHandle m_quiver_handle{k_invalid_equipment_handle};
  EquipmentHandle m_helmet_handle{k_invalid_equipment_handle};
  EquipmentHandle m_armor_handle{k_invalid_equipment_handle};
  EquipmentHandle m_cloak_handle{k_invalid_equipment_handle};
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
