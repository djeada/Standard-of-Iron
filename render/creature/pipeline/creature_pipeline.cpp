#include "creature_pipeline.h"

#include "../../bone_palette_arena.h"
#include "../../entity/registry.h"
#include "../../equipment/equipment_submit.h"
#include "../../scene_renderer.h"
#include "../../submitter.h"
#include "../skeleton.h"
#include "creature_asset.h"

#include <algorithm>
#include <array>

namespace Render::Creature::Pipeline {

auto pick_lod(const FrameContext &ctx, float distance) noexcept -> CreatureLOD {
  if (distance < ctx.view_distance_full) {
    return CreatureLOD::Full;
  }
  if (distance < ctx.view_distance_reduced) {
    return CreatureLOD::Reduced;
  }
  if (distance < ctx.view_distance_minimal) {
    return CreatureLOD::Minimal;
  }
  return CreatureLOD::Billboard;
}

auto compose_equipment_world(
    const QMatrix4x4 &unit_world, const EquipmentRecord &record,
    const Render::Creature::SkeletonTopology *topology,
    std::span<const QMatrix4x4> bone_palette) noexcept -> QMatrix4x4 {
  if (topology != nullptr &&
      record.socket != Render::Creature::kInvalidSocket &&
      !bone_palette.empty()) {
    const QMatrix4x4 socket = Render::Creature::socket_transform(
        *topology, bone_palette, record.socket);
    return unit_world * socket * record.local_offset;
  }
  return unit_world * record.local_offset;
}

void CreaturePipeline::gather(const Engine::Core::World &, const FrameContext &,
                              std::span<const UnitVisualSpec>,
                              CreatureFrame &) const {}

auto CreaturePipeline::process(const FrameContext &ctx,
                               std::span<const UnitVisualSpec> specs,
                               CreatureFrame &frame) const -> SubmitStats {
  SubmitStats stats{};
  if (frame.empty()) {
    return stats;
  }

  const auto n = frame.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto sid = frame.spec_id[i];
    if (sid >= specs.size()) {
      continue;
    }
    const auto &spec = specs[sid];

    if (spec.kind == CreatureKind::Humanoid) {

      if (spec.pose_hook != nullptr && i < frame.humanoid_anim.size() &&
          i < frame.humanoid_pose.size()) {

        Render::GL::DrawContext synth_ctx{};
        spec.pose_hook(synth_ctx, frame.humanoid_anim[i], frame.seed[i],
                       frame.humanoid_pose[i]);
      }
    }

    switch (frame.lod[i]) {
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

  (void)ctx;
  return stats;
}

namespace {

auto resolve_tint_from_roles(
    TintRole role, std::span<const QVector3D> role_colors,
    const Render::GL::HumanoidPalette &fallback) noexcept -> QVector3D {

  const std::size_t idx = (role == TintRole::TeamTint)
                              ? std::size_t{0}
                              : static_cast<std::size_t>(role) - 1;

  if (idx < role_colors.size()) {
    return role_colors[idx];
  }

  switch (role) {
  case TintRole::Cloth:
    return fallback.cloth;
  case TintRole::Skin:
    return fallback.skin;
  case TintRole::Leather:
    return fallback.leather;
  case TintRole::LeatherDark:
    return fallback.leather_dark;
  case TintRole::Wood:
    return fallback.wood;
  case TintRole::Metal:
    return fallback.metal;
  case TintRole::TeamTint:
    return fallback.cloth;
  case TintRole::None:
    break;
  }
  return {};
}

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

void submit_equipment_loadout(const UnitVisualSpec &spec,
                              const CreatureFrame &frame, std::size_t i,
                              const QMatrix4x4 &world,
                              const Render::GL::HumanoidVariant &variant,
                              const Render::GL::HumanoidPose &pose,
                              const Render::GL::HumanoidAnimationContext &anim,
                              Render::GL::ISubmitter &out, SubmitStats &stats) {
  auto loadout = spec.equipment;
  if (loadout.empty() && spec.equipment_registry_id != kInvalidSpec) {
    loadout = EquipmentRegistry::instance().get(spec.equipment_registry_id);
  }
  if (loadout.empty()) {
    return;
  }

  LegacyEquipmentSubmitArgs legacy_args{};
  legacy_args.world_matrix = &world;
  legacy_args.variant = &variant;
  legacy_args.pose = &pose;
  legacy_args.anim = &anim;
  legacy_args.seed = frame.seed[i];

  if (i < frame.legacy_ctx.size()) {
    legacy_args.ctx = frame.legacy_ctx[i];
  }
  if (i < frame.horse_frames.size()) {
    legacy_args.horse_frames = frame.horse_frames[i];
  }
  if (i < frame.horse_variant.size()) {
    legacy_args.horse_variant = &frame.horse_variant[i];
  }
  if (i < frame.horse_anim.size()) {
    legacy_args.horse_anim = &frame.horse_anim[i];
  }

  EquipmentSubmitContext sub_ctx{};
  sub_ctx.entity_id = static_cast<std::uint32_t>(i);
  sub_ctx.seed = frame.seed[i];
  sub_ctx.ctx = legacy_args.ctx;
  sub_ctx.frames = &pose.body_frames;
  sub_ctx.palette = &variant.palette;
  sub_ctx.anim = &anim;
  sub_ctx.pose = &pose;
  sub_ctx.horse_frames = legacy_args.horse_frames;
  sub_ctx.horse_variant = legacy_args.horse_variant;
  sub_ctx.horse_anim = legacy_args.horse_anim;

  for (const auto &record : loadout) {
    ++stats.equipment_submitted;

    if (record.dispatch) {
      Render::GL::EquipmentBatch batch;
      record.dispatch(sub_ctx, batch);
      Render::GL::submit_equipment_batch(batch, out);
      continue;
    }

    if (record.legacy_submit != nullptr) {
      record.legacy_submit(record.legacy_user, legacy_args, out);
      continue;
    }

    Render::GL::Mesh *mesh = record.static_mesh;
    if (record.mesh_provider != nullptr) {
      EquipmentMeshArgs args{};
      args.ctx = nullptr;
      args.socket_transform = nullptr;
      args.seed = frame.seed[i];
      args.size = record.size;
      mesh = record.mesh_provider(args);
    }
    if (mesh == nullptr) {
      continue;
    }

    const QMatrix4x4 model = compose_equipment_world(world, record);

    QVector3D color = record.override_color;
    if (record.tint_role != TintRole::None) {
      const std::uint32_t rc_count =
          (i < frame.role_color_count.size()) ? frame.role_color_count[i] : 0U;
      const std::span<const QVector3D> roles =
          (i < frame.role_colors.size())
              ? std::span<const QVector3D>(frame.role_colors[i].data(),
                                           rc_count)
              : std::span<const QVector3D>{};
      color = resolve_tint_from_roles(record.tint_role, roles, variant.palette);
    }

    out.mesh(mesh, model, color, nullptr, 1.0F,
             static_cast<int>(record.material_id));
  }
}

void submit_rigged_creature(const CreatureAsset &asset,
                            const void *species_pose, CreatureLOD lod,
                            std::span<const QVector3D> role_colors,
                            std::uint16_t variant_bucket,
                            const QVector3D &base_color,
                            const QMatrix4x4 &world_from_unit,
                            Render::GL::ISubmitter &out) {
  if (lod == CreatureLOD::Billboard || asset.spec == nullptr ||
      asset.compute_bones == nullptr || asset.bind_palette == nullptr) {
    return;
  }

  std::array<QMatrix4x4, kMaxCreatureBones> current_palette{};
  const auto bone_count = asset.compute_bones(
      species_pose,
      std::span<QMatrix4x4>(current_palette.data(), asset.max_bones));

  auto *renderer = resolve_renderer(out);
  if (renderer == nullptr) {
    Render::Creature::submit_creature(
        *asset.spec,
        std::span<const QMatrix4x4>(current_palette.data(), bone_count), lod,
        world_from_unit, out, role_colors);
    return;
  }

  auto bind = asset.bind_palette();
  auto *entry = renderer->rigged_mesh_cache().get_or_bake(*asset.spec, lod,
                                                          bind, variant_bucket);
  if (entry == nullptr || entry->mesh == nullptr ||
      entry->mesh->index_count() == 0U) {
    return;
  }

  auto &arena = renderer->bone_palette_arena();
  Render::GL::BonePaletteSlot palette_slot_h = arena.allocate_palette();
  QMatrix4x4 *palette_slot = palette_slot_h.cpu;

  const std::size_t n =
      std::min<std::size_t>(entry->inverse_bind.size(), bone_count);
  for (std::size_t i = 0; i < n; ++i) {
    palette_slot[i] = current_palette[i] * entry->inverse_bind[i];
  }

  Render::GL::RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.world = world_from_unit;
  cmd.bone_palette = palette_slot;
  cmd.palette_ubo = palette_slot_h.ubo;
  cmd.palette_offset = static_cast<std::uint32_t>(palette_slot_h.offset);
  cmd.bone_count = static_cast<std::uint32_t>(n);
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

const void *resolve_species_pose(const CreatureFrame &frame, std::size_t i,
                                 CreatureKind kind) {
  switch (kind) {
  case CreatureKind::Humanoid:
    return (i < frame.humanoid_pose.size()) ? &frame.humanoid_pose[i] : nullptr;
  case CreatureKind::Horse:
    return (i < frame.horse_pose.size()) ? &frame.horse_pose[i] : nullptr;
  case CreatureKind::Elephant:
    return (i < frame.elephant_pose.size()) ? &frame.elephant_pose[i] : nullptr;
  case CreatureKind::Mounted:
    return nullptr;
  }
  return nullptr;
}

const void *resolve_species_variant(const CreatureFrame &frame, std::size_t i,
                                    CreatureKind kind) {
  switch (kind) {
  case CreatureKind::Humanoid:
    return (i < frame.humanoid_variant.size()) ? &frame.humanoid_variant[i]
                                               : nullptr;
  case CreatureKind::Horse:
    return (i < frame.horse_variant.size()) ? &frame.horse_variant[i] : nullptr;
  case CreatureKind::Elephant:
    return (i < frame.elephant_variant.size()) ? &frame.elephant_variant[i]
                                               : nullptr;
  case CreatureKind::Mounted:
    return nullptr;
  }
  return nullptr;
}

auto resolve_base_color(const CreatureFrame &frame,
                        std::size_t i) -> QVector3D {
  if (i < frame.base_color.size()) {
    return frame.base_color[i];
  }
  return QVector3D(0.5F, 0.5F, 0.5F);
}

void resolve_role_colors(const CreatureFrame &frame, std::size_t i,
                         CreatureKind kind, const CreatureAsset *asset,
                         RoleColorArray &fallback,
                         std::span<const QVector3D> &out_roles) {

  if (i < frame.role_color_count.size() && i < frame.role_colors.size() &&
      frame.role_color_count[i] > 0U) {
    out_roles = std::span<const QVector3D>(frame.role_colors[i].data(),
                                           frame.role_color_count[i]);
    return;
  }

  if (asset != nullptr && asset->fill_role_colors != nullptr) {
    const void *variant = resolve_species_variant(frame, i, kind);
    if (variant != nullptr) {
      const auto count =
          asset->fill_role_colors(variant, fallback.data(), fallback.size());
      out_roles = std::span<const QVector3D>(fallback.data(), count);
      return;
    }
  }
}

void submit_creature_row(const UnitVisualSpec &spec, const CreatureFrame &frame,
                         std::size_t i, CreatureKind kind,
                         Render::GL::ISubmitter &out, SubmitStats &stats) {
  const auto &world = frame.world_matrix[i];

  const auto *asset =
      (i < frame.creature_asset_id.size() &&
       frame.creature_asset_id[i] != kInvalidCreatureAsset)
          ? CreatureAssetRegistry::instance().get(frame.creature_asset_id[i])
          : CreatureAssetRegistry::instance().resolve(spec);

  RoleColorArray fallback_roles{};
  std::span<const QVector3D> role_colors{};
  resolve_role_colors(frame, i, kind, asset, fallback_roles, role_colors);

  const std::uint16_t variant_bucket =
      (i < frame.variant_bucket.size()) ? frame.variant_bucket[i] : 0U;

  const void *species_pose = resolve_species_pose(frame, i, kind);
  const QVector3D base = resolve_base_color(frame, i);

  if (asset != nullptr && species_pose != nullptr) {
    submit_rigged_creature(*asset, species_pose, frame.lod[i], role_colors,
                           variant_bucket, base, world, out);
  }
  ++stats.entities_submitted;

  const auto &h_pose =
      (kind == CreatureKind::Humanoid && i < frame.humanoid_pose.size())
          ? frame.humanoid_pose[i]
          : Render::GL::HumanoidPose{};
  const auto &h_variant =
      (kind == CreatureKind::Humanoid && i < frame.humanoid_variant.size())
          ? frame.humanoid_variant[i]
          : Render::GL::HumanoidVariant{};
  const auto &h_anim =
      (kind == CreatureKind::Humanoid && i < frame.humanoid_anim.size())
          ? frame.humanoid_anim[i]
          : Render::GL::HumanoidAnimationContext{};
  submit_equipment_loadout(spec, frame, i, world, h_variant, h_pose, h_anim,
                           out, stats);
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

} // namespace

auto CreaturePipeline::submit(
    const FrameContext &, std::span<const UnitVisualSpec> specs,
    const CreatureFrame &frame,
    Render::GL::ISubmitter &out) const -> SubmitStats {
  SubmitStats stats{};
  if (frame.empty()) {
    return stats;
  }

  const auto n = frame.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto sid = frame.spec_id[i];
    if (sid >= specs.size()) {
      ++stats.entities_submitted;
      continue;
    }
    const auto &spec = specs[sid];

    const CreatureKind kind =
        (i < frame.render_kind.size()) ? frame.render_kind[i] : spec.kind;

    if (kind == CreatureKind::Mounted) {
      continue;
    }

    submit_creature_row(spec, frame, i, kind, out, stats);
    bump_lod_counters(frame.lod[i], stats);
  }

  return stats;
}

} // namespace Render::Creature::Pipeline
