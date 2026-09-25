

#pragma once

#include <QMatrix4x4>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct RenderArchetype;

}

namespace Render::Creature {

// Optional soft skinning for hanging cloth. A rigid attachment follows its
// socket bone only, so a cloak fixed to the chest pitches and twists with the
// torso and reads as a board. With a drape blend, vertices below `top_y`
// (bind-pose model space) hand their weight from the socket bone to the
// pelvis and the thighs, reaching the full hand-off at `bottom_y`: the hem
// then hangs from the hips and swings with the stride.
struct AttachmentDrapeBlend {
  bool enabled{false};
  std::uint16_t pelvis_bone{0};
  std::uint16_t leg_l_bone{0};
  std::uint16_t leg_r_bone{0};
  float top_y{0.0F};
  float bottom_y{0.0F};
  // Share of the handed-off weight that goes to the thighs rather than the
  // pelvis, and the half width over which it fades from one thigh to the
  // other across the body's midline.
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
