#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Render::GL {

struct ArmGuardsConfig {
  QVector3D leather_color = QVector3D(0.50F, 0.38F, 0.26F);
  QVector3D strap_color = QVector3D(0.32F, 0.26F, 0.18F);
  float guard_length = 0.18F;
  bool include_straps = true;
};

inline constexpr std::uint32_t kArmGuardsRoleCount = 2;

auto arm_guards_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                                 std::size_t max) -> std::uint32_t;

auto arm_guards_make_static_attachments(std::uint16_t shoulder_l_bone_index,
                                        std::uint16_t shoulder_r_bone_index,
                                        std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 2>;

class ArmGuardsRenderer : public IEquipmentRenderer {
public:
  explicit ArmGuardsRenderer(const ArmGuardsConfig &config = ArmGuardsConfig{});

  static void submit(const ArmGuardsConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const ArmGuardsConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  ArmGuardsConfig m_config;

  static void render_arm_guard(const ArmGuardsConfig &config,
                               const DrawContext &ctx, const QVector3D &elbow,
                               const QVector3D &wrist,
                               std::span<const QVector3D> palette,
                               EquipmentBatch &batch);
};

} // namespace Render::GL
