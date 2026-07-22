#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"

namespace Render::GL {

enum class CommanderHelmetStyle : std::uint8_t {
  Fabius,
  Scipio,
  Marcellus,
  Hanno,
  Hasdrubal,
  Hannibal,
};

inline constexpr std::uint32_t k_commander_helmet_role_count = 4U;

auto commander_helmet_archetype(CommanderHelmetStyle style) -> const RenderArchetype&;

auto commander_helmet_fill_role_colors(CommanderHelmetStyle style,
                                       const HumanoidPalette& palette,
                                       QVector3D* out,
                                       std::size_t max) -> std::uint32_t;

auto commander_helmet_make_static_attachment(CommanderHelmetStyle style,
                                             std::uint16_t socket_bone_index,
                                             std::uint8_t base_role_byte,
                                             const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
