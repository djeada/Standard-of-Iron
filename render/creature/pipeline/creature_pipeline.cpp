#include "creature_pipeline.h"

#include <QQuaternion>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_reader.h"
#include "animation/bpat/bpat_registry.h"
#include "animation/clip_manifest.h"
#include "creature_asset.h"
#include "creature_bone_probe.h"
#include "game/map/terrain_service.h"
#include "preparation_common.h"
#include "render/bone_palette_arena.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/skeleton.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/creature/spec.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/registry.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/cache_control.h"
#include "render/humanoid/skeleton.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/frame_profile.h"
#include "render/rigged_mesh_cache.h"
#include "render/scene_renderer.h"
#include "render/snapshot_mesh_cache.h"
#include "render/submitter.h"
#include "render/wildlife/wildlife_rig.h"

namespace Render::Creature::Pipeline {

namespace {

auto resolve_renderer(Render::GL::ISubmitter& out) noexcept -> Render::GL::Renderer* {
  return dynamic_cast<Render::GL::Renderer*>(out.unwrap_submitter());
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
  const auto* atlas = entry.skin_atlas.get();
  const bool had_atlas = atlas != nullptr && atlas->frame_total == blob.frame_total() &&
                         atlas->bone_count != 0U && !atlas->palettes.empty();
  Render::GL::rigged_entry_ensure_skin_atlas_from_blob(entry, blob);
  atlas = entry.skin_atlas.get();
  if (!had_atlas && atlas != nullptr && atlas->frame_total == blob.frame_total() &&
      atlas->bone_count != 0U && !atlas->palettes.empty()) {
    cache.record_skin_atlas_build();
  }
}

void ensure_skin_ubo_for_submit(Render::GL::RiggedMeshCache& cache,
                                const Render::GL::RiggedMeshEntry& entry) {
  const bool had_ubo =
      entry.skin_atlas != nullptr && entry.skin_atlas->palette_ubo != 0U;
  Render::GL::rigged_entry_ensure_skin_ubo(entry);
  if (!had_ubo && entry.skin_atlas != nullptr && entry.skin_atlas->palette_ubo != 0U) {
    const auto bytes = static_cast<std::uint64_t>(entry.skin_atlas->frame_total) *
                       Render::GL::BonePaletteArena::k_palette_bytes;
    cache.record_skin_ubo_upload(bytes);
  } else if (entry.skin_atlas != nullptr && entry.skin_atlas->palette_ubo == 0U &&
             !entry.skin_atlas->palettes.empty() &&
             entry.skin_atlas->frame_total != 0U &&
             entry.skin_atlas->bone_count != 0U) {
    cache.mark_skin_ubo_upload_pending();
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

auto material_id_for_species(CreatureKind species) noexcept -> std::int32_t {
  switch (species) {
  case CreatureKind::Horse:
    return Render::Horse::k_horse_material_id;
  case CreatureKind::Elephant:
    return Render::Elephant::k_elephant_material_id;
  case CreatureKind::Sheep:
  case CreatureKind::Wolf:
    return Render::Wildlife::k_wildlife_material_id;
  case CreatureKind::Humanoid:
  case CreatureKind::Mounted:
    break;
  }
  return 0;
}

auto make_rigged_cmd(Render::GL::RiggedMesh* mesh,
                     const QMatrix4x4& world_from_unit,
                     const QMatrix4x4* bone_palette,
                     std::uint32_t bone_count,
                     std::shared_ptr<const Render::RoleColorPalette> role_colors,
                     const QVector3D& base_color,
                     const QVector4D& wear_params,
                     std::int32_t material_id) -> Render::GL::RiggedCreatureCmd {
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
  return cmd;
}

struct ResolvedRequestPlayback {
  const CreatureRenderAssetHandle* handle{nullptr};
  const Render::Creature::Bpat::BpatBlob* blob{nullptr};
  ArchetypeId archetype{k_invalid_archetype};
  AnimationStateId state{AnimationStateId::Idle};
  std::uint16_t clip_id{k_invalid_bpat_clip};
  std::uint8_t clip_variant{0U};
  std::uint32_t global_frame{0U};
  std::uint32_t frame_in_clip{0U};
  std::uint32_t next_global_frame{0U};
  std::uint32_t next_frame_in_clip{0U};
  float frame_lerp{0.0F};
  bool snapshot{false};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return handle != nullptr && blob != nullptr && clip_id != k_invalid_bpat_clip;
  }
};

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

auto is_humanoid_upper_body_bone(std::size_t bone_index) noexcept -> bool {
  using Bone = Render::Humanoid::HumanoidBone;
  switch (static_cast<Bone>(bone_index)) {
  case Bone::Chest:
  case Bone::Neck:
  case Bone::Head:
  case Bone::ShoulderL:
  case Bone::UpperArmL:
  case Bone::ForearmL:
  case Bone::HandL:
  case Bone::ShoulderR:
  case Bone::UpperArmR:
  case Bone::ForearmR:
  case Bone::HandR:
    return true;
  default:
    return false;
  }
}

using BonePaletteArray =
    std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>;

template <class T>
struct PooledPaletteAllocator {
  using value_type = T;

  PooledPaletteAllocator() noexcept = default;
  template <class U>
  PooledPaletteAllocator(const PooledPaletteAllocator<U>&) noexcept {}

  static auto free_list() noexcept -> std::vector<void*>& {
    thread_local auto* list = new std::vector<void*>();
    return *list;
  }

  auto allocate(std::size_t n) -> T* {
    if (n == 1U) {
      auto& list = free_list();
      if (!list.empty()) {
        void* block = list.back();
        list.pop_back();
        return static_cast<T*>(block);
      }
      return static_cast<T*>(::operator new(sizeof(T)));
    }
    return static_cast<T*>(::operator new(n * sizeof(T)));
  }

  void deallocate(T* ptr, std::size_t n) noexcept {
    if (n == 1U) {
      auto& list = free_list();
      try {
        list.push_back(static_cast<void*>(ptr));
        return;
      } catch (...) {
      }
    }
    ::operator delete(static_cast<void*>(ptr));
  }

  template <class U>
  auto operator==(const PooledPaletteAllocator<U>&) const noexcept -> bool {
    return true;
  }
  template <class U>
  auto operator!=(const PooledPaletteAllocator<U>&) const noexcept -> bool {
    return false;
  }
};

constexpr std::uint32_t k_blend_weight_buckets = 32U;

auto blend_weight_bucket(float weight) noexcept -> std::uint32_t {
  return static_cast<std::uint32_t>(
      std::lround(std::clamp(weight, 0.0F, 1.0F) *
                  static_cast<float>(k_blend_weight_buckets - 1U)));
}

auto blend_bucket_weight(std::uint32_t bucket) noexcept -> float {
  return static_cast<float>(bucket) / static_cast<float>(k_blend_weight_buckets - 1U);
}

using OwnedPalette = std::shared_ptr<BonePaletteArray>;
using LocalPose = std::array<Render::Creature::Bpat::LocalBonePose,
                             Render::GL::RiggedCreatureCmd::k_max_owned_bones>;

auto blends_bone(CreatureKind species_kind,
                 std::size_t bone,
                 bool upper_body_only) noexcept -> bool {
  if (!upper_body_only) {
    return true;
  }
  return species_kind == CreatureKind::Humanoid && is_humanoid_upper_body_bone(bone);
}

void lerp_local_pose(LocalPose& io,
                     std::span<const Render::Creature::Bpat::LocalBonePose> layer,
                     std::uint32_t bone_count,
                     float weight,
                     CreatureKind species_kind,
                     bool upper_body_only) noexcept {
  for (std::uint32_t bone = 0; bone < bone_count && bone < layer.size(); ++bone) {
    if (!blends_bone(species_kind, bone, upper_body_only)) {
      continue;
    }
    io[bone].rotation =
        QQuaternion::slerp(io[bone].rotation, layer[bone].rotation, weight);
    io[bone].translation =
        io[bone].translation * (1.0F - weight) + layer[bone].translation * weight;
  }
}

auto sample_local_pose(const Render::Creature::Bpat::BpatBlob& blob,
                       const ResolvedRequestPlayback& playback,
                       std::uint32_t bone_count,
                       CreatureKind species_kind,
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
  lerp_local_pose(
      out, next, bone_count, blend_bucket_weight(bucket), species_kind, false);
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
    QMatrix4x4 local;
    local.translate(pose[bone].translation);
    local.rotate(pose[bone].rotation);
    std::uint8_t const parent = parents[bone];
    global[bone] = parent == Render::Creature::Bpat::k_no_parent_bone
                       ? local
                       : global[parent] * local;
    (*owned)[bone] = global[bone] * inverse_bind[bone];
  }
  return owned;
}

struct PaletteBlendKey {
  const Render::GL::RiggedMeshEntry* entry{nullptr};
  std::array<std::uint32_t, 6> frames{};
  std::array<std::uint32_t, 5> buckets{};

