#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

inline constexpr std::uint32_t kRomanScutumRoleCount = 7;

struct RomanScutumConfig {};

class RomanScutumRenderer : public IEquipmentRenderer {
public:
  RomanScutumRenderer() = default;

  static void submit(const RomanScutumConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

[[nodiscard]] auto
roman_scutum_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                              std::size_t max) -> std::uint32_t;

[[nodiscard]] auto
roman_scutum_make_static_attachment(std::uint16_t socket_bone_index,
                                    std::uint8_t base_role_byte,
                                    const QMatrix4x4 &bind_hand_l_matrix)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
