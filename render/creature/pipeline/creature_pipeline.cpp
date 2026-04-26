#include "creature_pipeline.h"

#include "../../bone_palette_arena.h"
#include "../../entity/registry.h"
#include "../../rigged_mesh_cache.h"
#include "../../scene_renderer.h"
#include "../../snapshot_mesh_cache.h"
#include "../../submitter.h"
#include "../archetype_registry.h"
#include "../bpat/bpat_format.h"
#include "../bpat/bpat_reader.h"
#include "../bpat/bpat_registry.h"
#include "../skeleton.h"
#include "../spec.h"
#include "creature_asset.h"

#include "../../humanoid/skeleton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Render::Creature::Pipeline {

namespace {

inline constexpr std::size_t kCreatureRolePaletteSize = 16;
using RoleColorArray = std::array<QVector3D, kCreatureRolePaletteSize>;

auto resolve_renderer(Render::GL::ISubmitter &out) noexcept
    -> Render::GL::Renderer * {
  if (auto *renderer = dynamic_cast<Render::GL::Renderer *>(&out)) {
    return renderer;
  }
  if (auto *batch = dynamic_cast<Render::GL::BatchingSubmitter *>(&out)) {
    return dynamic_cast<Render::GL::Renderer *>(batch->fallback_submitter());
  }
  return nullptr;
}

auto species_to_bpat_id(CreatureKind kind) noexcept -> std::uint32_t {
  switch (kind) {
  case CreatureKind::Humanoid:
    return Render::Creature::Bpat::kSpeciesHumanoid;
  case CreatureKind::Horse:
    return Render::Creature::Bpat::kSpeciesHorse;
  case CreatureKind::Elephant:
    return Render::Creature::Bpat::kSpeciesElephant;
  case CreatureKind::Mounted:
    return 0xFFFFFFFFu;
  }
  return 0xFFFFFFFFu;
}

void submit_rigged_creature(
    const CreatureAsset &asset, CreatureLOD lod,
    std::span<const QVector3D> role_colors, std::uint16_t variant_bucket,
    const QVector3D &base_color, const QMatrix4x4 &world_from_unit,
    const Render::Creature::Bpat::BpatBlob &blob, std::uint32_t global_frame,
    Render::GL::ISubmitter &out,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments = {}) {
  if (lod == CreatureLOD::Billboard || asset.spec == nullptr ||
      asset.bind_palette == nullptr) {
    return;
  }
  auto *renderer = resolve_renderer(out);
  auto bind = asset.bind_palette();
  auto &cache = (renderer != nullptr)
                    ? renderer->rigged_mesh_cache()
                    : ([]() -> Render::GL::RiggedMeshCache & {
                        thread_local Render::GL::RiggedMeshCache c;
                        return c;
                      })();
  auto *entry =
      cache.get_or_bake(*asset.spec, lod, bind, variant_bucket, attachments);
  if (entry == nullptr || entry->mesh == nullptr ||
      entry->mesh->index_count() == 0U) {
    return;
  }

  Render::GL::rigged_entry_ensure_skin_atlas_from_blob(*entry, blob);
  if (renderer != nullptr) {
    Render::GL::rigged_entry_ensure_skin_ubo(*entry);
  }

  if (entry->skinned_palettes.empty() || entry->skinned_bone_count == 0 ||
      global_frame >= entry->skinned_frame_total) {
    return;
  }

  const QMatrix4x4 *frame_palette =
      entry->skinned_palettes.data() +
      static_cast<std::size_t>(global_frame) * entry->skinned_bone_count;

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.world = world_from_unit;
  cmd.bone_count = entry->skinned_bone_count;
  cmd.bone_palette = frame_palette;
  cmd.palette_ubo = entry->skin_palette_ubo;
  cmd.palette_offset =
      static_cast<std::uint32_t>(static_cast<std::size_t>(global_frame) *
                                 entry->skin_palette_frame_stride_bytes);
  cmd.role_color_count = static_cast<std::uint32_t>(role_colors.size());
  for (std::size_t i = 0; i < role_colors.size(); ++i) {
    cmd.role_colors[i] = role_colors[i];
  }
  cmd.color = base_color;
  cmd.alpha = 1.0F;
  cmd.texture = nullptr;
  cmd.material_id = 0;
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);

  out.rigged(cmd);
}

