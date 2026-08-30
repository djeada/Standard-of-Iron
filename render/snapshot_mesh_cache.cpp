#include "snapshot_mesh_cache.h"

#include <QVector3D>
#include <QVector4D>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <utility>

#include "bone_palette_arena.h"
#include "creature/runtime_bake_guard.h"
#include "creature/snapshot_mesh_asset.h"
#include "rigged_mesh_cache.h"
#include "snapshot_mesh_bake.h"

namespace Render::GL {

namespace {

auto build_identity_palette() noexcept
    -> std::array<QMatrix4x4, BonePaletteArena::k_palette_width> {
  std::array<QMatrix4x4, BonePaletteArena::k_palette_width> arr{};
  for (auto& m : arr) {
    m.setToIdentity();
  }
  return arr;
}

} // namespace

auto SnapshotMeshCache::identity_palette() noexcept -> const QMatrix4x4* {
  static const auto k_palette = build_identity_palette();
  return k_palette.data();
}

namespace {

auto build_entry(std::span<const RiggedVertex> vertices,
                 std::span<const std::uint32_t> indices) -> SnapshotMeshEntry {
  SnapshotMeshEntry entry{};
  std::vector<RiggedVertex> baked(vertices.begin(), vertices.end());
  std::vector<std::uint32_t> indices_copy(indices.begin(), indices.end());
  entry.mesh = std::make_unique<RiggedMesh>(std::move(baked), std::move(indices_copy));
  return entry;
}

auto build_entry(std::vector<RiggedVertex>&& vertices,
                 std::span<const std::uint32_t> indices) -> SnapshotMeshEntry {
  SnapshotMeshEntry entry{};
  std::vector<std::uint32_t> indices_copy(indices.begin(), indices.end());
  entry.mesh =
      std::make_unique<RiggedMesh>(std::move(vertices), std::move(indices_copy));
  return entry;
}

auto describe_snapshot_key(const SnapshotMeshCache::Key& key,
                           std::uint32_t global_frame) -> std::string {
  std::ostringstream out;
  out << "asset=" << key.asset_id << " archetype=" << key.archetype
      << " attachment_set_id=" << key.attachment_set_id << " variant=" << key.variant
      << " state=" << static_cast<int>(key.state) << " clip=" << key.clip_id
      << " clip_variant=" << static_cast<int>(key.clip_variant)
      << " frame_in_clip=" << key.frame_in_clip << " global_frame=" << global_frame;
  return out.str();
}

auto mesh_bytes(const RiggedMesh* mesh) -> std::size_t {
  if (mesh == nullptr) {
    return 0;
  }

  const std::size_t cpu = (mesh->get_vertices().size() * sizeof(RiggedVertex)) +
                          (mesh->get_indices().size() * sizeof(std::uint32_t));

  return cpu * 2U;
}

} // namespace

auto SnapshotMeshCache::budget_bytes() noexcept -> std::size_t {
  static const std::size_t budget = [] {
    constexpr std::size_t k_default_megabytes = 384;
    const int override_mb = qEnvironmentVariableIntValue("SOI_SNAPSHOT_CACHE_MB");
    const std::size_t megabytes =
        override_mb > 0 ? static_cast<std::size_t>(override_mb) : k_default_megabytes;
    return megabytes * 1024U * 1024U;
  }();
  return budget;
}

auto SnapshotMeshCache::note_insertion(SnapshotMeshEntry& entry) -> void {
  entry.bytes = mesh_bytes(entry.mesh.get());
  for (const auto& attachment : entry.attachment_meshes) {
    entry.bytes += mesh_bytes(attachment.get());
  }
  entry.last_used_frame = m_frame_index;
  m_resident_bytes += entry.bytes;
}

void SnapshotMeshCache::trim_to_budget() {
  const std::size_t budget = budget_bytes();
  if (m_resident_bytes <= budget) {
    return;
  }

  std::vector<std::pair<std::uint64_t, const Key*>> candidates;
  candidates.reserve(m_entries.size());
  for (const auto& [key, entry] : m_entries) {
    if (entry.last_used_frame + k_eviction_grace_frames > m_frame_index) {
      continue;
    }
    candidates.emplace_back(entry.last_used_frame, &key);
  }
  std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  });

  for (const auto& [frame, key] : candidates) {
    if (m_resident_bytes <= budget) {
      break;
    }
    auto it = m_entries.find(*key);
    if (it == m_entries.end()) {
      continue;
    }
    m_resident_bytes -= std::min(m_resident_bytes, it->second.bytes);
    m_entries.erase(it);
    ++m_frame_stats.evictions;
  }
}

