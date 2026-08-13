

#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "creature/part_graph.h"
#include "rigged_mesh.h"
#include "rigged_mesh_bake.h"

namespace Render::Creature {
struct CreatureSpec;
}

namespace Render::GL {

struct RiggedSkinAtlas {
  std::vector<QMatrix4x4> palettes;
  std::uint32_t frame_total{0};
  std::uint32_t bone_count{0};
  GLuint palette_ubo{0};
  std::size_t frame_stride_bytes{0};
};

struct RiggedMeshEntry {
  std::shared_ptr<RiggedMesh> mesh;
  std::vector<std::shared_ptr<RiggedMesh>> attachment_meshes;

  std::vector<QMatrix4x4> inverse_bind;
  std::shared_ptr<RiggedSkinAtlas> skin_atlas;
};

class RiggedMeshCache {
public:
  struct Key {
    const Render::Creature::CreatureSpec* spec{nullptr};
    Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
    std::uint32_t skin_species_id{0};
    std::uint32_t attachment_set_id{0};
    std::uint64_t attachments_hash{0};

    auto operator==(const Key& o) const noexcept -> bool {
      return spec == o.spec && lod == o.lod && skin_species_id == o.skin_species_id &&
             attachment_set_id == o.attachment_set_id &&
             attachments_hash == o.attachments_hash;
    }
  };

  struct KeyHash {
    auto operator()(const Key& k) const noexcept -> std::size_t {
      const auto spec_bits = reinterpret_cast<std::uintptr_t>(k.spec);
      return (spec_bits * 0x9E3779B97F4A7C15ULL) ^
             (static_cast<std::size_t>(k.lod) << 1U) ^
             (static_cast<std::size_t>(k.skin_species_id) << 24U) ^
             (static_cast<std::size_t>(k.attachment_set_id) * 0x9E3779B185EBCA87ULL) ^
             static_cast<std::size_t>(k.attachments_hash);
    }
  };

  struct FrameStats {
    std::uint32_t hits{0};
    std::uint32_t misses{0};
    std::uint32_t bakes{0};
    std::uint32_t skin_atlas_builds{0};
    std::uint32_t skin_ubo_uploads{0};
    std::uint64_t skin_ubo_bytes_uploaded{0};
  };

  RiggedMeshCache() = default;
  ~RiggedMeshCache();
  RiggedMeshCache(const RiggedMeshCache&) = delete;
  auto operator=(const RiggedMeshCache&) -> RiggedMeshCache& = delete;

  void reset_frame_stats() noexcept { m_frame_stats = {}; }
  [[nodiscard]] auto frame_stats() const noexcept -> const FrameStats& {
    return m_frame_stats;
  }
  void reserve_for_frame(std::size_t expected_entries) {
    m_entries.reserve(m_entries.size() + expected_entries);
  }

  void record_skin_atlas_build() noexcept { ++m_frame_stats.skin_atlas_builds; }
  void record_skin_ubo_upload(std::uint64_t bytes) noexcept {
    ++m_frame_stats.skin_ubo_uploads;
    m_frame_stats.skin_ubo_bytes_uploaded += bytes;
  }
  void mark_skin_ubo_upload_pending() noexcept {
    m_has_pending_skin_ubo_uploads = true;
  }
  [[nodiscard]] auto has_pending_skin_ubo_uploads() const noexcept -> bool {
    return m_has_pending_skin_ubo_uploads;
  }

  void upload_pending_skin_ubos();

  auto get_or_bake(const Render::Creature::CreatureSpec& spec,
                   Render::Creature::CreatureLOD lod,
                   std::span<const QMatrix4x4> rest_palette,
                   std::uint16_t variant_bucket = 0,
                   std::span<const Render::Creature::StaticAttachmentSpec> attachments =
                       {}) -> const RiggedMeshEntry*;

  auto get_or_bake(const Render::Creature::CreatureSpec& spec,
                   Render::Creature::CreatureLOD lod,
                   std::span<const QMatrix4x4> rest_palette,
                   std::uint16_t variant_bucket,
                   std::span<const Render::Creature::StaticAttachmentSpec> attachments,
                   std::uint32_t skin_species_id) -> const RiggedMeshEntry*;

