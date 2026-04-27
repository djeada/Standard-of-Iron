#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct SpearRenderConfig {
  QVector3D shaft_color{0.5F, 0.3F, 0.2F};
  QVector3D spearhead_color{0.70F, 0.71F, 0.76F};
  float spear_length = 1.20F;
  float shaft_radius = 0.020F;
  float spearhead_length = 0.18F;
  int material_id = 3;
};

class SpearRenderer : public IEquipmentRenderer {
public:
  explicit SpearRenderer(SpearRenderConfig config = {});

  static void submit(const SpearRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const SpearRenderConfig & {
    return m_base;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  SpearRenderConfig m_base;
};

inline constexpr std::size_t kSpearRoleCount = 4;

auto spear_fill_role_colors(const HumanoidPalette &palette,
                            const SpearRenderConfig &config, QVector3D *out,
                            std::size_t max) -> std::uint32_t;

auto spear_make_static_attachments(const SpearRenderConfig &config,
                                   std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 4>;

} // namespace Render::GL
