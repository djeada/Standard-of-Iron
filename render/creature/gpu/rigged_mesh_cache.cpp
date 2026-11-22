#include "render/creature/gpu/rigged_mesh_cache.h"

#include <QtGlobal>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_reader.h"
#include "render/creature/gpu/attachment_mesh_library.h"
#include "render/creature/gpu/rigged_mesh_reduce.h"
#include "render/creature/rigged_mesh_registry.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/spec.h"
#include "render/frame/bone_palette_arena.h"

namespace Render::GL {

namespace {

auto describe_rigged_key(const RiggedMeshCache::Key& key) -> std::string {
  std::ostringstream out;
  out << "spec=" << key.spec << " lod=" << static_cast<int>(key.lod)
      << " skin_species_id=" << key.skin_species_id
      << " attachment_set_id=" << key.attachment_set_id << " attachments_hash=0x"
      << std::hex << key.attachments_hash;
  return out.str();
}

} // namespace

void rigged_entry_ensure_skin_ubo(const RiggedMeshEntry& entry) {
  if (entry.skin_atlas == nullptr) {
    return;
  }
  auto& atlas = *entry.skin_atlas;
  if (atlas.palette_block != nullptr) {
    return;
  }
  if (atlas.palettes.empty() || atlas.frame_total == 0 || atlas.bone_count == 0) {
    return;
  }
  if (Render::Creature::runtime_bake_forbidden()) {
    Render::Creature::report_runtime_bake_violation(
        Render::Creature::RuntimeBakeOperation::SkinUboUpload,
        "skin palette UBO missing for rigged mesh entry");
    return;
  }

  const std::size_t stride = BonePaletteArena::k_palette_bytes;
  const std::size_t bytes = static_cast<std::size_t>(atlas.frame_total) * stride;
  // The block writes the padded palettes straight from the atlas storage when a
  // backend uploads it, so the atlas is never held twice on the CPU.
  auto writer = [storage = atlas.palette_storage,
                 palettes = atlas.palettes,
                 frame_total = atlas.frame_total,
                 bone_count = atlas.bone_count](std::span<std::byte> destination) {
    (void)storage;
    auto* floats = reinterpret_cast<float*>(destination.data());
    const std::size_t frames = std::min<std::size_t>(
        frame_total, destination.size() / BonePaletteArena::k_palette_bytes);
    QMatrix4x4 const ident;
    for (std::size_t f = 0; f < frames; ++f) {
      const QMatrix4x4* frame_src = palettes.data() + (f * bone_count);
      float* frame_dst = floats + (f * BonePaletteArena::k_palette_floats);
      for (std::uint32_t b = 0; b < bone_count; ++b) {
        std::memcpy(frame_dst + (b * BonePaletteArena::k_matrix_floats),
                    frame_src[b].constData(),
                    sizeof(float) * BonePaletteArena::k_matrix_floats);
      }
      for (std::size_t b = bone_count; b < BonePaletteArena::k_palette_width; ++b) {
        std::memcpy(frame_dst + (b * BonePaletteArena::k_matrix_floats),
                    ident.constData(),
                    sizeof(float) * BonePaletteArena::k_matrix_floats);
      }
    }
  };
  auto block = std::make_shared<InstanceBlock>();
  block->set_target(InstanceBlock::Target::Uniform);
  block->assign_generated(bytes, std::move(writer));
  atlas.palette_block = std::move(block);
  atlas.frame_stride_bytes = stride;
}

auto RiggedMeshCache::get_or_bake(
    const Render::Creature::CreatureSpec& spec,
    Render::Creature::CreatureLOD lod,
    std::span<const QMatrix4x4> rest_palette,
    std::uint16_t variant_bucket,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments)
    -> const RiggedMeshEntry* {
  return get_or_bake(spec, lod, rest_palette, variant_bucket, attachments, 0);
}

auto RiggedMeshCache::get_or_bake(
    const Render::Creature::CreatureSpec& spec,
    Render::Creature::CreatureLOD lod,
    std::span<const QMatrix4x4> rest_palette,
    std::uint16_t variant_bucket,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments,
    std::uint32_t skin_species_id) -> const RiggedMeshEntry* {
  return get_or_bake_prehashed(
      spec,
      lod,
      rest_palette,
      variant_bucket,
      attachments,
      Render::Creature::static_attachments_hash(attachments.data(), attachments.size()),
      0U,
      skin_species_id);
}

auto RiggedMeshCache::get_or_bake_prehashed(
    const Render::Creature::CreatureSpec& spec,
    Render::Creature::CreatureLOD lod,
    std::span<const QMatrix4x4> rest_palette,
    std::uint16_t variant_bucket,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments,
    std::uint64_t attachments_hash,
    std::uint32_t attachment_set_id,
    std::uint32_t skin_species_id) -> const RiggedMeshEntry* {
  Key const key{&spec, lod, skin_species_id, attachment_set_id, attachments_hash};
  return create_rigged_asset(key, rest_palette, attachments, variant_bucket);
}

auto RiggedMeshCache::find_rigged_asset(const Key& key) const noexcept
    -> const RiggedMeshEntry* {
  auto it = m_entries.find(key);
  if (it == m_entries.end()) {
    return nullptr;
  }
  it->second.last_used_frame = m_frame_index;
  ++m_frame_stats.hits;
  return &it->second;
}

auto RiggedMeshCache::require_rigged_asset(const Key& key) const
    -> const RiggedMeshEntry* {
  const auto* entry = find_rigged_asset(key);
  if (entry == nullptr) {
    ++m_frame_stats.misses;
    Render::Creature::report_missing_preloaded_asset(describe_rigged_key(key));
  }
  return entry;
}

auto RiggedMeshCache::create_rigged_asset(
    const Key& key,
    std::span<const QMatrix4x4> rest_palette,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments,
    std::uint16_t,
    std::string_view attachment_set_name) -> const RiggedMeshEntry* {
  const Render::Creature::CreatureSpec& spec = *key.spec;
  const Render::Creature::CreatureLOD lod = key.lod;
  const std::uint32_t skin_species_id = key.skin_species_id;
  const std::uint64_t attachments_hash = key.attachments_hash;
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    ++m_frame_stats.hits;
    return &it->second;
  }
  RiggedMeshEntry entry;

