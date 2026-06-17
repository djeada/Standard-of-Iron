#include "archetype_registry.h"

#include <algorithm>
#include <mutex>

#include "animation/clip_manifest.h"
#include "bpat/bpat_format.h"

namespace Render::Creature {

namespace {

constexpr std::size_t k_state_count = animation_state_count();

struct ArchetypeAnimationManifest {
  std::array<std::uint16_t, k_state_count> clips{};
  std::array<std::uint8_t, k_state_count> variant_counts{};
  std::array<bool, k_state_count> snapshot{};
};

static_assert(k_state_count == Animation::state_count(),
              "render animation state ids must match animation_core manifests");

constexpr auto animation_manifest_to_render_manifest(
    const Animation::ClipManifest& manifest) -> ArchetypeAnimationManifest {
  ArchetypeAnimationManifest out{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    out.clips[i] = manifest.clips[i] == Animation::k_unmapped_clip
                       ? ArchetypeDescriptor::k_unmapped_clip
                       : manifest.clips[i];
    out.variant_counts[i] = manifest.variant_counts[i];
    out.snapshot[i] = manifest.snapshot[i];
  }
  return out;
}

constexpr auto make_humanoid_animation_manifest() -> ArchetypeAnimationManifest {
  return animation_manifest_to_render_manifest(Animation::humanoid_clip_manifest());
}

constexpr auto make_horse_animation_manifest() -> ArchetypeAnimationManifest {
  return animation_manifest_to_render_manifest(Animation::horse_clip_manifest());
}

constexpr auto make_elephant_animation_manifest() -> ArchetypeAnimationManifest {
  return animation_manifest_to_render_manifest(Animation::elephant_clip_manifest());
}

constexpr auto make_rider_animation_manifest() -> ArchetypeAnimationManifest {
  return animation_manifest_to_render_manifest(Animation::rider_clip_manifest());
}

auto make_baseline_archetype(std::string_view debug_name,
                             Render::Creature::Pipeline::CreatureKind species,
                             const ArchetypeAnimationManifest& manifest,
                             std::uint8_t role_count) -> ArchetypeDescriptor {
  ArchetypeDescriptor desc{};
  desc.debug_name = debug_name;
  desc.species = species;
  desc.bpat_clip = manifest.clips;
  desc.bpat_clip_variant_count = manifest.variant_counts;
  desc.snapshot = manifest.snapshot;
  desc.role_count = role_count;
  return desc;
}

} // namespace

auto ArchetypeRegistry::instance() noexcept -> ArchetypeRegistry& {
  static ArchetypeRegistry registry;
  return registry;
}

ArchetypeRegistry::ArchetypeRegistry() {
  seed_baseline();
}

void ArchetypeRegistry::seed_baseline() {
  static constexpr auto k_humanoid_manifest = make_humanoid_animation_manifest();
  static constexpr auto k_horse_manifest = make_horse_animation_manifest();
  static constexpr auto k_elephant_manifest = make_elephant_animation_manifest();
  static constexpr auto k_rider_manifest = make_rider_animation_manifest();

  register_archetype(
      make_baseline_archetype("humanoid_base",
                              Render::Creature::Pipeline::CreatureKind::Humanoid,
                              k_humanoid_manifest,
                              6U));
  register_archetype(
      make_baseline_archetype("horse_base",
                              Render::Creature::Pipeline::CreatureKind::Horse,
                              k_horse_manifest,
                              8U));
  register_archetype(
      make_baseline_archetype("elephant_base",
                              Render::Creature::Pipeline::CreatureKind::Elephant,
                              k_elephant_manifest,
                              6U));
  register_archetype(
      make_baseline_archetype("rider_base",
                              Render::Creature::Pipeline::CreatureKind::Humanoid,
                              k_rider_manifest,
                              6U));
}

auto ArchetypeRegistry::register_archetype(ArchetypeDescriptor desc) -> ArchetypeId {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (m_count >= k_max_archetypes) {
    return k_invalid_archetype;
  }
  auto const id = static_cast<ArchetypeId>(m_count);
  desc.id = id;
  m_table[m_count] = desc;
  ++m_count;
  return id;
}

auto ArchetypeRegistry::register_unit_archetype(
    std::string_view debug_name,
    Render::Creature::Pipeline::CreatureKind species,
    std::span<const StaticAttachmentSpec> attachments,
    ArchetypeDescriptor::ExtraRoleColorsFn extra_role_colors_fn) -> ArchetypeId {
  ArchetypeId base = k_invalid_archetype;
  switch (species) {
  case Render::Creature::Pipeline::CreatureKind::Humanoid:
    base = k_humanoid_base;
    break;
  case Render::Creature::Pipeline::CreatureKind::Horse:
    base = k_horse_base;
    break;
  case Render::Creature::Pipeline::CreatureKind::Elephant:
    base = k_elephant_base;
    break;
  case Render::Creature::Pipeline::CreatureKind::Mounted:
    return k_invalid_archetype;
  }
  auto const* base_desc = get(base);
  if (base_desc == nullptr) {
    return k_invalid_archetype;
  }
  ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = debug_name;
  desc.species = species;
  desc.bake_attachments = {};
  desc.bake_attachment_count = 0;
  std::size_t const n = std::min<std::size_t>(
      attachments.size(), ArchetypeDescriptor::k_max_bake_attachments);
  for (std::size_t i = 0; i < n; ++i) {
    desc.bake_attachments[i] = attachments[i];
  }
  desc.bake_attachment_count = static_cast<std::uint8_t>(n);
  if (extra_role_colors_fn != nullptr) {
    desc.append_extra_role_colors_fn(extra_role_colors_fn);
  }
  return register_archetype(desc);
}

auto ArchetypeRegistry::get(ArchetypeId id) const -> const ArchetypeDescriptor* {
  std::lock_guard<std::mutex> const lock(m_mutex);
  if (static_cast<std::size_t>(id) >= m_count) {
    return nullptr;
  }
  return &m_table[static_cast<std::size_t>(id)];
}

auto ArchetypeRegistry::species(ArchetypeId id) const noexcept
    -> Render::Creature::Pipeline::CreatureKind {
  if (auto const* d = get(id); d != nullptr) {
    return d->species;
  }
  return Render::Creature::Pipeline::CreatureKind::Humanoid;
}

auto ArchetypeRegistry::bpat_clip(ArchetypeId id, AnimationStateId state) const noexcept
    -> std::uint16_t {
  auto const* d = get(id);
  if (d == nullptr) {
    return ArchetypeDescriptor::k_unmapped_clip;
  }
  auto const idx = static_cast<std::size_t>(state);
  if (idx >= k_state_count) {
    return ArchetypeDescriptor::k_unmapped_clip;
  }
  return d->bpat_clip[idx];
}

auto ArchetypeRegistry::clip_variant_count(
    ArchetypeId id, AnimationStateId state) const noexcept -> std::uint8_t {
  auto const* d = get(id);
  if (d == nullptr) {
    return 0U;
  }
  auto const idx = static_cast<std::size_t>(state);
  if (idx >= k_state_count) {
    return 0U;
  }
  return d->bpat_clip_variant_count[idx];
}

auto ArchetypeRegistry::resolve_bpat_clip(ArchetypeId id,
                                          AnimationStateId state,
                                          std::uint8_t clip_variant) const noexcept
    -> std::uint16_t {
  auto const base_clip = bpat_clip(id, state);
  if (base_clip == ArchetypeDescriptor::k_unmapped_clip) {
    return base_clip;
  }
  auto const variant_count = clip_variant_count(id, state);
  if (variant_count <= 1U) {
    return base_clip;
  }
  return static_cast<std::uint16_t>(
      base_clip + std::min<std::uint8_t>(clip_variant, variant_count - 1U));
}

auto ArchetypeRegistry::is_snapshot(ArchetypeId id,
                                    AnimationStateId state) const noexcept -> bool {
  auto const* d = get(id);
  if (d == nullptr) {
    return false;
  }
  auto const idx = static_cast<std::size_t>(state);
  if (idx >= k_state_count) {
    return false;
  }
  return d->snapshot[idx];
}

} // namespace Render::Creature
