

#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_writer.h"
#include "render/creature/snapshot_mesh_asset.h"
#include "render/creature/species_manifest.h"

#include "render/creature/part_graph.h"
#include "render/creature/render_request.h"
#include "render/elephant/dimensions.h"
#include "render/elephant/elephant_gait.h"
#include "render/elephant/elephant_manifest.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/mounted_knight_pose.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_manifest.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/mounted_pose_controller.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/skeleton.h"
#include "render/rigged_mesh_bake.h"
#include "render/snapshot_mesh_bake.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace bpat = Render::Creature::Bpat;
namespace snapshot = Render::Creature::Snapshot;

enum class BakerAttackType : std::uint8_t { None, Sword, Spear, Bow };
enum class BakerRidingType : std::uint8_t {
  None,
  Idle,
  Charge,
  Reining,
  BowShot
};
enum class HumanoidBakeProfile : std::uint8_t { Default, SwordReady };

struct HumanoidClipSpec {
  const char *name;
  Render::GL::HumanoidMotionState state;
  BakerAttackType attack_type{BakerAttackType::None};
  std::uint8_t attack_variant{0};
  BakerRidingType riding_type{BakerRidingType::None};
  std::uint32_t frames;
  float fps;
  float cycle_time;
  bool loops;
};

constexpr std::array<HumanoidClipSpec, 15> kHumanoidClips{{
    {"idle", Render::GL::HumanoidMotionState::Idle, BakerAttackType::None, 0,
     BakerRidingType::None, 24U, 24.0F, 1.6F, true},
    {"walk", Render::GL::HumanoidMotionState::Walk, BakerAttackType::None, 0,
     BakerRidingType::None, 24U, 24.0F, 0.92F, true},
    {"run", Render::GL::HumanoidMotionState::Run, BakerAttackType::None, 0,
     BakerRidingType::None, 24U, 24.0F, 0.56F, true},
    {"hold", Render::GL::HumanoidMotionState::Hold, BakerAttackType::None, 0,
     BakerRidingType::None, 16U, 24.0F, 1.8F, true},
    {"attack_sword_a", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword, 0, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"attack_sword_b", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword, 1, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"attack_sword_c", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword, 2, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"attack_spear_a", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear, 0, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"attack_spear_b", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear, 1, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"attack_spear_c", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear, 2, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"attack_bow", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Bow, 0, BakerRidingType::None, 24U, 24.0F, 1.0F, false},
    {"riding_idle", Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None, 0, BakerRidingType::Idle, 24U, 24.0F, 1.6F, true},
    {"riding_charge", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None, 0, BakerRidingType::Charge, 24U, 24.0F, 1.0F,
     false},
    {"riding_reining", Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None, 0, BakerRidingType::Reining, 24U, 24.0F, 1.0F,
     false},
    {"riding_bow_shot", Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None, 0, BakerRidingType::BowShot, 24U, 24.0F, 1.0F,
     false},
}};

struct HumanoidSocketSpec {
  const char *name;
  Render::Humanoid::HumanoidSocket socket;
};

constexpr std::array<HumanoidSocketSpec, 10> kHumanoidSockets{{
    {"head", Render::Humanoid::HumanoidSocket::Head},
    {"hand_r", Render::Humanoid::HumanoidSocket::HandR},
    {"hand_l", Render::Humanoid::HumanoidSocket::HandL},
    {"back", Render::Humanoid::HumanoidSocket::Back},
    {"hip_l", Render::Humanoid::HumanoidSocket::HipL},
    {"hip_r", Render::Humanoid::HumanoidSocket::HipR},
    {"chest_front", Render::Humanoid::HumanoidSocket::ChestFront},
    {"chest_back", Render::Humanoid::HumanoidSocket::ChestBack},
    {"foot_l", Render::Humanoid::HumanoidSocket::FootL},
    {"foot_r", Render::Humanoid::HumanoidSocket::FootR},
}};