  auto operator==(const PaletteBlendKey& other) const noexcept -> bool {
    return entry == other.entry && frames == other.frames && buckets == other.buckets;
  }
};

class PaletteBlendCache {
public:
  template <typename Compute>
  auto get_or_compute(const PaletteBlendKey& key, Compute&& compute) -> OwnedPalette {
    if (std::uint32_t const now = Render::GL::humanoid_current_frame();
        now != m_frame) {
      m_frame = now;
    }
    std::size_t const home = hash(key) & (k_slots - 1U);
    std::size_t victim = home;
    for (std::size_t probe = 0; probe < k_probes; ++probe) {
      std::size_t const index = (home + probe) & (k_slots - 1U);
      Slot& slot = m_slots[index];
      if (slot.frame == m_frame && slot.key == key) {
        return slot.palette;
      }
      if (slot.frame != m_frame) {
        victim = index;
        break;
      }
    }
    Slot& slot = m_slots[victim];
    slot.frame = m_frame;
    slot.key = key;
    slot.palette = compute();
    return slot.palette;
  }

private:
  static constexpr std::size_t k_slots = 8192U;
  static constexpr std::size_t k_probes = 8U;

  struct Slot {
    PaletteBlendKey key{};
    std::uint32_t frame{std::numeric_limits<std::uint32_t>::max()};
    OwnedPalette palette{};
  };

