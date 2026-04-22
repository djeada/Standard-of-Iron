#include "shield_roman.h"

#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../equipment_submit.h"

#include "../../render_archetype.h"

#include <array>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace {

enum RomanShieldPaletteSlot : std::uint8_t {
  k_shield_slot = 0U,
  k_trim_slot = 1U,
  k_metal_slot = 2U,
  k_grip_slot = 3U,
};

constexpr float k_shield_yaw_degrees = -70.0F;
constexpr float k_shield_width = 0.38F;
constexpr float k_shield_height = 0.90F;

auto roman_shield_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder{"roman_shield"};

    QVector3D const shield_center_local{-k_shield_width * 0.40F, 0.06F, 0.05F};

    QMatrix4x4 shield_body;
    shield_body.translate(shield_center_local);
    shield_body.rotate(90.0F, 0.0F, 1.0F, 0.0F);
    shield_body.scale(k_shield_width * 0.005F, k_shield_height * 0.5F, 0.24F);
    builder.add_palette_mesh(get_unit_cube(), shield_body, k_shield_slot,
                             nullptr, 1.0F, 4);

    float const rim_thickness = 0.020F;
    QVector3D const top_left =
        shield_center_local +
        QVector3D(-k_shield_width * 0.5F, k_shield_height * 0.5F, 0.0F);
    QVector3D const top_right =
        shield_center_local +
        QVector3D(k_shield_width * 0.5F, k_shield_height * 0.5F, 0.0F);
    QVector3D const bot_left =
        shield_center_local +
        QVector3D(-k_shield_width * 0.5F, -k_shield_height * 0.5F, 0.0F);
    QVector3D const bot_right =
        shield_center_local +
        QVector3D(k_shield_width * 0.5F, -k_shield_height * 0.5F, 0.0F);
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(top_left, top_right, rim_thickness), k_trim_slot,
        nullptr, 1.0F, 4);
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(bot_left, bot_right, rim_thickness), k_trim_slot,
        nullptr, 1.0F, 4);

    builder.add_palette_mesh(
        get_unit_sphere(),
        sphere_at(shield_center_local + QVector3D(0.0F, 0.0F, 0.05F), 0.07F),
        k_metal_slot, nullptr, 1.0F, 4);

    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(shield_center_local + QVector3D(-0.06F, 0.0F, -0.03F),
                         shield_center_local + QVector3D(0.06F, 0.0F, -0.03F),
                         0.012F),
        k_grip_slot, nullptr, 1.0F, 0);

    return std::move(builder).build();
  }();
  return archetype;
}

auto roman_shield_transform(const QMatrix4x4 &parent,
                            const QVector3D &origin) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.translate(origin);
  local.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
  return parent * local;
}

} // namespace

RomanShieldRenderer::RomanShieldRenderer()
    : ShieldRenderer([]() {
        ShieldRenderConfig config;
        config.shield_color = {0.65F, 0.15F, 0.15F};
        config.trim_color = {0.78F, 0.70F, 0.45F};
        config.metal_color = {0.72F, 0.73F, 0.78F};
        config.shield_radius = 0.18F;
        config.shield_aspect = 1.3F;
        config.has_cross_decal = false;
        return config;
      }()) {}

void RomanShieldRenderer::render(const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &anim,
                                 EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanShieldRenderer::submit(const RomanShieldConfig &,
                                 const DrawContext &ctx,
                                 const BodyFrames &frames,
                                 const HumanoidPalette &palette,
                                 const HumanoidAnimationContext &,
                                 EquipmentBatch &batch) {
  std::array<QVector3D, 4> const palette_slots{
      QVector3D(0.68F, 0.14F, 0.12F), QVector3D(0.88F, 0.75F, 0.42F),
      QVector3D(0.82F, 0.84F, 0.88F), palette.leather};
  append_equipment_archetype(
      batch, roman_shield_archetype(),
      roman_shield_transform(ctx.model, frames.hand_l.origin), palette_slots);
}

} // namespace Render::GL
