#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct QuiverRenderConfig {
  QVector3D leather_color{0.35F, 0.25F, 0.15F};
  QVector3D wood_color{0.30F, 0.22F, 0.14F};
  QVector3D fletching_color{0.60F, 0.20F, 0.20F};
  float quiver_radius = 0.08F;
  float quiver_height = 0.30F;
  int num_arrows = 2;
  int material_id = 3;
};

class QuiverRenderer : public IEquipmentRenderer {
public:
  explicit QuiverRenderer(QuiverRenderConfig config = {});

  static void submit(const QuiverRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  void set_config(const QuiverRenderConfig &config) { m_config = config; }

  [[nodiscard]] auto base_config() const -> const QuiverRenderConfig & {
    return m_config;
  }

private:
  QuiverRenderConfig m_config;
};

inline constexpr std::uint32_t kQuiverRoleCount = 3;

auto quiver_fill_role_colors(const HumanoidPalette &palette,
                             const QuiverRenderConfig &config, QVector3D *out,
                             std::size_t max) -> std::uint32_t;

auto quiver_make_static_attachments(const QuiverRenderConfig &config,
                                    std::uint16_t socket_bone_index,
                                    std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 5>;

} // namespace Render::GL