  auto get_or_bake_prehashed(
      const Render::Creature::CreatureSpec& spec,
      Render::Creature::CreatureLOD lod,
      std::span<const QMatrix4x4> rest_palette,
      std::uint16_t variant_bucket,
      std::span<const Render::Creature::StaticAttachmentSpec> attachments,
      std::uint64_t attachments_hash,
      std::uint32_t attachment_set_id,
      std::uint32_t skin_species_id) -> const RiggedMeshEntry*;

  void clear() {
    release_skin_atlases();
    m_entries.clear();
    m_skin_atlases.clear();
    m_base_meshes.clear();
    m_attachment_meshes.clear();
    m_has_pending_skin_ubo_uploads = false;
  }

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_entries.size(); }

private:
  struct SkinAtlasKey {
    const Render::Creature::CreatureSpec* spec{nullptr};
    std::uint32_t skin_species_id{0};

    auto operator==(const SkinAtlasKey& other) const noexcept -> bool {
      return spec == other.spec && skin_species_id == other.skin_species_id;
    }
  };

  struct BaseMeshKey {
    const Render::Creature::CreatureSpec* spec{nullptr};
    Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
    std::uint32_t skin_species_id{0};

    auto operator==(const BaseMeshKey& other) const noexcept -> bool {
      return spec == other.spec && lod == other.lod &&
             skin_species_id == other.skin_species_id;
    }
  };

  struct BaseMeshKeyHash {
    auto operator()(const BaseMeshKey& key) const noexcept -> std::size_t {
      const auto spec_bits = reinterpret_cast<std::uintptr_t>(key.spec);
      return (spec_bits * 0x9E3779B97F4A7C15ULL) ^
             (static_cast<std::size_t>(key.lod) << 1U) ^
             (static_cast<std::size_t>(key.skin_species_id) * 0x85EBCA77ULL);
    }
  };

  struct AttachmentMeshKey {
    const Render::Creature::CreatureSpec* spec{nullptr};
    std::uint32_t skin_species_id{0};
    std::uint64_t attachments_hash{0};

    auto operator==(const AttachmentMeshKey& other) const noexcept -> bool {
      return spec == other.spec && skin_species_id == other.skin_species_id &&
             attachments_hash == other.attachments_hash;
    }
  };

  struct AttachmentMeshKeyHash {
    auto operator()(const AttachmentMeshKey& key) const noexcept -> std::size_t {
      const auto spec_bits = reinterpret_cast<std::uintptr_t>(key.spec);
      return (spec_bits * 0x9E3779B97F4A7C15ULL) ^
             (static_cast<std::size_t>(key.skin_species_id) * 0x85EBCA77ULL) ^
             static_cast<std::size_t>(key.attachments_hash);
    }
  };

  struct SkinAtlasKeyHash {
    auto operator()(const SkinAtlasKey& key) const noexcept -> std::size_t {
      const auto spec_bits = reinterpret_cast<std::uintptr_t>(key.spec);
      return (spec_bits * 0x9E3779B97F4A7C15ULL) ^
             (static_cast<std::size_t>(key.skin_species_id) * 0x85EBCA77ULL);
    }
  };

  void release_skin_atlases();

  std::unordered_map<Key, RiggedMeshEntry, KeyHash> m_entries;
  std::unordered_map<SkinAtlasKey, std::shared_ptr<RiggedSkinAtlas>, SkinAtlasKeyHash>
      m_skin_atlases;
  std::unordered_map<BaseMeshKey, std::shared_ptr<RiggedMesh>, BaseMeshKeyHash>
      m_base_meshes;
  std::unordered_map<AttachmentMeshKey,
                     std::shared_ptr<RiggedMesh>,
                     AttachmentMeshKeyHash>
      m_attachment_meshes;
  FrameStats m_frame_stats;
  bool m_has_pending_skin_ubo_uploads{false};
};

void rigged_entry_ensure_skin_atlas(const RiggedMeshEntry& entry,
                                    const QMatrix4x4* bpat_palettes,
                                    std::uint32_t frame_total,
                                    std::uint32_t bone_count);

void rigged_entry_ensure_skin_ubo(const RiggedMeshEntry& entry);

} // namespace Render::GL

namespace Render::Creature::Bpat {
class BpatBlob;
}

namespace Render::GL {

void rigged_entry_ensure_skin_atlas_from_blob(
    const RiggedMeshEntry& entry, const Render::Creature::Bpat::BpatBlob& blob);

}
