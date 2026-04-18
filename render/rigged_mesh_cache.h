// Stage 15.5c — RiggedMesh cache keyed by (CreatureSpec, CreatureLOD).
//
// Bakes a rigged mesh (one GPU-skinnable vertex buffer) on first request
// for a given (spec, lod) key and keeps the owned `RiggedMesh` + its
// cached inverse-bind palette for the process lifetime. Callers pass a
// resting BonePalette snapshot (the bake's bind pose) on miss; the cache
// computes inverse_bind[i] = rest[i].inverted() once so runtime skinning
// reduces to `arena[i] = current[i] * inverse_bind[i]`.
//
// Thread-unsafe by design — the renderer is single-threaded for draw
// submission and every other cache on Renderer (template_cache,
// pose_palette_cache, unit_render_cache) follows the same contract.

#pragma once

#include "creature/part_graph.h"
#include "rigged_mesh.h"
#include "rigged_mesh_bake.h"

#include <QMatrix4x4>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace Render::Creature {
struct CreatureSpec;
}

namespace Render::GL {

struct RiggedMeshEntry {
  std::unique_ptr<RiggedMesh> mesh;
  // inverse_bind[i] = rest_palette[i].inverted(). Sized to the rest
  // palette's length; may be smaller than BonePaletteArena::kPaletteWidth.
  std::vector<QMatrix4x4> inverse_bind;
};

class RiggedMeshCache {
public:
  // Composite key: (spec-pointer, lod-enum, variant_bucket). CreatureSpec
  // references are process-stable (static CreatureSpec instances returned by
  // humanoid_creature_spec() etc.), so the raw pointer is a safe key.
  // `variant_bucket` is reserved for Stage 15.5d+ callers that bake variant-
  // specific geometry (e.g. seed-bucketed facial hair). Default value 0
  // means "no variant bucketing" — all of 15.5c uses 0.
  struct Key {
    const Render::Creature::CreatureSpec *spec{nullptr};
    Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Reduced};
    std::uint16_t variant_bucket{0};

    auto operator==(const Key &o) const noexcept -> bool {
      return spec == o.spec && lod == o.lod &&
             variant_bucket == o.variant_bucket;
    }
  };

  struct KeyHash {
    auto operator()(const Key &k) const noexcept -> std::size_t {
      const auto spec_bits = reinterpret_cast<std::uintptr_t>(k.spec);
      return (spec_bits * 0x9E3779B97F4A7C15ULL) ^
             (static_cast<std::size_t>(k.lod) << 1U) ^
             (static_cast<std::size_t>(k.variant_bucket) << 16U);
    }
  };

  // Returns the cached entry, baking on miss. `rest_palette` is used
  // only on miss to compute inverse_bind and seed the bake's bind pose.
  // `graph` defaults to the spec's LOD-specific PartGraph; an override
  // may be provided for tests. `variant_bucket` keys the cache across
  // bake-time variations (default 0 = no bucketing).
  auto get_or_bake(const Render::Creature::CreatureSpec &spec,
                   Render::Creature::CreatureLOD lod,
                   std::span<const QMatrix4x4> rest_palette,
                   std::uint16_t variant_bucket = 0)
      -> const RiggedMeshEntry *;

  void clear() { m_entries.clear(); }

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }

private:
  std::unordered_map<Key, RiggedMeshEntry, KeyHash> m_entries;
};

} // namespace Render::GL