auto submit_snapshot_creature(
    const CreatureAsset &asset, CreatureLOD lod,
    Render::Creature::ArchetypeId archetype,
    Render::Creature::VariantId variant,
    Render::Creature::AnimationStateId state,
    std::span<const QVector3D> role_colors, std::uint16_t variant_bucket,
    const QVector3D &base_color, const QMatrix4x4 &world_from_unit,
    const Render::Creature::Bpat::BpatBlob &blob, std::uint32_t global_frame,
    std::uint32_t frame_in_clip, Render::GL::ISubmitter &out,
    std::span<const Render::Creature::StaticAttachmentSpec> attachments = {})
    -> bool {
  if (lod == CreatureLOD::Billboard || asset.spec == nullptr ||
      asset.bind_palette == nullptr) {
    return false;
  }

  auto *renderer = resolve_renderer(out);
  if (renderer == nullptr) {
    return false;
  }

  auto &rigged_cache = renderer->rigged_mesh_cache();
  auto bind = asset.bind_palette();
  auto *source = rigged_cache.get_or_bake(*asset.spec, lod, bind,
                                          variant_bucket, attachments);
  if (source == nullptr || source->mesh == nullptr ||
      source->mesh->index_count() == 0U) {
    return false;
  }

  Render::GL::rigged_entry_ensure_skin_atlas_from_blob(*source, blob);
  if (source->skinned_palettes.empty() || source->skinned_bone_count == 0 ||
      global_frame >= source->skinned_frame_total) {
    return false;
  }

  Render::GL::SnapshotMeshCache::Key key{};
  key.archetype = archetype;
  key.variant = variant;
  key.state = state;
  key.frame_in_clip = frame_in_clip;

  const auto *snap =
      renderer->snapshot_mesh_cache().get_or_bake(key, *source, global_frame);
  if (snap == nullptr || snap->mesh == nullptr ||
      snap->mesh->index_count() == 0U) {
    return false;
  }

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = snap->mesh.get();
  cmd.world = world_from_unit;
  cmd.bone_count = 1U;
  cmd.bone_palette = Render::GL::SnapshotMeshCache::identity_palette();
  cmd.palette_ubo = 0U;
  cmd.palette_offset = 0U;
  cmd.role_color_count = static_cast<std::uint32_t>(role_colors.size());
  for (std::size_t i = 0; i < role_colors.size(); ++i) {
    cmd.role_colors[i] = role_colors[i];
  }
  cmd.color = base_color;
  cmd.alpha = 1.0F;
  cmd.texture = nullptr;
  cmd.material_id = 0;
  cmd.variation_scale = QVector3D(1.0F, 1.0F, 1.0F);

  out.rigged(cmd);
  return true;
}

void bump_lod_counters(CreatureLOD lod, SubmitStats &stats) {
  switch (lod) {
  case CreatureLOD::Full:
    ++stats.lod_full;
    break;
  case CreatureLOD::Reduced:
    ++stats.lod_reduced;
    break;
  case CreatureLOD::Minimal:
    ++stats.lod_minimal;
    break;
  case CreatureLOD::Billboard:
    ++stats.lod_billboard;
    break;
  }
}

struct ResolvedPlayback {
  const Render::Creature::Bpat::BpatBlob *blob{nullptr};
  std::uint32_t global_frame{0U};
  std::uint32_t frame_in_clip{0U};
};

auto resolve_clip_playback(std::uint32_t species_id, std::uint16_t clip_id,
                           float phase) noexcept -> ResolvedPlayback {
  ResolvedPlayback r{};
  if (clip_id == Render::Creature::ArchetypeDescriptor::kUnmappedClip) {
    return r;
  }
  const auto *blob =
      Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return r;
  }
  auto const c = blob->clip(clip_id);
  if (c.frame_count == 0U) {
    return r;
  }
  float p = phase - std::floor(phase);
  if (p < 0.0F) {
    p += 1.0F;
  }
  auto const fc = static_cast<float>(c.frame_count);
  auto frame_idx = static_cast<int>(p * fc);
  if (frame_idx < 0) {
    frame_idx = 0;
  }
  if (frame_idx >= static_cast<int>(c.frame_count)) {
    frame_idx = static_cast<int>(c.frame_count) - 1;
  }
  r.blob = blob;
  r.frame_in_clip = static_cast<std::uint32_t>(frame_idx);
  r.global_frame = c.frame_offset + r.frame_in_clip;
  return r;
}

auto resolve_blob_palette(CreatureKind kind, BpatPlayback playback,
                          const Render::Creature::Bpat::BpatBlob *&out_blob,
                          std::uint32_t &out_global_frame) noexcept
    -> std::span<const QMatrix4x4> {
  out_blob = nullptr;
  out_global_frame = 0U;
  if (playback.clip_id == kInvalidBpatClip) {
    return {};
  }
  const auto species_id = species_to_bpat_id(kind);
  if (species_id == 0xFFFFFFFFu) {
    return {};
  }
  const auto *blob =
      Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || playback.clip_id >= blob->clip_count()) {
    return {};
  }
  auto const clip = blob->clip(playback.clip_id);
  if (clip.frame_count == 0U) {
    return {};
  }
  std::uint32_t const wrapped = playback.frame_in_clip % clip.frame_count;
  out_global_frame = clip.frame_offset + wrapped;
  out_blob = blob;
  return blob->frame_palette_view(out_global_frame);
}

