#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "../../palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"

namespace Render::GL {

enum class HistoricalHelmet : std::uint8_t {
  RomanMontefortino,
  RomanBoeotianCavalry,
  CarthagePunicConical,
  CarthageThracianCrested,
};

inline constexpr std::uint32_t k_historical_helmet_role_count = 4U;

auto historical_helmet_archetype(HistoricalHelmet helmet) -> const RenderArchetype&;
auto historical_helmet_fill_role_colors(HistoricalHelmet helmet,
                                        const HumanoidPalette& palette,
                                        QVector3D* out,
                                        std::size_t max) -> std::uint32_t;
auto historical_helmet_make_static_attachment(
    HistoricalHelmet helmet,
    std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte,
    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
