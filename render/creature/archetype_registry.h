

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
  ArchetypeId id{k_invalid_archetype};
  std::string_view debug_name{};

  Render::Creature::Pipeline::CreatureKind species{
      Render::Creature::Pipeline::CreatureKind::Humanoid};

  static constexpr std::uint16_t k_unmapped_clip = 0xFFFFu;
  std::array<std::uint16_t, animation_state_count()> bpat_clip{};
  std::array<std::uint8_t, animation_state_count()> bpat_clip_variant_count{};

  std::array<bool, animation_state_count()> snapshot{};

  std::uint8_t role_count{0};

  using ExtraRoleColorsFn = std::uint32_t (*)(const void *variant,
                                              QVector3D *out,
                                              std::uint32_t base_count,
                                              std::size_t max_count);
  static constexpr std::size_t k_max_extra_role_color_fns = 4;
  std::array<ExtraRoleColorsFn, k_max_extra_role_color_fns> extra_role_color_fns{};
  std::uint8_t extra_role_color_fn_count{0};

  static constexpr std::size_t k_max_bake_attachments = 16;
  std::array<StaticAttachmentSpec, k_max_bake_attachments> bake_attachments{};
  std::uint8_t bake_attachment_count{0};

  [[nodiscard]] auto
  attachments_view() const noexcept -> std::span<const StaticAttachmentSpec> {
    return {bake_attachments.data(),
            static_cast<std::size_t>(bake_attachment_count)};
  }

  void append_extra_role_colors_fn(ExtraRoleColorsFn fn) noexcept {
    if (fn == nullptr ||
        extra_role_color_fn_count >=
            static_cast<std::uint8_t>(extra_role_color_fns.size())) {
      return;
    }
    extra_role_color_fns[extra_role_color_fn_count++] = fn;
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

  [[nodiscard]] auto
  clip_variant_count(ArchetypeId id,
                     AnimationStateId state) const noexcept -> std::uint8_t;

  [[nodiscard]] auto resolve_bpat_clip(
      ArchetypeId id, AnimationStateId state,
      std::uint8_t clip_variant = 0U) const noexcept -> std::uint16_t;

  [[nodiscard]] auto is_snapshot(ArchetypeId id,
                                 AnimationStateId state) const noexcept -> bool;

  static constexpr ArchetypeId k_humanoid_base = 0;
  static constexpr ArchetypeId k_horse_base = 1;
  static constexpr ArchetypeId k_elephant_base = 2;
  static constexpr ArchetypeId k_rider_base = 3;

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

  static constexpr std::size_t k_max_archetypes = 256;
  std::array<ArchetypeDescriptor, k_max_archetypes> m_table{};
  std::size_t m_count{0};
};

} // namespace Render::Creature
