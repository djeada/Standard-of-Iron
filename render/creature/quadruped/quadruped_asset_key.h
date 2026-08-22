#pragma once

#include <QMatrix4x4>

#include <cstddef>
#include <cstdint>

#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/render_request.h"

namespace Render::Creature::Quadruped {

using GeometryVariantId = std::uint16_t;
using EquipmentGeometrySetId = std::uint32_t;
using MaterialVariantId = std::uint16_t;

struct QuadrupedAssetKey {
  Render::Creature::Pipeline::CreatureKind species{
      Render::Creature::Pipeline::CreatureKind::Horse};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  Render::Creature::Pipeline::CreatureAssetId creature_asset{
      Render::Creature::Pipeline::k_invalid_creature_asset};
  Render::Creature::CreatureLOD geometry_lod{Render::Creature::CreatureLOD::Full};
  GeometryVariantId body_variant{0U};
  EquipmentGeometrySetId equipment{0U};

  [[nodiscard]] auto operator==(const QuadrupedAssetKey& other) const noexcept -> bool {
    return species == other.species && archetype == other.archetype &&
           creature_asset == other.creature_asset &&
           geometry_lod == other.geometry_lod && body_variant == other.body_variant &&
           equipment == other.equipment;
  }
};

struct QuadrupedAssetKeyHash {
  [[nodiscard]] auto
  operator()(const QuadrupedAssetKey& key) const noexcept -> std::size_t;
};

struct QuadrupedInstanceState {
  QMatrix4x4 world{};

  Render::Creature::AnimationStateId animation{
      Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};

  MaterialVariantId material{0U};
  std::uint32_t visual_seed{0U};

  bool selected{false};
  bool hit_reacting{false};
};

[[nodiscard]] auto asset_key_of(
    const Render::Creature::CreatureRenderRequest& request,
    Render::Creature::Pipeline::CreatureKind species) noexcept -> QuadrupedAssetKey;

[[nodiscard]] auto
instance_state_of(const Render::Creature::CreatureRenderRequest& request) noexcept
    -> QuadrupedInstanceState;

} // namespace Render::Creature::Quadruped
