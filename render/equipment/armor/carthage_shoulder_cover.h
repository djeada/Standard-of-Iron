#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QMatrix4x4>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct CarthageShoulderCoverConfig {
  float outward_scale = 1.0F;
};

class CarthageShoulderCoverRenderer : public IEquipmentRenderer {
public:
  explicit CarthageShoulderCoverRenderer(float outward_scale = 1.0F)
      : m_config{outward_scale} {}

  static void submit(const CarthageShoulderCoverConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto
  base_config() const -> const CarthageShoulderCoverConfig & {
    return m_config;
  }

private:
  CarthageShoulderCoverConfig m_config;
};

auto carthage_shoulder_cover_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kCarthageShoulderCoverRoleCount = 2;

auto carthage_shoulder_cover_fill_role_colors(const HumanoidPalette &palette,
                                              QVector3D *out,
                                              std::size_t max) -> std::uint32_t;

auto carthage_shoulder_cover_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const QMatrix4x4 &bind_shoulder_frame)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
