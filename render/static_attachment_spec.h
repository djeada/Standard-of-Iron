

#pragma once

#include <QMatrix4x4>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct RenderArchetype;

}

namespace Render::Creature {

struct StaticAttachmentSpec {
  static constexpr std::size_t kPaletteSlotCount = 8;

  const Render::GL::RenderArchetype *archetype{nullptr};

  std::uint16_t socket_bone_index{0};

  QMatrix4x4 local_offset{};

  std::array<std::uint8_t, kPaletteSlotCount> palette_role_remap{};

  std::uint8_t override_color_role{0};

  float uniform_scale{1.0F};

  std::uint32_t material_id{0};
};

[[nodiscard]] auto static_attachment_hash(
    const StaticAttachmentSpec &spec) noexcept -> std::uint64_t;

[[nodiscard]] auto
static_attachments_hash(const StaticAttachmentSpec *attachments,
                        std::size_t count) noexcept -> std::uint64_t;

[[nodiscard]] auto
static_attachment_equal(const StaticAttachmentSpec &a,
                        const StaticAttachmentSpec &b) noexcept -> bool;

} // namespace Render::Creature
