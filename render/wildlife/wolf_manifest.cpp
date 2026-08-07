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

struct WolfClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  float speed_ratio{0.0F};
  float crouch{0.0F};
  float ear_pin{0.0F};
  bool bites{false};
  bool collapses{false};
  bool holds_collapsed{false};
  WolfGait gait{WolfGait::Stand};
};

constexpr std::array<WolfClipSpec, 7> k_wolf_clips{{
    {{"idle", 20U, 20.0F, true},
     0.0F,
     0.0F,
     0.0F,
     false,
     false,
     false,
     WolfGait::Stand},
    {{"stalk", 28U, 20.0F, true},
     0.30F,
     1.0F,
     0.85F,
     false,
     false,
     false,
     WolfGait::Stalk},
    {{"walk", 24U, 24.0F, true},
     0.45F,
     0.0F,
     0.0F,
     false,
     false,
     false,
     WolfGait::Walk},
    {{"run", 14U, 26.0F, true}, 1.0F, 0.0F, 0.25F, false, false, false, WolfGait::Run},
    {{"bite", 18U, 24.0F, false},
     0.0F,
     0.55F,
     1.0F,
     true,
     false,
     false,
     WolfGait::Stand},
    {{"die", 20U, 20.0F, false}, 0.0F, 0.0F, 1.0F, false, true, false, WolfGait::Stand},
    {{"dead", 1U, 1.0F, true}, 0.0F, 0.0F, 1.0F, false, true, true, WolfGait::Stand},
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
    }};

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
  if (clip.bites) {
    constexpr float k_contact_phase = 0.34F;
    auto smoothstep = [](float value) {
      float const t = std::clamp(value, 0.0F, 1.0F);
      return t * t * (3.0F - (2.0F * t));
    };
    float const windup = smoothstep(phase / k_contact_phase);
    float const recovery =
        1.0F - smoothstep((phase - k_contact_phase) / (1.0F - k_contact_phase));
    drive.lunge = windup * recovery;
    drive.jaw_open = std::sin(std::numbers::pi_v<float> *
                              std::clamp(phase / k_contact_phase, 0.0F, 1.0F));
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
