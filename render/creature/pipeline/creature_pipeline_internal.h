#pragma once

// Shared by the creature_pipeline*.cpp translation units; not a public header.

#include <QQuaternion>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_reader.h"
#include "animation/bpat/bpat_registry.h"
#include "animation/clip_manifest.h"
#include "game/map/terrain_service.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/combat_animation_diagnostics.h"
#include "render/creature/gpu/rigged_mesh_cache.h"
#include "render/creature/gpu/snapshot_mesh_cache.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/creature/pipeline/creature_bone_probe.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_submission_context.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/species_contract.h"
#include "render/creature/runtime_bake_guard.h"
#include "render/creature/runtime_frame.h"
#include "render/creature/skeleton.h"
#include "render/creature/skeleton_blend_profile.h"
#include "render/creature/snapshot_mesh_registry.h"
#include "render/creature/spec.h"
#include "render/frame/bone_palette_arena.h"
#include "render/frame/draw_context.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/submitter.h"
#include "render/profiling/asset_counters.h"
#include "render/profiling/frame_profile.h"

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
                              std::uint64_t attachments_hash);

auto rigged_cache_for(const CreatureSubmissionContext* submission)
    -> Render::GL::RiggedMeshCache&;

auto make_snapshot_key(const CreatureRenderAssetHandle& handle,
                       ArchetypeId archetype,
                       VariantId variant,
                       AnimationStateId state,
                       std::uint16_t clip_id,
                       std::uint8_t clip_variant,
                       std::uint32_t frame_in_clip) noexcept
    -> Render::GL::SnapshotMeshCache::Key;

auto material_id_for_species(CreatureKind species) noexcept -> std::int32_t;

auto make_rigged_cmd(Render::GL::RiggedMesh* mesh,
                     const QMatrix4x4& world_from_unit,
                     const QMatrix4x4* bone_palette,
                     std::uint32_t bone_count,
                     std::shared_ptr<const Render::RoleColorPalette> role_colors,
                     const QVector3D& base_color,
                     const QVector4D& wear_params,
                     std::int32_t material_id,
                     float alpha,
                     float team_emphasis) -> Render::GL::RiggedCreatureCmd;

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
    -> const CreatureRenderAssetHandle*;

auto resolve_request_playback(const CreatureRenderAssetHandle& primary_handle,
                              CreatureAssetId asset_id,
                              ArchetypeId archetype,
                              AnimationStateId state,
                              float phase,
                              std::uint8_t clip_variant,
                              std::uint16_t explicit_clip_id) noexcept
    -> ResolvedRequestPlayback;

auto frame_palette_for_global_frame(const Render::GL::RiggedMeshEntry& entry,
                                    std::uint32_t global_frame) noexcept
    -> const QMatrix4x4*;

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

constexpr std::uint32_t k_frame_lerp_buckets = 32U;

constexpr std::uint32_t k_layer_weight_buckets = 256U;

auto blend_weight_bucket(float weight,
                         std::uint32_t buckets = k_frame_lerp_buckets) noexcept
    -> std::uint32_t;

auto blend_bucket_weight(std::uint32_t bucket,
                         std::uint32_t buckets = k_frame_lerp_buckets) noexcept
    -> float;

using OwnedPalette = std::shared_ptr<BonePaletteArray>;

using LocalPose = std::array<Render::Creature::Bpat::LocalBonePose,
                             Render::GL::RiggedCreatureCmd::k_max_owned_bones>;

auto blends_bone(const Render::Creature::SkeletonBlendProfile* profile,
                 std::size_t bone,
                 bool upper_body_only) noexcept -> bool;

auto shortest_path_flips(const LocalPose& base,
                         std::span<const Render::Creature::Bpat::LocalBonePose> layer,
                         std::uint32_t bone_count) noexcept -> std::uint64_t;

auto slerp_in_hemisphere(const QQuaternion& from,
                         const QQuaternion& to,
                         float weight,
                         bool flip) noexcept -> QQuaternion;

void lerp_local_pose(LocalPose& io,
                     std::span<const Render::Creature::Bpat::LocalBonePose> layer,
                     std::uint32_t bone_count,
                     float weight,
                     const Render::Creature::SkeletonBlendProfile* blend_profile,
                     bool upper_body_only,
                     std::uint64_t flipped_bones) noexcept;

void lerp_local_pose_by_region(
    LocalPose& io,
    std::span<const Render::Creature::Bpat::LocalBonePose> layer,
    std::uint32_t bone_count,
    float upper_body_weight,
    float lower_body_weight,
    const Render::Creature::SkeletonBlendProfile* blend_profile,
    std::uint64_t flipped_bones) noexcept;

auto sample_local_pose(const Render::Creature::Bpat::BpatBlob& blob,
                       const ResolvedRequestPlayback& playback,
                       std::uint32_t bone_count,
                       LocalPose& out) noexcept -> bool;

auto skin_palette_from_local_pose(const Render::Creature::Bpat::BpatBlob& blob,
                                  const LocalPose& pose,
                                  std::uint32_t bone_count) -> OwnedPalette;

struct PaletteBlendKey {
  const Render::GL::RiggedMeshEntry* entry{nullptr};
  std::array<std::uint32_t, 12> frames{};
  std::array<std::uint32_t, 16> buckets{};
  std::array<std::uint64_t, 2> flipped_bones{};

