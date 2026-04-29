#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include "shield_renderer.h"

#include <cstddef>

namespace Render::GL {

inline constexpr std::uint32_t k_carthage_shield_role_count = 4;

struct CarthageShieldConfig {
  float scale_multiplier = 1.0F;
};

class CarthageShieldRenderer : public ShieldRenderer {
public:
  explicit CarthageShieldRenderer(float scale_multiplier = 1.0F);

  static void submit(const CarthageShieldConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto base_config() const -> CarthageShieldConfig {
    return {m_scale_multiplier};
  }

private:
  float m_scale_multiplier = 1.0F;
};

[[nodiscard]] auto
carthage_shield_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                                 std::size_t max) -> std::uint32_t;

[[nodiscard]] auto carthage_shield_make_static_attachment(
    const CarthageShieldConfig &config,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
