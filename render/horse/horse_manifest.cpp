#include "horse_manifest.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/rig/horse_gait.h"
#include "horse_source_asset.h"
#include "horse_spec.h"

namespace Render::Horse {

namespace {

constexpr std::uint8_t k_role_coat = 1;

struct HorseClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  Render::GL::GaitType gait;
  bool is_moving{};
  bool is_fighting{false};
  bool is_death{false};
  bool is_dead_hold{false};
  float bob_scale{0.0F};
};

const std::array<HorseClipSpec, 8> k_horse_clips{{
    {{"idle", 24U, 24.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     false,
     false,
     false,
     0.0F},
    {{"walk", 24U, 24.0F, true},
     Render::GL::GaitType::WALK,
     true,
     false,
     false,
     false,
     0.50F},
    {{"trot", 16U, 24.0F, true},
     Render::GL::GaitType::TROT,
     true,
     false,
     false,
     false,
     0.85F},
    {{"canter", 16U, 24.0F, true},
     Render::GL::GaitType::CANTER,
     true,
     false,
     false,
     false,
     1.00F},
    {{"gallop", 12U, 24.0F, true},
     Render::GL::GaitType::GALLOP,
     true,
     false,
     false,
     false,
     1.12F},
    {{"fight", 24U, 24.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     true,
     false,
     false,
     0.0F},
    {{"die", 20U, 24.0F, false},
     Render::GL::GaitType::IDLE,
     false,
     false,
     true,
     false,
     0.0F},
    {{"dead", 1U, 1.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     false,
     true,
     true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, k_horse_clips.size()>
    k_horse_clip_descs{{
        k_horse_clips[0].desc,
        k_horse_clips[1].desc,
        k_horse_clips[2].desc,
        k_horse_clips[3].desc,
        k_horse_clips[4].desc,
        k_horse_clips[5].desc,
        k_horse_clips[6].desc,
        k_horse_clips[7].desc,
    }};

void bake_horse_manifest_clip_frame(std::size_t clip_index,
                                    std::uint32_t frame_index,
                                    std::vector<QMatrix4x4>& out_palettes,
                                    std::vector<QMatrix4x4>* out_socket_transforms) {
  (void)out_socket_transforms;
  auto const& clip = k_horse_clips[clip_index];
  float const phase =
      static_cast<float>(frame_index) /
      static_cast<float>(std::max<std::uint32_t>(clip.desc.frame_count, 1U));
  std::string_view source_clip = "Idle";
  switch (clip.gait) {
  case Render::GL::GaitType::WALK:
  case Render::GL::GaitType::TROT:
    source_clip = "Walk";
    break;
  case Render::GL::GaitType::CANTER:
  case Render::GL::GaitType::GALLOP:
    source_clip = "Gallop";
    break;
  default:
    break;
  }
  if (clip.is_fighting) {
    source_clip = "Attack_Kick";
  } else if (clip.is_death) {
    source_clip = "Death";
  }
  float const source_phase = clip.is_dead_hold ? 1.0F : phase;
  Render::Horse::BonePalette palette{};
  if (!horse_source_sample_clip(source_clip, source_phase, palette)) {
    auto const bind = horse_source_bind_palette();
    std::copy(bind.begin(), bind.end(), palette.begin());
  }
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto horse_manifest() noexcept -> const Render::Creature::SpeciesManifest& {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "horse";
    m.species_id = Render::Creature::Bpat::k_species_horse;
    m.bpat_file_name = "horse.bpat";
    m.minimal_snapshot_file_name = "horse_minimal.bpsm";
    m.topology = &horse_topology();
    m.lod_full.primitive_name = "horse.full.body";
    m.lod_full.anchor_bone = static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_full.color_role = k_role_coat;
    m.lod_full.material_id = k_horse_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes = horse_source_mesh_nodes();
    m.lod_minimal.primitive_name = "horse.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::Authored;
    m.lod_minimal.color_role = k_role_coat;
    m.lod_minimal.material_id = k_horse_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes = horse_source_mesh_nodes();
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(k_horse_clip_descs);
    m.bind_palette = &horse_bind_palette;
    m.creature_spec = &horse_creature_spec;
    m.bake_clip_frame = &bake_horse_manifest_clip_frame;
    return m;
  }();
  return manifest;
}

} // namespace Render::Horse
