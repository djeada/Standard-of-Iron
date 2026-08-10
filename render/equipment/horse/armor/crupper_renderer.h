#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "render/equipment/horse/i_horse_equipment_renderer.h"
#include "render/render_archetype.h"
#include "render/static_attachment_spec.h"

namespace Render::GL {

auto crupper_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_crupper_role_count = 2;

auto crupper_fill_role_colors(const HorseVariant& variant,
                              QVector3D* out,
                              std::size_t max) -> std::uint32_t;

auto crupper_make_static_attachment(std::uint16_t socket_bone_index,
                                    std::uint8_t base_role_byte,
                                    const HorseAttachmentFrame& bind_pose_frame,
                                    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

class CrupperRenderer : public IHorseEquipmentRenderer {
public:
  CrupperRenderer() = default;

  static void submit(const DrawContext& ctx,
                     const HorseBodyFrames& frames,
                     const HorseVariant& variant,
                     const HorseAnimationContext& anim,
                     EquipmentBatch& batch);

  void render(const DrawContext& ctx,
              const HorseBodyFrames& frames,
              const HorseVariant& variant,
              const HorseAnimationContext& anim,
              EquipmentBatch& batch) const override {
    submit(ctx, frames, variant, anim, batch);
  }
};

} // namespace Render::GL
