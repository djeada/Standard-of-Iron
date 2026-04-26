

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

struct ShieldRenderConfig {
  QVector3D shield_color{0.7F, 0.3F, 0.2F};
  QVector3D trim_color{0.72F, 0.73F, 0.78F};
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float shield_radius = 0.18F;
  float shield_aspect = 1.0F;
  bool has_cross_decal = false;
  int material_id = 4;
};

class ShieldRenderer : public IEquipmentRenderer {
public:
  explicit ShieldRenderer(ShieldRenderConfig config = {});

  static void submit(const ShieldRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto
  base_config() const noexcept -> const ShieldRenderConfig & {
    return m_base;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ShieldRenderConfig m_base;
};

inline constexpr std::uint32_t kShieldRoleCount = 6;

auto shield_fill_role_colors(const HumanoidPalette &palette,
                             const ShieldRenderConfig &config, QVector3D *out,
                             std::size_t max) -> std::uint32_t;

auto shield_make_static_attachment(const ShieldRenderConfig &config,
                                   std::uint16_t socket_bone_index,
                                   std::uint8_t base_role_byte,
                                   const QMatrix4x4 &bind_hand_l_matrix)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
