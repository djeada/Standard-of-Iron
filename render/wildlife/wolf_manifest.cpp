#include "wolf_manifest.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "wildlife_rig.h"
#include "wolf_spec.h"

namespace Render::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;

enum class WolfAmbient : std::uint8_t {
  None = 0,
  Resting,
  Crouched,
};

struct WolfClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  float speed_ratio{0.0F};
  float crouch{0.0F};
  float ear_pin{0.0F};
  bool bites{false};
  bool collapses{false};
  bool holds_collapsed{false};
  WolfGait gait{WolfGait::Stand};
  WolfAmbient ambient{WolfAmbient::None};
};

constexpr std::array<WolfClipSpec, 8> k_wolf_clips{{
    {{"idle", 96U, 24.0F, true},
     0.0F,
     0.0F,
     0.0F,
     false,
     false,
     false,
     WolfGait::Stand,
     WolfAmbient::Resting},
    {{"stalk", 32U, 22.0F, true},
     0.30F,
     1.0F,
     0.85F,
     false,
     false,
     false,
     WolfGait::Stalk},
    {{"walk", 28U, 28.0F, true},
     0.45F,
     0.0F,
     0.0F,
     false,
     false,
     false,
     WolfGait::Walk},
    {{"run", 24U, 44.0F, true}, 1.0F, 0.0F, 0.25F, false, false, false, WolfGait::Run},
    {{"bite", 32U, 30.0F, false},
     0.0F,
     0.55F,
     1.0F,
     true,
     false,
     false,
     WolfGait::Stand},
    {{"die", 32U, 26.0F, false}, 0.0F, 0.0F, 1.0F, false, true, false, WolfGait::Stand},
    {{"dead", 1U, 1.0F, true}, 0.0F, 0.0F, 1.0F, false, true, true, WolfGait::Stand},
    {{"crouch", 72U, 24.0F, true},
     0.0F,
     0.92F,
     0.72F,
     false,
     false,
     false,
     WolfGait::Stand,
     WolfAmbient::Crouched},
}};

constexpr std::array<Render::Creature::BakeClipDescriptor, k_wolf_clips.size()>
    k_wolf_clip_descs{{
        k_wolf_clips[0].desc,
        k_wolf_clips[1].desc,
        k_wolf_clips[2].desc,
        k_wolf_clips[3].desc,
        k_wolf_clips[4].desc,
        k_wolf_clips[5].desc,
        k_wolf_clips[6].desc,
        k_wolf_clips[7].desc,
    }};

auto wave(float phase, float harmonic, float offset) -> float {
  return std::sin((phase * harmonic * k_two_pi) + offset);
}

auto pulse(float phase, float centre, float width) -> float {
  float const wrapped = phase - std::floor(phase);
  float delta = std::fabs(wrapped - centre);
  delta = std::min(delta, 1.0F - delta);
  if (delta >= width) {
    return 0.0F;
  }
  float const t = 1.0F - (delta / width);
  return t * t * (3.0F - (2.0F * t));
}

void apply_ambient(WolfDrive& drive, WolfAmbient ambient, float phase) {
  switch (ambient) {
  case WolfAmbient::None:
    return;
  case WolfAmbient::Resting:

    drive.breath = wave(phase, 2.0F, 0.0F);
    drive.weight_shift = wave(phase, 1.0F, 0.7F) * 0.85F;
    drive.head_turn =
        (wave(phase, 1.0F, 1.9F) * 0.62F) + (wave(phase, 2.0F, 0.4F) * 0.2F);
    drive.head_dip = wave(phase, 1.0F, 3.1F) * 0.45F;
    drive.ear_swivel =
        (pulse(phase, 0.31F, 0.055F) * 0.9F) - (pulse(phase, 0.74F, 0.045F) * 0.7F);
    drive.tail_sway =
        (wave(phase, 1.0F, 0.0F) * 0.7F) + (wave(phase, 3.0F, 1.2F) * 0.22F);
    return;
  case WolfAmbient::Crouched:

    drive.breath = wave(phase, 3.0F, 0.0F) * 0.85F;
    drive.weight_shift = wave(phase, 1.0F, 2.2F) * 0.55F;
    drive.head_turn = wave(phase, 1.0F, 0.0F) * 0.85F;
    drive.head_dip = wave(phase, 2.0F, 1.4F) * 0.30F;
    drive.ear_swivel =
        (pulse(phase, 0.18F, 0.05F) * 0.8F) - (pulse(phase, 0.62F, 0.05F) * 0.8F);
    drive.tail_sway = wave(phase, 2.0F, 0.6F) * 0.45F;
    return;
  }
}

