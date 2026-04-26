#include "roman_shoulder_cover.h"
#include "shoulder_cover_archetype.h"

#include "../../humanoid/humanoid_specs.h"
#include "../attachment_builder.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>

namespace Render::GL {

namespace {

enum RomanShoulderPaletteSlot : std::uint8_t {
  k_metal_base_slot = 0U,
  k_metal_dark_slot = 1U,
  k_edge_highlight_slot = 2U,
};

auto roman_shoulder_cover_archetype(float outward_scale)
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
  float const outward_offset = (is_infantry ? 0.003F : 0.009F) * outward_scale;
  float const upward_offset = is_infantry ? 0.052F : 0.054F;
  float const back_offset = (is_infantry ? -0.018F : -0.012F) * outward_scale;
  QVector3D const anchor{outward_offset, upward_offset, back_offset};

  std::array<ShoulderCoverLobe, 3> const lobes{{
      {anchor,
       QVector3D(HP::UPPER_ARM_R * 1.38F, HP::UPPER_ARM_R * 1.10F,
                 HP::UPPER_ARM_R * 1.22F),
       k_metal_base_slot},
      {anchor + QVector3D(0.006F * outward_scale, -0.030F, 0.0F),
       QVector3D(HP::UPPER_ARM_R * 1.22F, HP::UPPER_ARM_R * 0.94F,
                 HP::UPPER_ARM_R * 1.05F),
       k_metal_dark_slot},
      {anchor + QVector3D(0.012F * outward_scale, -0.058F, 0.0F),
       QVector3D(HP::UPPER_ARM_R * 1.10F, HP::UPPER_ARM_R * 0.40F,
                 HP::UPPER_ARM_R * 0.98F),
       k_edge_highlight_slot},
  }};

  std::string const debug_name = "roman_shoulder_cover_" + std::to_string(key);
  cache.push_back({key, build_shoulder_cover_archetype(debug_name, lobes)});
  return cache.back().archetype;
}

auto roman_shoulder_cover_palette() -> std::array<QVector3D, 3> {
  QVector3D const metal_base(0.76F, 0.77F, 0.80F);
  return {metal_base, metal_base * 0.82F, QVector3D(0.90F, 0.90F, 0.94F)};
}

} // namespace

void RomanShoulderCoverRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void RomanShoulderCoverRenderer::submit(const RomanShoulderCoverConfig &config,
                                        const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        EquipmentBatch &batch) {
  (void)anim;
  (void)palette;

  auto const palette_colors = roman_shoulder_cover_palette();
  auto const &archetype = roman_shoulder_cover_archetype(config.outward_scale);
  QVector3D const up = frames.torso.up;
  QVector3D const right = frames.torso.right;

  append_shoulder_cover_archetype(batch, ctx, frames.shoulder_l.origin, -right,
                                  up, archetype, palette_colors);
  append_shoulder_cover_archetype(batch, ctx, frames.shoulder_r.origin, right,
                                  up, archetype, palette_colors);
}

auto roman_shoulder_cover_archetype() -> const RenderArchetype & {
  return roman_shoulder_cover_archetype(1.0F);
}

auto roman_shoulder_cover_fill_role_colors(const HumanoidPalette &palette,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t {
  (void)palette;
  if (max < kRomanShoulderCoverRoleCount) {
    return 0;
  }
  auto const colors = roman_shoulder_cover_palette();
  out[0] = colors[0];
  out[1] = colors[1];
  out[2] = colors[2];
  return kRomanShoulderCoverRoleCount;
}

auto roman_shoulder_cover_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const QMatrix4x4 &bind_shoulder_frame)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_shoulder_cover_archetype(),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_shoulder_frame,
  });
  spec.palette_role_remap[k_metal_base_slot] = base_role_byte;
  spec.palette_role_remap[k_metal_dark_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_edge_highlight_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  return spec;
}

} // namespace Render::GL
