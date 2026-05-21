#include "archetype_registry.h"

#include <algorithm>
#include <mutex>

#include "bpat/bpat_format.h"
#include "humanoid_clip_ids.h"

namespace Render::Creature {

namespace {

constexpr std::size_t k_state_count = animation_state_count();
constexpr std::uint16_t k_horse_die_clip = 6U;
constexpr std::uint16_t k_horse_dead_clip = 7U;
constexpr std::uint16_t k_elephant_die_clip = 4U;
constexpr std::uint16_t k_elephant_dead_clip = 5U;

struct ArchetypeAnimationManifest {
  std::array<std::uint16_t, k_state_count> clips{};
  std::array<std::uint8_t, k_state_count> variant_counts{};
  std::array<bool, k_state_count> snapshot{};
};

constexpr auto make_unmapped_clip_table() -> std::array<std::uint16_t, k_state_count> {
  std::array<std::uint16_t, k_state_count> t{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    t[i] = ArchetypeDescriptor::k_unmapped_clip;
  }
  return t;
}

constexpr auto make_variant_count_table_for_clips(
    const std::array<std::uint16_t, k_state_count>& clips)
    -> std::array<std::uint8_t, k_state_count> {
  std::array<std::uint8_t, k_state_count> t{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    t[i] = (clips[i] != ArchetypeDescriptor::k_unmapped_clip) ? 1U : 0U;
  }
  return t;
}

constexpr auto
make_snapshot_table_for_clips(const std::array<std::uint16_t, k_state_count>& clips)
    -> std::array<bool, k_state_count> {
  std::array<bool, k_state_count> t{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    t[i] = (clips[i] != ArchetypeDescriptor::k_unmapped_clip);
  }
  t[static_cast<std::size_t>(AnimationStateId::Die)] = false;
  return t;
}

constexpr auto
make_humanoid_snapshot_table(const std::array<std::uint16_t, k_state_count>& clips)
    -> std::array<bool, k_state_count> {
  auto t = make_snapshot_table_for_clips(clips);
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = false;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = false;
  return t;
}

constexpr auto make_humanoid_clip_table() -> std::array<std::uint16_t, k_state_count> {
  auto t = make_unmapped_clip_table();
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = k_humanoid_idle_clip;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = k_humanoid_walk_clip;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = k_humanoid_run_clip;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = k_humanoid_hold_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = k_humanoid_hold_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = k_humanoid_hold_clip;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = k_humanoid_die_infantry_clip;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = k_humanoid_dead_infantry_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] =
      k_humanoid_attack_sword_a_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] =
      k_humanoid_attack_spear_a_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = k_humanoid_attack_bow_clip;
  t[static_cast<std::size_t>(AnimationStateId::Cast)] = k_humanoid_attack_bow_clip;
  return t;
}

constexpr auto make_horse_clip_table() -> std::array<std::uint16_t, k_state_count> {
  auto t = make_unmapped_clip_table();
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = 1U;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = 4U;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = 5U;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 5U;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = k_horse_die_clip;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = k_horse_dead_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 5U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 5U;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = 5U;
  t[static_cast<std::size_t>(AnimationStateId::Cast)] = 5U;
  return t;
}

constexpr auto make_elephant_clip_table() -> std::array<std::uint16_t, k_state_count> {
  auto t = make_unmapped_clip_table();
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = 1U;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = k_elephant_die_clip;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = k_elephant_dead_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::Cast)] = 3U;
  return t;
}

constexpr auto make_rider_clip_table() -> std::array<std::uint16_t, k_state_count> {
  auto t = make_unmapped_clip_table();
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = k_humanoid_riding_idle_clip;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = k_humanoid_riding_idle_clip;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = k_humanoid_riding_charge_clip;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = k_humanoid_riding_idle_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] =
      k_humanoid_riding_charge_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] =
      k_humanoid_riding_bow_shot_clip;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = k_humanoid_die_mounted_clip;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = k_humanoid_dead_mounted_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] =
      k_humanoid_riding_sword_strike_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] =
      k_humanoid_attack_spear_a_clip;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] =
      k_humanoid_riding_bow_shot_clip;
  t[static_cast<std::size_t>(AnimationStateId::Cast)] = k_humanoid_riding_bow_shot_clip;
  t[static_cast<std::size_t>(AnimationStateId::RidingIdle)] =
      k_humanoid_riding_idle_clip;
  t[static_cast<std::size_t>(AnimationStateId::RidingCharge)] =
      k_humanoid_riding_charge_clip;
  t[static_cast<std::size_t>(AnimationStateId::RidingReining)] =
      k_humanoid_riding_reining_clip;
  t[static_cast<std::size_t>(AnimationStateId::RidingBowShot)] =
      k_humanoid_riding_bow_shot_clip;
  return t;
}

constexpr auto
make_humanoid_variant_count_table() -> std::array<std::uint8_t, k_state_count> {
  auto t = make_variant_count_table_for_clips(make_humanoid_clip_table());
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 5U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 3U;
  return t;
}

constexpr auto
make_horse_variant_count_table() -> std::array<std::uint8_t, k_state_count> {
  return make_variant_count_table_for_clips(make_horse_clip_table());
}

constexpr auto
make_elephant_variant_count_table() -> std::array<std::uint8_t, k_state_count> {
  return make_variant_count_table_for_clips(make_elephant_clip_table());
}

constexpr auto
make_rider_variant_count_table() -> std::array<std::uint8_t, k_state_count> {
  auto t = make_variant_count_table_for_clips(make_rider_clip_table());
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 3U;
  return t;
}

constexpr auto make_humanoid_animation_manifest() -> ArchetypeAnimationManifest {
  auto const clips = make_humanoid_clip_table();
  return {
      clips, make_humanoid_variant_count_table(), make_humanoid_snapshot_table(clips)};
}

constexpr auto make_horse_animation_manifest() -> ArchetypeAnimationManifest {
  auto const clips = make_horse_clip_table();
  return {
      clips, make_horse_variant_count_table(), make_snapshot_table_for_clips(clips)};
}

constexpr auto make_elephant_animation_manifest() -> ArchetypeAnimationManifest {
  auto const clips = make_elephant_clip_table();
  return {
      clips, make_elephant_variant_count_table(), make_snapshot_table_for_clips(clips)};
}

constexpr auto make_rider_animation_manifest() -> ArchetypeAnimationManifest {
  auto const clips = make_rider_clip_table();
  return {
      clips, make_rider_variant_count_table(), make_snapshot_table_for_clips(clips)};
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