  auto const& rigged_registry =
      Render::Creature::Rigged::RiggedMeshRegistry::instance();
  const Render::Creature::Rigged::RiggedMeshBlob* prebaked =
      spec.body_variant != 0U ? rigged_registry.body(spec.species_name, lod)
                              : rigged_registry.blob(skin_species_id, lod);

  const BaseMeshKey base_key{&spec, lod, skin_species_id};
  const AttachmentMeshKey attachment_key{&spec, lod, skin_species_id, attachments_hash};
  const bool base_is_cached = m_base_meshes.find(base_key) != m_base_meshes.end();
  std::shared_ptr<const AttachmentMeshEntry> baked_attachments;
  std::string pack_key;
  if (!attachments.empty() && !attachment_set_name.empty()) {
    pack_key = attachment_mesh_key(spec, lod, skin_species_id, attachment_set_name);
    m_attachment_pack_keys.try_emplace(pack_key, attachment_key);
    if (!m_attachment_meshes.contains(attachment_key)) {
      baked_attachments = AttachmentMeshLibrary::instance().find(pack_key);
    }
  }
  const bool attachments_are_cached = attachments.empty() ||
                                      baked_attachments != nullptr ||
                                      m_attachment_meshes.contains(attachment_key);
  if (Render::Creature::runtime_bake_forbidden() &&
      ((!base_is_cached && prebaked == nullptr) || !attachments_are_cached)) {
    ++m_frame_stats.misses;
    Render::Creature::report_runtime_bake_violation(
        Render::Creature::RuntimeBakeOperation::RiggedMeshBake,
        describe_rigged_key(key));
    return nullptr;
  }

  {
    auto [base_it, inserted] = m_base_meshes.try_emplace(base_key);
    if (inserted || base_it->second == nullptr) {
      if (prebaked != nullptr) {
        auto const vertices = prebaked->vertices_view();
        auto const indices = prebaked->indices_view();
        base_it->second = std::make_shared<RiggedMesh>(
            std::vector<RiggedVertex>(vertices.begin(), vertices.end()),
            std::vector<std::uint32_t>(indices.begin(), indices.end()));
      } else {
        Render::Creature::BakeInput input{};
        input.graph = &Render::Creature::part_graph_for(spec, lod);
        input.bind_pose = rest_palette;
        input.lod = lod;
        base_it->second = std::shared_ptr<RiggedMesh>(
            Render::Creature::bake_rigged_mesh(input).release());
      }
    }
    entry.mesh = base_it->second;
    if (!attachments.empty()) {
      auto [attachment_it, attachment_inserted] =
          m_attachment_meshes.try_emplace(attachment_key);
      if (baked_attachments != nullptr) {
        attachment_it->second = std::make_shared<RiggedMesh>(
            baked_attachments->vertices, baked_attachments->indices);
      } else if (attachment_inserted || attachment_it->second == nullptr) {
        ++m_runtime_attachment_bakes;
        Render::Creature::BakeInput input{};
        input.bind_pose = rest_palette;
        input.attachments = attachments;
        input.lod = lod;
        attachment_it->second = std::shared_ptr<RiggedMesh>(
            Render::Creature::bake_rigged_mesh(input).release());
      }
      if (attachment_it->second != nullptr &&
          attachment_it->second->index_count() != 0U) {
        entry.attachment_meshes.push_back(attachment_it->second);
      }
    }
  }

