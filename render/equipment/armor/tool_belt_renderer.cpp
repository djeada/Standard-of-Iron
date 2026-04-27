#include "tool_belt_renderer.h"

#include "../../humanoid/humanoid_spec.h"
#include "../attachment_builder.h"
#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include <array>
#include <cmath>
#include <numbers>

namespace Render::GL {

namespace {

enum ToolBeltPaletteSlot : std::uint8_t {
  k_leather_slot = 0U,
  k_leather_dark_slot = 1U,
  k_metal_slot = 2U,
  k_metal_dark_slot = 3U,
  k_wood_slot = 4U,
};

constexpr float k_basis_radius = 0.28F;
constexpr float k_belt_y_norm = -0.02F / k_basis_radius;
constexpr float k_belt_thickness_norm = 0.022F / k_basis_radius;
constexpr float k_small_sphere_norm = 0.030F / k_basis_radius;
constexpr float k_pin_radius_norm = 0.008F / k_basis_radius;

auto tool_belt_ring_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 16> primitives{};
    constexpr int segs = 16;
    constexpr float pi = std::numbers::pi_v<float>;
    for (int i = 0; i < segs; ++i) {
      float const a1 = (static_cast<float>(i) / segs) * 2.0F * pi;
      float const a2 = (static_cast<float>(i + 1) / segs) * 2.0F * pi;
      primitives[i] = generated_cylinder(
          QVector3D(std::sin(a1), k_belt_y_norm, std::cos(a1)),
          QVector3D(std::sin(a2), k_belt_y_norm, std::cos(a2)),
          k_belt_thickness_norm, k_leather_slot);
    }
    return build_generated_equipment_archetype("tool_belt_ring", primitives);
  }();
  return archetype;
}

auto tool_belt_buckle_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 2> const primitives{{
        generated_sphere(QVector3D(-0.18F, k_belt_y_norm, 0.92F),
                         k_small_sphere_norm, k_metal_slot),
        generated_cylinder(QVector3D(-0.18F, k_belt_y_norm, 0.92F),
                           QVector3D(-0.055F, k_belt_y_norm, 0.92F),
                           k_pin_radius_norm, k_metal_dark_slot),
    }};
    return build_generated_equipment_archetype("tool_belt_buckle", primitives);
  }();
  return archetype;
}

auto tool_belt_hammer_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 5> const primitives{{
        generated_sphere(QVector3D(-0.891F, -0.179F, 0.454F), 0.050F,
                         k_leather_dark_slot),
        generated_sphere(QVector3D(-0.891F, -0.357F, 0.454F), 0.039F,
                         k_leather_dark_slot),
        generated_sphere(QVector3D(-0.891F, -0.536F, 0.454F), 0.029F,
                         k_leather_dark_slot),
        generated_cylinder(QVector3D(-0.891F, -0.286F, 0.454F),
                           QVector3D(-0.891F, -0.714F, 0.454F), 0.029F,
                           k_wood_slot),
        generated_cylinder(QVector3D(-0.980F, -0.232F, 0.454F),
                           QVector3D(-0.802F, -0.232F, 0.454F), 0.043F,
                           k_metal_dark_slot),
    }};
    return build_generated_equipment_archetype("tool_belt_hammer", primitives);
  }();
  return archetype;
}

auto tool_belt_chisel_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 3> const primitives{{
        generated_sphere(QVector3D(0.809F, -0.143F, 0.588F), 0.064F,
                         k_leather_dark_slot),
        generated_cylinder(QVector3D(0.809F, -0.214F, 0.588F),
                           QVector3D(0.809F, 0.143F, 0.588F), 0.021F,
                           k_metal_dark_slot),
        generated_sphere(QVector3D(0.809F, 0.143F, 0.588F), 0.029F,
                         k_metal_slot),
    }};
    return build_generated_equipment_archetype("tool_belt_chisel", primitives);
  }();
  return archetype;
}

auto tool_belt_pouches_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 24> primitives{};
    constexpr float pi = std::numbers::pi_v<float>;
    int index = 0;
    for (int side = 0; side < 2; ++side) {
      float const pouch_angle = (side == 0) ? 0.50F * pi : -0.50F * pi;
      float const side_sign = (side == 0) ? 1.0F : -1.0F;
      QVector3D const pouch_pos{0.95F * std::sin(pouch_angle), -0.214F,
                                0.85F * std::cos(pouch_angle)};
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
          float const x_off = (static_cast<float>(i) - 1.5F) * 0.0536F;
          float const y_off = static_cast<float>(j) * 0.0786F;
          float const radius = 0.0429F - static_cast<float>(j) * 0.0071F;
          primitives[index++] = generated_sphere(
              pouch_pos + QVector3D(x_off * side_sign, -y_off, 0.0F), radius,
              k_leather_dark_slot);
        }
      }
    }
    return build_generated_equipment_archetype("tool_belt_pouches", primitives);
  }();
  return archetype;
}

