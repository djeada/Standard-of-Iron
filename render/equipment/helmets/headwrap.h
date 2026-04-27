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

struct HeadwrapConfig {};

auto headwrap_helmet_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kHeadwrapRoleCount = 3;

auto headwrap_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                               std::size_t max) -> std::uint32_t;

auto headwrap_make_static_attachment(std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte,
                                     const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

class HeadwrapRenderer : public IEquipmentRenderer {
public:
  HeadwrapRenderer() = default;

  static void submit(const HeadwrapConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
