#pragma once

#include "../../../creature/archetype_registry.h"
#include "../../../horse/attachment_frames.h"
#include "../../../static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Render::GL {

struct HorseVariant;

auto register_mount_saddle_archetype(
    std::string_view debug_name,
    Render::Creature::StaticAttachmentSpec (*make_static_attachment)(
        std::uint16_t, std::uint8_t, const HorseAttachmentFrame &,
        const QMatrix4x4 &),
    std::uint32_t (*fill_role_colors)(const HorseVariant &, QVector3D *,
                                      std::size_t))
    -> Render::Creature::ArchetypeId;

} // namespace Render::GL
