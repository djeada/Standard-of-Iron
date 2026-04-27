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

struct ToolBeltConfig {
  QVector3D leather_color = QVector3D(0.52F, 0.40F, 0.28F);
  QVector3D metal_color = QVector3D(0.60F, 0.58F, 0.56F);
  QVector3D wood_color = QVector3D(0.45F, 0.35F, 0.22F);
  bool include_hammer = true;
  bool include_chisel = true;
  bool include_pouches = true;
};

inline constexpr std::uint32_t kToolBeltRoleCount = 5;

auto tool_belt_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                                std::size_t max) -> std::uint32_t;

auto tool_belt_make_static_attachments(std::uint16_t waist_socket_bone_index,
                                       std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 5>;

class ToolBeltRenderer : public IEquipmentRenderer {
public:
  explicit ToolBeltRenderer(const ToolBeltConfig &config = ToolBeltConfig{});

  static void submit(const ToolBeltConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const ToolBeltConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ToolBeltConfig m_config;
};

} // namespace Render::GL