void bake_wolf_clip_frame(std::size_t clip_index,
                          std::uint32_t frame_index,
                          std::vector<QMatrix4x4>& out_palettes,
                          std::vector<QMatrix4x4>* out_socket_transforms) {
  (void)out_socket_transforms;
  const WolfClipSpec& clip = k_wolf_clips[clip_index];
  std::uint32_t const phase_divisor =
      clip.desc.loops ? clip.desc.frame_count
                      : std::max<std::uint32_t>(clip.desc.frame_count, 2U) - 1U;
  float const phase = static_cast<float>(frame_index) /
                      static_cast<float>(std::max<std::uint32_t>(phase_divisor, 1U));

  WolfDrive drive;
  drive.stride_phase = phase;
  drive.speed_ratio = clip.speed_ratio;
  drive.crouch = clip.crouch;
  drive.ear_pin = clip.ear_pin;
  drive.gait = clip.gait;
  apply_ambient(drive, clip.ambient, phase);
  if (clip.bites) {

    constexpr float k_coil_end = 0.16F;
    constexpr float k_contact = 0.28F;
    constexpr float k_wrench_end = 0.56F;
    constexpr float k_worry_end = 0.86F;
    constexpr float k_pi = std::numbers::pi_v<float>;

    auto smoothstep = [](float value) {
      float const t = std::clamp(value, 0.0F, 1.0F);
      return t * t * (3.0F - (2.0F * t));
    };

    if (phase < k_coil_end) {

      float const t = smoothstep(phase / k_coil_end);
      drive.lunge = -0.34F * t;
      drive.jaw_open = t;
      drive.head_dip = -1.1F * t;
      drive.crouch = 0.55F + (0.45F * t);
      drive.rear = 0.22F * t;
    } else if (phase < k_contact) {

      float const t = (phase - k_coil_end) / (k_contact - k_coil_end);
      drive.lunge = -0.34F + (1.34F * (t * t));
      drive.jaw_open = 1.0F - (0.65F * t * t * t);
      drive.head_dip = -1.1F * (1.0F - t);
      drive.crouch = 1.0F - (0.72F * t);
      drive.rear = 0.22F + (0.78F * (t * t));
    } else if (phase < k_wrench_end) {

      float const t = (phase - k_contact) / (k_wrench_end - k_contact);
      float const heave = std::sin(t * k_pi);
      drive.lunge = 1.0F - (0.12F * t);
      drive.jaw_open = 0.0F;
      drive.crouch = 0.30F + (0.42F * smoothstep(t));
      drive.rear = 1.0F - (0.88F * smoothstep(t));
      drive.head_shake = std::sin(t * k_pi * 2.0F) * 0.75F;
      drive.head_roll = -(0.45F + (0.42F * heave));
    } else if (phase < k_worry_end) {

      float const t = (phase - k_wrench_end) / (k_worry_end - k_wrench_end);
      float const thrash = std::sin(t * k_pi * 5.0F);
      float const decay = 1.0F - (0.45F * t);
      drive.lunge = 0.88F - (0.20F * t);
      drive.jaw_open = 0.0F;
      drive.crouch = 0.42F + (0.26F * std::fabs(thrash));
      drive.head_shake = thrash * decay * 1.70F;
      drive.head_roll = thrash * decay * 0.70F;
    } else {

      float const t = smoothstep((phase - k_worry_end) / (1.0F - k_worry_end));
      drive.lunge = 0.68F * (1.0F - t);
      drive.jaw_open = 0.0F;
      drive.crouch = (0.42F * (1.0F - t)) + (0.55F * t);
      drive.head_roll = 0.20F * (1.0F - t);
    }
    drive.stride_phase = 0.0F;
  }
  if (clip.collapses) {
    drive.collapse = clip.holds_collapsed ? 1.0F : phase;
    drive.stride_phase = 0.0F;
  }

  RigPose pose = wolf_pose(drive);

  BonePalette palette{};
  evaluate_wildlife_skeleton(pose, palette);
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto wolf_manifest() noexcept -> const Render::Creature::SpeciesManifest& {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "wolf";
    m.species_id = Render::Creature::Bpat::k_species_wolf;
    m.bpat_file_name = "wolf.bpat";
    m.minimal_snapshot_file_name = "wolf_minimal.bpsm";
    m.topology = &wildlife_topology();
    m.lod_full.primitive_name = "wolf.full.body";
    m.lod_full.anchor_bone = bone_index(Bone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_full.color_role = k_wolf_role_fur;
    m.lod_full.material_id = k_wildlife_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes = wolf_mesh_nodes();
    m.lod_minimal.primitive_name = "wolf.minimal.whole";
    m.lod_minimal.anchor_bone = bone_index(Bone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_wolf_role_fur;
    m.lod_minimal.material_id = k_wildlife_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = wolf_minimal_mesh_nodes();
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(k_wolf_clip_descs);
    m.bind_palette = &wolf_bind_palette;
    m.creature_spec = &wolf_creature_spec;
    m.bake_clip_frame = &bake_wolf_clip_frame;
    return m;
  }();
  return manifest;
}

} // namespace Render::Wildlife
