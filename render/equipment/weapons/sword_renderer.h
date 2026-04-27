

#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct SwordRenderConfig {
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float sword_length = 0.80F;
  float sword_width = 0.065F;
  float guard_half_width = 0.12F;
  float handle_radius = 0.016F;
  float pommel_radius = 0.045F;
  float blade_ricasso = 0.16F;
  float blade_taper_bias = 0.65F;
  bool has_scabbard = true;
  int material_id = 3;
};

class SwordRenderer : public IEquipmentRenderer {
public:
  explicit SwordRenderer(SwordRenderConfig config = {});

  static void submit(const SwordRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const SwordRenderConfig & {
    return m_base;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  SwordRenderConfig m_base;
};

inline constexpr std::uint32_t kSwordRoleCount = 4U;
inline constexpr std::uint32_t kScabbardRoleCount = 2U;

auto sword_fill_role_colors(const HumanoidPalette &palette,
                            const SwordRenderConfig &config, QVector3D *out,
                            std::size_t max) -> std::uint32_t;

auto sword_make_static_attachment(const SwordRenderConfig &config,
                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto scabbard_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                               std::size_t max) -> std::uint32_t;

auto scabbard_make_static_attachment(
    float sheath_r, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
