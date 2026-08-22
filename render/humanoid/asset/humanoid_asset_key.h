#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <cstddef>
#include <cstdint>

#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/render_request.h"
#include "render/rigged_mesh_cache.h"

namespace Render::Humanoid {

struct HumanoidAssetKey {
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  Render::Creature::VariantId geometry_variant{Render::Creature::k_canonical_variant};
  Render::Creature::Pipeline::AttachmentSetId attachment_set{
      Render::Creature::Pipeline::k_invalid_attachment_set_id};
  Render::Creature::Pipeline::CreatureAssetId creature_asset{
      Render::Creature::Pipeline::k_invalid_creature_asset};

  std::uint64_t attachments_hash{0U};
  std::uint32_t skin_species{0U};

  [[nodiscard]] auto operator==(const HumanoidAssetKey& other) const noexcept -> bool {
    return archetype == other.archetype && lod == other.lod &&
           geometry_variant == other.geometry_variant &&
           attachment_set == other.attachment_set &&
           creature_asset == other.creature_asset &&
           attachments_hash == other.attachments_hash &&
           skin_species == other.skin_species;
  }
};

struct HumanoidAssetKeyHash {
  [[nodiscard]] auto
  operator()(const HumanoidAssetKey& key) const noexcept -> std::size_t;
};

struct HumanoidInstanceParams {
  QMatrix4x4 world{};

  Render::Creature::AnimationStateId animation{
      Render::Creature::AnimationStateId::Idle};
  std::uint16_t clip{Animation::k_unmapped_clip};
  std::uint8_t clip_variant{0U};
  float frame{0.0F};
  float blend{0.0F};

  QVector3D team_tint{1.0F, 1.0F, 1.0F};
  QVector4D wear{};

  std::uint32_t seed{0U};

  float hit_reaction{0.0F};
  float combat_emphasis{0.0F};
};

[[nodiscard]] auto asset_key_of(
    const Render::Creature::CreatureRenderRequest& request,
    const Render::Creature::Pipeline::CreatureRenderAssetHandle* handle) noexcept
    -> HumanoidAssetKey;

[[nodiscard]] auto
instance_params_of(const Render::Creature::CreatureRenderRequest& request) noexcept
    -> HumanoidInstanceParams;

[[nodiscard]] auto rigged_cache_key(
    const Render::Creature::Pipeline::CreatureAsset& asset,
    const HumanoidAssetKey& key) noexcept -> Render::GL::RiggedMeshCache::Key;

} // namespace Render::Humanoid
