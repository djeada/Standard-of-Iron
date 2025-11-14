#pragma once

#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "../humanoid/rig.h"
#include "horse_renderer.h"

#include <QString>
#include <QVector3D>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Render::GL {

struct HorseSpearmanRendererConfig {
  std::string spear_equipment_id;
  std::string shield_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float mount_scale = 0.75F;
  bool has_spear = true;
  bool has_shield = false;
  std::vector<std::shared_ptr<IHorseEquipmentRenderer>> horse_attachments;
};

class HorseSpearmanRendererBase : public HumanoidRendererBase {
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
  void customize_pose(const DrawContext &ctx,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override;
  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override;
  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override;
  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override;

  auto resolve_shader_key(const DrawContext &ctx) const -> QString;

protected:
  const HorseSpearmanRendererConfig &config() const { return m_config; }

private:
  struct HorseSpearmanExtras {
    HorseProfile horse_profile;
    float spear_length = 1.20F;
    float spear_shaft_radius = 0.020F;
  };

  auto get_scaled_horse_dimensions(uint32_t seed) const -> HorseDimensions;
  auto compute_horse_spearman_extras(uint32_t seed, const HumanoidVariant &v,
                                     const HorseDimensions &dims) const
      -> HorseSpearmanExtras;

  HorseSpearmanRendererConfig m_config;
  mutable std::unordered_map<uint32_t, HorseSpearmanExtras> m_extras_cache;
  mutable const HumanoidPose *m_last_pose = nullptr;
  mutable MountedAttachmentFrame m_last_mount{};
  mutable ReinState m_last_rein_state{};
  mutable bool m_has_last_reins = false;
  HorseRenderer m_horseRenderer;
};

} // namespace Render::GL
