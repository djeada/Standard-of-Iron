#include "rigged_mesh_cache.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QtGlobal>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_reader.h"
#include "bone_palette_arena.h"
#include "creature/rigged_mesh_registry.h"
#include "creature/runtime_bake_guard.h"
#include "creature/spec.h"
#include "gl/platform_gl.h"

namespace Render::GL {

namespace {

auto rigged_cache_gl_funcs() -> QOpenGLFunctions_3_3_Core* {
  auto* ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }
  return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

auto describe_rigged_key(const Render::Creature::CreatureSpec& spec,
                         Render::Creature::CreatureLOD lod,
                         std::uint16_t variant_bucket,
                         std::uint32_t attachment_set_id,
                         std::uint64_t attachments_hash,
                         std::uint32_t skin_species_id) -> std::string {
  std::ostringstream out;
  out << "spec=" << &spec << " lod=" << static_cast<int>(lod)
      << " variant_bucket=" << variant_bucket << " skin_species_id=" << skin_species_id
      << " attachment_set_id=" << attachment_set_id << " attachments_hash=0x"
      << std::hex << attachments_hash;
  return out.str();
}

} // namespace

void rigged_entry_ensure_skin_ubo(const RiggedMeshEntry& entry) {
  if (entry.skin_atlas == nullptr) {
    return;
  }
  auto& atlas = *entry.skin_atlas;
  if (atlas.palette_ubo != 0) {
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
  auto* fn = rigged_cache_gl_funcs();
  if (fn == nullptr) {
    return;
  }

  const std::size_t stride = BonePaletteArena::k_palette_bytes;
  std::vector<float> staging(static_cast<std::size_t>(atlas.frame_total) *
                                 BonePaletteArena::k_palette_floats,
                             0.0F);
  for (std::uint32_t f = 0; f < atlas.frame_total; ++f) {
    const QMatrix4x4* frame_src =
        atlas.palettes.data() + static_cast<std::size_t>(f) * atlas.bone_count;

    float* frame_dst = staging.data() +
                       static_cast<std::size_t>(f) * BonePaletteArena::k_palette_floats;
    for (std::uint32_t b = 0; b < atlas.bone_count; ++b) {
      std::memcpy(frame_dst + b * BonePaletteArena::k_matrix_floats,
                  frame_src[b].constData(),
                  sizeof(float) * BonePaletteArena::k_matrix_floats);
    }

    for (std::uint32_t b = atlas.bone_count; b < BonePaletteArena::k_palette_width;
         ++b) {
      QMatrix4x4 const ident;
      std::memcpy(frame_dst + b * BonePaletteArena::k_matrix_floats,
                  ident.constData(),
                  sizeof(float) * BonePaletteArena::k_matrix_floats);
    }
  }
  GLuint ubo = 0;
  fn->glGenBuffers(1, &ubo);
  if (ubo == 0) {
    return;
  }
  fn->glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  fn->glBufferData(GL_UNIFORM_BUFFER,
                   static_cast<GLsizeiptr>(staging.size() * sizeof(float)),
                   staging.data(),
                   GL_STATIC_DRAW);
  fn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
  atlas.palette_ubo = ubo;
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

auto RiggedMeshCache::require_rigged_asset(
    const Key& key, std::string_view detail) const -> const RiggedMeshEntry* {
  const auto* entry = find_rigged_asset(key);
  if (entry == nullptr) {
    ++m_frame_stats.misses;
    Render::Creature::report_missing_preloaded_asset(detail);
  }
  return entry;
}

auto RiggedMeshCache::create_rigged_asset(
    const Key& key,
    std::span<const QMatrix4x4> rest_palette,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments,
    std::uint16_t variant_bucket) -> const RiggedMeshEntry* {
  const Render::Creature::CreatureSpec& spec = *key.spec;
  const Render::Creature::CreatureLOD lod = key.lod;
  const std::uint32_t skin_species_id = key.skin_species_id;
  const std::uint32_t attachment_set_id = key.attachment_set_id;
  const std::uint64_t attachments_hash = key.attachments_hash;
  if (auto it = m_entries.find(key); it != m_entries.end()) {
    ++m_frame_stats.hits;
    return &it->second;
  }
  RiggedMeshEntry entry;

  const Render::Creature::Rigged::RiggedMeshBlob* prebaked =
      Render::Creature::Rigged::RiggedMeshRegistry::instance().blob(skin_species_id,
                                                                    lod);

  const BaseMeshKey base_key{&spec, lod, skin_species_id};
  const AttachmentMeshKey attachment_key{&spec, lod, skin_species_id, attachments_hash};
  const bool base_is_cached = m_base_meshes.find(base_key) != m_base_meshes.end();
  const bool attachments_are_cached =
      attachments.empty() || m_attachment_meshes.contains(attachment_key);
  if (Render::Creature::runtime_bake_forbidden() &&
      ((!base_is_cached && prebaked == nullptr) || !attachments_are_cached)) {
    ++m_frame_stats.misses;
    Render::Creature::report_runtime_bake_violation(
        Render::Creature::RuntimeBakeOperation::RiggedMeshBake,
        describe_rigged_key(spec,
                            lod,
                            variant_bucket,
                            attachment_set_id,
                            attachments_hash,
                            skin_species_id));
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
      if (attachment_inserted || attachment_it->second == nullptr) {
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
        atlas->palette_ubo != 0U
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
  if (m_frame_index < k_min_idle_frames) {
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
    recount_residency();
  }

  const std::uint64_t freed = before - m_residency.total_bytes();
  m_frame_stats.evicted_bytes += freed;
  return freed;
}

void RiggedMeshCache::release_skin_atlases() {
  auto* fn = rigged_cache_gl_funcs();
  if (fn == nullptr) {
    return;
  }
  for (auto& [_, atlas] : m_skin_atlases) {
    if (atlas != nullptr && atlas->palette_ubo != 0) {
      fn->glDeleteBuffers(1, &atlas->palette_ubo);
      atlas->palette_ubo = 0;
    }
  }
}

RiggedMeshCache::~RiggedMeshCache() {
  release_skin_atlases();
}

void RiggedMeshCache::upload_pending_skin_ubos() {
  if (!m_has_pending_skin_ubo_uploads || rigged_cache_gl_funcs() == nullptr) {
    return;
  }

  Render::Creature::RuntimeBakeAllowScope const initialization_scope;
  m_has_pending_skin_ubo_uploads = false;
  for (auto& [_, entry] : m_entries) {
    if (entry.skin_atlas == nullptr || entry.skin_atlas->palette_ubo != 0U ||
        entry.skin_atlas->palettes.empty() || entry.skin_atlas->frame_total == 0U ||
        entry.skin_atlas->bone_count == 0U) {
      continue;
    }

    rigged_entry_ensure_skin_ubo(entry);
    if (entry.skin_atlas->palette_ubo == 0U) {
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
  if (atlas.palette_ubo != 0U) {
    if (auto* fn = rigged_cache_gl_funcs(); fn != nullptr) {
      GLuint stale = atlas.palette_ubo;
      fn->glDeleteBuffers(1, &stale);
    }
    atlas.palette_ubo = 0U;
    atlas.frame_stride_bytes = 0;
  }
  atlas.palette_storage = std::move(storage);
  atlas.palettes = blob.palette_matrices();
  atlas.frame_total = blob.frame_total();
  atlas.bone_count = blob.bone_count();
}

} // namespace Render::GL
