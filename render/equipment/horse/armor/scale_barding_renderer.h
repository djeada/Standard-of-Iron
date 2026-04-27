#pragma once

#include "../../../render_archetype.h"
#include "../../../static_attachment_spec.h"
#include "../i_horse_equipment_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

auto scale_barding_chest_archetype() -> const RenderArchetype &;
auto scale_barding_barrel_archetype() -> const RenderArchetype &;
auto scale_barding_neck_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kScaleBardingRoleCount = 1;

auto scale_barding_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                                    std::size_t max) -> std::uint32_t;

auto scale_barding_make_static_attachment(
    const RenderArchetype &archetype, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte, const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

class ScaleBardingRenderer : public IHorseEquipmentRenderer {
public:
  ScaleBardingRenderer() = default;

  static void submit(const DrawContext &ctx, const HorseBodyFrames &frames,
                     const HorseVariant &variant,
                     const HorseAnimationContext &anim, EquipmentBatch &batch);

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              EquipmentBatch &batch) const override {
    submit(ctx, frames, variant, anim, batch);
  }
};

} // namespace Render::GL
