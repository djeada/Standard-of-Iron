#include "horse_bake_recipe.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "animation/rig/horse_gait.h"
#include "horse_manifest.h"
#include "horse_source_asset.h"
#include "horse_spec.h"

namespace Render::Horse {

namespace {

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

auto horse_bake_recipe() noexcept -> const Render::Creature::CreatureBakeRecipe& {
  static const Render::Creature::CreatureBakeRecipe recipe = [] {
    Render::Creature::CreatureBakeRecipe r;
    r.runtime = &horse_runtime_manifest();
    r.clips = std::span<const Render::Creature::BakeClipDescriptor>(k_horse_clip_descs);
    r.bake_clip_frame = &bake_horse_manifest_clip_frame;
    return r;
  }();
  return recipe;
}

} // namespace Render::Horse
