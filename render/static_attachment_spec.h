

#pragma once

#include <QMatrix4x4>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct RenderArchetype;

}

namespace Render::Creature {

struct AttachmentDrapeBlend {
  bool enabled{false};
  std::uint16_t pelvis_bone{0};
  std::uint16_t leg_l_bone{0};
  std::uint16_t leg_r_bone{0};
  float top_y{0.0F};
  float bottom_y{0.0F};

  float leg_share{0.0F};
  float leg_crossfade_half_width{0.1F};
};

struct StaticAttachmentSpec {
  static constexpr std::size_t k_palette_slot_count = 8;

  const Render::GL::RenderArchetype* archetype{nullptr};

  std::uint16_t socket_bone_index{0};

  QMatrix4x4 local_offset{};

  std::array<std::uint8_t, k_palette_slot_count> palette_role_remap{};

  std::uint8_t override_color_role{0};

  float uniform_scale{1.0F};

  std::uint32_t material_id{0};

  AttachmentDrapeBlend drape{};
};

[[nodiscard]] auto
static_attachment_hash(const StaticAttachmentSpec& spec) noexcept -> std::uint64_t;

[[nodiscard]] auto static_attachments_hash(const StaticAttachmentSpec* attachments,
                                           std::size_t count) noexcept -> std::uint64_t;

[[nodiscard]] auto
static_attachment_equal(const StaticAttachmentSpec& a,
                        const StaticAttachmentSpec& b) noexcept -> bool;

} // namespace Render::Creature