auto tool_belt_palette(const ToolBeltConfig &config)
    -> std::array<QVector3D, 5> {
  return {config.leather_color, config.leather_color * 0.90F,
          config.metal_color, config.metal_color * 0.92F, config.wood_color};
}

} // namespace

ToolBeltRenderer::ToolBeltRenderer(const ToolBeltConfig &config)
    : m_config(config) {}

void ToolBeltRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                              const HumanoidPalette &palette,
                              const HumanoidAnimationContext &anim,
                              EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void ToolBeltRenderer::submit(const ToolBeltConfig &config,
                              const DrawContext &ctx, const BodyFrames &frames,
                              const HumanoidPalette &,
                              const HumanoidAnimationContext &,
                              EquipmentBatch &batch) {
  const AttachmentFrame &waist = frames.waist;
  if (waist.radius <= 0.0F) {
    return;
  }

  float const waist_r = waist.radius * 1.05F;
  float const waist_d =
      (waist.depth > 0.0F) ? waist.depth * 0.90F : waist.radius * 0.80F;
  auto const palette = tool_belt_palette(config);

  append_humanoid_attachment_archetype_scaled(
      batch, ctx, waist, tool_belt_ring_archetype(),
      QVector3D(waist_r, waist.radius, waist_d), palette);
  append_humanoid_attachment_archetype(batch, ctx, waist,
                                       tool_belt_buckle_archetype(), palette);

  if (config.include_hammer) {
    append_humanoid_attachment_archetype(batch, ctx, waist,
                                         tool_belt_hammer_archetype(), palette);
  }
  if (config.include_chisel) {
    append_humanoid_attachment_archetype(batch, ctx, waist,
                                         tool_belt_chisel_archetype(), palette);
  }
  if (config.include_pouches) {
    append_humanoid_attachment_archetype(
        batch, ctx, waist, tool_belt_pouches_archetype(), palette);
  }
}

auto tool_belt_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                                std::size_t max) -> std::uint32_t {
  (void)palette;
  if (max < kToolBeltRoleCount) {
    return 0U;
  }
  constexpr ToolBeltConfig cfg{};
  out[0] = cfg.leather_color;
  out[1] = cfg.leather_color * 0.90F;
  out[2] = cfg.metal_color;
  out[3] = cfg.metal_color * 0.92F;
  out[4] = cfg.wood_color;
  return kToolBeltRoleCount;
}

auto tool_belt_make_static_attachments(std::uint16_t waist_socket_bone_index,
                                       std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 5> {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const AttachmentFrame &waist = bind_frames.waist;

  float const waist_r = waist.radius * 1.05F;
  float const waist_d =
      (waist.depth > 0.0F) ? waist.depth * 0.90F : waist.radius * 0.80F;

  QMatrix4x4 const ring_bind = make_humanoid_attachment_transform_scaled(
      QMatrix4x4{}, waist, QVector3D(0.0F, 0.0F, 0.0F),
      QVector3D(waist_r, waist.radius, waist_d));
  QMatrix4x4 const uniform_bind = make_humanoid_attachment_transform(
      QMatrix4x4{}, waist, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

  auto make_spec = [&](const RenderArchetype &arch,
                       const QMatrix4x4 &bind_mat) {
    auto spec = Render::Equipment::build_static_attachment({
        .archetype = &arch,
        .socket_bone_index = waist_socket_bone_index,
        .unit_local_pose_at_bind = bind_mat,
    });
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(kToolBeltRoleCount);
         ++i) {
      spec.palette_role_remap[i] =
          static_cast<std::uint8_t>(base_role_byte + i);
    }
    return spec;
  };

  return {
      make_spec(tool_belt_ring_archetype(), ring_bind),
      make_spec(tool_belt_buckle_archetype(), uniform_bind),
      make_spec(tool_belt_hammer_archetype(), uniform_bind),
      make_spec(tool_belt_chisel_archetype(), uniform_bind),
      make_spec(tool_belt_pouches_archetype(), uniform_bind),
  };
}

} // namespace Render::GL
