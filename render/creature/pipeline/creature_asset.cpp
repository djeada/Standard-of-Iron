#include "creature_asset.h"

#include "../../elephant/elephant_spec.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"
#include "../bpat/bpat_format.h"
#include "creature_visual_definition.h"

namespace Render::Creature::Pipeline {

namespace {

auto humanoid_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Humanoid::humanoid_bind_palette();
}

auto horse_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Horse::horse_bind_palette();
}

auto elephant_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Elephant::elephant_bind_palette();
}

auto humanoid_fill_roles(const void *variant, QVector3D *out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto &v = *static_cast<const Render::GL::HumanoidVariant *>(variant);
  std::array<QVector3D, Render::Humanoid::kHumanoidRoleCount> filled{};
  Render::Humanoid::fill_humanoid_role_colors(v, filled);
  const auto n = std::min(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i)
    out[i] = filled[i];
  return static_cast<std::uint32_t>(n);
}

auto horse_fill_roles(const void *variant, QVector3D *out,
                      std::size_t max_roles) -> std::uint32_t {
  const auto &v = *static_cast<const Render::GL::HorseVariant *>(variant);
  std::array<QVector3D, 8> filled{};
  Render::Horse::fill_horse_role_colors(v, filled);
  const auto n = std::min<std::size_t>(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i)
    out[i] = filled[i];
  return static_cast<std::uint32_t>(n);
}

auto elephant_fill_roles(const void *variant, QVector3D *out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto &v = *static_cast<const Render::GL::ElephantVariant *>(variant);
  std::array<QVector3D, Render::Elephant::kElephantRoleCount> filled{};
  Render::Elephant::fill_elephant_role_colors(v, filled);
  const auto n = std::min(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i)
    out[i] = filled[i];
  return static_cast<std::uint32_t>(n);
}

} // namespace

auto CreatureAssetRegistry::instance() noexcept
    -> const CreatureAssetRegistry & {
  static const CreatureAssetRegistry registry;
  return registry;
}

CreatureAssetRegistry::CreatureAssetRegistry() {
  m_humanoid.id = kHumanoidAsset;
  m_humanoid.debug_name = "humanoid.v1";
  m_humanoid.kind = CreatureKind::Humanoid;
  m_humanoid.bpat_species_id = Render::Creature::Bpat::kSpeciesHumanoid;
  m_humanoid.spec = &Render::Humanoid::humanoid_creature_spec();
  m_humanoid.topology = &m_humanoid.spec->topology;
  m_humanoid.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount);
  m_humanoid.max_bones =
      static_cast<std::uint8_t>(Render::Humanoid::kBoneCount);
  m_humanoid.bind_palette = &humanoid_bind;
  m_humanoid.fill_role_colors = &humanoid_fill_roles;

  m_horse.id = kHorseAsset;
  m_horse.debug_name = "horse.v1";
  m_horse.kind = CreatureKind::Horse;
  m_horse.bpat_species_id = Render::Creature::Bpat::kSpeciesHorse;
  m_horse.spec = &Render::Horse::horse_creature_spec();
  m_horse.topology = &m_horse.spec->topology;
  m_horse.role_count = 8;
  m_horse.max_bones = static_cast<std::uint8_t>(Render::Horse::kHorseBoneCount);
  m_horse.bind_palette = &horse_bind;
  m_horse.fill_role_colors = &horse_fill_roles;
  m_horse.visual_definition = &horse_creature_visual_definition();

  m_elephant.id = kElephantAsset;
  m_elephant.debug_name = "elephant.v1";
  m_elephant.kind = CreatureKind::Elephant;
  m_elephant.bpat_species_id = Render::Creature::Bpat::kSpeciesElephant;
  m_elephant.spec = &Render::Elephant::elephant_creature_spec();
  m_elephant.topology = &m_elephant.spec->topology;
  m_elephant.role_count =
      static_cast<std::uint8_t>(Render::Elephant::kElephantRoleCount);
  m_elephant.max_bones =
      static_cast<std::uint8_t>(Render::Elephant::kElephantBoneCount);
  m_elephant.bind_palette = &elephant_bind;
  m_elephant.fill_role_colors = &elephant_fill_roles;
  m_elephant.visual_definition = &elephant_creature_visual_definition();

  m_humanoid_sword.id = kHumanoidSwordAsset;
  m_humanoid_sword.debug_name = "humanoid.sword_ready.v1";
  m_humanoid_sword.kind = CreatureKind::Humanoid;
  m_humanoid_sword.bpat_species_id =
      Render::Creature::Bpat::kSpeciesHumanoidSword;
  m_humanoid_sword.spec = &Render::Humanoid::humanoid_creature_spec();
  m_humanoid_sword.topology = &m_humanoid_sword.spec->topology;
  m_humanoid_sword.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount);
  m_humanoid_sword.max_bones =
      static_cast<std::uint8_t>(Render::Humanoid::kBoneCount);
  m_humanoid_sword.bind_palette = &humanoid_bind;
  m_humanoid_sword.fill_role_colors = &humanoid_fill_roles;
}

auto CreatureAssetRegistry::get(CreatureAssetId id) const noexcept
    -> const CreatureAsset * {
  switch (id) {
  case kHumanoidAsset:
    return &m_humanoid;
  case kHorseAsset:
    return &m_horse;
  case kElephantAsset:
    return &m_elephant;
  case kHumanoidSwordAsset:
    return &m_humanoid_sword;
  default:
    return nullptr;
  }
}

auto CreatureAssetRegistry::resolve(const UnitVisualSpec &spec) const noexcept
    -> const CreatureAsset * {
  if (spec.creature_asset_id != kInvalidCreatureAsset) {
    if (const auto *asset = get(spec.creature_asset_id)) {
      return asset;
    }
  }
  return for_species(spec.kind);
}

auto CreatureAssetRegistry::for_species(CreatureKind kind) const noexcept
    -> const CreatureAsset * {
  switch (kind) {
  case CreatureKind::Humanoid:
    return &m_humanoid;
  case CreatureKind::Horse:
    return &m_horse;
  case CreatureKind::Elephant:
    return &m_elephant;
  case CreatureKind::Mounted:
    return nullptr;
  }
  return nullptr;
}

} // namespace Render::Creature::Pipeline
