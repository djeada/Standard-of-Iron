#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/cache/static_attachment_spec.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/spear_pose_utils.h"
#include "render/equipment/i_equipment_renderer.h"
#include "render/frame/palette.h"

namespace Render::GL {

class SpearRenderer : public IEquipmentRenderer {
public:
  explicit SpearRenderer(SpearRenderConfig config = {});

  static void submit(const SpearRenderConfig& config,
                     const DrawContext& ctx,
                     const BodyFrames& frames,
                     const HumanoidPalette& palette,
                     const HumanoidAnimationContext& anim,
                     EquipmentBatch& batch);

  [[nodiscard]] auto base_config() const noexcept -> const SpearRenderConfig& {
    return m_base;
  }

  void render(const DrawContext& ctx,
              const BodyFrames& frames,
              const HumanoidPalette& palette,
              const HumanoidAnimationContext& anim,
              EquipmentBatch& batch) override;

private:
  SpearRenderConfig m_base;
};

inline constexpr std::size_t k_spear_role_count = 4;

auto spear_fill_role_colors(const HumanoidPalette& palette,
                            const SpearRenderConfig& config,
                            QVector3D* out,
                            std::size_t max) -> std::uint32_t;

auto spear_make_static_attachments(const SpearRenderConfig& config,
                                   std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 4>;

} // namespace Render::GL
