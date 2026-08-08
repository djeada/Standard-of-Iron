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

struct SheepClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  float graze{0.0F};
  float speed_ratio{0.0F};
  float alert{0.0F};
  bool collapses{false};
  bool holds_collapsed{false};
  SheepGait gait{SheepGait::Stand};
};

constexpr std::array<SheepClipSpec, 6> k_sheep_clips{{
    {{"idle", 20U, 20.0F, true}, 0.0F, 0.0F, 0.0F, false, false, SheepGait::Stand},
    {{"graze", 120U, 24.0F, true}, 1.0F, 0.0F, 0.0F, false, false, SheepGait::Stand},
    {{"walk", 24U, 24.0F, true}, 0.0F, 0.42F, 0.0F, false, false, SheepGait::Walk},
    {{"run", 16U, 24.0F, true}, 0.0F, 1.0F, 1.0F, false, false, SheepGait::Run},
    {{"die", 30U, 22.0F, false}, 0.0F, 0.0F, 1.0F, true, false, SheepGait::Stand},
    {{"dead", 1U, 1.0F, true}, 0.0F, 0.0F, 1.0F, true, true, SheepGait::Stand},
}};

constexpr std::array<Render::Creature::BakeClipDescriptor, k_sheep_clips.size()>
    k_sheep_clip_descs{{
        k_sheep_clips[0].desc,
        k_sheep_clips[1].desc,
        k_sheep_clips[2].desc,
        k_sheep_clips[3].desc,
        k_sheep_clips[4].desc,
        k_sheep_clips[5].desc,
    }};

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
