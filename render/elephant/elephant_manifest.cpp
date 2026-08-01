#include "elephant_manifest.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "elephant_gait.h"
#include "elephant_source_asset.h"
#include "elephant_spec.h"

namespace Render::Elephant {

namespace {

constexpr int k_elephant_material_id = 6;

struct ElephantClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  bool is_moving{};
  Render::GL::ElephantGait gait;
  bool is_fighting{false};
  bool is_death{false};
  bool is_dead_hold{false};
  float bob_scale{0.0F};
};

const std::array<ElephantClipSpec, 6> k_elephant_clips{{
    {{"idle", 24U, 24.0F, true},
     false,
     Render::GL::ElephantGait{2.0F, 0.0F, 0.0F, 0.02F, 0.01F},
     false,
     false,
     false,
     0.0F},
    {{"walk", 24U, 24.0F, true},
     true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F},
     false,
     false,
     false,
     0.62F},
    {{"run", 16U, 24.0F, true},
     true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F},
     false,
     false,
     false,
     0.75F},
    {{"fight", 24U, 24.0F, true},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     true,
     false,
     false,
     0.0F},
    {{"die", 24U, 24.0F, false},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     false,
     true,
     false,
     0.0F},
    {{"dead", 1U, 1.0F, true},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     false,
     true,
     true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, k_elephant_clips.size()>
    k_elephant_clip_descs{{
        k_elephant_clips[0].desc,
        k_elephant_clips[1].desc,
        k_elephant_clips[2].desc,
        k_elephant_clips[3].desc,
        k_elephant_clips[4].desc,
        k_elephant_clips[5].desc,
    }};

void bake_elephant_manifest_clip_frame(std::size_t clip_index,
                                       std::uint32_t frame_index,
                                       std::vector<QMatrix4x4>& out_palettes,
                                       std::vector<QMatrix4x4>* out_socket_transforms) {
  (void)out_socket_transforms; // the howdah rides an authored bone, not a socket
  auto const& clip = k_elephant_clips[clip_index];
  float const phase =
      static_cast<float>(frame_index) /
      static_cast<float>(std::max<std::uint32_t>(clip.desc.frame_count, 1U));

  Render::Elephant::BonePalette palette{};
  std::string_view source_clip = "Idle";
  if (clip.is_moving) {
    source_clip = clip.desc.name == "run" ? "Run" : "Walk";
  } else if (clip.is_fighting) {
    source_clip = "Angry";
  } else if (clip.is_death) {
    source_clip = "Sitting";
  }
  float const source_phase = clip.is_dead_hold ? 1.0F : phase;
  if (!elephant_source_sample_clip(source_clip, source_phase, palette)) {
    auto const bind = elephant_source_bind_palette();
    std::copy(bind.begin(), bind.end(), palette.begin());
  }
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto elephant_manifest() noexcept -> const Render::Creature::SpeciesManifest& {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "elephant";
    m.species_id = Render::Creature::Bpat::k_species_elephant;
    m.bpat_file_name = "elephant.bpat";
    m.minimal_snapshot_file_name = "elephant_minimal.bpsm";
    m.topology = &elephant_topology();
    m.lod_full.primitive_name = "elephant.full.body";
    m.lod_full.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_full.color_role = k_elephant_role_skin;
    m.lod_full.material_id = k_elephant_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes = elephant_source_mesh_nodes();
    m.lod_minimal.primitive_name = "elephant.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_elephant_role_skin;
    m.lod_minimal.material_id = k_elephant_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = elephant_source_mesh_nodes();
    m.clips =
        std::span<const Render::Creature::BakeClipDescriptor>(k_elephant_clip_descs);
    m.bind_palette = &elephant_bind_palette;
    m.creature_spec = &elephant_creature_spec;
    m.bake_clip_frame = &bake_elephant_manifest_clip_frame;
    return m;
  }();
  return manifest;
}

} // namespace Render::Elephant
