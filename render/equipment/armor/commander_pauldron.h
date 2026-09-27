#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "render/render_archetype.h"
#include "render/static_attachment_spec.h"

namespace Render::GL {

[[nodiscard]] auto commander_pauldron_archetype() -> const RenderArchetype&;

[[nodiscard]] auto commander_pauldron_make_static_attachment(
    std::uint16_t shoulder_bone_index,
    std::uint8_t metal_role_byte,
    std::uint8_t strap_role_byte,
    const QMatrix4x4& bind_shoulder_frame) -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
