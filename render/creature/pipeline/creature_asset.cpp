#include "creature_asset.h"

#include "../../elephant/elephant_spec.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr CreatureAssetId kHumanoidAssetId = 0;
constexpr CreatureAssetId kHorseAssetId = 1;
constexpr CreatureAssetId kElephantAssetId = 2;

auto humanoid_compute_bones(const void *pose,
                            std::span<QMatrix4x4> out) -> std::uint32_t {
  return Render::Humanoid::compute_bone_palette(
      *static_cast<const Render::GL::HumanoidPose *>(pose), out);
}

auto horse_compute_bones(const void *pose,
                         std::span<QMatrix4x4> out) -> std::uint32_t {
  return Render::Horse::compute_horse_bone_palette(
      *static_cast<const Render::Horse::HorseSpecPose *>(pose), out);
}

auto elephant_compute_bones(const void *pose,
                            std::span<QMatrix4x4> out) -> std::uint32_t {
  return Render::Elephant::compute_elephant_bone_palette(
      *static_cast<const Render::Elephant::ElephantSpecPose *>(pose), out);
}

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
  m_humanoid.id = kHumanoidAssetId;
  m_humanoid.debug_name = "humanoid.v1";
  m_humanoid.kind = CreatureKind::Humanoid;
  m_humanoid.spec = &Render::Humanoid::humanoid_creature_spec();
  m_humanoid.topology = &m_humanoid.spec->topology;
  m_humanoid.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount);
  m_humanoid.max_bones =
      static_cast<std::uint8_t>(Render::Humanoid::kBoneCount);
  m_humanoid.compute_bones = &humanoid_compute_bones;
  m_humanoid.bind_palette = &humanoid_bind;
  m_humanoid.fill_role_colors = &humanoid_fill_roles;

  m_horse.id = kHorseAssetId;
  m_horse.debug_name = "horse.v1";
  m_horse.kind = CreatureKind::Horse;
  m_horse.spec = &Render::Horse::horse_creature_spec();
  m_horse.topology = &m_horse.spec->topology;
  m_horse.role_count = 8;
  m_horse.max_bones = static_cast<std::uint8_t>(Render::Horse::kHorseBoneCount);
  m_horse.compute_bones = &horse_compute_bones;
  m_horse.bind_palette = &horse_bind;
  m_horse.fill_role_colors = &horse_fill_roles;

  m_elephant.id = kElephantAssetId;
  m_elephant.debug_name = "elephant.v1";
  m_elephant.kind = CreatureKind::Elephant;
  m_elephant.spec = &Render::Elephant::elephant_creature_spec();
  m_elephant.topology = &m_elephant.spec->topology;
  m_elephant.role_count =
      static_cast<std::uint8_t>(Render::Elephant::kElephantRoleCount);
  m_elephant.max_bones =
      static_cast<std::uint8_t>(Render::Elephant::kElephantBoneCount);
  m_elephant.compute_bones = &elephant_compute_bones;
  m_elephant.bind_palette = &elephant_bind;
  m_elephant.fill_role_colors = &elephant_fill_roles;
}

auto CreatureAssetRegistry::get(CreatureAssetId id) const noexcept
    -> const CreatureAsset * {
  switch (id) {
  case kHumanoidAssetId:
    return &m_humanoid;
  case kHorseAssetId:
    return &m_horse;
  case kElephantAssetId:
    return &m_elephant;
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

  switch (spec.kind) {
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