  if (lod == Render::Creature::CreatureLOD::Full && entry.mesh != nullptr) {
    entry.height = rigged_mesh_height(*entry.mesh);
    entry.reduced_mesh = reduced_copy(entry.mesh);
    entry.reduced_attachment_meshes.reserve(entry.attachment_meshes.size());
    for (const auto& attachment : entry.attachment_meshes) {
      entry.reduced_attachment_meshes.push_back(reduced_copy(attachment));
    }
  }

  const SkinAtlasKey atlas_key{&spec, skin_species_id};
  auto [atlas_it, atlas_inserted] = m_skin_atlases.try_emplace(atlas_key);
  if (atlas_inserted || atlas_it->second == nullptr) {
    atlas_it->second = std::make_shared<RiggedSkinAtlas>();
  }
  entry.skin_atlas = atlas_it->second;

  ++m_frame_stats.bakes;
  entry.last_used_frame = m_frame_index;
  auto [it, _] = m_entries.emplace(key, std::move(entry));
  recount_residency();
  return &it->second;
}

auto RiggedMeshCache::reduced_copy(const std::shared_ptr<RiggedMesh>& mesh)
    -> std::shared_ptr<RiggedMesh> {
  if (mesh == nullptr) {
    return nullptr;
  }
  auto [it, inserted] = m_reduced_meshes.try_emplace(mesh->id());
  if (inserted) {
    it->second = reduce_rigged_mesh(*mesh, k_reduced_geometry_max_error);
  }
  return it->second;
}

namespace {

auto mesh_bytes(const RiggedMesh& mesh) -> std::uint64_t {
  const auto cpu =
      static_cast<std::uint64_t>(mesh.get_vertices().size() * sizeof(RiggedVertex)) +
      static_cast<std::uint64_t>(mesh.get_indices().size() * sizeof(std::uint32_t));
  return cpu * 2U;
}

} // namespace

void RiggedMeshCache::recount_residency() {
  std::uint64_t base = 0;
  for (const auto& [_, mesh] : m_base_meshes) {
    if (mesh != nullptr) {
      base += mesh_bytes(*mesh);
    }
  }
  for (const auto& [_, mesh] : m_reduced_meshes) {
    if (mesh != nullptr) {
      base += mesh_bytes(*mesh);
    }
  }
  std::uint64_t attachments = 0;
  for (const auto& [_, mesh] : m_attachment_meshes) {
    if (mesh != nullptr) {
      attachments += mesh_bytes(*mesh);
    }
  }
  std::uint64_t atlases = 0;
  for (const auto& [_, atlas] : m_skin_atlases) {
    if (atlas == nullptr) {
      continue;
    }
    const auto palette_bytes =
        atlas->palette_storage != nullptr
            ? static_cast<std::uint64_t>(atlas->palette_storage->size() *
                                         sizeof(QMatrix4x4))
            : 0U;
    const auto ubo_bytes =
        atlas->palette_block != nullptr
            ? static_cast<std::uint64_t>(atlas->frame_total) * atlas->frame_stride_bytes
            : 0U;
    atlases += palette_bytes + ubo_bytes;
  }
  m_residency.mesh_bytes = base;
  m_residency.attachment_bytes = attachments;
  m_residency.atlas_bytes = atlases;
  m_residency.high_water_bytes =
      std::max(m_residency.high_water_bytes, m_residency.total_bytes());
}

void RiggedMeshCache::begin_frame() {
  ++m_frame_index;
  if (m_residency.budget_bytes == 0U) {
    return;
  }
  if (m_residency.total_bytes() > m_residency.budget_bytes) {
    evict_unused_over_budget();
  }
}

auto RiggedMeshCache::evict_unused_over_budget() -> std::uint64_t {
  constexpr std::uint64_t k_min_idle_frames = 240U;
  if (m_frame_index < k_min_idle_frames || Render::Creature::runtime_bake_forbidden()) {
    return 0;
  }
  const std::uint64_t before = m_residency.total_bytes();

  std::vector<Key> cold;
  for (const auto& [key, entry] : m_entries) {
    if (m_frame_index - entry.last_used_frame < k_min_idle_frames) {
      continue;
    }
    if (entry.mesh != nullptr && entry.mesh.use_count() > 2) {
      continue;
    }
    cold.push_back(key);
  }
  std::sort(cold.begin(), cold.end(), [this](const Key& lhs, const Key& rhs) {
    return m_entries.at(lhs).last_used_frame < m_entries.at(rhs).last_used_frame;
  });

  for (const Key& key : cold) {
    if (m_residency.total_bytes() <= m_residency.budget_bytes) {
      break;
    }
    m_entries.erase(key);
    ++m_frame_stats.evictions;
    for (auto it = m_base_meshes.begin(); it != m_base_meshes.end();) {
      it = (it->second != nullptr && it->second.use_count() == 1)
               ? m_base_meshes.erase(it)
               : std::next(it);
    }
    for (auto it = m_attachment_meshes.begin(); it != m_attachment_meshes.end();) {
      it = (it->second != nullptr && it->second.use_count() == 1)
               ? m_attachment_meshes.erase(it)
               : std::next(it);
    }
    // A null copy (nothing worth simplifying) stays, so the mesh is not
    // simplified again when an entry drawing it comes back.
    for (auto it = m_reduced_meshes.begin(); it != m_reduced_meshes.end();) {
      it = (it->second != nullptr && it->second.use_count() == 1)
               ? m_reduced_meshes.erase(it)
               : std::next(it);
    }
    recount_residency();
  }

  const std::uint64_t freed = before - m_residency.total_bytes();
  m_frame_stats.evicted_bytes += freed;
  return freed;
}

