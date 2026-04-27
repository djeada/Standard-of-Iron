#include "roman_greaves.h"

#include "../../entity/registry.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/style_palette.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"

#include "../../render_archetype.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanGreavesPaletteSlot : std::uint8_t {
  k_greaves_slot = 0U,
};

constexpr int k_num_segments = 3;
constexpr std::array<float, k_num_segments> k_segment_angles{-0.8F, 0.0F, 0.8F};

auto make_leg_attachment_transform(const QMatrix4x4 &parent,
                                   const AttachmentFrame &shin) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(shin.right, 0.0F));
  local.setColumn(1, QVector4D(shin.up, 0.0F));
  local.setColumn(2, QVector4D(shin.forward, 0.0F));
  local.setColumn(3, QVector4D(shin.origin, 1.0F));
  return parent * local;
}

auto greave_segment_local_model(float shin_radius, float angle) -> QMatrix4x4 {
  using HP = HumanProportions;

  float const shin_length = HP::LOWER_LEG_LEN;
  float const greave_start = shin_length * 0.10F;
  float const greave_end = shin_length * 0.92F;
  float const greave_length = greave_end - greave_start;
  float const greave_offset = shin_radius * 1.08F;
  float const greave_thickness = 0.006F;
  float const segment_width = shin_radius * 0.55F;

  float const cos_a = std::cos(angle);
  float const sin_a = std::sin(angle);
  QVector3D const segment_center(
      greave_offset * sin_a, shin_length - ((greave_start + greave_end) * 0.5F),
      greave_offset * cos_a);
  QVector3D const segment_normal(sin_a, 0.0F, cos_a);
  QVector3D const segment_tangent(cos_a, 0.0F, -sin_a);

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(segment_tangent * segment_width, 0.0F));
  local.setColumn(1,
                  QVector4D(QVector3D(0.0F, greave_length * 0.5F, 0.0F), 0.0F));
  local.setColumn(2, QVector4D(segment_normal * greave_thickness, 0.0F));
  local.setColumn(3, QVector4D(segment_center, 1.0F));
  return local;
}

auto roman_greaves_archetype(float shin_radius) -> const RenderArchetype & {
  struct CachedArchetype {
    int key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const key = std::lround(shin_radius * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{"roman_greaves_" + std::to_string(key)};
  for (float angle : k_segment_angles) {
    builder.add_palette_mesh(get_unit_cube(),
                             greave_segment_local_model(shin_radius, angle),
                             k_greaves_slot, nullptr, 1.0F, 5);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

void RomanGreavesRenderer::render(const DrawContext &ctx,
                                  const BodyFrames &frames,
                                  const HumanoidPalette &palette,
                                  const HumanoidAnimationContext &anim,
                                  EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanGreavesRenderer::submit(const RomanGreavesConfig &,
                                  const DrawContext &ctx,
                                  const BodyFrames &frames,
                                  const HumanoidPalette &palette,
                                  const HumanoidAnimationContext &anim,
                                  EquipmentBatch &batch) {
  (void)anim;

  QVector3D const greaves_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.88F, 0.68F));
  std::array<QVector3D, 1> const palette_slots{greaves_color};

  auto render_greave = [&](const AttachmentFrame &shin) {
    if (shin.radius <= 0.0F) {
      return;
    }

    append_equipment_archetype(batch, roman_greaves_archetype(shin.radius),
                               make_leg_attachment_transform(ctx.model, shin),
                               palette_slots);
  };

  render_greave(frames.shin_l);
  render_greave(frames.shin_r);
}

auto roman_greaves_archetype() -> const RenderArchetype & {
  using HP = HumanProportions;
  return roman_greaves_archetype(HP::LOWER_LEG_R);
}

auto roman_greaves_fill_role_colors(const HumanoidPalette &palette,
                                    QVector3D *out,
                                    std::size_t max) -> std::uint32_t {
  if (max < kRomanGreavesRoleCount) {
    return 0;
  }
  out[0] = saturate_color(palette.metal * QVector3D(0.95F, 0.88F, 0.68F));
  return kRomanGreavesRoleCount;
}

auto roman_greaves_make_static_attachment(std::uint16_t socket_bone_index,
                                          std::uint8_t base_role_byte,
                                          const QMatrix4x4 &bind_shin_frame)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_greaves_archetype(),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_shin_frame,
  });
  spec.palette_role_remap[k_greaves_slot] = base_role_byte;
  return spec;
}

} // namespace Render::GL
