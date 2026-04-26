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

struct RomanGreavesConfig {};

class RomanGreavesRenderer : public IEquipmentRenderer {
public:
  RomanGreavesRenderer() = default;

  static void submit(const RomanGreavesConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

auto roman_greaves_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kRomanGreavesRoleCount = 1;

auto roman_greaves_fill_role_colors(const HumanoidPalette &palette,
                                    QVector3D *out,
                                    std::size_t max) -> std::uint32_t;

auto roman_greaves_make_static_attachment(std::uint16_t socket_bone_index,
                                          std::uint8_t base_role_byte,
                                          const QMatrix4x4 &bind_shin_frame)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
