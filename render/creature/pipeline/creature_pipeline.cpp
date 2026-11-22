#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ios>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "render/creature/pipeline/creature_pipeline_internal.h"

namespace Render::Creature::Pipeline {

namespace creature_pipeline_detail {

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
  const auto asset_id = static_cast<std::uint32_t>(
      handle.asset != nullptr ? handle.asset->id : k_invalid_creature_asset);

  auto mix = [](std::uint64_t seed, std::uint64_t value) noexcept -> std::uint64_t {
    seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
    return seed;
  };
  std::uint64_t identity = std::hash<std::string_view>{}(path);
  identity = mix(identity, static_cast<std::uint64_t>(archetype));
  identity = mix(identity, asset_id);
  identity = mix(identity, static_cast<std::uint64_t>(lod));
  identity = mix(identity, static_cast<std::uint64_t>(state));
  identity = mix(identity, clip_id);
  identity = mix(identity, frame_in_clip);
  identity = mix(identity, static_cast<std::uint64_t>(variant));
  identity = mix(identity, clip_variant);
  identity = mix(identity, attachment_set_id);
  identity = mix(identity, attachments_hash);
  if (!note_runtime_bake_violation(RuntimeBakeOperation::CreatureSubmitMiss,
                                   identity)) {
    return;
  }

