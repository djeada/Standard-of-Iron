#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "render/palette.h"
#include "render/render_archetype.h"
#include "render/static_attachment_spec.h"

namespace Render::GL {

auto gallic_braccae_archetype() -> const RenderArchetype&;

auto gallic_braccae_make_static_attachment(std::uint16_t socket_bone_index,
                                           std::uint8_t cloth_role_byte,
                                           const QMatrix4x4& bind_shin_frame)
    -> Render::Creature::StaticAttachmentSpec;

inline constexpr std::uint32_t k_allied_tunic_role_count = 4;

auto gallic_tunic_fill_role_colors(const HumanoidPalette& palette,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t;
auto iberian_tunic_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t;

auto gallic_tunic_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;
auto iberian_tunic_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;
auto iberian_hem_band_make_static_attachment(bool left, std::uint8_t crimson_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
