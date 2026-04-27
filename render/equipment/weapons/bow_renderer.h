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

enum class ArrowVisibility { Hidden, AttackCycleOnly, IdleAndAttackCycle };

struct BowRenderConfig {
  QVector3D string_color{0.30F, 0.30F, 0.32F};
  QVector3D metal_color{0.50F, 0.50F, 0.55F};
  QVector3D fletching_color{0.60F, 0.20F, 0.20F};
  float bow_rod_radius = 0.035F;
  float string_radius = 0.008F;
  float bow_depth = 0.25F;
  float bow_x = 0.0F;
  float bow_top_y = 0.0F;
  float bow_bot_y = 0.0F;
  float bow_height_scale = 1.0F;
  float bow_curve_factor = 1.0F;
  int material_id = 3;
  ArrowVisibility arrow_visibility = ArrowVisibility::AttackCycleOnly;
  bool draw_body = true;
  bool draw_string = true;
};

class BowRenderer : public IEquipmentRenderer {
public:
  explicit BowRenderer(BowRenderConfig config = {});

  static void submit(const BowRenderConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const BowRenderConfig & {
    return m_base;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  BowRenderConfig m_base;
};

inline constexpr std::size_t kBowRoleCount = 2;

auto bow_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                          std::size_t max) -> std::uint32_t;

auto bow_make_static_attachments(const BowRenderConfig &config,
                                 std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 2>;

} // namespace Render::GL