auto row_variant_ptr(const PreparedCreatureRenderRow &row) noexcept -> const
    void * {
  switch (row.spec.kind) {
  case CreatureKind::Humanoid:
    return &row.humanoid_variant;
  case CreatureKind::Horse:
    return &row.horse_variant;
  case CreatureKind::Elephant:
    return &row.elephant_variant;
  case CreatureKind::Mounted:
    return nullptr;
  }
  return nullptr;
}

} // namespace

void submit_row_body(const PreparedCreatureRenderRow &row,
                     Render::GL::ISubmitter &out, SubmitStats &stats) noexcept {
  if (row.pass == RenderPassIntent::Shadow) {
    return;
  }
  ++stats.entities_submitted;
  bump_lod_counters(row.lod, stats);
  if (row.lod == CreatureLOD::Billboard) {
    return;
  }

  const auto *asset =
      (row.spec.creature_asset_id != kInvalidCreatureAsset)
          ? CreatureAssetRegistry::instance().get(row.spec.creature_asset_id)
          : CreatureAssetRegistry::instance().resolve(row.spec);
  if (asset == nullptr) {
    return;
  }

  RoleColorArray fallback_roles{};
  std::span<const QVector3D> role_colors{};
  if (asset->fill_role_colors != nullptr) {
    const void *variant = row_variant_ptr(row);
    if (variant != nullptr) {
      const auto count = asset->fill_role_colors(variant, fallback_roles.data(),
                                                 fallback_roles.size());
      role_colors = std::span<const QVector3D>(fallback_roles.data(), count);
    }
  }

  const auto species_id = species_to_bpat_id(row.spec.kind);
  if (species_id == 0xFFFFFFFFu) {
    return;
  }

  const Render::Creature::Bpat::BpatBlob *blob = nullptr;
  std::uint32_t global_frame = 0U;
  auto palette = resolve_blob_palette(row.spec.kind, row.bpat_playback, blob,
                                      global_frame);
  if (blob == nullptr || palette.empty()) {
    return;
  }

  submit_rigged_creature(*asset, row.lod, role_colors, 0,
                         row.humanoid_variant.palette.cloth,
                         row.world_from_unit, *blob, global_frame, out);
}

auto CreaturePipeline::submit_requests(
    std::span<const Render::Creature::CreatureRenderRequest> requests,
    Render::GL::ISubmitter &out) const -> SubmitStats {
  SubmitStats stats{};
  if (requests.empty()) {
    return stats;
  }

  const auto &arch_reg = Render::Creature::ArchetypeRegistry::instance();
  const auto &asset_reg = CreatureAssetRegistry::instance();

  std::unordered_map<std::uint32_t, QMatrix4x4> parent_worlds;

  auto emit_request = [&](const Render::Creature::CreatureRenderRequest &req) {
    ++stats.entities_submitted;
    bump_lod_counters(req.lod, stats);

    const auto *desc = arch_reg.get(req.archetype);
    if (desc == nullptr) {
      return;
    }
    if (req.lod == CreatureLOD::Billboard) {
      return;
    }

    const auto species_kind = desc->species;
    const auto species_id = species_to_bpat_id(species_kind);
    if (species_id == 0xFFFFFFFFu) {
      return;
    }

    const std::uint16_t clip_id = arch_reg.bpat_clip(req.archetype, req.state);
    auto const playback = resolve_clip_playback(species_id, clip_id, req.phase);
    if (playback.blob == nullptr) {
      return;
    }

    const auto *asset = asset_reg.for_species(species_kind);
    if (asset == nullptr) {
      return;
    }

    if (arch_reg.is_snapshot(req.archetype, req.state)) {
      const bool emitted = submit_snapshot_creature(
          *asset, req.lod, req.archetype, req.variant, req.state,
          req.role_colors_view(), static_cast<std::uint16_t>(req.variant),
          req.base_color, req.world, *playback.blob, playback.global_frame,
          playback.frame_in_clip, out, desc->attachments_view());
      if (emitted) {
        return;
      }
    }

    submit_rigged_creature(
        *asset, req.lod, req.role_colors_view(),
        static_cast<std::uint16_t>(req.variant), req.base_color, req.world,
        *playback.blob, playback.global_frame, out, desc->attachments_view());
  };

  for (const auto &req : requests) {
    if (req.parent_entity_id != 0u) {
      continue;
    }
    if (req.entity_id != 0u) {
      parent_worlds.emplace(req.entity_id, req.world);
    }
    emit_request(req);
  }

  for (const auto &req : requests) {
    if (req.parent_entity_id == 0u) {
      continue;
    }
    Render::Creature::CreatureRenderRequest child = req;
    auto it = parent_worlds.find(req.parent_entity_id);
    if (it != parent_worlds.end()) {
      child.world = it->second;
    }
    emit_request(child);
  }

  return stats;
}

} // namespace Render::Creature::Pipeline
