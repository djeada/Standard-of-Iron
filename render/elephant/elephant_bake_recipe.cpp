#include "elephant_bake_recipe.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "elephant_gait.h"
#include "elephant_manifest.h"
#include "elephant_source_asset.h"
#include "elephant_spec.h"

namespace Render::Elephant {

namespace {

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
    {{"walk", 48U, 48.0F, true},
     true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F},
     false,
     false,
     false,
     0.62F},
    {{"run", 32U, 48.0F, true},
     true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F},
     false,
     false,
     false,
     0.75F},
    {{"fight", 96U, 96.0F, true},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     true,
     false,
     false,
     0.0F},
    {{"die", 48U, 48.0F, false},
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

auto smooth_window(float phase, float begin, float end) -> float {
  float const t = std::clamp((phase - begin) / (end - begin), 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

void settle_death_pose(float phase, Render::Elephant::BonePalette& palette) {
  float const fall = smooth_window(phase, 0.22F, 0.80F);
  float const settle = smooth_window(phase, 0.80F, 1.0F);
  float const recoil = std::sin(settle * 6.2831853F) * (1.0F - settle);
  QMatrix4x4 roll;
  roll.translate(0.0F, 0.72F, 0.0F);
  roll.rotate(-82.0F * fall + 2.5F * recoil, 0.0F, 0.0F, 1.0F);
  roll.translate(0.0F, -0.72F, 0.0F);
  for (auto& bone : palette) {
    bone = roll * bone;
  }

  auto const bind = elephant_source_bind_palette();
  std::array<QMatrix4x4, k_elephant_source_bone_count> skin{};
  for (std::size_t bone = 0; bone < bind.size(); ++bone) {
    skin[bone] = palette[bone] * bind[bone].inverted();
  }
  float lowest = std::numeric_limits<float>::max();
  for (auto const& node : elephant_source_mesh_nodes()) {
    auto const& mesh = std::get<Render::Creature::Quadruped::CustomMeshNode>(node.data);
    for (auto const& vertex : mesh.vertices) {
      QVector3D const rest = bind[node.anchor_bone].map(
          {vertex.position[0], vertex.position[1], vertex.position[2]});
      QVector3D posed;
      for (std::size_t influence = 0; influence < 4; ++influence) {
        posed += skin[vertex.bone_indices[influence]].map(rest) *
                 vertex.bone_weights[influence];
      }
      lowest = std::min(lowest, posed.y());
    }
  }
  if (std::isfinite(lowest) && lowest != std::numeric_limits<float>::max()) {
    QMatrix4x4 ground;
    ground.translate(0.0F, -lowest, 0.0F);
    for (auto& bone : palette) {
      bone = ground * bone;
    }
  }
}

void bake_elephant_manifest_clip_frame(std::size_t clip_index,
                                       std::uint32_t frame_index,
                                       std::vector<QMatrix4x4>& out_palettes,
                                       std::vector<QMatrix4x4>* out_socket_transforms) {
  (void)out_socket_transforms;
  auto const& clip = k_elephant_clips[clip_index];

  std::uint32_t const divisor =
      clip.desc.loops ? clip.desc.frame_count
                      : std::max<std::uint32_t>(clip.desc.frame_count, 2U) - 1U;
  float const phase =
      static_cast<float>(frame_index) / static_cast<float>(std::max(divisor, 1U));

  Render::Elephant::BonePalette palette{};
  std::string_view source_clip = "Idle";
  if (clip.is_moving) {
    source_clip = clip.desc.name == "run" ? "Run" : "Walk";
  } else if (clip.is_fighting) {
    source_clip = "Angry";
  } else if (clip.is_death) {
    source_clip = "Sitting";
  }
  float const death_phase = clip.is_dead_hold ? 1.0F : phase;

  float const source_phase =
      clip.is_death ? 0.42F * smooth_window(death_phase, 0.0F, 0.58F) : phase;
  if (!elephant_source_sample_clip(source_clip, source_phase, palette)) {
    auto const bind = elephant_source_bind_palette();
    std::copy(bind.begin(), bind.end(), palette.begin());
  }
  if (clip.is_death) {
    settle_death_pose(death_phase, palette);
  }
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto elephant_bake_recipe() noexcept -> const Render::Creature::CreatureBakeRecipe& {
  static const Render::Creature::CreatureBakeRecipe recipe = [] {
    Render::Creature::CreatureBakeRecipe r;
    r.runtime = &elephant_runtime_manifest();
    r.clips =
        std::span<const Render::Creature::BakeClipDescriptor>(k_elephant_clip_descs);
    r.bake_clip_frame = &bake_elephant_manifest_clip_frame;
    return r;
  }();
  return recipe;
}

} // namespace Render::Elephant
