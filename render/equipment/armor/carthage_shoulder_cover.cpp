#include "carthage_shoulder_cover.h"
#include "shoulder_cover_archetype.h"

#include "../../humanoid/humanoid_specs.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>

namespace Render::GL {

namespace {

enum CarthageShoulderPaletteSlot : std::uint8_t {
  k_leather_base_slot = 0U,
  k_leather_dark_slot = 1U,
};

auto carthage_shoulder_cover_archetype(float outward_scale)
    -> const RenderArchetype & {
  struct CachedArchetype {
    int key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const key = std::lround(outward_scale * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  using HP = HumanProportions;

  bool const is_infantry = outward_scale <= 1.1F;
  float const outward_offset =
      (is_infantry ? 0.0035F : 0.0065F) * outward_scale;
  float const inward_offset = (is_infantry ? 0.012F : 0.018F) * outward_scale;
  float const upper_y_offset = is_infantry ? 0.062F : 0.074F;
  QVector3D const anchor{-inward_offset, 0.0F, 0.0F};
  QVector3D const upper_center =
      anchor + QVector3D(outward_offset, upper_y_offset, 0.0F);
  QVector3D const lower_center =
      upper_center + QVector3D(outward_offset * 0.75F, -0.055F, 0.0F);
  QVector3D const trim_center =
      lower_center + QVector3D(outward_offset * 0.55F, -0.030F, 0.0F);

  std::array<ShoulderCoverLobe, 3> const lobes{{
      {upper_center,
       QVector3D(HP::UPPER_ARM_R * 1.75F, HP::UPPER_ARM_R * 0.38F,
                 HP::UPPER_ARM_R * 1.55F),
       k_leather_base_slot},
      {lower_center,
       QVector3D(HP::UPPER_ARM_R * 1.58F, HP::UPPER_ARM_R * 0.34F,
                 HP::UPPER_ARM_R * 1.40F),
       k_leather_base_slot},
      {trim_center,
       QVector3D(HP::UPPER_ARM_R * 1.42F, HP::UPPER_ARM_R * 0.18F,
                 HP::UPPER_ARM_R * 1.25F),
       k_leather_dark_slot},
  }};

  std::string const debug_name =
      "carthage_shoulder_cover_" + std::to_string(key);
  cache.push_back({key, build_shoulder_cover_archetype(debug_name, lobes)});
  return cache.back().archetype;
}

auto carthage_shoulder_cover_palette() -> std::array<QVector3D, 2> {
  QVector3D const leather_color(0.44F, 0.30F, 0.19F);
  return {leather_color * 1.05F, leather_color * 0.78F};
}

} // namespace

void CarthageShoulderCoverRenderer::render(const DrawContext &ctx,
                                           const BodyFrames &frames,
                                           const HumanoidPalette &palette,
                                           const HumanoidAnimationContext &anim,
                                           EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void CarthageShoulderCoverRenderer::submit(
    const CarthageShoulderCoverConfig &config, const DrawContext &ctx,
    const BodyFrames &frames, const HumanoidPalette &palette,
    const HumanoidAnimationContext &anim, EquipmentBatch &batch) {
  (void)anim;
  (void)palette;

  auto const palette_colors = carthage_shoulder_cover_palette();
  auto const &archetype =
      carthage_shoulder_cover_archetype(config.outward_scale);
  QVector3D const up = frames.torso.up;
  QVector3D const right = frames.torso.right;

  append_shoulder_cover_archetype(batch, ctx, frames.shoulder_l.origin, -right,
                                  up, archetype, palette_colors);
  append_shoulder_cover_archetype(batch, ctx, frames.shoulder_r.origin, right,
                                  up, archetype, palette_colors);
}

} // namespace Render::GL
