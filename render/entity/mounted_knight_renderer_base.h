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

struct MountedKnightRendererConfig {
  std::string sword_equipment_id;
  std::string shield_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float mount_scale = 0.75F;
  bool has_sword = true;
  bool has_cavalry_shield = true;
  std::vector<std::shared_ptr<IHorseEquipmentRenderer>> horse_attachments;
};

class MountedKnightRendererBase : public HumanoidRendererBase {
public:
  explicit MountedKnightRendererBase(MountedKnightRendererConfig config);
  MountedKnightRendererBase(const MountedKnightRendererBase &) = delete;
  MountedKnightRendererBase &
  operator=(const MountedKnightRendererBase &) = delete;
  MountedKnightRendererBase(MountedKnightRendererBase &&) = delete;
  MountedKnightRendererBase &operator=(MountedKnightRendererBase &&) = delete;
  ~MountedKnightRendererBase() override = default;

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
  const MountedKnightRendererConfig &config() const { return m_config; }

private:
  struct MountedKnightExtras {
    HorseProfile horseProfile;
    float swordLength = 0.85F;
    float swordWidth = 0.045F;
  };

  auto getScaledHorseDimensions(uint32_t seed) const -> HorseDimensions;
  auto computeMountedKnightExtras(uint32_t seed, const HumanoidVariant &v,
                                  const HorseDimensions &dims) const
      -> MountedKnightExtras;

  MountedKnightRendererConfig m_config;
  mutable std::unordered_map<uint32_t, MountedKnightExtras> m_extrasCache;
  mutable const HumanoidPose *m_lastPose = nullptr;
  mutable MountedAttachmentFrame m_lastMount{};
  mutable ReinState m_lastReinState{};
  mutable bool m_hasLastReins = false;
  HorseRenderer m_horseRenderer;
};

} // namespace Render::GL
