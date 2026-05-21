

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct SwordRenderConfig {
  QVector3D metal_color{0.72F, 0.73F, 0.78F};
  float sword_length = 0.84F;
  float sword_width = 0.072F;
  float guard_half_width = 0.125F;
  float handle_radius = 0.016F;
  float pommel_radius = 0.045F;
  float pommel_length = 0.05F;
  float blade_ricasso = 0.12F;
  float blade_taper_bias = 0.62F;
  float blade_mid_width_scale = 1.12F;
  float blade_tip_width_scale = 0.16F;
  float blade_curve = 0.02F;
  float guard_curve = 0.03F;
  float guard_spike_length = 0.06F;
  bool has_scabbard = true;
  int material_id = 3;
};

class SwordRenderer : public IEquipmentRenderer {
public:
  explicit SwordRenderer(SwordRenderConfig config = {});

  static void submit(const SwordRenderConfig& config,
                     const DrawContext& ctx,
                     const BodyFrames& frames,
                     const HumanoidPalette& palette,
                     const HumanoidAnimationContext& anim,
                     EquipmentBatch& batch);

  [[nodiscard]] auto base_config() const noexcept -> const SwordRenderConfig& {
    return m_base;
  }

  void render(const DrawContext& ctx,
              const BodyFrames& frames,
              const HumanoidPalette& palette,
              const HumanoidAnimationContext& anim,
              EquipmentBatch& batch) override;

private:
  SwordRenderConfig m_base;
};

inline constexpr std::uint32_t k_sword_role_count = 4U;
inline constexpr std::uint32_t k_scabbard_role_count = 2U;

auto sword_fill_role_colors(const HumanoidPalette& palette,
                            const SwordRenderConfig& config,
                            QVector3D* out,
                            std::size_t max) -> std::uint32_t;

auto sword_make_static_attachment(const SwordRenderConfig& config,
                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto sword_make_static_attachment(const SwordRenderConfig& config,
                                  std::uint8_t base_role_byte,
                                  const QVector3D& blade_dir_local)
    -> Render::Creature::StaticAttachmentSpec;

auto scabbard_fill_role_colors(const HumanoidPalette& palette,
                               QVector3D* out,
                               std::size_t max) -> std::uint32_t;

auto scabbard_make_static_attachment(float sheath_r,
                                     std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