void bake_humanoid_clip_frame(HumanoidBakeProfile profile,
                              const HumanoidClipSpec &clip,
                              std::uint32_t frame_index,
                              std::vector<QMatrix4x4> &out_palettes,
                              std::vector<QMatrix4x4> &out_sockets) {
  Render::GL::VariationParams variation{};
  variation.height_scale = 1.0F;
  variation.bulk_scale = 1.0F;
  variation.stance_width = 1.0F;
  variation.arm_swing_amp = 1.0F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;

  float const phase =
      static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::GL::HumanoidPose pose{};

  if (clip.attack_type != BakerAttackType::None) {

    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx{};
    anim_ctx.gait = hold_gait;
    anim_ctx.motion_state = Render::GL::HumanoidMotionState::Attacking;
    anim_ctx.attack_phase = phase;
    anim_ctx.jitter_seed = 0U;
    anim_ctx.inputs.is_attacking = true;
    anim_ctx.inputs.is_melee = (clip.attack_type != BakerAttackType::Bow);
    anim_ctx.inputs.attack_variant = clip.attack_variant;

    Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
    switch (clip.attack_type) {
    case BakerAttackType::Sword:
      ctrl.sword_slash_variant(phase, clip.attack_variant);
      break;
    case BakerAttackType::Spear:
      ctrl.spear_thrust_variant(phase, clip.attack_variant);
      break;
    case BakerAttackType::Bow:
      ctrl.aim_bow(phase);
      break;
    default:
      break;
    }
  } else if (clip.riding_type != BakerRidingType::None) {
    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx_r{};
    anim_ctx_r.gait = hold_gait;
    anim_ctx_r.motion_state = Render::GL::HumanoidMotionState::Hold;
    anim_ctx_r.inputs.is_moving = (clip.riding_type != BakerRidingType::Idle);

    auto horse_profile = Render::GL::make_horse_profile(0U, {}, {});
    auto mount = Render::GL::compute_mount_frame(horse_profile);
    Render::GL::tune_mounted_knight_frame(horse_profile.dims, mount);

    Render::GL::MountedPoseController ctrl(pose, anim_ctx_r);
    ctrl.mount_on_horse(mount);

    switch (clip.riding_type) {
    case BakerRidingType::Idle:
      ctrl.riding_idle(mount);
      break;
    case BakerRidingType::Charge:
      ctrl.riding_charging(mount, 1.0F);
      break;
    case BakerRidingType::Reining:
      ctrl.riding_reining(mount, 0.7F, 0.7F);
      break;
    case BakerRidingType::BowShot:
      ctrl.riding_bow_shot(mount, phase);
      break;
    default:
      break;
    }
    if (profile == HumanoidBakeProfile::SwordReady &&
        clip.riding_type != BakerRidingType::BowShot) {
      Render::GL::HumanoidPoseController ready_ctrl(pose, anim_ctx_r);
      ready_ctrl.hold_sword_and_shield();
    }
  } else {
    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = clip.state;
    gait.cycle_time = clip.cycle_time;
    gait.cycle_phase = phase;

    switch (clip.state) {
    case Render::GL::HumanoidMotionState::Idle:
      gait.speed = 0.0F;
      gait.normalized_speed = 0.0F;
      break;
    case Render::GL::HumanoidMotionState::Walk:
      gait.speed = 1.5F;
      gait.normalized_speed = 1.0F;
      break;
    case Render::GL::HumanoidMotionState::Run:
      gait.speed = 4.0F;
      gait.normalized_speed = 1.6F;
      break;
    case Render::GL::HumanoidMotionState::Hold:
    default:
      gait.speed = 0.0F;
      gait.normalized_speed = 0.0F;
      break;
    }

    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, phase * clip.cycle_time, gait, variation, pose);
    if (profile == HumanoidBakeProfile::SwordReady) {
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.motion_state = clip.state;
      anim_ctx.inputs.is_moving = gait.speed > 0.1F;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      ctrl.hold_sword_and_shield();
    }
  }

  Render::Humanoid::BonePalette palette{};
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F),
                                      palette);

  for (std::size_t b = 0; b < Render::Humanoid::kBoneCount; ++b) {
    out_palettes.push_back(palette[b]);
  }

  for (auto const &spec : kHumanoidSockets) {
    out_sockets.push_back(
        Render::Humanoid::socket_transform(palette, spec.socket));
  }
}

bool bake_humanoid(const std::filesystem::path &out_dir,
                   std::uint32_t species_id, std::string_view file_name,
                   HumanoidBakeProfile profile) {
  bpat::BpatWriter writer(
      species_id, static_cast<std::uint32_t>(Render::Humanoid::kBoneCount));
  for (auto const &spec : kHumanoidSockets) {
    Render::Humanoid::SocketDef const &def =
        Render::Humanoid::socket_def(spec.socket);
    bpat::SocketDescriptor s{};
    s.name = spec.name;
    s.anchor_bone = static_cast<std::uint32_t>(def.bone);
    s.local_offset = def.local_offset;
    writer.add_socket(std::move(s));
  }

  for (auto const &clip : kHumanoidClips) {
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frames) *
                     Render::Humanoid::kBoneCount);
    std::vector<QMatrix4x4> sockets;
    sockets.reserve(static_cast<std::size_t>(clip.frames) *
                    kHumanoidSockets.size());
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      bake_humanoid_clip_frame(profile, clip, f, palettes, sockets);
    }
    writer.append_clip_palettes(palettes);
    writer.append_clip_socket_transforms(sockets);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / std::string(file_name);
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << kHumanoidClips.size() << " clips, "
            << Render::Humanoid::kBoneCount << " bones, "
            << kHumanoidSockets.size() << " sockets)\n";
  return true;
}

