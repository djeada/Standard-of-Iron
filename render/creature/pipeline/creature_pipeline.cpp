#include "creature_pipeline.h"

#include "../../elephant/elephant_spec.h"
#include "../../entity/registry.h"
#include "../../equipment/equipment_submit.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../submitter.h"
#include "../skeleton.h"

#include <algorithm>

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
    } else if (spec.kind == CreatureKind::Mounted &&
               spec.pose_hook != nullptr && i < frame.humanoid_anim.size() &&
               i < frame.humanoid_pose.size()) {

      Render::GL::DrawContext synth_ctx{};
      spec.pose_hook(synth_ctx, frame.humanoid_anim[i], frame.seed[i],
                     frame.humanoid_pose[i]);
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

void submit_equipment_loadout(const UnitVisualSpec &spec,
                              const CreatureFrame &frame, std::size_t i,
                              const QMatrix4x4 &world,
                              const Render::GL::HumanoidVariant &variant,
                              const Render::GL::HumanoidPose &pose,
                              const Render::GL::HumanoidAnimationContext &anim,
                              Render::GL::ISubmitter &out, SubmitStats &stats) {
  auto loadout = spec.equipment;
  if (loadout.empty()) {
    loadout = EquipmentRegistry::instance().get(frame.spec_id[i]);
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
      const auto &p = variant.palette;
      switch (record.tint_role) {
      case TintRole::Cloth:
        color = p.cloth;
        break;
      case TintRole::Skin:
        color = p.skin;
        break;
      case TintRole::Leather:
        color = p.leather;
        break;
      case TintRole::LeatherDark:
        color = p.leather_dark;
        break;
      case TintRole::Wood:
        color = p.wood;
        break;
      case TintRole::Metal:
        color = p.metal;
        break;
      case TintRole::TeamTint:
        color = p.cloth;
        break;
      case TintRole::None:
        break;
      }
    }

    out.mesh(mesh, model, color, nullptr, 1.0F,
             static_cast<int>(record.material_id));
  }
}

void submit_humanoid_row(const UnitVisualSpec &spec, const CreatureFrame &frame,
                         std::size_t i, Render::GL::ISubmitter &out,
                         SubmitStats &stats) {
  const auto &world = frame.world_matrix[i];
  const auto &pose = frame.humanoid_pose[i];
  const auto &variant = frame.humanoid_variant[i];
  const auto &anim = frame.humanoid_anim[i];

  switch (frame.lod[i]) {
  case CreatureLOD::Full:
    Render::Humanoid::submit_humanoid_full_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Reduced:
    Render::Humanoid::submit_humanoid_reduced_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Minimal:
    Render::Humanoid::submit_humanoid_minimal_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Billboard:

    break;
  }
  ++stats.entities_submitted;
  submit_equipment_loadout(spec, frame, i, world, variant, pose, anim, out,
                           stats);
}

void submit_horse_row(const UnitVisualSpec &spec, const CreatureFrame &frame,
                      std::size_t i, const QMatrix4x4 &world,
                      Render::GL::ISubmitter &out, SubmitStats &stats) {
  const auto &pose = frame.horse_pose[i];
  const auto &variant = frame.horse_variant[i];

  switch (frame.lod[i]) {
  case CreatureLOD::Full:
    Render::Horse::submit_horse_full_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Reduced:
    Render::Horse::submit_horse_reduced_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Minimal:
    Render::Horse::submit_horse_minimal_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Billboard:
    break;
  }
  ++stats.entities_submitted;

  Render::GL::HumanoidPose blank_pose{};
  Render::GL::HumanoidVariant blank_variant{};
  Render::GL::HumanoidAnimationContext blank_anim{};
  submit_equipment_loadout(spec, frame, i, world, blank_variant, blank_pose,
                           blank_anim, out, stats);
}

void submit_elephant_row(const UnitVisualSpec &spec, const CreatureFrame &frame,
                         std::size_t i, const QMatrix4x4 &world,
                         Render::GL::ISubmitter &out, SubmitStats &stats) {
  const auto &pose = frame.elephant_pose[i];
  const auto &variant = frame.elephant_variant[i];

  switch (frame.lod[i]) {
  case CreatureLOD::Full:
    Render::Elephant::submit_elephant_full_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Reduced:
    Render::Elephant::submit_elephant_reduced_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Minimal:
    Render::Elephant::submit_elephant_minimal_rigged(pose, variant, world, out);
    break;
  case CreatureLOD::Billboard:
    break;
  }
  ++stats.entities_submitted;

  Render::GL::HumanoidPose blank_pose{};
  Render::GL::HumanoidVariant blank_variant{};
  Render::GL::HumanoidAnimationContext blank_anim{};
  submit_equipment_loadout(spec, frame, i, world, blank_variant, blank_pose,
                           blank_anim, out, stats);
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

    switch (spec.kind) {
    case CreatureKind::Humanoid:
      submit_humanoid_row(spec, frame, i, out, stats);
      bump_lod_counters(frame.lod[i], stats);
      break;

    case CreatureKind::Horse:
      submit_horse_row(spec, frame, i, frame.world_matrix[i], out, stats);
      bump_lod_counters(frame.lod[i], stats);
      break;

    case CreatureKind::Elephant:
      submit_elephant_row(spec, frame, i, frame.world_matrix[i], out, stats);
      bump_lod_counters(frame.lod[i], stats);
      break;

    case CreatureKind::Mounted: {

      if (spec.mounted == nullptr) {
        ++stats.entities_submitted;
        bump_lod_counters(frame.lod[i], stats);
        break;
      }
      const auto &mounted = *spec.mounted;
      const auto &mount_world = frame.mount_world_matrix[i];
      if (mounted.mount.kind == CreatureKind::Elephant) {
        submit_elephant_row(mounted.mount, frame, i, mount_world, out, stats);
      } else {
        submit_horse_row(mounted.mount, frame, i, mount_world, out, stats);
      }

      submit_humanoid_row(mounted.rider, frame, i, out, stats);

      bump_lod_counters(frame.lod[i], stats);
      break;
    }
    }
  }

  return stats;
}

} // namespace Render::Creature::Pipeline
