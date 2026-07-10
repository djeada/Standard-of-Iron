#include "creature_pipeline.h"

#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "../../../game/map/terrain_service.h"
#include "../../bone_palette_arena.h"
#include "../../entity/registry.h"
#include "../../humanoid/skeleton.h"
#include "../../profiling/frame_profile.h"
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
#include "animation/clip_manifest.h"
#include "creature_asset.h"
#include "preparation_common.h"

namespace Render::Creature::Pipeline {

namespace {

auto resolve_renderer(Render::GL::ISubmitter& out) noexcept -> Render::GL::Renderer* {
  return dynamic_cast<Render::GL::Renderer*>(out.unwrap_submitter());
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

auto humanoid_idle_variant_clip_is_usable(const Render::Creature::Bpat::BpatBlob* blob,
                                          std::uint16_t clip_id,
                                          Render::Creature::AnimationStateId state,
                                          std::uint8_t clip_variant) noexcept -> bool;

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
  if (!has_explicit_clip &&
      !humanoid_idle_variant_clip_is_usable(
          playback.blob, effective_clip_id, state, effective_clip_variant)) {
    effective_clip_variant = 0U;
    effective_clip_id = playback_desc.clip_id;
    playback = resolve_bpat_playback(playback_desc.blob, playback_desc.clip_id, phase);
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
  if (entry.skinned_palettes.empty() || entry.skinned_bone_count == 0 ||
      global_frame >= entry.skinned_frame_total) {
    return nullptr;
  }
  return entry.skinned_palettes.data() +
         static_cast<std::size_t>(global_frame) * entry.skinned_bone_count;
}

auto lerp_matrix(const QMatrix4x4& a,
                 const QMatrix4x4& b,
                 float t) noexcept -> QMatrix4x4 {
  QMatrix4x4 out;
  float* dst = out.data();
  const float* av = a.constData();
  const float* bv = b.constData();
  float const inv_t = 1.0F - t;
  for (int i = 0; i < 16; ++i) {
    dst[i] = av[i] * inv_t + bv[i] * t;
  }
  return out;
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
    thread_local std::vector<void*> list;
    return list;
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

auto acquire_pooled_palette() -> std::shared_ptr<BonePaletteArray> {
  return std::allocate_shared<BonePaletteArray>(
      PooledPaletteAllocator<BonePaletteArray>{});
}

auto blend_palette_owned(const QMatrix4x4* primary_palette,
                         const QMatrix4x4* secondary_palette,
                         std::uint32_t bone_count,
                         float secondary_weight,
                         bool upper_body_only,
                         CreatureKind species_kind) noexcept
    -> std::shared_ptr<
        std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>> {
  if (primary_palette == nullptr || secondary_palette == nullptr || bone_count == 0U) {
    return {};
  }
  auto owned = acquire_pooled_palette();
  float const weight = std::clamp(secondary_weight, 0.0F, 1.0F);
  for (std::uint32_t bone = 0;
       bone < bone_count && bone < Render::GL::RiggedCreatureCmd::k_max_owned_bones;
       ++bone) {
    bool const apply_secondary =
        !upper_body_only ||
        (species_kind == CreatureKind::Humanoid && is_humanoid_upper_body_bone(bone));
    (*owned)[bone] =
        apply_secondary
            ? lerp_matrix(primary_palette[bone], secondary_palette[bone], weight)
            : primary_palette[bone];
  }
  return owned;
}

auto interpolated_palette_for_playback(
    const Render::GL::RiggedMeshEntry& entry,
    const ResolvedRequestPlayback& playback,
    CreatureKind species_kind,
    std::shared_ptr<
        std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>&
        owned_palette) noexcept -> const QMatrix4x4* {
  const QMatrix4x4* current =
      frame_palette_for_global_frame(entry, playback.global_frame);
  if (current == nullptr) {
    return nullptr;
  }
  float const frame_lerp = std::clamp(playback.frame_lerp, 0.0F, 1.0F);
  if (frame_lerp <= 1.0e-4F || playback.next_global_frame == playback.global_frame) {
    return current;
  }
  const QMatrix4x4* next =
      frame_palette_for_global_frame(entry, playback.next_global_frame);
  if (next == nullptr) {
    return current;
  }
  owned_palette = blend_palette_owned(
      current, next, entry.skinned_bone_count, frame_lerp, false, species_kind);
  return owned_palette ? owned_palette->data() : current;
}

auto contact_y_for_playback(CreatureKind species_kind,
                            const ResolvedRequestPlayback& playback) noexcept -> float {
  if (playback.blob == nullptr) {
    return 0.0F;
  }
  float const current = palette_contact_y(
      species_kind, playback.blob->frame_palette_view(playback.global_frame));
  float const frame_lerp = std::clamp(playback.frame_lerp, 0.0F, 1.0F);
  if (frame_lerp <= 1.0e-4F || playback.next_global_frame == playback.global_frame) {
    return current;
  }
  float const next = palette_contact_y(
      species_kind, playback.blob->frame_palette_view(playback.next_global_frame));
  return current * (1.0F - frame_lerp) + next * frame_lerp;
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
                            std::span<const QVector3D> role_colors,
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

  std::shared_ptr<
      std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>
      primary_interpolated_palette;
  const QMatrix4x4* frame_palette =
      interpolated_palette_for_playback(*entry,
                                        primary_playback,
                                        handle.archetype->species,
                                        primary_interpolated_palette);
  if (frame_palette == nullptr) {
    return;
  }

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
  if (primary_interpolated_palette) {
    attach_owned_palette(
        cmd, std::move(primary_interpolated_palette), entry->skinned_bone_count);
  }
  if (full_body_blend != nullptr && full_body_blend->valid() &&
      full_body_blend_weight > 0.0F) {
    std::shared_ptr<
        std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>
        full_body_interpolated_palette;
    if (const QMatrix4x4* secondary_palette =
            interpolated_palette_for_playback(*entry,
                                              *full_body_blend,
                                              handle.archetype->species,
                                              full_body_interpolated_palette);
        secondary_palette != nullptr) {
      attach_owned_palette(cmd,
                           blend_palette_owned(frame_palette,
                                               secondary_palette,
                                               entry->skinned_bone_count,
                                               full_body_blend_weight,
                                               false,
                                               handle.archetype->species),
                           entry->skinned_bone_count);
    }
  }
  if (upper_body_overlay != nullptr && upper_body_overlay->valid() &&
      upper_body_overlay_weight > 0.0F) {
    const QMatrix4x4* primary_palette = cmd.bone_palette;
    std::shared_ptr<
        std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>
        overlay_interpolated_palette;
    if (const QMatrix4x4* secondary_palette =
            interpolated_palette_for_playback(*entry,
                                              *upper_body_overlay,
                                              handle.archetype->species,
                                              overlay_interpolated_palette);
        primary_palette != nullptr && secondary_palette != nullptr) {
      attach_owned_palette(cmd,
                           blend_palette_owned(primary_palette,
                                               secondary_palette,
                                               cmd.bone_count,
                                               upper_body_overlay_weight,
                                               true,
                                               handle.archetype->species),
                           cmd.bone_count);
    }
  }
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
  return blob->clip(clip_id).name ==
         Animation::humanoid_idle_variant_clip_name(clip_variant);
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
    if (req.lod == CreatureLOD::Billboard) {
      return;
    }

    const auto species_kind = handle->archetype->species;
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
      float contact_y = contact_y_for_playback(species_kind, primary);
      if (req.full_body_blend.active() && full_body.valid()) {
        float const secondary_contact = contact_y_for_playback(species_kind, full_body);
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
    const bool prebaked_lowpoly_required =
        req.lod == CreatureLOD::Minimal && handle->requires_prebaked_minimal_snapshot;
    bool const has_dynamic_layers =
        (req.full_body_blend.active() && full_body.valid()) ||
        (req.upper_body_overlay.active() && overlay.valid());
    if (req.full_body_blend.active() && full_body.valid()) {
      ++stats.full_body_blend_requests;
    }
    if (req.upper_body_overlay.active() && overlay.valid()) {
      ++stats.upper_body_overlay_requests;
    }

    bool const full_lod_needs_frame_interpolation =
        req.lod == CreatureLOD::Full && primary.frame_lerp > 1.0e-4F;
    if (primary.snapshot && (!has_dynamic_layers || req.lod != CreatureLOD::Full) &&
        !full_lod_needs_frame_interpolation) {
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
                                   req.role_colors_view(),
                                   static_cast<std::uint16_t>(req.variant),
                                   req.base_color,
                                   req.wear_params,
                                   draw_world,
                                   *snapshot_playback.blob,
                                   snapshot_playback.global_frame,
                                   snapshot_playback.frame_in_clip,
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
                           req.role_colors_view(),
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
