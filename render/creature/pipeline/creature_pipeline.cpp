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
#include <unordered_map>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_reader.h"
#include "animation/bpat/bpat_registry.h"
#include "animation/clip_manifest.h"
#include "creature_asset.h"
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
#include "render/humanoid/humanoid_spec.h"
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

auto species_to_bpat_id(CreatureKind kind) noexcept -> std::uint32_t {
  switch (kind) {
  case CreatureKind::Humanoid:
    return Render::Creature::Bpat::k_species_humanoid;
  case CreatureKind::Horse:
    return Render::Creature::Bpat::k_species_horse;
  case CreatureKind::Elephant:
    return Render::Creature::Bpat::k_species_elephant;
  case CreatureKind::Sheep:
    return Render::Creature::Bpat::k_species_sheep;
  case CreatureKind::Wolf:
    return Render::Creature::Bpat::k_species_wolf;
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

auto affine_inverse(const QMatrix4x4& m) noexcept -> QMatrix4x4 {
  const float* d = m.constData();
  float const a00 = d[0];
  float const a10 = d[1];
  float const a20 = d[2];
  float const a01 = d[4];
  float const a11 = d[5];
  float const a21 = d[6];
  float const a02 = d[8];
  float const a12 = d[9];
  float const a22 = d[10];
  float const tx = d[12];
  float const ty = d[13];
  float const tz = d[14];

  float const c00 = (a11 * a22) - (a12 * a21);
  float const c10 = (a12 * a20) - (a10 * a22);
  float const c20 = (a10 * a21) - (a11 * a20);
  float const det = (a00 * c00) + (a01 * c10) + (a02 * c20);
  if (std::abs(det) < 1.0e-12F) {
    return m.inverted();
  }
  float const inv_det = 1.0F / det;

  float const i00 = c00 * inv_det;
  float const i01 = ((a02 * a21) - (a01 * a22)) * inv_det;
  float const i02 = ((a01 * a12) - (a02 * a11)) * inv_det;
  float const i10 = c10 * inv_det;
  float const i11 = ((a00 * a22) - (a02 * a20)) * inv_det;
  float const i12 = ((a02 * a10) - (a00 * a12)) * inv_det;
  float const i20 = c20 * inv_det;
  float const i21 = ((a01 * a20) - (a00 * a21)) * inv_det;
  float const i22 = ((a00 * a11) - (a01 * a10)) * inv_det;

  return {i00,
          i01,
          i02,
          -((i00 * tx) + (i01 * ty) + (i02 * tz)),
          i10,
          i11,
          i12,
          -((i10 * tx) + (i11 * ty) + (i12 * tz)),
          i20,
          i21,
          i22,
          -((i20 * tx) + (i21 * ty) + (i22 * tz)),
          0.0F,
          0.0F,
          0.0F,
          1.0F};
}

auto matrix_rotation_quaternion(const QMatrix4x4& matrix) noexcept -> QQuaternion {
  QMatrix3x3 basis;
  for (int col = 0; col < 3; ++col) {
    QVector3D axis = matrix.column(col).toVector3D();
    if (axis.lengthSquared() > 1.0e-8F) {
      axis.normalize();
    }
    basis(0, col) = axis.x();
    basis(1, col) = axis.y();
    basis(2, col) = axis.z();
  }
  return QQuaternion::fromRotationMatrix(basis).normalized();
}

auto rigid_lerp_matrix(const QMatrix4x4& a,
                       const QMatrix4x4& b,
                       float t) noexcept -> QMatrix4x4 {
  float const weight = std::clamp(t, 0.0F, 1.0F);
  if (weight <= 1.0e-4F) {
    return a;
  }
  if (weight >= 1.0F - 1.0e-4F) {
    return b;
  }

  QVector3D const translation =
      a.column(3).toVector3D() * (1.0F - weight) + b.column(3).toVector3D() * weight;
  QMatrix4x4 out;
  out.translate(translation);
  out.rotate(QQuaternion::slerp(
      matrix_rotation_quaternion(a), matrix_rotation_quaternion(b), weight));
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
  if (species_kind == CreatureKind::Humanoid) {

    auto const bind = Render::Humanoid::humanoid_bind_palette();
    auto const inverse_bind = Render::Humanoid::humanoid_inverse_bind_palette();
    std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>
        primary_global{};
    std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>
        secondary_global{};
    std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>
        result_global{};

    std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>
        primary_parent_inverse{};
    std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>
        secondary_parent_inverse{};
    std::array<bool, Render::GL::RiggedCreatureCmd::k_max_owned_bones>
        parent_inverse_ready{};

    std::uint32_t const count =
        std::min(bone_count,
                 std::min(static_cast<std::uint32_t>(bind.size()),
                          static_cast<std::uint32_t>(
                              Render::GL::RiggedCreatureCmd::k_max_owned_bones)));
    for (std::uint32_t bone = 0; bone < count; ++bone) {
      primary_global[bone] = primary_palette[bone] * bind[bone];
      secondary_global[bone] = secondary_palette[bone] * bind[bone];
    }
    for (std::uint32_t bone = 0; bone < count; ++bone) {
      bool const blend_bone = !upper_body_only || is_humanoid_upper_body_bone(bone);
      if (!blend_bone) {
        result_global[bone] = primary_global[bone];
      } else {
        auto const parent = Render::Humanoid::k_bone_parents[bone];
        if (parent == Render::Humanoid::k_invalid_bone || parent >= count) {
          result_global[bone] =
              rigid_lerp_matrix(primary_global[bone], secondary_global[bone], weight);
        } else {
          if (!parent_inverse_ready[parent]) {
            primary_parent_inverse[parent] = affine_inverse(primary_global[parent]);
            secondary_parent_inverse[parent] = affine_inverse(secondary_global[parent]);
            parent_inverse_ready[parent] = true;
          }
          QMatrix4x4 const primary_local =
              primary_parent_inverse[parent] * primary_global[bone];
          QMatrix4x4 const secondary_local =
              secondary_parent_inverse[parent] * secondary_global[bone];
          result_global[bone] =
              result_global[parent] *
              rigid_lerp_matrix(primary_local, secondary_local, weight);
        }
      }
      (*owned)[bone] = result_global[bone] * inverse_bind[bone];
    }
    for (std::uint32_t bone = count;
         bone < bone_count && bone < Render::GL::RiggedCreatureCmd::k_max_owned_bones;
         ++bone) {
      (*owned)[bone] = primary_palette[bone];
    }
    return owned;
  }
  for (std::uint32_t bone = 0;
       bone < bone_count && bone < Render::GL::RiggedCreatureCmd::k_max_owned_bones;
       ++bone) {
    bool const apply_secondary = !upper_body_only;
    (*owned)[bone] =
        apply_secondary
            ? rigid_lerp_matrix(primary_palette[bone], secondary_palette[bone], weight)
            : primary_palette[bone];
  }
  return owned;
}

struct BlendCacheKey {
  const Render::GL::RiggedMeshEntry* entry{nullptr};
  std::uint32_t frame_a{0};
  std::uint32_t frame_b{0};
  std::uint32_t weight_bucket{0};

  auto operator==(const BlendCacheKey& other) const noexcept -> bool {
    return entry == other.entry && frame_a == other.frame_a &&
           frame_b == other.frame_b && weight_bucket == other.weight_bucket;
  }
};

struct BlendCacheKeyHash {
  auto operator()(const BlendCacheKey& key) const noexcept -> std::size_t {
    std::size_t hash = std::hash<const void*>{}(key.entry);
    hash ^= key.frame_a * 0x9E3779B1U + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
    hash ^= key.frame_b * 0x85EBCA77U + 0xC2B2AE3DU + (hash << 6U) + (hash >> 2U);
    hash ^= key.weight_bucket * 0x27D4EB2FU + (hash << 6U) + (hash >> 2U);
    return hash;
  }
};

constexpr std::uint32_t k_blend_weight_buckets = 32U;

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

  auto bucket = static_cast<std::uint32_t>(frame_lerp *
                                           static_cast<float>(k_blend_weight_buckets));
  bucket = std::min(bucket, k_blend_weight_buckets - 1U);
  float const bucket_weight =
      (static_cast<float>(bucket) + 0.5F) / static_cast<float>(k_blend_weight_buckets);

  BlendCacheKey const key{
      &entry, playback.global_frame, playback.next_global_frame, bucket};

  using OwnedPalette = std::shared_ptr<
      std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>;
  thread_local std::unordered_map<BlendCacheKey, OwnedPalette, BlendCacheKeyHash> cache;
  thread_local std::uint32_t cache_frame = std::numeric_limits<std::uint32_t>::max();
  if (std::uint32_t const now = Render::GL::humanoid_current_frame();
      now != cache_frame) {
    cache.clear();
    cache_frame = now;
  }

  if (auto it = cache.find(key); it != cache.end()) {
    owned_palette = it->second;
    return owned_palette ? owned_palette->data() : current;
  }

  owned_palette = blend_palette_owned(
      current, next, entry.skin_atlas->bone_count, bucket_weight, false, species_kind);
  cache.emplace(key, owned_palette);
  return owned_palette ? owned_palette->data() : current;
}

auto clip_supplies_ground_contact(const Render::Creature::Bpat::BpatBlob* blob,
                                  std::uint16_t clip_id) noexcept -> bool {
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return false;
  }
  auto const name = blob->clip(clip_id).name;
  return !name.starts_with("riding_") && !name.starts_with("showcase_");
}

