#include "archetype_registry.h"

#include "bpat/bpat_format.h"

#include <algorithm>

namespace Render::Creature {

namespace {

constexpr std::size_t kStateCount = animation_state_count();

constexpr auto
make_humanoid_clip_table() -> std::array<std::uint16_t, kStateCount> {
  std::array<std::uint16_t, kStateCount> t{};
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = 1U;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 3U;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 4U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 7U;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = 10U;
  return t;
}

constexpr auto
make_horse_clip_table() -> std::array<std::uint16_t, kStateCount> {
  std::array<std::uint16_t, kStateCount> t{};
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = 1U;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = 4U;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = 2U;
  return t;
}

constexpr auto
make_elephant_clip_table() -> std::array<std::uint16_t, kStateCount> {
  std::array<std::uint16_t, kStateCount> t{};
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = 1U;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = 0U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 2U;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = 2U;
  return t;
}

constexpr auto
make_rider_clip_table() -> std::array<std::uint16_t, kStateCount> {
  std::array<std::uint16_t, kStateCount> t{};
  t[static_cast<std::size_t>(AnimationStateId::Idle)] = 11U;
  t[static_cast<std::size_t>(AnimationStateId::Walk)] = 11U;
  t[static_cast<std::size_t>(AnimationStateId::Run)] = 12U;
  t[static_cast<std::size_t>(AnimationStateId::Hold)] = 11U;
  t[static_cast<std::size_t>(AnimationStateId::AttackMelee)] = 4U;
  t[static_cast<std::size_t>(AnimationStateId::AttackRanged)] = 10U;
  t[static_cast<std::size_t>(AnimationStateId::Die)] = 11U;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = 11U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSword)] = 4U;
  t[static_cast<std::size_t>(AnimationStateId::AttackSpear)] = 7U;
  t[static_cast<std::size_t>(AnimationStateId::AttackBow)] = 10U;
  t[static_cast<std::size_t>(AnimationStateId::RidingIdle)] = 11U;
  t[static_cast<std::size_t>(AnimationStateId::RidingCharge)] = 12U;
  t[static_cast<std::size_t>(AnimationStateId::RidingReining)] = 13U;
  t[static_cast<std::size_t>(AnimationStateId::RidingBowShot)] = 14U;
  return t;
}

constexpr auto make_snapshot_table() -> std::array<bool, kStateCount> {
  std::array<bool, kStateCount> t{};

  t[static_cast<std::size_t>(AnimationStateId::Idle)] = true;
  t[static_cast<std::size_t>(AnimationStateId::Dead)] = true;
  return t;
}

} // namespace

auto ArchetypeRegistry::instance() noexcept -> ArchetypeRegistry & {
  static ArchetypeRegistry registry;
  return registry;
}

ArchetypeRegistry::ArchetypeRegistry() { seed_baseline(); }

void ArchetypeRegistry::seed_baseline() {
  ArchetypeDescriptor humanoid{};
  humanoid.debug_name = "humanoid_base";
  humanoid.species = Render::Creature::Pipeline::CreatureKind::Humanoid;
  humanoid.bpat_clip = make_humanoid_clip_table();
  humanoid.snapshot = make_snapshot_table();
  humanoid.role_count = 6;
  register_archetype(humanoid);

  ArchetypeDescriptor horse{};
  horse.debug_name = "horse_base";
  horse.species = Render::Creature::Pipeline::CreatureKind::Horse;
  horse.bpat_clip = make_horse_clip_table();
  horse.snapshot = make_snapshot_table();
  horse.role_count = 8;
  register_archetype(horse);

  ArchetypeDescriptor elephant{};
  elephant.debug_name = "elephant_base";
  elephant.species = Render::Creature::Pipeline::CreatureKind::Elephant;
  elephant.bpat_clip = make_elephant_clip_table();
  elephant.snapshot = make_snapshot_table();
  elephant.role_count = 6;
  register_archetype(elephant);

  ArchetypeDescriptor rider{};
  rider.debug_name = "rider_base";
  rider.species = Render::Creature::Pipeline::CreatureKind::Humanoid;
  rider.bpat_clip = make_rider_clip_table();
  rider.snapshot = make_snapshot_table();
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
  if (idx >= kStateCount) {
    return ArchetypeDescriptor::kUnmappedClip;
  }
  return d->bpat_clip[idx];
}

auto ArchetypeRegistry::is_snapshot(
    ArchetypeId id, AnimationStateId state) const noexcept -> bool {
  auto const *d = get(id);
  if (d == nullptr) {
    return false;
  }
  auto const idx = static_cast<std::size_t>(state);
  if (idx >= kStateCount) {
    return false;
  }
  return d->snapshot[idx];
}

} // namespace Render::Creature
