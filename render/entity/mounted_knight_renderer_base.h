#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/render_request.h"
#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "mounted_humanoid_renderer_base.h"

#include <QVector3D>

#include <string>
#include <vector>

namespace Render::GL {

struct MountedKnightRendererConfig {
  std::string sword_equipment_id;
  std::string shield_equipment_id;
  std::string helmet_equipment_id;
  std::string armor_equipment_id;
  std::string shoulder_equipment_id;
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float mount_scale = 0.95F;
  bool has_sword = true;
  bool has_cavalry_shield = true;
  bool has_shoulder = false;
  float helmet_offset_moving = 0.0F;

  Render::Creature::ArchetypeId rider_archetype_id{
      Render::Creature::kInvalidArchetype};

  Render::Creature::ArchetypeId mount_archetype_id{
      Render::Creature::kInvalidArchetype};
  std::vector<std::shared_ptr<IHorseEquipmentRenderer>> horse_attachments;
};

class MountedKnightRendererBase : public MountedHumanoidRendererBase {
public:
  explicit MountedKnightRendererBase(MountedKnightRendererConfig config);
  MountedKnightRendererBase(const MountedKnightRendererBase &) = delete;
  MountedKnightRendererBase &
  operator=(const MountedKnightRendererBase &) = delete;
  MountedKnightRendererBase(MountedKnightRendererBase &&) = delete;
  MountedKnightRendererBase &operator=(MountedKnightRendererBase &&) = delete;
  ~MountedKnightRendererBase() override = default;

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

protected:
  const MountedKnightRendererConfig &config() const { return m_config; }

  void apply_riding_animation(MountedPoseController &controller,
                              MountedAttachmentFrame &mount,
                              const HumanoidAnimationContext &anim_ctx,
                              HumanoidPose &pose, const HorseDimensions &dims,
                              const ReinState &reins) const override;

private:
  MountedKnightRendererConfig m_config;
  Render::Creature::Pipeline::UnitVisualSpec m_spec{};
  void build_visual_spec();
};

} // namespace Render::GL
