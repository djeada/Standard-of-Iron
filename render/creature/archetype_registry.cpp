#include "archetype_registry.h"

#include "bpat/bpat_format.h"
#include "humanoid_clip_ids.h"

#include <algorithm>

namespace Render::Creature {

namespace {

constexpr std::size_t k_state_count = animation_state_count();
constexpr std::uint16_t k_horse_die_clip = 6U;
constexpr std::uint16_t k_horse_dead_clip = 7U;
constexpr std::uint16_t k_elephant_die_clip = 4U;
constexpr std::uint16_t k_elephant_dead_clip = 5U;

constexpr auto
make_unmapped_clip_table() -> std::array<std::uint16_t, k_state_count> {
  std::array<std::uint16_t, k_state_count> t{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    t[i] = ArchetypeDescriptor::kUnmappedClip;
  }
  return t;
}

constexpr auto make_variant_count_table_for_clips(
    const std::array<std::uint16_t, k_state_count> &clips)
    -> std::array<std::uint8_t, k_state_count> {
  std::array<std::uint8_t, k_state_count> t{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    t[i] = (clips[i] != ArchetypeDescriptor::kUnmappedClip) ? 1U : 0U;
  }
  return t;
}

constexpr auto make_snapshot_table_for_clips(
    const std::array<std::uint16_t, k_state_count> &clips)
    -> std::array<bool, k_state_count> {
  std::array<bool, k_state_count> t{};
  for (std::size_t i = 0; i < k_state_count; ++i) {
    t[i] = (clips[i] != ArchetypeDescriptor::kUnmappedClip);
  }
  t[static_cast<std::size_t>(AnimationStateId::Die)] = false;
  return t;
}

constexpr auto
make_humanoid_clip_table() -> std::array<std::uint16_t, k_state_count> {
  auto t = make_unmapped_clip_table();
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = kHumanoidIdleClip;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = kHumanoidWalkClip;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = kHumanoidRunClip;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = kHumanoidHoldClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] =
      kHumanoidHoldClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] =
      kHumanoidHoldClip;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = kHumanoidDieInfantryClip;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] =
      kHumanoidDeadInfantryClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] =
      kHumanoidAttackSwordAClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] =
      kHumanoidAttackSpearAClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] =
      kHumanoidAttackBowClip;
  return t;
}

constexpr auto
make_horse_clip_table() -> std::array<std::uint16_t, k_state_count> {
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
  return t;
}

constexpr auto
make_elephant_clip_table() -> std::array<std::uint16_t, k_state_count> {
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
  return t;
}

constexpr auto
make_rider_clip_table() -> std::array<std::uint16_t, k_state_count> {
  auto t = make_unmapped_clip_table();
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = kHumanoidRidingIdleClip;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = kHumanoidRidingIdleClip;
  t[static_cast<std::size_t>(AnimationStateId::Run)] =
      kHumanoidRidingChargeClip;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = kHumanoidRidingIdleClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] =
      kHumanoidAttackSwordAClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] =
      kHumanoidAttackBowClip;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = kHumanoidDieMountedClip;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] =
      kHumanoidDeadMountedClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] =
      kHumanoidAttackSwordAClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] =
      kHumanoidAttackSpearAClip;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] =
      kHumanoidAttackBowClip;
  t[static_cast<std::size_t>(AnimationStateId::RidingIdle)] =
      kHumanoidRidingIdleClip;
  t[static_cast<std::size_t>(AnimationStateId::RidingCharge)] =
      kHumanoidRidingChargeClip;
  t[static_cast<std::size_t>(AnimationStateId::RidingReining)] =
      kHumanoidRidingReiningClip;
  t[static_cast<std::size_t>(AnimationStateId::RidingBowShot)] =
      kHumanoidRidingBowShotClip;
  return t;
}

constexpr auto
make_humanoid_variant_count_table() -> std::array<std::uint8_t, k_state_count> {
  auto t = make_variant_count_table_for_clips(make_humanoid_clip_table());
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
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 3U;
  return t;
}

} // namespace

auto ArchetypeRegistry::instance() noexcept -> ArchetypeRegistry & {
  static ArchetypeRegistry registry;
  return registry;
}

ArchetypeRegistry::ArchetypeRegistry() { seed_baseline(); }

