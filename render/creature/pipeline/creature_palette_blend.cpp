#include "render/creature/pipeline/creature_pipeline_internal.h"

namespace Render::Creature::Pipeline {

namespace creature_pipeline_detail {

auto frame_palette_for_global_frame(const Render::GL::RiggedMeshEntry& entry,
                                    std::uint32_t global_frame) noexcept
    -> const QMatrix4x4* {
  if (entry.skin_atlas == nullptr || entry.skin_atlas->palettes.empty() ||
      entry.skin_atlas->bone_count == 0 ||
      global_frame >= entry.skin_atlas->frame_total) {
    return nullptr;
  }
  return entry.skin_atlas->palettes.data() +
         static_cast<std::size_t>(global_frame) * entry.skin_atlas->bone_count;
}

auto blend_weight_bucket(float weight,
                         std::uint32_t buckets) noexcept -> std::uint32_t {
  return static_cast<std::uint32_t>(
      std::lround(std::clamp(weight, 0.0F, 1.0F) * static_cast<float>(buckets - 1U)));
}

auto blend_bucket_weight(std::uint32_t bucket,
                         std::uint32_t buckets) noexcept -> float {
  return static_cast<float>(bucket) / static_cast<float>(buckets - 1U);
}

auto blends_bone(const Render::Creature::SkeletonBlendProfile* profile,
                 std::size_t bone,
                 bool upper_body_only) noexcept -> bool {
  if (!upper_body_only) {
    return true;
  }
  return profile != nullptr && profile->upper_body.contains(bone);
}

auto shortest_path_flips(const LocalPose& base,
                         std::span<const Render::Creature::Bpat::LocalBonePose> layer,
                         std::uint32_t bone_count) noexcept -> std::uint64_t {
  std::uint64_t flipped = 0ULL;
  for (std::uint32_t bone = 0; bone < bone_count && bone < layer.size(); ++bone) {
    if (QQuaternion::dotProduct(base[bone].rotation, layer[bone].rotation) < 0.0F) {
      flipped |= 1ULL << bone;
    }
  }
  return flipped;
}

auto slerp_in_hemisphere(const QQuaternion& from,
                         const QQuaternion& to,
                         float weight,
                         bool flip) noexcept -> QQuaternion {
  QQuaternion const target = flip ? -to : to;
  float const angle =
      std::acos(std::clamp(QQuaternion::dotProduct(from, target), -1.0F, 1.0F));
  float const sine = std::sin(angle);
  if (sine < 1.0e-3F) {
    QQuaternion const mixed = from * (1.0F - weight) + target * weight;
    return mixed.length() > 1.0e-4F ? mixed.normalized()
                                    : (weight < 0.5F ? from : target);
  }
  return (from * (std::sin((1.0F - weight) * angle) / sine) +
          target * (std::sin(weight * angle) / sine))
      .normalized();
}

void lerp_local_pose(LocalPose& io,
                     std::span<const Render::Creature::Bpat::LocalBonePose> layer,
                     std::uint32_t bone_count,
                     float weight,
                     const Render::Creature::SkeletonBlendProfile* blend_profile,
                     bool upper_body_only,
                     std::uint64_t flipped_bones) noexcept {
  for (std::uint32_t bone = 0; bone < bone_count && bone < layer.size(); ++bone) {
    if (!blends_bone(blend_profile, bone, upper_body_only)) {
      continue;
    }
    io[bone].rotation = slerp_in_hemisphere(io[bone].rotation,
                                            layer[bone].rotation,
                                            weight,
                                            ((flipped_bones >> bone) & 1ULL) != 0ULL);
    io[bone].translation =
        io[bone].translation * (1.0F - weight) + layer[bone].translation * weight;
    io[bone].scale = io[bone].scale * (1.0F - weight) + layer[bone].scale * weight;
  }
}

void lerp_local_pose_by_region(
    LocalPose& io,
    std::span<const Render::Creature::Bpat::LocalBonePose> layer,
    std::uint32_t bone_count,
    float upper_body_weight,
    float lower_body_weight,
    const Render::Creature::SkeletonBlendProfile* blend_profile,
    std::uint64_t flipped_bones) noexcept {
  for (std::uint32_t bone = 0; bone < bone_count && bone < layer.size(); ++bone) {
    bool const upper =
        blend_profile == nullptr || blend_profile->upper_body.contains(bone);
    float const weight = upper ? upper_body_weight : lower_body_weight;
    if (weight <= 0.0F) {
      continue;
    }
    io[bone].rotation = slerp_in_hemisphere(io[bone].rotation,
                                            layer[bone].rotation,
                                            weight,
                                            ((flipped_bones >> bone) & 1ULL) != 0ULL);
    io[bone].translation =
        io[bone].translation * (1.0F - weight) + layer[bone].translation * weight;
    io[bone].scale = io[bone].scale * (1.0F - weight) + layer[bone].scale * weight;
  }
}

auto sample_local_pose(const Render::Creature::Bpat::BpatBlob& blob,
                       const ResolvedRequestPlayback& playback,
                       std::uint32_t bone_count,
                       LocalPose& out) noexcept -> bool {
  auto const current = blob.frame_local_pose_view(playback.global_frame);
  if (current.size() < bone_count) {
    return false;
  }
  std::copy_n(current.begin(), bone_count, out.begin());
  std::uint32_t const bucket = blend_weight_bucket(playback.frame_lerp);
  if (bucket == 0U || playback.next_global_frame == playback.global_frame) {
    return true;
  }
  auto const next = blob.frame_local_pose_view(playback.next_global_frame);
  if (next.size() < bone_count) {
    return true;
  }
  lerp_local_pose(out,
                  next,
                  bone_count,
                  blend_bucket_weight(bucket),
                  nullptr,
                  false,
                  shortest_path_flips(out, next, bone_count));
  return true;
}

auto skin_palette_from_local_pose(const Render::Creature::Bpat::BpatBlob& blob,
                                  const LocalPose& pose,
                                  std::uint32_t bone_count) -> OwnedPalette {
  auto const parents = blob.bone_parents();
  auto const inverse_bind = blob.inverse_bind_palette();
  if (parents.size() < bone_count || inverse_bind.size() < bone_count) {
    return {};
  }
  auto owned = std::allocate_shared<BonePaletteArray>(
      PooledPaletteAllocator<BonePaletteArray>{});
  BonePaletteArray global{};
  for (std::uint32_t bone = 0; bone < bone_count; ++bone) {
    QMatrix4x4 const local = pose[bone].matrix();
    std::uint8_t const parent = parents[bone];
    global[bone] = parent == Render::Creature::Bpat::k_no_parent_bone
                       ? local
                       : global[parent] * local;
    (*owned)[bone] = global[bone] * inverse_bind[bone];
  }
  return owned;
}

auto blend_cache() -> PaletteBlendCache& {
  thread_local PaletteBlendCache cache;
  return cache;
}

auto blend_hemispheres() -> BlendHemisphereTable& {
  static BlendHemisphereTable table;
  return table;
}

auto contact_y_for_playback(const ResolvedRequestPlayback& playback) noexcept -> float {
  if (playback.blob == nullptr ||
      !playback.blob->clip_supplies_ground_contact(playback.clip_id)) {
    return 0.0F;
  }
  auto const contacts = playback.blob->frame_contacts();
  if (playback.global_frame >= contacts.size()) {
    return 0.0F;
  }
  float const current = contacts[playback.global_frame].sole_y;
  float const frame_lerp = std::clamp(playback.frame_lerp, 0.0F, 1.0F);
  if (frame_lerp <= 1.0e-4F || playback.next_global_frame == playback.global_frame ||
      playback.next_global_frame >= contacts.size()) {
    return current;
  }
  float const next = contacts[playback.next_global_frame].sole_y;
  return current * (1.0F - frame_lerp) + next * frame_lerp;
}

auto bone_global_at(const ResolvedRequestPlayback& playback,
                    std::uint16_t bone_index) -> QMatrix4x4 {
  QMatrix4x4 const current =
      playback.blob->bone_global_matrix(playback.global_frame, bone_index);
  float const frame_lerp = std::clamp(playback.frame_lerp, 0.0F, 1.0F);
  if (frame_lerp <= 1.0e-4F || playback.next_global_frame == playback.global_frame) {
    return current;
  }
  QMatrix4x4 const next =
      playback.blob->bone_global_matrix(playback.next_global_frame, bone_index);

  QMatrix4x4 blended;
  const float* a = current.constData();
  const float* b = next.constData();
  float* out = blended.data();
  for (int i = 0; i < 16; ++i) {
    out[i] = a[i] * (1.0F - frame_lerp) + b[i] * frame_lerp;
  }
  return blended;
}

void resolve_bone_probe(const Render::Creature::CreatureRenderRequest& req,
                        const QMatrix4x4& draw_world,
                        const ResolvedRequestPlayback& playback) {
  auto* probe = active_bone_probe();
  if (probe == nullptr || probe->resolved) {
    return;
  }
  if (probe->entity_id != req.entity_id ||
      probe->instance_index != req.instance_index) {
    return;
  }
  if (!playback.valid() || probe->bone_index >= playback.blob->bone_count()) {
    return;
  }
  probe->world = draw_world * bone_global_at(playback, probe->bone_index);
  probe->resolved = true;
}

void attach_owned_palette(
    Render::GL::RiggedCreatureCmd& cmd,
    std::shared_ptr<
        std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>
        owned_palette,
    std::uint32_t bone_count) {
  if (!owned_palette) {
    return;
  }
  cmd.owned_bone_palette = std::move(owned_palette);
  cmd.bone_palette = cmd.owned_bone_palette->data();
  cmd.bone_count = std::min<std::uint32_t>(
      bone_count, Render::GL::RiggedCreatureCmd::k_max_owned_bones);
  cmd.palette_block = nullptr;
  cmd.palette_offset = 0U;
}

void blend_transition_into(
    LocalPose& pose,
    const Render::Creature::Bpat::BpatBlob& blob,
    const ResolvedTransition& transition,
    float upper_body_weight,
    float lower_body_weight,
    std::uint32_t bone_count,
    const Render::Creature::SkeletonBlendProfile* blend_profile) {
  LocalPose outgoing{};
  float upper_total = 0.0F;
  float lower_total = 0.0F;
  bool seeded = false;
  for (std::size_t index = 0; index < transition.sources.size(); ++index) {
    auto const& source = transition.sources[index];
    float const upper = transition.upper_shares[index];
    float const lower = transition.lower_shares[index];
    if (!source.valid() || std::max(upper, lower) <= 0.0F) {
      continue;
    }
    if (!seeded) {
      if (!sample_local_pose(blob, source, bone_count, outgoing)) {
        return;
      }
      seeded = true;
    } else {
      LocalPose layer{};
      if (!sample_local_pose(blob, source, bone_count, layer)) {
        continue;
      }
      float const upper_mix = upper > 0.0F ? upper / (upper_total + upper) : 0.0F;
      float const lower_mix = lower > 0.0F ? lower / (lower_total + lower) : 0.0F;
      lerp_local_pose_by_region(outgoing,
                                layer,
                                bone_count,
                                upper_mix,
                                lower_mix,
                                blend_profile,
                                shortest_path_flips(outgoing, layer, bone_count));
    }
    upper_total += upper;
    lower_total += lower;
  }
  if (!seeded) {
    return;
  }
  lerp_local_pose_by_region(pose,
                            outgoing,
                            bone_count,
                            upper_body_weight,
                            lower_body_weight,
                            blend_profile,
                            shortest_path_flips(pose, outgoing, bone_count));
}

} // namespace creature_pipeline_detail

} // namespace Render::Creature::Pipeline
