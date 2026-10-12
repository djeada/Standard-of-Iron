#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "render/palette.h"
#include "render/render_archetype.h"
#include "render/static_attachment_spec.h"

namespace Render::GL {

auto hair_bun_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_hair_bun_role_count = 2;

auto hair_bun_fill_role_colors(const HumanoidPalette& palette,
                               QVector3D* out,
                               std::size_t max) -> std::uint32_t;

auto hair_bun_make_static_attachment(std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte,
                                     const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

auto hair_crop_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_hair_crop_role_count = 1;

auto hair_crop_fill_role_colors(const HumanoidPalette& palette,
                                QVector3D* out,
                                std::size_t max) -> std::uint32_t;

auto hair_crop_make_static_attachment(std::uint16_t socket_bone_index,
                                      std::uint8_t base_role_byte,
                                      const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

auto skull_face_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_skull_face_role_count = 2;

auto skull_face_fill_role_colors(const HumanoidPalette& palette,
                                 QVector3D* out,
                                 std::size_t max) -> std::uint32_t;

auto skull_face_make_static_attachment(std::uint16_t socket_bone_index,
                                       std::uint8_t base_role_byte,
                                       const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

auto palla_veil_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_palla_veil_role_count = 2;

auto palla_veil_fill_role_colors(const HumanoidPalette& palette,
                                 QVector3D* out,
                                 std::size_t max) -> std::uint32_t;

auto palla_veil_make_static_attachment(std::uint16_t socket_bone_index,
                                       std::uint8_t base_role_byte,
                                       const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

auto pileus_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_pileus_role_count = 2;

auto pileus_fill_role_colors(const HumanoidPalette& palette,
                             QVector3D* out,
                             std::size_t max) -> std::uint32_t;

auto pileus_make_static_attachment(std::uint16_t socket_bone_index,
                                   std::uint8_t base_role_byte,
                                   const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

auto turban_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_turban_role_count = 2;

auto turban_fill_role_colors(const HumanoidPalette& palette,
                             QVector3D* out,
                             std::size_t max) -> std::uint32_t;

auto turban_make_static_attachment(std::uint16_t socket_bone_index,
                                   std::uint8_t base_role_byte,
                                   const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

} // namespace Render::GL
