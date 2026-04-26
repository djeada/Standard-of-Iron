

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

  mutable std::vector<QMatrix4x4> skinned_palettes;
  mutable std::uint32_t skinned_frame_total{0};
  mutable std::uint32_t skinned_bone_count{0};

  mutable GLuint skin_palette_ubo{0};
  mutable std::size_t skin_palette_frame_stride_bytes{0};
};

class RiggedMeshCache {
public:
  struct Key {
    const Render::Creature::CreatureSpec *spec{nullptr};
    Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Reduced};
    std::uint16_t variant_bucket{0};
    std::uint64_t attachments_hash{0};

    auto operator==(const Key &o) const noexcept -> bool {
      return spec == o.spec && lod == o.lod &&
             variant_bucket == o.variant_bucket &&
             attachments_hash == o.attachments_hash;
    }
  };

  struct KeyHash {
    auto operator()(const Key &k) const noexcept -> std::size_t {
      const auto spec_bits = reinterpret_cast<std::uintptr_t>(k.spec);
      return (spec_bits * 0x9E3779B97F4A7C15ULL) ^
             (static_cast<std::size_t>(k.lod) << 1U) ^
             (static_cast<std::size_t>(k.variant_bucket) << 16U) ^
             static_cast<std::size_t>(k.attachments_hash);
    }
  };

  RiggedMeshCache() = default;
  ~RiggedMeshCache();
  RiggedMeshCache(const RiggedMeshCache &) = delete;
  auto operator=(const RiggedMeshCache &) -> RiggedMeshCache & = delete;

  auto get_or_bake(const Render::Creature::CreatureSpec &spec,
                   Render::Creature::CreatureLOD lod,
                   std::span<const QMatrix4x4> rest_palette,
                   std::uint16_t variant_bucket = 0,
                   std::span<const Render::Creature::StaticAttachmentSpec>
                       attachments = {}) -> const RiggedMeshEntry *;

  void clear() { m_entries.clear(); }

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }

private:
  std::unordered_map<Key, RiggedMeshEntry, KeyHash> m_entries;
};

void rigged_entry_ensure_skin_atlas(const RiggedMeshEntry &entry,
                                    const QMatrix4x4 *bpat_palettes,
                                    std::uint32_t frame_total,
                                    std::uint32_t bone_count);

void rigged_entry_ensure_skin_ubo(const RiggedMeshEntry &entry);

} // namespace Render::GL

namespace Render::Creature::Bpat {
class BpatBlob;
}

namespace Render::GL {

void rigged_entry_ensure_skin_atlas_from_blob(
    const RiggedMeshEntry &entry, const Render::Creature::Bpat::BpatBlob &blob);

}
