#pragma once

#include "creature/render_request.h"
#include "rigged_mesh.h"

#include <QMatrix4x4>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Render::Creature::Bpat {
class BpatBlob;
}
namespace Render::Creature::Pipeline {
struct CreatureAsset;
}
namespace Render::GL {
struct RiggedMeshEntry;
class RiggedMeshCache;
} // namespace Render::GL

namespace Render::GL {

struct SnapshotMeshEntry {
  std::unique_ptr<RiggedMesh> mesh;
};

class SnapshotMeshCache {
public:
  struct Key {
    Render::Creature::ArchetypeId archetype{
        Render::Creature::kInvalidArchetype};
    Render::Creature::VariantId variant{Render::Creature::kCanonicalVariant};
    Render::Creature::AnimationStateId state{
        Render::Creature::AnimationStateId::Idle};
    std::uint32_t frame_in_clip{0};

    auto operator==(const Key &o) const noexcept -> bool {
      return archetype == o.archetype && variant == o.variant &&
             state == o.state && frame_in_clip == o.frame_in_clip;
    }
  };

  struct KeyHash {
    auto operator()(const Key &k) const noexcept -> std::size_t {
      auto const a = static_cast<std::size_t>(k.archetype);
      auto const v = static_cast<std::size_t>(k.variant);
      auto const s = static_cast<std::size_t>(k.state);
      auto const f = static_cast<std::size_t>(k.frame_in_clip);
      return ((a * 0x9E3779B1u) ^ (v << 13U) ^ (s << 21U) ^
              (f * 0x85EBCA6BULL));
    }
  };

  SnapshotMeshCache() = default;
  ~SnapshotMeshCache() = default;
  SnapshotMeshCache(const SnapshotMeshCache &) = delete;
  auto operator=(const SnapshotMeshCache &) -> SnapshotMeshCache & = delete;

  auto get_or_bake(const Key &key, const RiggedMeshEntry &source,
                   std::uint32_t global_frame) -> const SnapshotMeshEntry *;

  void clear() { m_entries.clear(); }

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }

  [[nodiscard]] static auto identity_palette() noexcept -> const QMatrix4x4 *;

private:
  std::unordered_map<Key, SnapshotMeshEntry, KeyHash> m_entries;
};

} // namespace Render::GL