  const auto* descriptor =
      Render::Creature::ArchetypeRegistry::instance().get(archetype);
  std::ostringstream detail;
  detail << "path=" << path << " archetype=" << static_cast<std::uint32_t>(archetype)
         << " (" << (descriptor != nullptr ? descriptor->debug_name : std::string("?"))
         << ")" << " asset=" << asset_id << " lod=" << static_cast<int>(lod)
         << " state=" << static_cast<int>(state) << " clip=" << clip_id
         << " frame_in_clip=" << frame_in_clip
         << " variant=" << static_cast<std::uint32_t>(variant)
         << " clip_variant=" << static_cast<int>(clip_variant)
         << " attachment_set_id=" << attachment_set_id << " attachments_hash=0x"
         << std::hex << attachments_hash;
  log_runtime_bake_violation(RuntimeBakeOperation::CreatureSubmitMiss, detail.str());
}

auto rigged_cache_for(const CreatureSubmissionContext* submission)
    -> Render::GL::RiggedMeshCache& {
  return submission != nullptr ? submission->rigged_meshes()
                               : detached_rigged_mesh_cache();
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

auto material_id_for_species(CreatureKind species) noexcept -> std::int32_t {
  const SpeciesContract* contract = species_contract(species);
  return contract != nullptr ? contract->material_id : 0;
}

auto make_rigged_cmd(Render::GL::RiggedMesh* mesh,
                     const QMatrix4x4& world_from_unit,
                     const QMatrix4x4* bone_palette,
                     std::uint32_t bone_count,
                     std::shared_ptr<const Render::RoleColorPalette> role_colors,
                     const QVector3D& base_color,
                     const QVector4D& wear_params,
                     std::int32_t material_id,
                     float alpha,
                     float team_emphasis) -> Render::GL::RiggedCreatureCmd {
  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = mesh;
  cmd.world = world_from_unit;
  cmd.bone_count = bone_count;
  cmd.bone_palette = bone_palette;
  cmd.role_color_count = role_colors != nullptr ? role_colors->count : 0U;
  cmd.role_colors = std::move(role_colors);
  cmd.color = base_color;
  cmd.wear_params = wear_params;
  cmd.material_id = material_id;
  cmd.alpha = alpha;
  cmd.team_emphasis = team_emphasis;
  return cmd;
}

auto resolve_playback_handle(const CreatureRenderAssetHandle& primary_handle,
                             CreatureAssetId asset_id,
                             ArchetypeId archetype) noexcept
    -> const CreatureRenderAssetHandle* {
  if (archetype == k_invalid_archetype) {
    return nullptr;
  }
  if (primary_handle.archetype != nullptr &&
      primary_handle.archetype->id == archetype) {
    return &primary_handle;
  }
  auto const alt_handle_id =
      CreatureRenderAssetHandleRegistry::instance().get_or_create(asset_id, archetype);
  return CreatureRenderAssetHandleRegistry::instance().get(alt_handle_id);
}

auto resolve_request_playback(const CreatureRenderAssetHandle& primary_handle,
                              CreatureAssetId asset_id,
                              ArchetypeId archetype,
                              AnimationStateId state,
                              float phase,
                              std::uint8_t clip_variant,
                              std::uint16_t explicit_clip_id) noexcept
    -> ResolvedRequestPlayback {
  ResolvedRequestPlayback resolved{};
  resolved.archetype = archetype;
  resolved.state = state;
  resolved.clip_variant = clip_variant;
  resolved.handle = resolve_playback_handle(primary_handle, asset_id, archetype);
  if (resolved.handle == nullptr) {
    return resolved;
  }

  auto const state_index = static_cast<std::size_t>(state);
  if (state_index >= resolved.handle->playback.size()) {
    return resolved;
  }

  const CreatureClipPlaybackDesc& playback_desc =
      resolved.handle->playback[state_index];
  bool const has_explicit_clip = explicit_clip_id != Animation::k_unmapped_clip;
  std::uint8_t effective_clip_variant = has_explicit_clip ? 0U : clip_variant;
  std::uint16_t effective_clip_id =
      has_explicit_clip
          ? explicit_clip_id
          : ((clip_variant == 0U)
                 ? playback_desc.clip_id
                 : Render::Creature::ArchetypeRegistry::instance().resolve_bpat_clip(
                       archetype, state, clip_variant));

  ResolvedClipPlayback playback =
      (effective_clip_id == playback_desc.clip_id)
          ? resolve_bpat_playback(playback_desc.blob, playback_desc.clip_id, phase)
          : resolve_bpat_playback(
                resolved.handle->asset->bpat_species_id, effective_clip_id, phase);
  if (!has_explicit_clip && effective_clip_id != playback_desc.clip_id &&
      (playback.blob == nullptr ||
       !playback.blob->clip_is_variant_of(
           playback_desc.clip_id, effective_clip_id, effective_clip_variant))) {
    effective_clip_variant = 0U;
    effective_clip_id = playback_desc.clip_id;

    playback =
        playback_desc.blob != nullptr
            ? resolve_bpat_playback(playback_desc.blob, playback_desc.clip_id, phase)
            : resolve_bpat_playback(resolved.handle->asset->bpat_species_id,
                                    playback_desc.clip_id,
                                    phase);
  }
  if (playback.blob == nullptr) {
    return resolved;
  }

  resolved.blob = playback.blob;
  resolved.clip_id = effective_clip_id;
  resolved.clip_variant = effective_clip_variant;
  resolved.global_frame = playback.global_frame;
  resolved.frame_in_clip = playback.frame_in_clip;
  resolved.next_global_frame = playback.next_global_frame;
  resolved.next_frame_in_clip = playback.next_frame_in_clip;
  resolved.frame_lerp = playback.frame_lerp;
  resolved.snapshot = playback_desc.snapshot;
  return resolved;
}

void bump_lod_counters(CreatureLOD lod, SubmitStats& stats) {
  switch (lod) {
  case CreatureLOD::Full:
    ++stats.lod_full;
    break;
  case CreatureLOD::Minimal:
    ++stats.lod_minimal;
    break;
  case CreatureLOD::Culled:
    ++stats.lod_billboard;
    break;
  }
}

void publish_submit_stats(const SubmitStats& stats) noexcept {
  using Render::Profiling::AssetCounter;
  using Render::Profiling::count_asset;
  count_asset(AssetCounter::RiggedCacheHit, stats.rigged_cache_hits);
  count_asset(AssetCounter::RiggedCacheMiss, stats.rigged_cache_misses);
  count_asset(AssetCounter::SnapshotCacheHit, stats.snapshot_cache_hits);
  count_asset(AssetCounter::SnapshotCacheMiss, stats.snapshot_misses);
  count_asset(AssetCounter::SnapshotLoad, stats.snapshot_loads);
  count_asset(AssetCounter::SnapshotEviction, stats.snapshot_evictions);
  count_asset(AssetCounter::SkinAtlasBuild, stats.skin_atlas_builds);
  count_asset(AssetCounter::SkinUboUpload, stats.skin_ubo_uploads);
  count_asset(AssetCounter::SkinUboBytes, stats.skin_ubo_bytes_uploaded);
}

} // namespace creature_pipeline_detail

using namespace creature_pipeline_detail;

auto CreaturePipeline::submit_requests(
    std::span<const Render::Creature::CreatureRenderRequest> requests,
    Render::GL::ISubmitter& out) const -> SubmitStats {
  SubmitStats stats{};
  if (requests.empty()) {
    return stats;
  }

  const CreatureSubmissionContext* submission = resolve_creature_submission(out);
  if (submission != nullptr) {
    submission->rigged_meshes().reset_frame_stats();
    submission->snapshot_meshes().reset_frame_stats();
    submission->rigged_meshes().reserve_for_frame(requests.size());
    submission->snapshot_meshes().reserve_for_frame(requests.size());
  }

  auto emit_request = [&](const Render::Creature::CreatureRenderRequest& req) {
    auto& profile = Render::Profiling::global_profile();
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
    if (req.lod == CreatureLOD::Culled) {
      return;
    }

    auto const primary = resolve_request_playback(*handle,
                                                  req.creature_asset_id,
                                                  req.archetype,
                                                  req.state,
                                                  req.phase,
                                                  req.clip_variant,
                                                  req.clip_id);
    if (!primary.valid()) {
      return;
    }
    ResolvedRequestPlayback full_body{};
    ResolvedRequestPlayback overlay{};
    ResolvedTransition transition{};
    {
      Render::Profiling::AccumulatorScope const playback_scope(
          &profile.bpat_playback_us);
      if (req.full_body_blend.active()) {
        full_body = resolve_request_playback(*handle,
                                             req.creature_asset_id,
                                             req.full_body_blend.archetype,
                                             req.full_body_blend.state,
                                             req.full_body_blend.phase,
                                             req.full_body_blend.clip_variant,
                                             req.full_body_blend.clip_id);
      }
      if (req.upper_body_overlay.active()) {
        overlay = resolve_request_playback(*handle,
                                           req.creature_asset_id,
                                           req.upper_body_overlay.archetype,
                                           req.upper_body_overlay.state,
                                           req.upper_body_overlay.phase,
                                           req.upper_body_overlay.clip_variant,
                                           req.upper_body_overlay.clip_id);
      }
      if (req.transition.active()) {
        for (std::size_t index = 0; index < req.transition.sources.size(); ++index) {
          auto const& source = req.transition.sources[index];
          if (source.clip_id == Animation::k_unmapped_clip) {
            continue;
          }
          transition.sources[index] = resolve_request_playback(*handle,
                                                               req.creature_asset_id,
                                                               req.transition.archetype,
                                                               source.state,
                                                               source.phase,
                                                               source.clip_variant,
                                                               source.clip_id);
          transition.upper_shares[index] = source.upper_body_share;
          transition.lower_shares[index] = source.lower_body_share;
        }
        transition.upper_body_weight = req.transition.upper_body_weight;
        transition.lower_body_weight = req.transition.lower_body_weight;
      }
    }

    QMatrix4x4 draw_world = req.world;
    if (!req.world_already_grounded) {
      float contact_y = contact_y_for_playback(primary);
      if (req.full_body_blend.active() && full_body.valid()) {
        float const secondary_contact = contact_y_for_playback(full_body);
        float const blend_weight = std::clamp(req.full_body_blend.weight, 0.0F, 1.0F);
        contact_y =
            contact_y * (1.0F - blend_weight) + secondary_contact * blend_weight;
      }
      if (transition.active()) {
        float outgoing_contact = 0.0F;
        float outgoing_share = 0.0F;
        for (std::size_t index = 0; index < transition.sources.size(); ++index) {
          if (transition.sources[index].valid()) {
            outgoing_contact += contact_y_for_playback(transition.sources[index]) *
                                transition.lower_shares[index];
            outgoing_share += transition.lower_shares[index];
          }
        }
        if (outgoing_share > 1.0e-4F) {
          float const outgoing_weight =
              std::clamp(transition.lower_body_weight, 0.0F, 1.0F);
          contact_y = contact_y * (1.0F - outgoing_weight) +
                      (outgoing_contact / outgoing_share) * outgoing_weight;
        }
      }
      if (std::abs(contact_y) > 1.0e-6F) {
        QMatrix4x4 adjusted = req.world;
        QVector3D origin = adjusted.column(3).toVector3D();
        origin.setY(origin.y() - contact_y);
        adjusted.setColumn(3, QVector4D(origin, 1.0F));
        draw_world = adjusted;
      }
    }
    resolve_bone_probe(req, draw_world, primary);

    const bool prebaked_lowpoly_required =
        req.lod == CreatureLOD::Minimal && handle->requires_prebaked_minimal_snapshot;
    if (req.full_body_blend.active() && full_body.valid()) {
      ++stats.full_body_blend_requests;
    }
    if (req.upper_body_overlay.active() && overlay.valid()) {
      ++stats.upper_body_overlay_requests;
    }
    if (transition.active()) {
      ++stats.transition_blend_requests;
    }

    const bool use_snapshot_mesh = snapshot_mesh_serves_request(
        req.lod,
        primary.snapshot,
        prebaked_lowpoly_required,
        Render::GraphicsSettings::instance().creature_lod().snapshot_meshes);
    if (use_snapshot_mesh) {
      auto snapshot_playback = primary;
      float const outgoing_weight =
          transition.active() ? std::clamp(transition.upper_body_weight, 0.0F, 1.0F)
                              : 0.0F;
      float const blend_weight =
          req.full_body_blend.active() && full_body.valid()
              ? std::clamp(req.full_body_blend.weight, 0.0F, 1.0F)
              : 0.0F;
      float best_share = (1.0F - outgoing_weight) * (1.0F - blend_weight);
      float const blend_share = (1.0F - outgoing_weight) * blend_weight;
      if (blend_share >= best_share && blend_weight > 0.0F) {
        snapshot_playback = full_body;
        best_share = blend_share;
      }
      for (std::size_t index = 0; index < transition.sources.size(); ++index) {
        float const share = outgoing_weight * transition.upper_shares[index];
        if (transition.sources[index].valid() && share > best_share) {
          snapshot_playback = transition.sources[index];
          best_share = share;
        }
      }
      if (snapshot_playback.clip_id != primary.clip_id ||
          snapshot_playback.global_frame != primary.global_frame) {
        ++stats.dominant_snapshot_collapses;
      }
      const bool emitted =
          submit_snapshot_creature(*handle,
                                   req.lod,
                                   snapshot_playback.archetype,
                                   req.variant,
                                   snapshot_playback.state,
                                   snapshot_playback.clip_id,
                                   snapshot_playback.clip_variant,
                                   req.role_colors,
                                   static_cast<std::uint16_t>(req.variant),
                                   req.base_color,
                                   req.wear_params,
                                   draw_world,
                                   *snapshot_playback.blob,
                                   snapshot_playback.global_frame,
                                   snapshot_playback.frame_in_clip,
                                   req.entity_id,
                                   req.instance_index,
                                   req.alpha,
                                   req.team_emphasis,
                                   out,
                                   submission,
                                   !prebaked_lowpoly_required);
      if (emitted) {
        return;
      }
    }
    if (prebaked_lowpoly_required) {
      report_submit_cache_miss("snapshot_prebaked_required",
                               *handle,
                               req.lod,
                               primary.archetype,
                               req.variant,
                               primary.state,
                               primary.clip_id,
                               primary.clip_variant,
                               primary.frame_in_clip,
                               handle->attachment_set_id,
                               handle->attachments_hash);
      return;
    }

    if (primary.blob != nullptr) {
      auto& asset_cache = rigged_cache_for(submission);
      const auto asset_key =
          rigged_asset_key(*handle, req.lod, primary.blob->species_id());
      if (asset_cache.find_rigged_asset(asset_key) == nullptr) {
        create_creature_render_asset(asset_cache,
                                     *handle,
                                     req.lod,
                                     *primary.blob,
                                     static_cast<std::uint16_t>(req.variant),
                                     submission != nullptr);
      }
    }

    submit_rigged_creature(*handle,
                           req.lod,
                           primary.archetype,
                           req.variant,
                           primary.state,
                           primary.clip_id,
                           primary.clip_variant,
                           primary.frame_in_clip,
                           req.role_colors,
                           static_cast<std::uint16_t>(req.variant),
                           req.base_color,
                           req.wear_params,
                           draw_world,
                           *primary.blob,
                           primary.global_frame,
                           primary,
                           full_body.valid() ? &full_body : nullptr,
                           overlay.valid() ? &overlay : nullptr,
                           transition.active() ? &transition : nullptr,
                           req.full_body_blend.weight,
                           req.upper_body_overlay.weight,
                           req.entity_id,
                           req.instance_index,
                           req.alpha,
                           req.team_emphasis,
                           out,
                           submission);
  };

  for (const auto& req : requests) {
    emit_request(req);
  }

  if (submission != nullptr) {
    const auto& rs = submission->rigged_meshes().frame_stats();
    stats.rigged_cache_hits = rs.hits;
    stats.rigged_cache_misses = rs.misses;
    stats.rigged_cache_bakes = rs.bakes;
    stats.skin_atlas_builds = rs.skin_atlas_builds;
    stats.skin_ubo_uploads = rs.skin_ubo_uploads;
    stats.skin_ubo_bytes_uploaded = rs.skin_ubo_bytes_uploaded;
    const auto& ss = submission->snapshot_meshes().frame_stats();
    stats.snapshot_cache_hits = ss.hits;
    stats.snapshot_loads = ss.loads;
    stats.snapshot_bakes = ss.bakes;
    stats.snapshot_misses = ss.misses;
    stats.snapshot_evictions = ss.evictions;
    publish_submit_stats(stats);
  }

  return stats;
}

} // namespace Render::Creature::Pipeline