  static auto hash(const PaletteBlendKey& key) noexcept -> std::size_t {
    std::size_t hash = std::hash<const void*>{}(key.entry);
    for (auto const v : key.frames) {
      hash ^= v * 0x9E3779B1U + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
    }
    for (auto const v : key.buckets) {
      hash ^= v * 0x85EBCA77U + 0xC2B2AE3DU + (hash << 6U) + (hash >> 2U);
    }
    return hash;
  }

  std::array<Slot, k_slots> m_slots{};
  std::uint32_t m_frame{std::numeric_limits<std::uint32_t>::max()};
};

auto blend_cache() -> PaletteBlendCache& {
  thread_local PaletteBlendCache cache;
  return cache;
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
  cmd.palette_ubo = 0U;
  cmd.palette_offset = 0U;
}

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
                            float full_body_blend_weight,
                            float upper_body_overlay_weight,
                            std::uint32_t entity_id,
                            std::uint16_t instance_index,
                            Render::GL::ISubmitter& out,
                            Render::GL::Renderer* renderer) {
  const CreatureAsset* asset = handle.asset;
  if (lod == CreatureLOD::Culled || asset == nullptr || asset->spec == nullptr ||
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

  const bool wants_layered_pose =
      (full_body_blend != nullptr && full_body_blend->valid() &&
       full_body_blend_weight > 0.0F) ||
      (upper_body_overlay != nullptr && upper_body_overlay->valid() &&
       upper_body_overlay_weight > 0.0F);
  const auto* skin_atlas = entry->skin_atlas.get();
  if (skin_atlas == nullptr) {
    return;
  }
  const bool frames_resident =
      skin_atlas->palette_ubo != 0U && skin_atlas->frame_total != 0U &&
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
                             material_id_for_species(handle.archetype->species));

  const bool skin_ubo_covers_frame = skin_atlas->palette_ubo != 0U &&
                                     skin_atlas->frame_total != 0U &&
                                     global_frame < skin_atlas->frame_total;
  if (skin_ubo_covers_frame) {
    cmd.palette_ubo = skin_atlas->palette_ubo;
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
  const std::uint32_t primary_lerp_bucket =
      blend_weight_bucket(primary_playback.frame_lerp);
  const bool needs_owned_palette =
      !use_resident_frames &&
      (primary_lerp_bucket != 0U || full_body_active || overlay_active);
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
      key.buckets[2] = blend_weight_bucket(full_body_blend_weight);
    }
    if (overlay_active) {
      key.frames[4] = upper_body_overlay->global_frame;
      key.frames[5] = upper_body_overlay->next_global_frame;
      key.buckets[3] = blend_weight_bucket(upper_body_overlay->frame_lerp);
      key.buckets[4] = blend_weight_bucket(upper_body_overlay_weight);
    }
    const std::uint32_t bone_count = std::min<std::uint32_t>(
        skin_atlas->bone_count, Render::GL::RiggedCreatureCmd::k_max_owned_bones);
    const auto species_kind = handle.archetype->species;
    OwnedPalette owned = blend_cache().get_or_compute(key, [&]() -> OwnedPalette {
      LocalPose pose{};
      if (!sample_local_pose(blob, primary_playback, bone_count, species_kind, pose)) {
        return {};
      }
      LocalPose layer{};
      if (full_body_active &&
          sample_local_pose(blob, *full_body_blend, bone_count, species_kind, layer)) {
        lerp_local_pose(pose,
                        layer,
                        bone_count,
                        blend_bucket_weight(key.buckets[2]),
                        species_kind,
                        false);
      }
      if (overlay_active &&
          sample_local_pose(
              blob, *upper_body_overlay, bone_count, species_kind, layer)) {
        lerp_local_pose(pose,
                        layer,
                        bone_count,
                        blend_bucket_weight(key.buckets[4]),
                        species_kind,
                        true);
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
  if (record_body_pose && handle.archetype->species == CreatureKind::Humanoid &&
      cmd.bone_palette != nullptr &&
      cmd.bone_count >
          static_cast<std::uint32_t>(Render::Humanoid::HumanoidBone::HandR)) {
    auto const pelvis_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Pelvis);
    auto const neck_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Neck);
    QMatrix4x4 const posed_pelvis =
        cmd.bone_palette[pelvis_index] * handle.bind_palette[pelvis_index];
    QMatrix4x4 const posed_neck =
        cmd.bone_palette[neck_index] * handle.bind_palette[neck_index];
    auto const shoulder_l_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::ShoulderL);
    auto const hand_l_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandL);
    auto const shoulder_r_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::ShoulderR);
    auto const hand_r_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandR);
    QMatrix4x4 const posed_shoulder_l =
        cmd.bone_palette[shoulder_l_index] * handle.bind_palette[shoulder_l_index];
    QMatrix4x4 const posed_hand_l =
        cmd.bone_palette[hand_l_index] * handle.bind_palette[hand_l_index];
    QMatrix4x4 const posed_shoulder_r =
        cmd.bone_palette[shoulder_r_index] * handle.bind_palette[shoulder_r_index];
    QMatrix4x4 const posed_hand_r =
        cmd.bone_palette[hand_r_index] * handle.bind_palette[hand_r_index];
    QVector3D const pelvis_world = draw_world.map(posed_pelvis.column(3).toVector3D());
    QVector3D const neck_world = draw_world.map(posed_neck.column(3).toVector3D());
    QVector3D const visible_torso = neck_world - pelvis_world;
    float const body_up_y =
        visible_torso.lengthSquared() > 1.0e-8F ? visible_torso.normalized().y() : 1.0F;
    float const max_arm_reach = std::max(
        (posed_hand_l.column(3).toVector3D() - posed_shoulder_l.column(3).toVector3D())
            .length(),
        (posed_hand_r.column(3).toVector3D() - posed_shoulder_r.column(3).toVector3D())
            .length());
    animation_diagnostics.record_submitted_body_pose(
        entity_id, instance_index, body_up_y, max_arm_reach);
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
    Render::GL::ISubmitter& out,
    Render::GL::Renderer* renderer,
    bool allow_bake_fallback = true) -> bool {
  const CreatureAsset* asset = handle.asset;
  if (lod == CreatureLOD::Culled || asset == nullptr || asset->spec == nullptr ||
      handle.bind_palette.empty()) {
    return false;
  }

  auto& animation_diagnostics =
      Render::Profiling::CombatAnimationDiagnostics::instance();
  const bool record_body_pose =
      animation_diagnostics.enabled() || animation_diagnostics.logging_enabled();
  if (record_body_pose && handle.archetype->species == CreatureKind::Humanoid) {
    auto const palette = blob.frame_palette_view(global_frame);
    auto const pelvis_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Pelvis);
    auto const neck_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Neck);
    auto const shoulder_l_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::ShoulderL);
    auto const hand_l_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandL);
    auto const shoulder_r_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::ShoulderR);
    auto const hand_r_index =
        static_cast<std::size_t>(Render::Humanoid::HumanoidBone::HandR);
    if (palette.size() > hand_r_index) {
      auto const bone_origin = [&](std::size_t bone) {
        return blob.bone_global_matrix(global_frame, static_cast<std::uint32_t>(bone))
            .column(3)
            .toVector3D();
      };
      QVector3D const pelvis_world = world_from_unit.map(bone_origin(pelvis_index));
      QVector3D const neck_world = world_from_unit.map(bone_origin(neck_index));
      QVector3D const visible_torso = neck_world - pelvis_world;
      float const body_up_y = visible_torso.lengthSquared() > 1.0e-8F
                                  ? visible_torso.normalized().y()
                                  : 1.0F;
      float const max_arm_reach = std::max(
          (bone_origin(hand_l_index) - bone_origin(shoulder_l_index)).length(),
          (bone_origin(hand_r_index) - bone_origin(shoulder_r_index)).length());
      animation_diagnostics.record_submitted_body_pose(
          entity_id, instance_index, body_up_y, max_arm_reach);
    }
  }

  if (renderer == nullptr) {
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
        const auto* snap = renderer->snapshot_mesh_cache().get_or_load(
            key, *mesh_blob, mesh_global_frame);
        if (snap != nullptr && snap->mesh != nullptr &&
            snap->mesh->index_count() != 0U) {
          auto cmd =
              make_rigged_cmd(snap->mesh.get(),
                              world_from_unit,
                              Render::GL::SnapshotMeshCache::identity_palette(),
                              1U,
                              role_colors,
                              base_color,
                              wear_params,
                              material_id_for_species(handle.archetype->species));
          cmd.palette_ubo = 0U;
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
  if (source->skin_atlas == nullptr || source->skin_atlas->palettes.empty() ||
      source->skin_atlas->bone_count == 0 ||
      global_frame >= source->skin_atlas->frame_total) {
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
                             std::move(role_colors),
                             base_color,
                             wear_params,
                             material_id_for_species(handle.archetype->species));
  cmd.palette_ubo = 0U;
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

    const bool use_snapshot_mesh = req.lod != CreatureLOD::Full && primary.snapshot;
    if (use_snapshot_mesh) {
      auto snapshot_playback = primary;
      if (req.full_body_blend.active() && full_body.valid() &&
          req.full_body_blend.weight >= 0.5F) {
        snapshot_playback = full_body;
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
                           req.full_body_blend.weight,
                           req.upper_body_overlay.weight,
                           req.entity_id,
                           req.instance_index,
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
