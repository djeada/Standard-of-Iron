

#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_writer.h"

#include "render/elephant/dimensions.h"
#include "render/elephant/elephant_gait.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/mounted_knight_pose.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/mounted_pose_controller.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

namespace bpat = Render::Creature::Bpat;

enum class BakerAttackType : std::uint8_t { None, Sword, Spear, Bow };
enum class BakerRidingType : std::uint8_t {
  None,
  Idle,
  Charge,
  Reining,
  BowShot
};

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

void bake_humanoid_clip_frame(const HumanoidClipSpec &clip,
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

    auto profile = Render::GL::make_horse_profile(0U, {}, {});
    auto mount = Render::GL::compute_mount_frame(profile);
    Render::GL::tune_mounted_knight_frame(profile.dims, mount);

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

bool bake_humanoid(const std::filesystem::path &out_dir) {
  bpat::BpatWriter writer(
      bpat::kSpeciesHumanoid,
      static_cast<std::uint32_t>(Render::Humanoid::kBoneCount));
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
      bake_humanoid_clip_frame(clip, f, palettes, sockets);
    }
    writer.append_clip_palettes(palettes);
    writer.append_clip_socket_transforms(sockets);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / "humanoid.bpat";
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

struct HorseClipSpec {
  const char *name;
  Render::GL::GaitType gait;
  std::uint32_t frames;
  float fps;
  bool loops;
  bool is_moving;
};

constexpr std::array<HorseClipSpec, 5> kHorseClips{{
    {"idle", Render::GL::GaitType::IDLE, 24U, 24.0F, true, false},
    {"walk", Render::GL::GaitType::WALK, 24U, 24.0F, true, true},
    {"trot", Render::GL::GaitType::TROT, 16U, 24.0F, true, true},
    {"canter", Render::GL::GaitType::CANTER, 16U, 24.0F, true, true},
    {"gallop", Render::GL::GaitType::GALLOP, 12U, 24.0F, true, true},
}};

void bake_horse_clip_frame(const HorseClipSpec &clip, std::uint32_t frame_index,
                           const Render::GL::HorseDimensions &dims,
                           std::vector<QMatrix4x4> &out_palettes) {
  float const phase =
      static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::GL::HorseGait base{};
  Render::GL::HorseGait const gait = Render::GL::gait_for_type(clip.gait, base);

  Render::Horse::HorseReducedMotion motion{};
  motion.phase = phase;
  motion.bob = 0.0F;
  motion.is_moving = clip.is_moving;

  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_reduced(dims, gait, motion, pose);

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
  return true;
}

struct ElephantClipSpec {
  const char *name;
  std::uint32_t frames;
  float fps;
  bool loops;
  bool is_moving;
  Render::GL::ElephantGait gait;
};

const std::array<ElephantClipSpec, 3> kElephantClips{{
    {"idle", 24U, 24.0F, true, false,
     Render::GL::ElephantGait{2.0F, 0.0F, 0.0F, 0.02F, 0.01F}},
    {"walk", 24U, 24.0F, true, true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F}},
    {"run", 16U, 24.0F, true, true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F}},
}};

void bake_elephant_clip_frame(const ElephantClipSpec &clip,
                              std::uint32_t frame_index,
                              const Render::GL::ElephantDimensions &dims,
                              std::vector<QMatrix4x4> &out_palettes) {
  float const phase =
      static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::Elephant::ElephantReducedMotion motion{};
  motion.phase = phase;
  motion.bob = 0.0F;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = false;
  motion.anim_time = phase * clip.gait.cycle_time;

  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_reduced(dims, clip.gait, motion,
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
  return true;
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path out_dir = "assets/creatures";
  if (argc >= 2) {
    out_dir = argv[1];
  }
  bool ok = true;
  ok = bake_humanoid(out_dir) && ok;
  ok = bake_horse(out_dir) && ok;
  ok = bake_elephant(out_dir) && ok;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