void RiggedMeshCache::release_skin_atlases() {
  for (auto& [_, atlas] : m_skin_atlases) {
    if (atlas != nullptr) {
      atlas->palette_block.reset();
    }
  }
}

RiggedMeshCache::~RiggedMeshCache() {
  release_skin_atlases();
}

auto RiggedMeshCache::prepare_gpu_data() -> bool {
  if (!m_palette_blocks_enabled) {
    return false;
  }
  Render::Creature::RuntimeBakeAllowScope const loading_scope;
  bool ready = true;
  for (auto& [_, entry] : m_entries) {
    const auto* atlas = entry.skin_atlas.get();
    if (atlas != nullptr && atlas->palette_block == nullptr &&
        !atlas->palettes.empty() && atlas->frame_total > 0 && atlas->bone_count > 0) {
      rigged_entry_ensure_skin_ubo(entry);
      if (atlas->palette_block != nullptr) {
        record_skin_ubo_upload(static_cast<std::uint64_t>(atlas->frame_total) *
                               BonePaletteArena::k_palette_bytes);
      } else {
        ready = false;
      }
    }
  }
  return ready;
}

void RiggedMeshCache::for_each_mesh(
    const std::function<void(const RiggedMesh&)>& visit) const {
  for (const auto& [_, entry] : m_entries) {
    if (entry.mesh != nullptr) {
      visit(*entry.mesh);
    }
    for (const auto& attachment : entry.attachment_meshes) {
      if (attachment != nullptr) {
        visit(*attachment);
      }
    }
    if (entry.reduced_mesh != nullptr) {
      visit(*entry.reduced_mesh);
    }
    for (const auto& attachment : entry.reduced_attachment_meshes) {
      if (attachment != nullptr) {
        visit(*attachment);
      }
    }
  }
}

void RiggedMeshCache::for_each_palette_block(const InstanceBlockVisitor& visit) const {
  for (const auto& [_, entry] : m_entries) {
    if (entry.skin_atlas != nullptr && entry.skin_atlas->palette_block != nullptr) {
      visit(*entry.skin_atlas->palette_block);
    }
  }
}

void RiggedMeshCache::upload_pending_skin_ubos() {
  if (!m_has_pending_skin_ubo_uploads || !m_palette_blocks_enabled) {
    return;
  }

  Render::Creature::RuntimeBakeAllowScope const initialization_scope;
  m_has_pending_skin_ubo_uploads = false;
  for (auto& [_, entry] : m_entries) {
    if (entry.skin_atlas == nullptr || entry.skin_atlas->palette_block != nullptr ||
        entry.skin_atlas->palettes.empty() || entry.skin_atlas->frame_total == 0U ||
        entry.skin_atlas->bone_count == 0U) {
      continue;
    }

    rigged_entry_ensure_skin_ubo(entry);
    if (entry.skin_atlas->palette_block == nullptr) {
      m_has_pending_skin_ubo_uploads = true;
      continue;
    }
    record_skin_ubo_upload(static_cast<std::uint64_t>(entry.skin_atlas->frame_total) *
                           BonePaletteArena::k_palette_bytes);
  }
}

void rigged_entry_ensure_skin_atlas_from_blob(
    const RiggedMeshEntry& entry, const Render::Creature::Bpat::BpatBlob& blob) {
  if (!blob.loaded() || entry.skin_atlas == nullptr) {
    return;
  }
  auto& atlas = *entry.skin_atlas;
  auto storage = blob.palette_storage();
  if (storage == nullptr || storage->empty() || atlas.palette_storage == storage) {
    return;
  }
  if (atlas.palette_block != nullptr) {
    atlas.palette_block.reset();
    atlas.frame_stride_bytes = 0;
  }
  atlas.palette_storage = std::move(storage);
  atlas.palettes = blob.palette_matrices();
  atlas.frame_total = blob.frame_total();
  atlas.bone_count = blob.bone_count();
}

} // namespace Render::GL
