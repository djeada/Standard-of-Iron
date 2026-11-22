#include "render/creature/pipeline/creature_pipeline_internal.h"

namespace Render::Creature::Pipeline {

namespace creature_pipeline_detail {

void submit_rigged_creature(const CreatureRenderAssetHandle& handle,
                            CreatureLOD lod,
                            ArchetypeId archetype,
                            VariantId variant,
                            AnimationStateId state,
                            std::uint16_t clip_id,
                            std::uint8_t clip_variant,
                            std::uint32_t frame_in_clip,
                            std::shared_ptr<const Render::RoleColorPalette> role_colors,
                            std::uint16_t variant_bucket,
                            const QVector3D& base_color,
                            const QVector4D& wear_params,
                            const QMatrix4x4& world_from_unit,
                            const Render::Creature::Bpat::BpatBlob& blob,
                            std::uint32_t global_frame,
                            const ResolvedRequestPlayback& primary_playback,
                            const ResolvedRequestPlayback* full_body_blend,
                            const ResolvedRequestPlayback* upper_body_overlay,
                            const ResolvedTransition* transition,
                            float full_body_blend_weight,
                            float upper_body_overlay_weight,
                            std::uint32_t entity_id,
                            std::uint16_t instance_index,
                            float alpha,
                            float team_emphasis,
                            Render::GL::ISubmitter& out,
                            const CreatureSubmissionContext* submission) {
  const CreatureAsset* asset = handle.asset;
  if (lod == CreatureLOD::Culled || asset == nullptr || asset->spec == nullptr ||
      handle.bind_palette.empty()) {
    return;
  }
  auto& cache = rigged_cache_for(submission);

  const auto asset_key = rigged_asset_key(handle, lod, blob.species_id());
  const auto* entry = cache.require_rigged_asset(asset_key);
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

  const bool wants_layered_pose =
      (full_body_blend != nullptr && full_body_blend->valid() &&
       full_body_blend_weight > 0.0F) ||
      (upper_body_overlay != nullptr && upper_body_overlay->valid() &&
       upper_body_overlay_weight > 0.0F) ||
      (transition != nullptr && transition->active());
  const auto* skin_atlas = entry->skin_atlas.get();
  if (skin_atlas == nullptr) {
    return;
  }
  const bool frames_resident =
      skin_atlas->palette_block != nullptr && skin_atlas->frame_total != 0U &&
      global_frame < skin_atlas->frame_total &&
      primary_playback.next_global_frame < skin_atlas->frame_total;
  const bool use_resident_frames = frames_resident && !wants_layered_pose;

  const QMatrix4x4* frame_palette =
      frame_palette_for_global_frame(*entry, primary_playback.global_frame);
  if (frame_palette == nullptr) {
    return;
  }
  QMatrix4x4 const& draw_world = world_from_unit;

  auto cmd = make_rigged_cmd(entry->mesh.get(),
                             draw_world,
                             frame_palette,
                             skin_atlas->bone_count,
                             std::move(role_colors),
                             base_color,
                             wear_params,
                             material_id_for_species(handle.archetype->species),
                             alpha,
                             team_emphasis);

  const bool skin_ubo_covers_frame = skin_atlas->palette_block != nullptr &&
                                     skin_atlas->frame_total != 0U &&
                                     global_frame < skin_atlas->frame_total;
  if (skin_ubo_covers_frame) {
    cmd.palette_block = skin_atlas->palette_block.get();
    cmd.palette_offset = static_cast<std::uint32_t>(
        static_cast<std::size_t>(global_frame) * skin_atlas->frame_stride_bytes);
    if (use_resident_frames) {
      cmd.palette_next_offset = static_cast<std::uint32_t>(
          static_cast<std::size_t>(primary_playback.next_global_frame) *
          skin_atlas->frame_stride_bytes);
      cmd.palette_lerp = std::clamp(primary_playback.frame_lerp, 0.0F, 1.0F);
      cmd.palette_frames_resident = true;
      cmd.bone_palette_next =
          frame_palette_for_global_frame(*entry, primary_playback.next_global_frame);
    }
  }

  const bool full_body_active = full_body_blend != nullptr &&
                                full_body_blend->valid() &&
                                full_body_blend_weight > 0.0F;
  const bool overlay_active = upper_body_overlay != nullptr &&
                              upper_body_overlay->valid() &&
                              upper_body_overlay_weight > 0.0F;
  const bool transition_active = transition != nullptr && transition->active();
  const std::uint32_t primary_lerp_bucket =
      blend_weight_bucket(primary_playback.frame_lerp);
  const bool needs_owned_palette =
      !use_resident_frames && (primary_lerp_bucket != 0U || full_body_active ||
                               overlay_active || transition_active);
  if (needs_owned_palette) {
    PaletteBlendKey key{};
    key.entry = entry;
    key.frames[0] = primary_playback.global_frame;
    key.frames[1] = primary_playback.next_global_frame;
    key.buckets[0] = primary_lerp_bucket;
    if (full_body_active) {
      key.frames[2] = full_body_blend->global_frame;
      key.frames[3] = full_body_blend->next_global_frame;
      key.buckets[1] = blend_weight_bucket(full_body_blend->frame_lerp);
      key.buckets[2] =
          blend_weight_bucket(full_body_blend_weight, k_layer_weight_buckets);
    }
    if (overlay_active) {
      key.frames[4] = upper_body_overlay->global_frame;
      key.frames[5] = upper_body_overlay->next_global_frame;
      key.buckets[3] = blend_weight_bucket(upper_body_overlay->frame_lerp);
      key.buckets[4] =
          blend_weight_bucket(upper_body_overlay_weight, k_layer_weight_buckets);
    }
    if (transition_active) {
      for (std::size_t index = 0; index < transition->sources.size(); ++index) {
        auto const& source = transition->sources[index];
        if (!source.valid()) {
          continue;
        }
        key.frames[6 + index * 2] = source.global_frame;
        key.frames[7 + index * 2] = source.next_global_frame;
        key.buckets[5 + index * 3] = blend_weight_bucket(source.frame_lerp);
        key.buckets[6 + index * 3] = blend_weight_bucket(
            transition->upper_shares[index], k_layer_weight_buckets);
        key.buckets[7 + index * 3] = blend_weight_bucket(
            transition->lower_shares[index], k_layer_weight_buckets);
      }
      key.buckets[14] =
          blend_weight_bucket(transition->upper_body_weight, k_layer_weight_buckets);
      key.buckets[15] =
          blend_weight_bucket(transition->lower_body_weight, k_layer_weight_buckets);
    }
    const std::uint32_t bone_count = std::min<std::uint32_t>(
        skin_atlas->bone_count, Render::GL::RiggedCreatureCmd::k_max_owned_bones);
    const auto* blend_profile = asset->blend_profile;
    LocalPose pose{};
    auto blend_layer = [&](const ResolvedRequestPlayback& playback,
                           std::size_t slot,
                           float weight,
                           bool upper_body_only) {
      LocalPose layer{};
      if (!sample_local_pose(blob, playback, bone_count, layer)) {
        return;
      }
      std::uint64_t const layer_key =
          (static_cast<std::uint64_t>(entity_id) << 18U) |
          (static_cast<std::uint64_t>(instance_index) << 2U) | slot;
      key.flipped_bones[slot] =
          entity_id != 0U ? blend_hemispheres().flipped_bones(
                                layer_key, playback.clip_id, pose, layer, bone_count)
                          : shortest_path_flips(pose, layer, bone_count);
      lerp_local_pose(pose,
                      layer,
                      bone_count,
                      weight,
                      blend_profile,
                      upper_body_only,
                      key.flipped_bones[slot]);
    };
    auto compose_pose = [&]() -> bool {
      if (!sample_local_pose(blob, primary_playback, bone_count, pose)) {
        return false;
      }
      if (full_body_active) {
        blend_layer(*full_body_blend,
                    0U,
                    blend_bucket_weight(key.buckets[2], k_layer_weight_buckets),
                    false);
      }
      if (transition_active) {
        blend_transition_into(
            pose,
            blob,
            *transition,
            blend_bucket_weight(key.buckets[14], k_layer_weight_buckets),
            blend_bucket_weight(key.buckets[15], k_layer_weight_buckets),
            bone_count,
            blend_profile);
      }
      if (overlay_active) {
        blend_layer(*upper_body_overlay,
                    1U,
                    blend_bucket_weight(key.buckets[4], k_layer_weight_buckets),
                    true);
      }
      return true;
    };
    bool const layered = full_body_active || overlay_active || transition_active;
    bool const layered_pose_ready = layered && compose_pose();
    OwnedPalette owned = blend_cache().get_or_compute(key, [&]() -> OwnedPalette {
      if (layered ? !layered_pose_ready : !compose_pose()) {
        return {};
      }
      return skin_palette_from_local_pose(blob, pose, bone_count);
    });
    if (owned) {
      attach_owned_palette(cmd, std::move(owned), bone_count);
    }
  }
  auto& animation_diagnostics =
      Render::Profiling::CombatAnimationDiagnostics::instance();
  const bool record_body_pose =
      animation_diagnostics.enabled() || animation_diagnostics.logging_enabled();
  if (record_body_pose && asset->body_pose_probe != nullptr &&
      cmd.bone_palette != nullptr) {

    struct PalettePose {
      const QMatrix4x4* palette;
      std::span<const QMatrix4x4> bind;
    };
    const PalettePose pose_source{cmd.bone_palette, handle.bind_palette};
    asset->body_pose_probe(
        entity_id,
        instance_index,
        draw_world,
        cmd.bone_count,
        [](const void* user, std::uint32_t bone) -> QVector3D {
          const auto* source = static_cast<const PalettePose*>(user);
          return (source->palette[bone] * source->bind[bone]).column(3).toVector3D();
        },
        &pose_source);
  }
  for (const auto& attachment_mesh : entry->attachment_meshes) {
    if (attachment_mesh == nullptr || attachment_mesh->index_count() == 0U) {
      continue;
    }
    auto attachment_cmd = cmd;
    attachment_cmd.mesh = attachment_mesh.get();
    attachment_cmd.shadow_mesh = nullptr;
    out.rigged(std::move(attachment_cmd));
  }
  out.rigged(std::move(cmd));
}

auto submit_snapshot_creature(
    const CreatureRenderAssetHandle& handle,
    CreatureLOD lod,
    Render::Creature::ArchetypeId archetype,
    Render::Creature::VariantId variant,
    Render::Creature::AnimationStateId state,
    std::uint16_t clip_id,
    std::uint8_t clip_variant,
    std::shared_ptr<const Render::RoleColorPalette> role_colors,
    std::uint16_t variant_bucket,
    const QVector3D& base_color,
    const QVector4D& wear_params,
    const QMatrix4x4& world_from_unit,
    const Render::Creature::Bpat::BpatBlob& blob,
    std::uint32_t global_frame,
    std::uint32_t frame_in_clip,
    std::uint32_t entity_id,
    std::uint16_t instance_index,
    float alpha,
    float team_emphasis,
    Render::GL::ISubmitter& out,
    const CreatureSubmissionContext* submission,
    bool allow_bake_fallback) -> bool {
  const CreatureAsset* asset = handle.asset;
  if (lod == CreatureLOD::Culled || asset == nullptr || asset->spec == nullptr ||
      handle.bind_palette.empty()) {
    return false;
  }

  auto& animation_diagnostics =
      Render::Profiling::CombatAnimationDiagnostics::instance();
  const bool record_body_pose =
      animation_diagnostics.enabled() || animation_diagnostics.logging_enabled();
  if (record_body_pose && asset->body_pose_probe != nullptr) {

    struct BlobPose {
      const Render::Creature::Bpat::BpatBlob* blob;
      std::uint32_t global_frame;
    };
    const BlobPose pose_source{&blob, global_frame};
    auto const palette = blob.frame_palette_view(global_frame);
    asset->body_pose_probe(
        entity_id,
        instance_index,
        world_from_unit,
        static_cast<std::uint32_t>(palette.size()),
        [](const void* user, std::uint32_t bone) -> QVector3D {
          const auto* source = static_cast<const BlobPose*>(user);
          return source->blob->bone_global_matrix(source->global_frame, bone)
              .column(3)
              .toVector3D();
        },
        &pose_source);
  }

  if (submission == nullptr) {
    return false;
  }

  const auto key = make_snapshot_key(
      handle, archetype, variant, state, clip_id, clip_variant, frame_in_clip);

  if (!handle.has_static_attachments && has_prebaked_snapshot_mesh(*asset, lod)) {
    const auto* mesh_blob =
        Render::Creature::Snapshot::SnapshotMeshRegistry::instance().blob(
            asset->snapshot_mesh_species_id, lod);
    if (mesh_blob != nullptr) {
      std::uint32_t mesh_global_frame = 0U;
      if (mesh_blob->resolve_global_frame(clip_id, frame_in_clip, mesh_global_frame)) {
        const auto* snap = submission->snapshot_meshes().get_or_load(
            key, *mesh_blob, mesh_global_frame);
        if (snap != nullptr && snap->mesh != nullptr &&
            snap->mesh->index_count() != 0U) {
          auto cmd = make_rigged_cmd(snap->mesh.get(),
                                     world_from_unit,
                                     Render::GL::SnapshotMeshCache::identity_palette(),
                                     1U,
                                     role_colors,
                                     base_color,
                                     wear_params,
                                     material_id_for_species(handle.archetype->species),
                                     alpha,
                                     team_emphasis);
          cmd.palette_block = nullptr;
          cmd.palette_offset = 0U;

          out.rigged(std::move(cmd));
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

  auto& rigged_cache = submission->rigged_meshes();
  const auto* source = create_creature_render_asset(
      rigged_cache, handle, lod, blob, variant_bucket, false);
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

  if (source->skin_atlas == nullptr || source->skin_atlas->palettes.empty() ||
      source->skin_atlas->bone_count == 0 ||
      global_frame >= source->skin_atlas->frame_total) {
    return false;
  }

  const auto* snap =
      submission->snapshot_meshes().get_or_bake(key, *source, global_frame);
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
                             std::move(role_colors),
                             base_color,
                             wear_params,
                             material_id_for_species(handle.archetype->species),
                             alpha,
                             team_emphasis);
  cmd.palette_block = nullptr;
  cmd.palette_offset = 0U;

  for (const auto& attachment_mesh : snap->attachment_meshes) {
    if (attachment_mesh == nullptr || attachment_mesh->index_count() == 0U) {
      continue;
    }
    auto attachment_cmd = cmd;
    attachment_cmd.mesh = attachment_mesh.get();
    attachment_cmd.shadow_mesh = nullptr;
    out.rigged(std::move(attachment_cmd));
  }

  out.rigged(std::move(cmd));
  return true;
}

} // namespace creature_pipeline_detail

} // namespace Render::Creature::Pipeline
