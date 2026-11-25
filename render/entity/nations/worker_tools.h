#pragma once

#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "render/cache/static_attachment_spec.h"
#include "render/creature/archetype_registry.h"
#include "render/frame/render_archetype.h"

namespace Render::GL::WorkerTools {

enum class Kind : std::uint8_t {
  FellingAxe,
  QuarryPick,
  Dolabra,
  LongMallet,
};

inline constexpr std::uint8_t k_iron_role_count = 1U;
inline constexpr float k_hand_tool_scale = 1.35F;

auto archetype(Kind kind) -> const RenderArchetype&;

auto hand_attachment(Kind kind, std::uint8_t iron_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto back_attachment(Kind kind, std::uint8_t iron_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto iron_role_colors(const void* variant,
                      QVector3D* out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t;

auto with_attachment(std::string_view debug_name,
                     Render::Creature::ArchetypeId base_archetype_id,
                     const Render::Creature::StaticAttachmentSpec& spec,
                     bool adds_iron_role) -> Render::Creature::ArchetypeId;

} // namespace Render::GL::WorkerTools
