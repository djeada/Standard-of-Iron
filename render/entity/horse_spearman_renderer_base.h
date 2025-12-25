#pragma once

#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "../equipment/i_equipment_renderer.h"
#include "mounted_humanoid_renderer_base.h"

#include <QString>
#include <QVector3D>

#include <memory>
#include <string>
#include <vector>

namespace Render::GL {

struct HorseSpearmanRendererConfig {
  std::string spear_equipment_id;
  std::string shield_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  std::string shoulder_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float mount_scale = 0.75F;
  bool has_spear = true;
  bool has_shield = false;
  bool has_shoulder = false;
  float helmet_offset_moving = 0.0F;
  std::vector<std::shared_ptr<IHorseEquipmentRenderer>> horse_attachments;
};

class HorseSpearmanRendererBase : public MountedHumanoidRendererBase {
public:
  explicit HorseSpearmanRendererBase(HorseSpearmanRendererConfig config);
  HorseSpearmanRendererBase(const HorseSpearmanRendererBase &) = delete;
  HorseSpearmanRendererBase &
  operator=(const HorseSpearmanRendererBase &) = delete;
  HorseSpearmanRendererBase(HorseSpearmanRendererBase &&) = delete;
  HorseSpearmanRendererBase &operator=(HorseSpearmanRendererBase &&) = delete;
  ~HorseSpearmanRendererBase() override = default;

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
  const HorseSpearmanRendererConfig &config() const { return m_config; }

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
  void cache_equipment();

  HorseSpearmanRendererConfig m_config;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_spear;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_shield;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_shoulder;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_helmet;
  mutable std::shared_ptr<IEquipmentRenderer> m_cached_armor;
};

} 