bool bake_species_manifest(const std::filesystem::path &out_dir,
                           const Render::Creature::SpeciesManifest &manifest) {
  if (manifest.bind_palette == nullptr || manifest.creature_spec == nullptr ||
      manifest.bake_clip_palette == nullptr) {
    std::cerr << "[bpat_baker] manifest for " << manifest.species_name
              << " is incomplete\n";
    return false;
  }

  auto const bind_palette = manifest.bind_palette();
  std::vector<QMatrix4x4> inverse_bind;
  inverse_bind.reserve(bind_palette.size());
  for (const QMatrix4x4 &m : bind_palette) {
    inverse_bind.push_back(m.inverted());
  }
  bpat::BpatWriter writer(manifest.species_id,
                          static_cast<std::uint32_t>(bind_palette.size()));

  for (std::size_t i = 0; i < manifest.clips.size(); ++i) {
    auto const &clip = manifest.clips[i];
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frame_count;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frame_count) *
                     bind_palette.size());
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      manifest.bake_clip_palette(i, f, palettes);
    }
    writer.append_clip_palettes(palettes);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path =
      out_dir / std::string(manifest.bpat_file_name);
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << manifest.clips.size() << " clips, "
            << bind_palette.size() << " bones)\n";

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph = &Render::Creature::part_graph_for(
      manifest.creature_spec(), Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = bind_palette;
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      manifest.species_id, Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()), source.indices);
  for (std::size_t i = 0; i < manifest.clips.size(); ++i) {
    auto const &clip = manifest.clips[i];
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frame_count;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frame_count) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(bind_palette.size());
      manifest.bake_clip_palette(i, f, frame_palette);
      std::size_t const n = std::min(frame_palette.size(), inverse_bind.size());
      for (std::size_t b = 0; b < n; ++b) {
        frame_palette[b] = frame_palette[b] * inverse_bind[b];
      }
      auto baked =
          Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  std::filesystem::path const snapshot_out_path =
      out_dir / std::string(manifest.minimal_snapshot_file_name);
  std::ofstream snapshot_out(snapshot_out_path,
                             std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path
              << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, "
            << source.indices.size() << " indices, "
            << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

struct HorseClipSpec {
  const char *name;
  Render::GL::GaitType gait;
  std::uint32_t frames;
  float fps;
  bool loops;
  bool is_moving;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

constexpr std::array<HorseClipSpec, 6> kHorseClips{{
    {"idle", Render::GL::GaitType::IDLE, 24U, 24.0F, true, false, false, 0.0F},
    {"walk", Render::GL::GaitType::WALK, 24U, 24.0F, true, true, false, 0.50F},
    {"trot", Render::GL::GaitType::TROT, 16U, 24.0F, true, true, false, 0.85F},
    {"canter", Render::GL::GaitType::CANTER, 16U, 24.0F, true, true, false,
     1.00F},
    {"gallop", Render::GL::GaitType::GALLOP, 12U, 24.0F, true, true, false,
     1.12F},
    {"fight", Render::GL::GaitType::IDLE, 24U, 24.0F, true, false, true, 0.0F},
}};

void bake_horse_clip_frame(const HorseClipSpec &clip, std::uint32_t frame_index,
                           const Render::GL::HorseDimensions &dims,
                           std::vector<QMatrix4x4> &out_palettes) {
  float const phase =
      static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::GL::HorseGait base{};
  Render::GL::HorseGait const gait = Render::GL::gait_for_type(clip.gait, base);

  Render::Horse::HorsePoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  if (clip.is_fighting) {

    float const fight_bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float> + 0.5F) *
        dims.idle_bob_amplitude * 0.45F;
    motion.bob = fight_bob;
  } else {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_animated(dims, gait, motion, pose);

  Render::Horse::BonePalette palette{};
  Render::Horse::evaluate_horse_skeleton(pose, palette);

  for (std::size_t b = 0; b < Render::Horse::kHorseBoneCount; ++b) {
    out_palettes.push_back(palette[b]);
  }
}

bool bake_horse(const std::filesystem::path &out_dir) {
  bpat::BpatWriter writer(
      bpat::kSpeciesHorse,
      static_cast<std::uint32_t>(Render::Horse::kHorseBoneCount));

  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);

  for (auto const &clip : kHorseClips) {
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frames) *
                     Render::Horse::kHorseBoneCount);
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      bake_horse_clip_frame(clip, f, dims, palettes);
    }
    writer.append_clip_palettes(palettes);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / "horse.bpat";
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << kHorseClips.size() << " clips, "
            << Render::Horse::kHorseBoneCount << " bones)\n";

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph =
      &Render::Creature::part_graph_for(Render::Horse::horse_creature_spec(),
                                        Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = Render::Horse::horse_bind_palette();
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      bpat::kSpeciesHorse, Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()), source.indices);
  for (auto const &clip : kHorseClips) {
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frames) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(Render::Horse::kHorseBoneCount);
      bake_horse_clip_frame(clip, f, dims, frame_palette);
      auto baked =
          Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  std::filesystem::path const snapshot_out_path =
      out_dir / "horse_minimal.bpsm";
  std::ofstream snapshot_out(snapshot_out_path,
                             std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path
              << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, "
            << source.indices.size() << " indices, "
            << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

struct ElephantClipSpec {
  const char *name;
  std::uint32_t frames;
  float fps;
  bool loops;
  bool is_moving;
  Render::GL::ElephantGait gait;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

const std::array<ElephantClipSpec, 4> kElephantClips{{
    {"idle", 24U, 24.0F, true, false,
     Render::GL::ElephantGait{2.0F, 0.0F, 0.0F, 0.02F, 0.01F}, false, 0.0F},
    {"walk", 24U, 24.0F, true, true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F}, false, 0.62F},
    {"run", 16U, 24.0F, true, true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F}, false, 0.75F},
    {"fight", 24U, 24.0F, true, false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F}, true, 0.0F},
}};