auto contact_y_for_playback(CreatureKind species_kind,
                            const ResolvedRequestPlayback& playback) noexcept -> float {
  if (playback.blob == nullptr) {
    return 0.0F;
  }
  if (!clip_supplies_ground_contact(playback.blob, playback.clip_id)) {
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

  std::shared_ptr<
      std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>
      primary_interpolated_palette;
  const QMatrix4x4* frame_palette =
      use_resident_frames
          ? frame_palette_for_global_frame(*entry, primary_playback.global_frame)
          : interpolated_palette_for_playback(*entry,
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
  if (primary_interpolated_palette) {
    attach_owned_palette(
        cmd, std::move(primary_interpolated_palette), skin_atlas->bone_count);
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
                                               skin_atlas->bone_count,
                                               full_body_blend_weight,
                                               false,
                                               handle.archetype->species),
                           skin_atlas->bone_count);
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
  if (lod == CreatureLOD::Billboard || asset == nullptr || asset->spec == nullptr ||
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
      QVector3D const pelvis_world =
          world_from_unit.map(palette[pelvis_index].column(3).toVector3D());
      QVector3D const neck_world =
          world_from_unit.map(palette[neck_index].column(3).toVector3D());
      QVector3D const visible_torso = neck_world - pelvis_world;
      float const body_up_y = visible_torso.lengthSquared() > 1.0e-8F
                                  ? visible_torso.normalized().y()
                                  : 1.0F;
      float const max_arm_reach =
          std::max((palette[hand_l_index].column(3).toVector3D() -
                    palette[shoulder_l_index].column(3).toVector3D())
                       .length(),
                   (palette[hand_r_index].column(3).toVector3D() -
                    palette[shoulder_r_index].column(3).toVector3D())
                       .length());
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