auto SnapshotMeshCache::get_or_bake(const Key& key,
                                    const RiggedMeshEntry& source,
                                    std::uint32_t global_frame)
    -> const SnapshotMeshEntry* {
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    ++m_frame_stats.hits;
    it->second.last_used_frame = m_frame_index;
    return &it->second;
  }
  if (Render::Creature::runtime_bake_forbidden()) {
    ++m_frame_stats.misses;
    Render::Creature::report_runtime_bake_violation(
        Render::Creature::RuntimeBakeOperation::SnapshotMeshBake,
        describe_snapshot_key(key, global_frame));
    return nullptr;
  }

  if (source.mesh == nullptr || source.skin_atlas == nullptr ||
      source.skin_atlas->palettes.empty() || source.skin_atlas->bone_count == 0U ||
      global_frame >= source.skin_atlas->frame_total ||
      source.mesh->get_vertices().empty() || source.mesh->get_indices().empty()) {
    ++m_frame_stats.misses;
    return nullptr;
  }

  const auto& src_vertices = source.mesh->get_vertices();
  const auto& src_indices = source.mesh->get_indices();
  const QMatrix4x4* frame_palette =
      source.skin_atlas->palettes.data() +
      static_cast<std::size_t>(global_frame) *
          static_cast<std::size_t>(source.skin_atlas->bone_count);
  auto baked = bake_snapshot_vertices(
      src_vertices,
      {frame_palette, static_cast<std::size_t>(source.skin_atlas->bone_count)});

  SnapshotMeshEntry entry = build_entry(std::move(baked), src_indices);

  const std::span<const QMatrix4x4> palette_span{
      frame_palette, static_cast<std::size_t>(source.skin_atlas->bone_count)};
  for (const auto& attachment : source.attachment_meshes) {
    if (attachment == nullptr || attachment->get_indices().empty() ||
        attachment->get_vertices().empty()) {
      continue;
    }
    auto baked_attachment =
        bake_snapshot_vertices(attachment->get_vertices(), palette_span);
    entry.attachment_meshes.push_back(std::make_unique<RiggedMesh>(
        std::move(baked_attachment),
        std::vector<std::uint32_t>(attachment->get_indices().begin(),
                                   attachment->get_indices().end())));
  }

  auto [it, _ok] = m_entries.emplace(key, std::move(entry));
  note_insertion(it->second);
  ++m_frame_stats.bakes;
  return &it->second;
}

auto SnapshotMeshCache::get_or_load(
    const Key& key,
    const Render::Creature::Snapshot::SnapshotMeshBlob& source,
    std::uint32_t global_frame) -> const SnapshotMeshEntry* {
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    ++m_frame_stats.hits;
    it->second.last_used_frame = m_frame_index;
    return &it->second;
  }

  const auto vertices = source.frame_vertices_view(global_frame);
  const auto indices = source.indices_view();
  if (vertices.empty() || indices.empty()) {
    ++m_frame_stats.misses;
    return nullptr;
  }

  SnapshotMeshEntry entry = build_entry(vertices, indices);
  auto [it, _ok] = m_entries.emplace(key, std::move(entry));
  note_insertion(it->second);
  ++m_frame_stats.loads;
  return &it->second;
}

} // namespace Render::GL
