#include "sheep_manifest.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "sheep_spec.h"
#include "wildlife_rig.h"

namespace Render::Wildlife {

namespace {

enum class SheepAmbient : std::uint8_t {
  None = 0,
  Resting,
  Browsing,
  Watchful,
};

struct SheepClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  float graze{0.0F};
  float speed_ratio{0.0F};
  float alert{0.0F};
  bool collapses{false};
  bool holds_collapsed{false};
  bool startles{false};
  SheepGait gait{SheepGait::Stand};
  SheepAmbient ambient{SheepAmbient::None};
};

constexpr std::array<SheepClipSpec, 8> k_sheep_clips{{
    {{"idle", 96U, 24.0F, true},
     0.0F,
     0.0F,
     0.0F,
     false,
     false,
     false,
     SheepGait::Stand,
     SheepAmbient::Resting},
    {{"graze", 120U, 24.0F, true},
     1.0F,
     0.0F,
     0.0F,
     false,
     false,
     false,
     SheepGait::Stand,
     SheepAmbient::Browsing},
    {{"walk", 28U, 28.0F, true},
     0.0F,
     0.42F,
     0.0F,
     false,
     false,
     false,
     SheepGait::Walk},
    {{"run", 24U, 40.0F, true}, 0.0F, 1.0F, 1.0F, false, false, false, SheepGait::Run},
    {{"die", 32U, 26.0F, false},
     0.0F,
     0.0F,
     1.0F,
     true,
     false,
     false,
     SheepGait::Stand},
    {{"dead", 1U, 1.0F, true}, 0.0F, 0.0F, 1.0F, true, true, false, SheepGait::Stand},
    {{"startle", 26U, 30.0F, false},
     0.0F,
     0.0F,
     1.0F,
     false,
     false,
     true,
     SheepGait::Stand},
    {{"alert", 60U, 24.0F, true},
     0.0F,
     0.0F,
     1.0F,
     false,
     false,
     false,
     SheepGait::Stand,
     SheepAmbient::Watchful},
}};

constexpr std::array<Render::Creature::BakeClipDescriptor, k_sheep_clips.size()>
    k_sheep_clip_descs{{
        k_sheep_clips[0].desc,
        k_sheep_clips[1].desc,
        k_sheep_clips[2].desc,
        k_sheep_clips[3].desc,
        k_sheep_clips[4].desc,
        k_sheep_clips[5].desc,
        k_sheep_clips[6].desc,
        k_sheep_clips[7].desc,
    }};

constexpr float k_two_pi = 6.28318530718F;

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

void apply_ambient(SheepDrive& drive, SheepAmbient ambient, float phase) {
  switch (ambient) {
  case SheepAmbient::None:
    return;
  case SheepAmbient::Resting:

    drive.breath = wave(phase, 2.0F, 0.0F);
    drive.weight_shift = wave(phase, 1.0F, 1.1F) * 0.80F;
    drive.head_turn =
        (wave(phase, 1.0F, 2.4F) * 0.55F) + (wave(phase, 2.0F, 0.9F) * 0.18F);
    drive.head_dip = wave(phase, 1.0F, 0.3F) * 0.50F;
    drive.ear_flick =
        (pulse(phase, 0.22F, 0.045F) * 1.0F) - (pulse(phase, 0.66F, 0.04F) * 0.8F);
    drive.tail_flick =
        (wave(phase, 2.0F, 0.0F) * 0.35F) + (pulse(phase, 0.48F, 0.06F) * 0.9F);
    return;
  case SheepAmbient::Browsing:

    drive.breath = wave(phase, 5.0F, 0.0F) * 0.55F;
    drive.weight_shift = wave(phase, 1.0F, 0.0F) * 0.65F;
    drive.head_turn = wave(phase, 2.0F, 1.7F) * 0.42F;
    drive.ear_flick = pulse(phase, 0.55F, 0.03F) * 0.9F;
    drive.tail_flick = (wave(phase, 3.0F, 0.5F) * 0.30F) + pulse(phase, 0.18F, 0.04F);
    return;
  case SheepAmbient::Watchful:

    drive.breath = wave(phase, 4.0F, 0.0F) * 0.75F;
    drive.head_turn =
        (wave(phase, 2.0F, 0.0F) * 0.85F) + (wave(phase, 5.0F, 1.3F) * 0.2F);
    drive.head_dip = -0.55F + (wave(phase, 2.0F, 1.6F) * 0.25F);
    drive.weight_shift = wave(phase, 2.0F, 2.6F) * 0.45F;
    drive.ear_flick = (pulse(phase, 0.14F, 0.035F) * 1.0F) -
                      (pulse(phase, 0.47F, 0.03F) * 0.9F) +
                      (pulse(phase, 0.81F, 0.035F) * 0.8F);
    drive.tail_flick = wave(phase, 4.0F, 0.9F) * 0.55F;
    return;
  }
}

void bake_sheep_clip_frame(std::size_t clip_index,
                           std::uint32_t frame_index,
                           std::vector<QMatrix4x4>& out_palettes,
                           std::vector<QMatrix4x4>* out_socket_transforms) {
  (void)out_socket_transforms;
  const SheepClipSpec& clip = k_sheep_clips[clip_index];
  std::uint32_t const phase_divisor =
      clip.desc.loops ? clip.desc.frame_count
                      : std::max<std::uint32_t>(clip.desc.frame_count, 2U) - 1U;
  float const phase = static_cast<float>(frame_index) /
                      static_cast<float>(std::max<std::uint32_t>(phase_divisor, 1U));

  SheepDrive drive;
  drive.graze = clip.graze > 0.0F ? sheep_graze_amount(phase) : 0.0F;
  drive.speed_ratio = clip.speed_ratio;
  drive.alert = clip.alert;
  drive.stride_phase = phase;
  drive.gait = clip.gait;
  apply_ambient(drive, clip.ambient, phase);
  if (clip.startles) {
    drive.startle = phase;
    drive.stride_phase = 0.0F;
  }
  if (clip.collapses) {
    drive.collapse = clip.holds_collapsed ? 1.0F : phase;
    drive.stride_phase = 0.0F;
  }

  RigPose pose = sheep_pose(drive);

  BonePalette palette{};
  evaluate_wildlife_skeleton(pose, palette);
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto sheep_graze_amount(float phase) noexcept -> float {
  float const wrapped = phase - std::floor(phase);
  constexpr float k_lower_end = 0.28F;
  constexpr float k_raise_start = 0.72F;

  auto const smoother_step = [](float value) {
    float const t = std::clamp(value, 0.0F, 1.0F);
    return t * t * t * ((t * ((t * 6.0F) - 15.0F)) + 10.0F);
  };

  if (wrapped < k_lower_end) {
    return smoother_step(wrapped / k_lower_end);
  }
  if (wrapped <= k_raise_start) {
    return 1.0F;
  }
  return 1.0F - smoother_step((wrapped - k_raise_start) / (1.0F - k_raise_start));
}

auto sheep_manifest() noexcept -> const Render::Creature::SpeciesManifest& {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "sheep";
    m.species_id = Render::Creature::Bpat::k_species_sheep;
    m.bpat_file_name = "sheep.bpat";
    m.minimal_snapshot_file_name = "sheep_minimal.bpsm";
    m.topology = &wildlife_topology();
    m.lod_full.primitive_name = "sheep.full.body";
    m.lod_full.anchor_bone = bone_index(Bone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_full.color_role = k_sheep_role_wool;
    m.lod_full.material_id = k_wildlife_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes = sheep_mesh_nodes();
    m.lod_minimal.primitive_name = "sheep.minimal.whole";
    m.lod_minimal.anchor_bone = bone_index(Bone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_sheep_role_wool;
    m.lod_minimal.material_id = k_wildlife_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = sheep_minimal_mesh_nodes();
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(k_sheep_clip_descs);
    m.bind_palette = &sheep_bind_palette;
    m.creature_spec = &sheep_creature_spec;
    m.bake_clip_frame = &bake_sheep_clip_frame;
    return m;
  }();
  return manifest;
}

} // namespace Render::Wildlife
