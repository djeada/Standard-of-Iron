#pragma once

#include "../../../horse/dimensions.h"
#include "../../../render_archetype.h"
#include "../../../static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

auto light_cavalry_saddle_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kLightCavalrySaddleRoleCount = 1;

auto light_cavalry_saddle_fill_role_colors(const HorseVariant &variant,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t;

auto light_cavalry_saddle_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
