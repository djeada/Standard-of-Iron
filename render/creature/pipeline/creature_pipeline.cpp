#include "creature_pipeline.h"

#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "../../../game/map/terrain_service.h"
#include "../../bone_palette_arena.h"
#include "../../entity/registry.h"
#include "../../humanoid/skeleton.h"
#include "../../rigged_mesh_cache.h"
#include "../../scene_renderer.h"
#include "../../snapshot_mesh_cache.h"
#include "../../submitter.h"
#include "../archetype_registry.h"
#include "../bpat/bpat_format.h"
#include "../bpat/bpat_reader.h"
#include "../bpat/bpat_registry.h"
#include "../runtime_bake_guard.h"
#include "../skeleton.h"
#include "../snapshot_mesh_registry.h"
#include "../spec.h"
#include "creature_asset.h"
#include "preparation_common.h"

namespace Render::Creature::Pipeline {

namespace {

auto resolve_renderer(Render::GL::ISubmitter& out) noexcept -> Render::GL::Renderer* {
  if (auto* renderer = dynamic_cast<Render::GL::Renderer*>(&out)) {
    return renderer;
  }
  if (auto* batch = dynamic_cast<Render::GL::BatchingSubmitter*>(&out)) {
    return dynamic_cast<Render::GL::Renderer*>(batch->fallback_submitter());
  }
  return nullptr;
}

auto species_to_bpat_id(CreatureKind kind) noexcept -> std::uint32_t {
  switch (kind) {
  case CreatureKind::Humanoid:
    return Render::Creature::Bpat::k_species_humanoid;
  case CreatureKind::Horse:
    return Render::Creature::Bpat::k_species_horse;
  case CreatureKind::Elephant:
    return Render::Creature::Bpat::k_species_elephant;
  case CreatureKind::Mounted:
    return 0xFFFFFFFFU;
  }
  return 0xFFFFFFFFU;
}

auto adjust_world_to_palette_contact(const QMatrix4x4& world_from_unit,
                                     CreatureKind kind,
                                     std::span<const QMatrix4x4> palette) noexcept
    -> QMatrix4x4 {
  float const contact_y = palette_contact_y(kind, palette);
  if (std::abs(contact_y) <= 1.0e-6F) {
    return world_from_unit;
  }
  QMatrix4x4 adjusted = world_from_unit;
  QVector3D origin = adjusted.column(3).toVector3D();
  origin.setY(origin.y() - contact_y);
  adjusted.setColumn(3, QVector4D(origin, 1.0F));
  return adjusted;
}

auto creature_lod_bit(CreatureLOD lod) noexcept -> std::uint8_t {
  return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(lod));
}

void report_submit_cache_miss(std::string_view path,
                              const CreatureRenderAssetHandle& handle,
                              CreatureLOD lod,
                              ArchetypeId archetype,
                              VariantId variant,
                              AnimationStateId state,
                              std::uint16_t clip_id,
                              std::uint8_t clip_variant,
                              std::uint32_t frame_in_clip,
                              std::uint32_t attachment_set_id,
                              std::uint64_t attachments_hash) {
  if (!runtime_bake_forbidden()) {
    return;
  }
  std::ostringstream detail;
  detail << "path=" << path << " archetype=" << static_cast<std::uint32_t>(archetype)
         << " asset="
         << static_cast<std::uint32_t>(
                handle.asset != nullptr ? handle.asset->id : k_invalid_creature_asset)
         << " lod=" << static_cast<int>(lod) << " state=" << static_cast<int>(state)
         << " clip=" << clip_id << " frame_in_clip=" << frame_in_clip
         << " variant=" << static_cast<std::uint32_t>(variant)
         << " clip_variant=" << static_cast<int>(clip_variant)
         << " attachment_set_id=" << attachment_set_id << " attachments_hash=0x"
         << std::hex << attachments_hash;
  report_runtime_bake_violation(RuntimeBakeOperation::CreatureSubmitMiss, detail.str());
}

void ensure_skin_atlas_for_submit(Render::GL::RiggedMeshCache& cache,
                                  const Render::GL::RiggedMeshEntry& entry,
                                  const Render::Creature::Bpat::BpatBlob& blob) {
  const bool had_atlas = entry.skinned_frame_total == blob.frame_total() &&
                         entry.skinned_bone_count != 0U &&
                         !entry.skinned_palettes.empty();
  Render::GL::rigged_entry_ensure_skin_atlas_from_blob(entry, blob);
  if (!had_atlas && entry.skinned_frame_total == blob.frame_total() &&
      entry.skinned_bone_count != 0U && !entry.skinned_palettes.empty()) {
    cache.record_skin_atlas_build();
  }
}

void ensure_skin_ubo_for_submit(Render::GL::RiggedMeshCache& cache,
                                const Render::GL::RiggedMeshEntry& entry) {
  const bool had_ubo = entry.skin_palette_ubo != 0U;
  Render::GL::rigged_entry_ensure_skin_ubo(entry);
  if (!had_ubo && entry.skin_palette_ubo != 0U) {
    const auto bytes = static_cast<std::uint64_t>(entry.skinned_frame_total) *
                       Render::GL::BonePaletteArena::k_palette_bytes;
    cache.record_skin_ubo_upload(bytes);
  }
}

auto make_snapshot_key(const CreatureRenderAssetHandle& handle,
                       ArchetypeId archetype,
                       VariantId variant,
                       AnimationStateId state,
                       std::uint16_t clip_id,
                       std::uint8_t clip_variant,
                       std::uint32_t frame_in_clip) noexcept
    -> Render::GL::SnapshotMeshCache::Key {
  Render::GL::SnapshotMeshCache::Key key{};
  key.asset_id = handle.asset != nullptr ? handle.asset->id : k_invalid_creature_asset;
  key.archetype = archetype;
  key.attachment_set_id = handle.attachment_set_id;
  key.variant = variant;
  key.state = state;
  key.clip_id = clip_id;
  key.clip_variant = clip_variant;
  key.frame_in_clip = frame_in_clip;
  return key;
}

void copy_role_colors(Render::GL::RiggedCreatureCmd& cmd,
                      std::span<const QVector3D> role_colors) noexcept {
  const auto count = std::min<std::size_t>(role_colors.size(), cmd.role_colors.size());
  cmd.role_color_count = static_cast<std::uint32_t>(count);
  std::copy_n(role_colors.data(), count, cmd.role_colors.data());
}

auto make_rigged_cmd(Render::GL::RiggedMesh* mesh,
                     const QMatrix4x4& world_from_unit,
                     const QMatrix4x4* bone_palette,
                     std::uint32_t bone_count,
                     std::span<const QVector3D> role_colors,
                     const QVector3D& base_color,
                     const QVector4D& wear_params) -> Render::GL::RiggedCreatureCmd {
  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = mesh;
  cmd.world = world_from_unit;
  cmd.bone_count = bone_count;
  cmd.bone_palette = bone_palette;
  copy_role_colors(cmd, role_colors);
  cmd.color = base_color;
  cmd.wear_params = wear_params;
  return cmd;
}

void submit_rigged_creature(const CreatureRenderAssetHandle& handle,
                            CreatureLOD lod,
                            ArchetypeId archetype,
                            VariantId variant,
                            AnimationStateId state,
                            std::uint16_t clip_id,
                            std::uint8_t clip_variant,
                            std::uint32_t frame_in_clip,
                            std::span<const QVector3D> role_colors,
                            std::uint16_t variant_bucket,
                            const QVector3D& base_color,
                            const QVector4D& wear_params,
                            const QMatrix4x4& world_from_unit,
                            const Render::Creature::Bpat::BpatBlob& blob,
                            std::uint32_t global_frame,
                            Render::GL::ISubmitter& out,
                            Render::GL::Renderer* renderer) {
  const CreatureAsset* asset = handle.asset;
  if (lod == CreatureLOD::Billboard || asset == nullptr || asset->spec == nullptr ||
      handle.bind_palette.empty()) {
    return;
  }
  auto& cache = (renderer != nullptr) ? renderer->rigged_mesh_cache()
                                      : ([]() -> Render::GL::RiggedMeshCache& {
                                          thread_local Render::GL::RiggedMeshCache c;
                                          return c;
                                        })();
  const auto* entry = cache.get_or_bake_prehashed(*asset->spec,
                                                  lod,
                                                  handle.bind_palette,
                                                  variant_bucket,
                                                  handle.attachments,
                                                  handle.attachments_hash,
                                                  handle.attachment_set_id,
                                                  blob.species_id());
  if (entry == nullptr || entry->mesh == nullptr || entry->mesh->index_count() == 0U) {
    report_submit_cache_miss("rigged",
                             handle,
                             lod,
                             archetype,
                             variant,
                             state,
                             clip_id,
                             clip_variant,
                             frame_in_clip,
                             handle.attachment_set_id,
                             handle.attachments_hash);
    return;
  }

  ensure_skin_atlas_for_submit(cache, *entry, blob);
  if (renderer != nullptr) {
    ensure_skin_ubo_for_submit(cache, *entry);
  }

  if (entry->skinned_palettes.empty() || entry->skinned_bone_count == 0 ||
      global_frame >= entry->skinned_frame_total) {
    return;
  }

  const QMatrix4x4* frame_palette =
      entry->skinned_palettes.data() +
      static_cast<std::size_t>(global_frame) * entry->skinned_bone_count;
  QMatrix4x4 const& draw_world = world_from_unit;

  auto cmd = make_rigged_cmd(entry->mesh.get(),
                             draw_world,
                             frame_palette,
                             entry->skinned_bone_count,
                             role_colors,
                             base_color,
                             wear_params);
  cmd.palette_ubo = entry->skin_palette_ubo;
  cmd.palette_offset = static_cast<std::uint32_t>(
      static_cast<std::size_t>(global_frame) * entry->skin_palette_frame_stride_bytes);
  out.rigged(cmd);
}

auto submit_snapshot_creature(const CreatureRenderAssetHandle& handle,
                              CreatureLOD lod,
                              Render::Creature::ArchetypeId archetype,
                              Render::Creature::VariantId variant,
                              Render::Creature::AnimationStateId state,
                              std::uint16_t clip_id,
                              std::uint8_t clip_variant,
                              std::span<const QVector3D> role_colors,
                              std::uint16_t variant_bucket,
                              const QVector3D& base_color,
                              const QVector4D& wear_params,
                              const QMatrix4x4& world_from_unit,
                              const Render::Creature::Bpat::BpatBlob& blob,
                              std::uint32_t global_frame,
                              std::uint32_t frame_in_clip,
                              Render::GL::ISubmitter& out,
                              Render::GL::Renderer* renderer,
                              bool allow_bake_fallback = true) -> bool {
  const CreatureAsset* asset = handle.asset;
  if (lod == CreatureLOD::Billboard || asset == nullptr || asset->spec == nullptr ||
      handle.bind_palette.empty()) {
    return false;
  }

  if (renderer == nullptr) {
    return false;
  }

  const auto key = make_snapshot_key(
      handle, archetype, variant, state, clip_id, clip_variant, frame_in_clip);

  if (!handle.has_static_attachments &&
      asset->snapshot_mesh_species_id != 0xFFFFFFFFU &&
      (asset->snapshot_mesh_lod_mask & creature_lod_bit(lod)) != 0U) {
    const auto* mesh_blob =
        Render::Creature::Snapshot::SnapshotMeshRegistry::instance().blob(
            asset->snapshot_mesh_species_id, lod);
    if (mesh_blob != nullptr) {
      std::uint32_t mesh_global_frame = 0U;
      if (mesh_blob->resolve_global_frame(clip_id, frame_in_clip, mesh_global_frame)) {
        const auto* snap = renderer->snapshot_mesh_cache().get_or_load(
            key, *mesh_blob, mesh_global_frame);
        if (snap != nullptr && snap->mesh != nullptr &&
            snap->mesh->index_count() != 0U) {
          auto cmd = make_rigged_cmd(snap->mesh.get(),
                                     world_from_unit,
                                     Render::GL::SnapshotMeshCache::identity_palette(),
                                     1U,
                                     role_colors,
                                     base_color,
                                     wear_params);
          cmd.palette_ubo = 0U;
          cmd.palette_offset = 0U;

          out.rigged(cmd);
          return true;
        }
        report_submit_cache_miss("snapshot_load",
                                 handle,
                                 lod,
                                 archetype,
                                 variant,
                                 state,
                                 clip_id,
                                 clip_variant,
                                 frame_in_clip,
                                 handle.attachment_set_id,
                                 handle.attachments_hash);
      }
    }
  }

  if (!allow_bake_fallback) {
    return false;
  }

  auto& rigged_cache = renderer->rigged_mesh_cache();
  const auto* source = rigged_cache.get_or_bake_prehashed(*asset->spec,
                                                          lod,
                                                          handle.bind_palette,
                                                          variant_bucket,
                                                          handle.attachments,
                                                          handle.attachments_hash,
                                                          handle.attachment_set_id,
                                                          blob.species_id());
  if (source == nullptr || source->mesh == nullptr ||
      source->mesh->index_count() == 0U) {
    report_submit_cache_miss("snapshot_source_rigged",
                             handle,
                             lod,
                             archetype,
                             variant,
                             state,
                             clip_id,
                             clip_variant,
                             frame_in_clip,
                             handle.attachment_set_id,
                             handle.attachments_hash);
    return false;
  }

  ensure_skin_atlas_for_submit(rigged_cache, *source, blob);
  if (source->skinned_palettes.empty() || source->skinned_bone_count == 0 ||
      global_frame >= source->skinned_frame_total) {
    return false;
  }

  const auto* snap =
      renderer->snapshot_mesh_cache().get_or_bake(key, *source, global_frame);
  if (snap == nullptr || snap->mesh == nullptr || snap->mesh->index_count() == 0U) {
    report_submit_cache_miss("snapshot_bake",
                             handle,
                             lod,
                             archetype,
                             variant,
                             state,
                             clip_id,
                             clip_variant,
                             frame_in_clip,
                             handle.attachment_set_id,
                             handle.attachments_hash);
    return false;
  }

  auto cmd = make_rigged_cmd(snap->mesh.get(),
                             world_from_unit,
                             Render::GL::SnapshotMeshCache::identity_palette(),
                             1U,
                             role_colors,
                             base_color,
                             wear_params);
  cmd.palette_ubo = 0U;
  cmd.palette_offset = 0U;

  out.rigged(cmd);
  return true;
}

void bump_lod_counters(CreatureLOD lod, SubmitStats& stats) {
  switch (lod) {
  case CreatureLOD::Full:
    ++stats.lod_full;
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
  const Render::Creature::Bpat::BpatBlob* blob{nullptr};
  std::uint32_t global_frame{0U};
  std::uint32_t frame_in_clip{0U};
};

auto resolve_clip_playback(std::uint32_t species_id,
                           std::uint16_t clip_id,
                           float phase) noexcept -> ResolvedPlayback {
  ResolvedPlayback r{};
  if (clip_id == Render::Creature::ArchetypeDescriptor::k_unmapped_clip) {
    return r;
  }
  const auto* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
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

auto resolve_clip_playback(const CreatureClipPlaybackDesc& desc,
                           float phase) noexcept -> ResolvedPlayback {
  ResolvedPlayback r{};
  if (desc.blob == nullptr ||
      desc.clip_id == Render::Creature::ArchetypeDescriptor::k_unmapped_clip ||
      desc.frame_count == 0U) {
    return r;
  }
  float p = phase - std::floor(phase);
  if (p < 0.0F) {
    p += 1.0F;
  }
  auto const fc = static_cast<float>(desc.frame_count);
  auto frame_idx = static_cast<int>(p * fc);
  if (frame_idx < 0) {
    frame_idx = 0;
  }
  if (frame_idx >= static_cast<int>(desc.frame_count)) {
    frame_idx = static_cast<int>(desc.frame_count) - 1;
  }
  r.blob = desc.blob;
  r.frame_in_clip = static_cast<std::uint32_t>(frame_idx);
  r.global_frame = desc.frame_offset + r.frame_in_clip;
  return r;
}

auto resolve_blob_palette(std::uint32_t species_id,
                          BpatPlayback playback,
                          const Render::Creature::Bpat::BpatBlob*& out_blob,
                          std::uint32_t& out_global_frame) noexcept
    -> std::span<const QMatrix4x4> {
  out_blob = nullptr;
  out_global_frame = 0U;
  if (playback.clip_id == k_invalid_bpat_clip) {
    return {};
  }
  if (species_id == 0xFFFFFFFFU) {
    return {};
  }
  const auto* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
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

auto expected_humanoid_idle_variant_name(std::uint8_t clip_variant) noexcept
    -> std::string_view {
  switch (clip_variant) {
  case 1U:
    return "idle_squat";
  case 2U:
    return "idle_jump";
  case 3U:
    return "idle_weapon";
  case 4U:
    return "idle_weave";
  default:
    return "idle";
  }
}

auto humanoid_idle_variant_clip_is_usable(const Render::Creature::Bpat::BpatBlob* blob,
                                          std::uint16_t clip_id,
                                          Render::Creature::AnimationStateId state,
                                          std::uint8_t clip_variant) noexcept -> bool {
  if (state != Render::Creature::AnimationStateId::Idle || clip_variant == 0U) {
    return true;
  }
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return false;
  }
  return blob->clip(clip_id).name == expected_humanoid_idle_variant_name(clip_variant);
}

} // namespace

auto CreaturePipeline::submit_requests(
    std::span<const Render::Creature::CreatureRenderRequest> requests,
    Render::GL::ISubmitter& out) const -> SubmitStats {
  SubmitStats stats{};
  if (requests.empty()) {
    return stats;
  }

  auto* renderer = resolve_renderer(out);
  if (renderer != nullptr) {
    renderer->rigged_mesh_cache().reset_frame_stats();
    renderer->snapshot_mesh_cache().reset_frame_stats();
    renderer->rigged_mesh_cache().reserve_for_frame(requests.size());
    renderer->snapshot_mesh_cache().reserve_for_frame(requests.size());
  }

  auto emit_request = [&](const Render::Creature::CreatureRenderRequest& req) {
    ++stats.entities_submitted;
    bump_lod_counters(req.lod, stats);

    auto handle_id = req.render_asset_handle;
    if (handle_id == Render::Creature::k_invalid_creature_render_asset_handle) {
      handle_id = CreatureRenderAssetHandleRegistry::instance().get_or_create(
          req.creature_asset_id, req.archetype);
    }
    const CreatureRenderAssetHandle* handle =
        CreatureRenderAssetHandleRegistry::instance().get(handle_id);
    if (handle == nullptr || !handle->valid()) {
      return;
    }
    if (req.lod == CreatureLOD::Billboard) {
      return;
    }

    const auto species_kind = handle->archetype->species;
    const auto state_index = static_cast<std::size_t>(req.state);
    if (state_index >= handle->playback.size()) {
      return;
    }

    const CreatureClipPlaybackDesc& playback_desc = handle->playback[state_index];

    std::uint8_t effective_clip_variant = req.clip_variant;
    std::uint16_t effective_clip_id =
        (req.clip_variant == 0U)
            ? playback_desc.clip_id
            : Render::Creature::ArchetypeRegistry::instance().resolve_bpat_clip(
                  req.archetype, req.state, req.clip_variant);

    auto playback = (effective_clip_id == playback_desc.clip_id)
                        ? resolve_clip_playback(playback_desc, req.phase)
                        : resolve_clip_playback(handle->asset->bpat_species_id,
                                                effective_clip_id,
                                                req.phase);
    if (!humanoid_idle_variant_clip_is_usable(
            playback.blob, effective_clip_id, req.state, effective_clip_variant)) {
      effective_clip_variant = 0U;
      effective_clip_id = playback_desc.clip_id;
      playback = resolve_clip_playback(playback_desc, req.phase);
    }
    if (playback.blob == nullptr) {
      return;
    }

    QMatrix4x4 draw_world = req.world;
    if (!req.world_already_grounded) {
      draw_world = adjust_world_to_palette_contact(
          req.world,
          species_kind,
          playback.blob->frame_palette_view(playback.global_frame));
    }
    const bool prebaked_lowpoly_required =
        req.lod == CreatureLOD::Minimal && handle->requires_prebaked_minimal_snapshot;

    if (playback_desc.snapshot) {
      const bool emitted =
          submit_snapshot_creature(*handle,
                                   req.lod,
                                   req.archetype,
                                   req.variant,
                                   req.state,
                                   effective_clip_id,
                                   effective_clip_variant,
                                   req.role_colors_view(),
                                   static_cast<std::uint16_t>(req.variant),
                                   req.base_color,
                                   req.wear_params,
                                   draw_world,
                                   *playback.blob,
                                   playback.global_frame,
                                   playback.frame_in_clip,
                                   out,
                                   renderer,
                                   !prebaked_lowpoly_required);
      if (emitted) {
        return;
      }
    }
    if (prebaked_lowpoly_required) {
      report_submit_cache_miss("snapshot_prebaked_required",
                               *handle,
                               req.lod,
                               req.archetype,
                               req.variant,
                               req.state,
                               effective_clip_id,
                               effective_clip_variant,
                               playback.frame_in_clip,
                               handle->attachment_set_id,
                               handle->attachments_hash);
      return;
    }

    submit_rigged_creature(*handle,
                           req.lod,
                           req.archetype,
                           req.variant,
                           req.state,
                           effective_clip_id,
                           effective_clip_variant,
                           playback.frame_in_clip,
                           req.role_colors_view(),
                           static_cast<std::uint16_t>(req.variant),
                           req.base_color,
                           req.wear_params,
                           draw_world,
                           *playback.blob,
                           playback.global_frame,
                           out,
                           renderer);
  };

  for (const auto& req : requests) {
    emit_request(req);
  }

  if (renderer != nullptr) {
    const auto& rs = renderer->rigged_mesh_cache().frame_stats();
    stats.rigged_cache_hits = rs.hits;
    stats.rigged_cache_misses = rs.misses;
    stats.rigged_cache_bakes = rs.bakes;
    stats.skin_atlas_builds = rs.skin_atlas_builds;
    stats.skin_ubo_uploads = rs.skin_ubo_uploads;
    stats.skin_ubo_bytes_uploaded = rs.skin_ubo_bytes_uploaded;
    const auto& ss = renderer->snapshot_mesh_cache().frame_stats();
    stats.snapshot_cache_hits = ss.hits;
    stats.snapshot_loads = ss.loads;
    stats.snapshot_bakes = ss.bakes;
    stats.snapshot_misses = ss.misses;
  }

  return stats;
}

} // namespace Render::Creature::Pipeline