void bake_elephant_clip_frame(const ElephantClipSpec &clip,
                              std::uint32_t frame_index,
                              const Render::GL::ElephantDimensions &dims,
                              std::vector<QMatrix4x4> &out_palettes) {
  float const phase =
      static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  motion.anim_time = phase * clip.gait.cycle_time;
  if (clip.is_fighting) {

    float const fight_bob = std::sin(phase * 2.0F * std::numbers::pi_v<float>) *
                            dims.idle_bob_amplitude * 0.5F;
    motion.bob = fight_bob;
  } else {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(dims, clip.gait, motion,
                                                     pose);

  Render::Elephant::BonePalette palette{};
  Render::Elephant::evaluate_elephant_skeleton(pose, palette);

  for (std::size_t b = 0; b < Render::Elephant::kElephantBoneCount; ++b) {
    out_palettes.push_back(palette[b]);
  }
}

bool bake_elephant(const std::filesystem::path &out_dir) {
  bpat::BpatWriter writer(
      bpat::kSpeciesElephant,
      static_cast<std::uint32_t>(Render::Elephant::kElephantBoneCount));

  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);

  for (auto const &clip : kElephantClips) {
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frames) *
                     Render::Elephant::kElephantBoneCount);
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      bake_elephant_clip_frame(clip, f, dims, palettes);
    }
    writer.append_clip_palettes(palettes);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / "elephant.bpat";
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << kElephantClips.size() << " clips, "
            << Render::Elephant::kElephantBoneCount << " bones)\n";

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph = &Render::Creature::part_graph_for(
      Render::Elephant::elephant_creature_spec(),
      Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = Render::Elephant::elephant_bind_palette();
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      bpat::kSpeciesElephant, Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()), source.indices);
  for (auto const &clip : kElephantClips) {
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frames) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(Render::Elephant::kElephantBoneCount);
      bake_elephant_clip_frame(clip, f, dims, frame_palette);
      auto baked =
          Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  std::filesystem::path const snapshot_out_path =
      out_dir / "elephant_minimal.bpsm";
  std::ofstream snapshot_out(snapshot_out_path,
                             std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path
              << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, "
            << source.indices.size() << " indices, "
            << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path out_dir = "assets/creatures";
  if (argc >= 2) {
    out_dir = argv[1];
  }
  bool ok = true;
  ok = bake_humanoid(out_dir, bpat::kSpeciesHumanoid, "humanoid.bpat",
                     HumanoidBakeProfile::Default) &&
       ok;
  ok = bake_humanoid(out_dir, bpat::kSpeciesHumanoidSword,
                     "humanoid_sword.bpat", HumanoidBakeProfile::SwordReady) &&
       ok;
  ok = bake_species_manifest(out_dir, Render::Horse::horse_manifest()) && ok;
  ok = bake_species_manifest(out_dir, Render::Elephant::elephant_manifest()) &&
       ok;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
