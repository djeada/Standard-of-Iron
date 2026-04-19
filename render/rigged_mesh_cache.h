

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

  std::vector<QMatrix4x4> inverse_bind;
};

class RiggedMeshCache {
public:
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

  auto get_or_bake(const Render::Creature::CreatureSpec &spec,
                   Render::Creature::CreatureLOD lod,
                   std::span<const QMatrix4x4> rest_palette,
                   std::uint16_t variant_bucket = 0) -> const RiggedMeshEntry *;

  void clear() { m_entries.clear(); }

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }

private:
  std::unordered_map<Key, RiggedMeshEntry, KeyHash> m_entries;
};

} // namespace Render::GL
