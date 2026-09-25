#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "render/render_archetype.h"
#include "render/static_attachment_spec.h"

namespace Render::GL {

// A shoulder guard of overlapping curved lames for the commanders. It rides
// the same shoulder bone as the body's round deltoid and is sized to close
// over it, so the shoulder reads as armour instead of a ball in tunic colour.
//
// It adds no palette roles of its own: commander loadouts already sit at the
// 32-colour limit of the skinned shader, and roles past it silently take the
// wrong colours. The lames use the body's metal and dark leather roles
// instead, so each commander's guard matches his own kit.
[[nodiscard]] auto commander_pauldron_archetype() -> const RenderArchetype&;

[[nodiscard]] auto commander_pauldron_make_static_attachment(
    std::uint16_t shoulder_bone_index,
    std::uint8_t metal_role_byte,
    std::uint8_t strap_role_byte,
    const QMatrix4x4& bind_shoulder_frame) -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