  auto operator==(const PaletteBlendKey& other) const noexcept -> bool {
    return entry == other.entry && frames == other.frames && buckets == other.buckets &&
           flipped_bones == other.flipped_bones;
  }
};

class PaletteBlendCache {
public:
  template <typename Compute>
  auto get_or_compute(const PaletteBlendKey& key, Compute&& compute) -> OwnedPalette {
    if (std::uint32_t const now = Render::Creature::creature_frame_serial();
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
    for (auto const v : key.flipped_bones) {
      hash ^= std::hash<std::uint64_t>{}(v) + 0x9E3779B9U + (hash << 6U) + (hash >> 2U);
    }
    return hash;
  }

  std::array<Slot, k_slots> m_slots{};
  std::uint32_t m_frame{std::numeric_limits<std::uint32_t>::max()};
};

auto blend_cache() -> PaletteBlendCache&;

class BlendHemisphereTable {
public:
  auto flipped_bones(std::uint64_t layer_key,
                     std::uint16_t clip_id,
                     const LocalPose& base,
                     const LocalPose& layer,
                     std::uint32_t bone_count) -> std::uint64_t {
    std::uint32_t const now = Render::Creature::creature_frame_serial();
    std::uint32_t const generation = Render::Creature::creature_runtime_generation();
    std::lock_guard<std::mutex> const lock(m_mutex);
    if (m_layers.size() > k_max_layers) {
      std::erase_if(m_layers, [now](const auto& item) {
        return now - item.second.frame > k_stale_frames;
      });
    }
    auto& state = m_layers[layer_key];
    bool const continues = state.valid && state.generation == generation &&
                           state.clip_id == clip_id &&
                           now - state.frame <= k_continuity_frames;
    std::uint64_t flipped = 0ULL;
    if (continues) {
      for (std::uint32_t bone = 0; bone < bone_count; ++bone) {
        bool const base_crossed =
            QQuaternion::dotProduct(base[bone].rotation, state.base[bone]) < 0.0F;
        bool const layer_crossed =
            QQuaternion::dotProduct(layer[bone].rotation, state.layer[bone]) < 0.0F;
        bool const was_flipped = ((state.flipped >> bone) & 1ULL) != 0ULL;
        if (was_flipped != (base_crossed != layer_crossed)) {
          flipped |= 1ULL << bone;
        }
      }
    } else {
      flipped = shortest_path_flips(base, layer, bone_count);
    }
    for (std::uint32_t bone = 0; bone < bone_count; ++bone) {
      state.base[bone] = base[bone].rotation;
      state.layer[bone] = layer[bone].rotation;
    }
    state.valid = true;
    state.clip_id = clip_id;
    state.frame = now;
    state.generation = generation;
    state.flipped = flipped;
    return flipped;
  }

private:
  static constexpr std::size_t k_max_layers = 16384U;
  static constexpr std::uint32_t k_stale_frames = 240U;
  static constexpr std::uint32_t k_continuity_frames = 2U;

  struct LayerState {
    bool valid{false};
    std::uint16_t clip_id{0U};
    std::uint32_t frame{0U};
    std::uint32_t generation{0U};
    std::uint64_t flipped{0ULL};
    std::array<QQuaternion, Render::GL::RiggedCreatureCmd::k_max_owned_bones> base{};
    std::array<QQuaternion, Render::GL::RiggedCreatureCmd::k_max_owned_bones> layer{};
  };

  std::mutex m_mutex;
  std::unordered_map<std::uint64_t, LayerState> m_layers;
};

auto blend_hemispheres() -> BlendHemisphereTable&;

auto contact_y_for_playback(const ResolvedRequestPlayback& playback) noexcept -> float;

auto bone_global_at(const ResolvedRequestPlayback& playback,
                    std::uint16_t bone_index) -> QMatrix4x4;

void resolve_bone_probe(const Render::Creature::CreatureRenderRequest& req,
                        const QMatrix4x4& draw_world,
                        const ResolvedRequestPlayback& playback);

void attach_owned_palette(
    Render::GL::RiggedCreatureCmd& cmd,
    std::shared_ptr<
        std::array<QMatrix4x4, Render::GL::RiggedCreatureCmd::k_max_owned_bones>>
        owned_palette,
    std::uint32_t bone_count);

struct ResolvedTransition {
  std::array<ResolvedRequestPlayback, Render::Creature::k_transition_source_count>
      sources{};
  std::array<float, Render::Creature::k_transition_source_count> upper_shares{};
  std::array<float, Render::Creature::k_transition_source_count> lower_shares{};
  float upper_body_weight{0.0F};
  float lower_body_weight{0.0F};

  [[nodiscard]] auto active() const noexcept -> bool {
    return sources[0].valid() && std::max(upper_body_weight, lower_body_weight) > 0.0F;
  }
};

void blend_transition_into(LocalPose& pose,
                           const Render::Creature::Bpat::BpatBlob& blob,
                           const ResolvedTransition& transition,
                           float upper_body_weight,
                           float lower_body_weight,
                           std::uint32_t bone_count,
                           const Render::Creature::SkeletonBlendProfile* blend_profile);

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
                            const CreatureSubmissionContext* submission);

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
    bool allow_bake_fallback = true) -> bool;

void bump_lod_counters(CreatureLOD lod, SubmitStats& stats);

void publish_submit_stats(const SubmitStats& stats) noexcept;

} // namespace creature_pipeline_detail

} // namespace Render::Creature::Pipeline
