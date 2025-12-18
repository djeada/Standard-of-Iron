#pragma once

#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "mounted_humanoid_renderer_base.h"

#include <QString>
#include <QVector3D>

#include <memory>
#include <string>
#include <vector>

namespace Render::GL {

struct HorseArcherRendererConfig {
  std::string bow_equipment_id;
  std::string quiver_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  std::string cloak_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  QVector3D fletching_color{0.85F, 0.40F, 0.40F};
  QVector3D cloak_color{0.14F, 0.38F, 0.54F};
  QVector3D cloak_trim_color{0.75F, 0.66F, 0.42F};
  int cloak_back_material_id = 5;
  int cloak_shoulder_material_id = 6;
  float mount_scale = 0.75F;
  bool has_bow = true;
  bool has_quiver = true;
  bool has_cloak = false;
  float helmet_offset_moving = 0.0F;
  std::vector<std::shared_ptr<IHorseEquipmentRenderer>> horse_attachments;
};

class HorseArcherRendererBase : public MountedHumanoidRendererBase {
public:
  explicit HorseArcherRendererBase(HorseArcherRendererConfig config);
  HorseArcherRendererBase(const HorseArcherRendererBase &) = delete;
  HorseArcherRendererBase &operator=(const HorseArcherRendererBase &) = delete;
  HorseArcherRendererBase(HorseArcherRendererBase &&) = delete;
  HorseArcherRendererBase &operator=(HorseArcherRendererBase &&) = delete;
  ~HorseArcherRendererBase() override = default;

  auto get_proportion_scaling() const -> QVector3D override;
  auto get_mount_scale() const -> float override;
  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override;
  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override;

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override;
  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override;

  auto resolve_shader_key(const DrawContext &ctx) const -> QString;

protected:
  const HorseArcherRendererConfig &config() const { return m_config; }

  void apply_riding_animation(MountedPoseController &controller,
                              MountedAttachmentFrame &mount,
                              const HumanoidAnimationContext &anim_ctx,
                              HumanoidPose &pose, const HorseDimensions &dims,
                              const ReinState &reins) const override;

  void draw_equipment(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override;

private:
  HorseArcherRendererConfig m_config;
};

} 
