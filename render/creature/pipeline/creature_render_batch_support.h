#pragma once

#include <QVector3D>

#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"

// The species-independent half of adding a body to a CreatureRenderBatch. Each
// species' batch entry points (the add_humanoid / add_quadruped overloads,
// defined in the species folders) build their request with these helpers.

namespace Render::Creature::Pipeline::BatchSupport {

[[nodiscard]] inline auto hash_combine(std::uint64_t seed,
                                       std::uint64_t value) noexcept -> std::uint64_t {
  return seed ^ (value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U));
}

[[nodiscard]] inline auto hash_float(float value) noexcept -> std::uint64_t {
  std::uint32_t bits = 0U;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

[[nodiscard]] inline auto hash_vec3(const QVector3D& v) noexcept -> std::uint64_t {
  std::uint64_t h = 0xCBF29CE484222325ULL;
  h = hash_combine(h, hash_float(v.x()));
  h = hash_combine(h, hash_float(v.y()));
  h = hash_combine(h, hash_float(v.z()));
  return h;
}

struct RoleColorCacheKey {
  CreatureAssetId asset{k_invalid_creature_asset};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  std::uint64_t variant_hash{0U};

  auto operator==(const RoleColorCacheKey& other) const noexcept -> bool {
    return asset == other.asset && archetype == other.archetype &&
           variant_hash == other.variant_hash;
  }
};

struct RoleColorCacheKeyHash {
  auto operator()(const RoleColorCacheKey& key) const noexcept -> std::size_t {
    auto h = static_cast<std::uint64_t>(key.asset);
    h = hash_combine(h, key.archetype);
    h = hash_combine(h, key.variant_hash);
    return static_cast<std::size_t>(h);
  }
};

// Resolved role-colour palettes by asset, archetype and variant. Each species
// keeps its own thread-local cache.
using RoleColorCache =
    std::unordered_map<RoleColorCacheKey,
                       std::shared_ptr<const Render::RoleColorPalette>,
                       RoleColorCacheKeyHash>;

[[nodiscard]] auto
build_request(const CreatureGraphOutput& output,
              Render::Creature::ArchetypeId archetype,
              Render::Creature::AnimationStateId state,
              float phase) noexcept -> Render::Creature::CreatureRenderRequest;

[[nodiscard]] auto
default_archetype_for(CreatureKind kind) noexcept -> Render::Creature::ArchetypeId;

// Fills req.role_colors from the asset's fill_role_colors and the archetype's
// extra role-colour functions, both reading `variant`.
void populate_role_colors(RoleColorCache& cache,
                          Render::Creature::CreatureRenderRequest& req,
                          const void* variant,
                          std::uint64_t variant_hash);

// The common start of a quadruped body: the shadow row (shadow pass only) and
// the request with asset, archetype, render handle and clip variant set. False
// when the body is culled or the species has no asset; nothing is added then.
[[nodiscard]] auto
build_quadruped_body(CreatureRenderBatch& batch,
                     const CreatureGraphOutput& output,
                     CreatureKind kind,
                     Render::Creature::AnimationStateId state,
                     float phase,
                     std::uint32_t clip_variant,
                     bool profile_asset_lookup,
                     Render::Creature::CreatureRenderRequest& out) -> bool;

} // namespace Render::Creature::Pipeline::BatchSupport
