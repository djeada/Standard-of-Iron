

#pragma once

#include "../static_attachment_spec.h"
#include "pipeline/bpat_playback.h"
#include "pipeline/unit_visual_spec.h"
#include "render_request.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace Render::Creature {

struct ArchetypeDescriptor {
  ArchetypeId id{kInvalidArchetype};
  std::string_view debug_name{};

  Render::Creature::Pipeline::CreatureKind species{
      Render::Creature::Pipeline::CreatureKind::Humanoid};

  static constexpr std::uint16_t kUnmappedClip = 0xFFFFu;
  std::array<std::uint16_t, animation_state_count()> bpat_clip{};

  std::array<bool, animation_state_count()> snapshot{};

  std::uint8_t role_count{0};

  using ExtraRoleColorsFn = std::uint32_t (*)(const void *variant,
                                              QVector3D *out,
                                              std::uint32_t base_count,
                                              std::size_t max_count);
  ExtraRoleColorsFn extra_role_colors_fn{nullptr};

  static constexpr std::size_t kMaxBakeAttachments = 16;
  std::array<StaticAttachmentSpec, kMaxBakeAttachments> bake_attachments{};
  std::uint8_t bake_attachment_count{0};

  [[nodiscard]] auto
  attachments_view() const noexcept -> std::span<const StaticAttachmentSpec> {
    return {bake_attachments.data(),
            static_cast<std::size_t>(bake_attachment_count)};
  }
};

class ArchetypeRegistry {
public:
  [[nodiscard]] static auto instance() noexcept -> ArchetypeRegistry &;

  [[nodiscard]] auto
  get(ArchetypeId id) const noexcept -> const ArchetypeDescriptor *;

  [[nodiscard]] auto species(ArchetypeId id) const noexcept
      -> Render::Creature::Pipeline::CreatureKind;

  [[nodiscard]] auto
  bpat_clip(ArchetypeId id,
            AnimationStateId state) const noexcept -> std::uint16_t;

  [[nodiscard]] auto is_snapshot(ArchetypeId id,
                                 AnimationStateId state) const noexcept -> bool;

  static constexpr ArchetypeId kHumanoidBase = 0;
  static constexpr ArchetypeId kHorseBase = 1;
  static constexpr ArchetypeId kElephantBase = 2;
  static constexpr ArchetypeId kRiderBase = 3;

  auto register_archetype(ArchetypeDescriptor desc) -> ArchetypeId;

  auto register_unit_archetype(
      std::string_view debug_name,
      Render::Creature::Pipeline::CreatureKind species,
      std::span<const StaticAttachmentSpec> attachments,
      ArchetypeDescriptor::ExtraRoleColorsFn extra_role_colors_fn = nullptr)
      -> ArchetypeId;

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_count; }

private:
  ArchetypeRegistry();
  void seed_baseline();

  static constexpr std::size_t kMaxArchetypes = 256;
  std::array<ArchetypeDescriptor, kMaxArchetypes> m_table{};
  std::size_t m_count{0};
};

} // namespace Render::Creature
