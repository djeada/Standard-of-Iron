#include "creature_pipeline.h"

#include "../../../game/map/terrain_service.h"
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
#include "../snapshot_mesh_registry.h"
#include "../spec.h"
#include "creature_asset.h"
#include "preparation_common.h"

#include "../../humanoid/skeleton.h"

#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <unordered_map>

namespace Render::Creature::Pipeline {

namespace {

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

auto adjust_world_to_palette_contact(
    const QMatrix4x4 &world_from_unit, CreatureKind kind,
    std::span<const QMatrix4x4> palette) noexcept -> QMatrix4x4 {
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
  auto *entry = cache.get_or_bake(*asset.spec, lod, bind, variant_bucket,
                                  attachments, blob.species_id());
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
  QMatrix4x4 const &draw_world = world_from_unit;

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.world = draw_world;
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
    Render::Creature::AnimationStateId state, std::uint16_t clip_id,
    std::uint8_t clip_variant, std::span<const QVector3D> role_colors,
    std::uint16_t variant_bucket, const QVector3D &base_color,
    const QMatrix4x4 &world_from_unit,
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

  Render::GL::SnapshotMeshCache::Key key{};
  key.archetype = archetype;
  key.variant = variant;
  key.state = state;
  key.clip_id = clip_id;
  key.clip_variant = clip_variant;
  key.frame_in_clip = frame_in_clip;

  if (attachments.empty() && asset.snapshot_mesh_species_id != 0xFFFFFFFFu &&
      (asset.snapshot_mesh_lod_mask & creature_lod_bit(lod)) != 0U) {
    const auto *mesh_blob =
        Render::Creature::Snapshot::SnapshotMeshRegistry::instance().blob(
            asset.snapshot_mesh_species_id, lod);
    if (mesh_blob != nullptr) {
      std::uint32_t mesh_global_frame = 0U;
      if (mesh_blob->resolve_global_frame(clip_id, frame_in_clip,
                                          mesh_global_frame)) {
        const auto *snap = renderer->snapshot_mesh_cache().get_or_load(
            key, *mesh_blob, mesh_global_frame);
        if (snap != nullptr && snap->mesh != nullptr &&
            snap->mesh->index_count() != 0U) {
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
      }
    }
  }

  auto &rigged_cache = renderer->rigged_mesh_cache();
  auto bind = asset.bind_palette();
  auto *source = rigged_cache.get_or_bake(
      *asset.spec, lod, bind, variant_bucket, attachments, blob.species_id());
  if (source == nullptr || source->mesh == nullptr ||
      source->mesh->index_count() == 0U) {
    return false;
  }

  Render::GL::rigged_entry_ensure_skin_atlas_from_blob(*source, blob);
  if (source->skinned_palettes.empty() || source->skinned_bone_count == 0 ||
      global_frame >= source->skinned_frame_total) {
    return false;
  }

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

auto resolve_blob_palette(std::uint32_t species_id, BpatPlayback playback,
                          const Render::Creature::Bpat::BpatBlob *&out_blob,
                          std::uint32_t &out_global_frame) noexcept
    -> std::span<const QMatrix4x4> {
  out_blob = nullptr;
  out_global_frame = 0U;
  if (playback.clip_id == kInvalidBpatClip) {
    return {};
  }
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

} // namespace

auto CreaturePipeline::submit_requests(
    std::span<const Render::Creature::CreatureRenderRequest> requests,
    Render::GL::ISubmitter &out) const -> SubmitStats {
  SubmitStats stats{};
  if (requests.empty()) {
    return stats;
  }

  const auto &arch_reg = Render::Creature::ArchetypeRegistry::instance();
  const auto &asset_reg = CreatureAssetRegistry::instance();

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
    const auto *asset = (req.creature_asset_id != kInvalidCreatureAsset)
                            ? asset_reg.get(req.creature_asset_id)
                            : asset_reg.for_species(species_kind);
    if (asset == nullptr) {
      return;
    }
    const auto species_id = asset->bpat_species_id;
    if (species_id == 0xFFFFFFFFu) {
      return;
    }

    const std::uint16_t clip_id = arch_reg.bpat_clip(req.archetype, req.state);
    auto const playback = resolve_clip_playback(species_id, clip_id, req.phase);
    if (playback.blob == nullptr) {
      return;
    }

    QMatrix4x4 draw_world = req.world;
    if (!req.world_already_grounded) {
      draw_world = adjust_world_to_palette_contact(
          req.world, species_kind,
          playback.blob->frame_palette_view(playback.global_frame));
    }

    if (arch_reg.is_snapshot(req.archetype, req.state)) {
      const bool emitted = submit_snapshot_creature(
          *asset, req.lod, req.archetype, req.variant, req.state, clip_id,
          req.clip_variant, req.role_colors_view(),
          static_cast<std::uint16_t>(req.variant), req.base_color, draw_world,
          *playback.blob, playback.global_frame, playback.frame_in_clip, out,
          desc->attachments_view());
      if (emitted) {
        return;
      }
    }

    submit_rigged_creature(
        *asset, req.lod, req.role_colors_view(),
        static_cast<std::uint16_t>(req.variant), req.base_color, draw_world,
        *playback.blob, playback.global_frame, out, desc->attachments_view());
  };

  for (const auto &req : requests) {
    emit_request(req);
  }

  return stats;
}

} // namespace Render::Creature::Pipeline