void ArchetypeRegistry::seed_baseline() {
  auto const humanoid_clips = make_humanoid_clip_table();
  ArchetypeDescriptor humanoid{};
  humanoid.debug_name = "humanoid_base";
  humanoid.species = Render::Creature::Pipeline::CreatureKind::Humanoid;
  humanoid.bpat_clip = humanoid_clips;
  humanoid.bpat_clip_variant_count = make_humanoid_variant_count_table();
  humanoid.snapshot = make_snapshot_table_for_clips(humanoid_clips);
  humanoid.role_count = 6;
  register_archetype(humanoid);

  auto const horse_clips = make_horse_clip_table();
  ArchetypeDescriptor horse{};
  horse.debug_name = "horse_base";
  horse.species = Render::Creature::Pipeline::CreatureKind::Horse;
  horse.bpat_clip = horse_clips;
  horse.bpat_clip_variant_count = make_horse_variant_count_table();
  horse.snapshot = make_snapshot_table_for_clips(horse_clips);
  horse.role_count = 8;
  register_archetype(horse);

  auto const elephant_clips = make_elephant_clip_table();
  ArchetypeDescriptor elephant{};
  elephant.debug_name = "elephant_base";
  elephant.species = Render::Creature::Pipeline::CreatureKind::Elephant;
  elephant.bpat_clip = elephant_clips;
  elephant.bpat_clip_variant_count = make_elephant_variant_count_table();
  elephant.snapshot = make_snapshot_table_for_clips(elephant_clips);
  elephant.role_count = 6;
  register_archetype(elephant);

  auto const rider_clips = make_rider_clip_table();
  ArchetypeDescriptor rider{};
  rider.debug_name = "rider_base";
  rider.species = Render::Creature::Pipeline::CreatureKind::Humanoid;
  rider.bpat_clip = rider_clips;
  rider.bpat_clip_variant_count = make_rider_variant_count_table();
  rider.snapshot = make_snapshot_table_for_clips(rider_clips);
  rider.role_count = 6;
  register_archetype(rider);
}

auto ArchetypeRegistry::register_archetype(ArchetypeDescriptor desc)
    -> ArchetypeId {
  if (m_count >= kMaxArchetypes) {
    return kInvalidArchetype;
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
    ArchetypeDescriptor::ExtraRoleColorsFn extra_role_colors_fn)
    -> ArchetypeId {
  ArchetypeId base = kInvalidArchetype;
  switch (species) {
  case Render::Creature::Pipeline::CreatureKind::Humanoid:
    base = kHumanoidBase;
    break;
  case Render::Creature::Pipeline::CreatureKind::Horse:
    base = kHorseBase;
    break;
  case Render::Creature::Pipeline::CreatureKind::Elephant:
    base = kElephantBase;
    break;
  case Render::Creature::Pipeline::CreatureKind::Mounted:
    return kInvalidArchetype;
  }
  auto const *base_desc = get(base);
  if (base_desc == nullptr) {
    return kInvalidArchetype;
  }
  ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = debug_name;
  desc.species = species;
  desc.bake_attachments = {};
  desc.bake_attachment_count = 0;
  std::size_t const n = std::min<std::size_t>(
      attachments.size(), ArchetypeDescriptor::kMaxBakeAttachments);
  for (std::size_t i = 0; i < n; ++i) {
    desc.bake_attachments[i] = attachments[i];
  }
  desc.bake_attachment_count = static_cast<std::uint8_t>(n);
  if (extra_role_colors_fn != nullptr) {
    desc.append_extra_role_colors_fn(extra_role_colors_fn);
  }
  return register_archetype(desc);
}

auto ArchetypeRegistry::get(ArchetypeId id) const noexcept
    -> const ArchetypeDescriptor * {
  if (static_cast<std::size_t>(id) >= m_count) {
    return nullptr;
  }
  return &m_table[static_cast<std::size_t>(id)];
}

auto ArchetypeRegistry::species(ArchetypeId id) const noexcept
    -> Render::Creature::Pipeline::CreatureKind {
  if (auto const *d = get(id); d != nullptr) {
    return d->species;
  }
  return Render::Creature::Pipeline::CreatureKind::Humanoid;
}

auto ArchetypeRegistry::bpat_clip(
    ArchetypeId id, AnimationStateId state) const noexcept -> std::uint16_t {
  auto const *d = get(id);
  if (d == nullptr) {
    return ArchetypeDescriptor::kUnmappedClip;
  }
  auto const idx = static_cast<std::size_t>(state);
  if (idx >= k_state_count) {
    return ArchetypeDescriptor::kUnmappedClip;
  }
  return d->bpat_clip[idx];
}

auto ArchetypeRegistry::clip_variant_count(
    ArchetypeId id, AnimationStateId state) const noexcept -> std::uint8_t {
  auto const *d = get(id);
  if (d == nullptr) {
    return 0U;
  }
  auto const idx = static_cast<std::size_t>(state);
  if (idx >= k_state_count) {
    return 0U;
  }
  return d->bpat_clip_variant_count[idx];
}

auto ArchetypeRegistry::resolve_bpat_clip(
    ArchetypeId id, AnimationStateId state,
    std::uint8_t clip_variant) const noexcept -> std::uint16_t {
  auto const base_clip = bpat_clip(id, state);
  if (base_clip == ArchetypeDescriptor::kUnmappedClip) {
    return base_clip;
  }
  auto const variant_count = clip_variant_count(id, state);
  if (variant_count <= 1U) {
    return base_clip;
  }
  return static_cast<std::uint16_t>(
      base_clip + std::min<std::uint8_t>(clip_variant, variant_count - 1U));
}

auto ArchetypeRegistry::is_snapshot(
    ArchetypeId id, AnimationStateId state) const noexcept -> bool {
  auto const *d = get(id);
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
