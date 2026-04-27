#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct RomanShoulderCoverConfig {
  float outward_scale = 1.0F;
};

class RomanShoulderCoverRenderer : public IEquipmentRenderer {
public:
  explicit RomanShoulderCoverRenderer(float outward_scale = 1.0F)
      : m_config{outward_scale} {}

  static void submit(const RomanShoulderCoverConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto base_config() const -> const RomanShoulderCoverConfig & {
    return m_config;
  }

private:
  RomanShoulderCoverConfig m_config;
};

auto roman_shoulder_cover_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kRomanShoulderCoverRoleCount = 3;

auto roman_shoulder_cover_fill_role_colors(const HumanoidPalette &palette,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t;

auto roman_shoulder_cover_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const QMatrix4x4 &bind_shoulder_frame)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
