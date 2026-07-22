#pragma once

#include <cstdint>

#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"

namespace Render::GL {

enum class CommanderRegaliaStyle : std::uint8_t {
  Fabius,
  Scipio,
  Marcellus,
  Hanno,
  Hasdrubal,
  Hannibal,
};

auto commander_regalia_archetype(CommanderRegaliaStyle style) -> const RenderArchetype&;

auto commander_regalia_make_static_attachment(CommanderRegaliaStyle style,
                                              std::uint16_t torso_socket_bone_index,
                                              std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
