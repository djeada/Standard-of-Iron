#include "shield_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>

#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_cache_key.h"
#include "render/equipment/equipment_submit.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/render_archetype.h"
#include "shield_anchor.h"

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace {

enum ShieldPaletteSlot : std::uint8_t {
  k_shield_slot = 0U,
  k_back_slot = 1U,
  k_trim_slot = 2U,
  k_inner_ring_slot = 3U,
  k_metal_slot = 4U,
  k_grip_slot = 5U,
};

constexpr float k_scale_factor = 3.2F;

struct ShieldArchetypeKey {
  int radius_key{0};
  int aspect_key{0};
  bool has_cross_decal{false};
  bool has_spine{false};
  int dome_key{0};
  int material_id{0};
  bool has_radial_decoration{false};
};

auto operator==(const ShieldArchetypeKey& lhs, const ShieldArchetypeKey& rhs) -> bool {
  return lhs.radius_key == rhs.radius_key && lhs.aspect_key == rhs.aspect_key &&
         lhs.has_cross_decal == rhs.has_cross_decal && lhs.has_spine == rhs.has_spine &&
         lhs.dome_key == rhs.dome_key && lhs.material_id == rhs.material_id &&
         lhs.has_radial_decoration == rhs.has_radial_decoration;
}

auto quantize_shield_value(float value) -> int {
  return equipment_key(value);
}

auto shield_center_local(const ShieldRenderConfig& config) -> QVector3D {
  static_cast<void>(config);
  return {};
}

auto shield_archetype(const ShieldRenderConfig& config) -> const RenderArchetype& {
  struct CachedArchetype {
    ShieldArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;

  ShieldArchetypeKey const key{quantize_shield_value(config.shield_radius),
                               quantize_shield_value(config.shield_aspect),
                               config.has_cross_decal,
                               config.has_spine,
                               quantize_shield_value(config.dome_depth),
                               config.material_id,
                               config.has_radial_decoration};
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const base_extent = config.shield_radius * k_scale_factor;
  float const shield_width = base_extent;
  float const shield_height = base_extent * config.shield_aspect;
  float const min_extent = std::min(shield_width, shield_height);
  float const plate_half = 0.0025F;
  float const plate_full = plate_half * 2.0F;

  QVector3D const grip_center_local{0.0F, -0.02F, 0.0F};
  QVector3D const shield_center = shield_center_local(config);

  RenderArchetypeBuilder builder{
      "shield_" + std::to_string(key.radius_key) + "_" +
      std::to_string(key.aspect_key) + "_" +
      std::to_string(static_cast<int>(key.has_cross_decal)) + "_" +
      std::to_string(key.material_id) + (key.has_spine ? "_spine" : "") + "_d" +
      std::to_string(key.dome_key) + (key.has_radial_decoration ? "_painted" : "")};

  QMatrix4x4 front_plate;
  front_plate.translate(shield_center + QVector3D(0.0F, 0.0F, plate_half));
  front_plate.scale(shield_width, shield_height, plate_full);
  front_plate.rotate(90.0F, 1.0F, 0.0F, 0.0F);
  builder.add_palette_mesh(get_unit_cylinder(),
                           front_plate,
                           k_shield_slot,
                           nullptr,
                           1.0F,
                           config.material_id);

  QMatrix4x4 back_plate;
  back_plate.translate(shield_center - QVector3D(0.0F, 0.0F, plate_half));
  back_plate.scale(shield_width * 0.985F, shield_height * 0.985F, plate_full);
  back_plate.rotate(90.0F, 1.0F, 0.0F, 0.0F);
  builder.add_palette_mesh(
      get_unit_cylinder(), back_plate, k_back_slot, nullptr, 1.0F, config.material_id);

  float const dome = min_extent * std::max(0.0F, config.dome_depth);
  if (dome > 0.0F) {
    QMatrix4x4 dome_shell;
    dome_shell.translate(shield_center);
    dome_shell.scale(shield_width * 0.985F, shield_height * 0.985F, dome);
    builder.add_palette_mesh(get_unit_sphere(),
                             dome_shell,
                             k_shield_slot,
                             nullptr,
                             1.0F,
                             config.material_id);
  }
  auto face_z = [&](float x, float y) {
    float const nx = x / (shield_width * 0.985F);
    float const ny = y / (shield_height * 0.985F);
    return plate_full + dome * std::sqrt(std::max(0.0F, 1.0F - nx * nx - ny * ny));
  };
  auto face_point = [&](float x, float y) {
    return shield_center + QVector3D(x, y, face_z(x, y));
  };

  auto add_ring =
      [&](float width, float height, float thickness, ShieldPaletteSlot slot) {
        constexpr int k_segments = 18;
        for (int i = 0; i < k_segments; ++i) {
          float const a0 = static_cast<float>(i) / static_cast<float>(k_segments) *
                           2.0F * std::numbers::pi_v<float>;
          float const a1 = static_cast<float>(i + 1) / static_cast<float>(k_segments) *
                           2.0F * std::numbers::pi_v<float>;
          QVector3D const p0 = face_point(width * std::cos(a0), height * std::sin(a0));
          QVector3D const p1 = face_point(width * std::cos(a1), height * std::sin(a1));
          builder.add_palette_mesh(get_unit_cylinder(),
                                   cylinder_between(p0, p1, thickness),
                                   slot,
                                   nullptr,
                                   1.0F,
                                   config.material_id);
        }
      };

  add_ring(shield_width, shield_height, min_extent * 0.010F, k_trim_slot);
  add_ring(shield_width * 0.72F,
           shield_height * 0.72F,
           min_extent * 0.006F,
           k_inner_ring_slot);

  if (config.has_radial_decoration) {
    // Follow the dome so the painted rays and rivets remain on its surface.
    constexpr int k_rays = 8;
    constexpr int k_ray_segments = 4;
    for (int ray = 0; ray < k_rays; ++ray) {
      float const angle = (static_cast<float>(ray) + 0.5F) *
                          (2.0F * std::numbers::pi_v<float> / k_rays);
      for (int segment = 0; segment < k_ray_segments; ++segment) {
        float const r0 = 0.38F + 0.065F * static_cast<float>(segment);
        float const r1 = r0 + 0.065F;
        builder.add_palette_mesh(
            get_unit_cylinder(),
            cylinder_between(face_point(shield_width * r0 * std::cos(angle),
                                        shield_height * r0 * std::sin(angle)),
                             face_point(shield_width * r1 * std::cos(angle),
                                        shield_height * r1 * std::sin(angle)),
                             min_extent * 0.018F),
            k_trim_slot,
            nullptr,
            1.0F,
            0);
      }
      builder.add_palette_mesh(
          get_unit_sphere(),
          sphere_at(face_point(shield_width * 0.91F * std::cos(angle),
                               shield_height * 0.91F * std::sin(angle)),
                    min_extent * 0.024F),
          k_metal_slot,
          nullptr,
          1.0F,
          3);
    }
  }

  if (config.has_spine) {
    constexpr int k_spine_segments = 8;
    float const spine_r = min_extent * 0.045F;
    for (int i = 0; i < k_spine_segments; ++i) {
      float const y0 = shield_height * 0.94F *
                       (-1.0F + 2.0F * static_cast<float>(i) / k_spine_segments);
      float const y1 = shield_height * 0.94F *
                       (-1.0F + 2.0F * static_cast<float>(i + 1) / k_spine_segments);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(
              shield_center + QVector3D(0.0F, y0, face_z(0.0F, y0) + 0.002F),
              shield_center + QVector3D(0.0F, y1, face_z(0.0F, y1) + 0.002F),
              spine_r),
          k_trim_slot,
          nullptr,
          1.0F,
          config.material_id);
    }

    QMatrix4x4 boss;
    boss.translate(shield_center + QVector3D(0.0F, 0.0F, face_z(0.0F, 0.0F)));
    boss.scale(min_extent * 0.20F, min_extent * 0.36F, 0.018F * k_scale_factor);
    builder.add_palette_mesh(
        get_unit_sphere(), boss, k_metal_slot, nullptr, 1.0F, config.material_id);
  } else if (config.has_radial_decoration) {
    // Keep the caetra's low bronze boss in proportion to the small shield.
    QMatrix4x4 flange;
    flange.translate(face_point(0.0F, 0.0F));
    flange.scale(min_extent * 0.29F, min_extent * 0.29F, min_extent * 0.055F);
    builder.add_palette_mesh(get_unit_sphere(), flange, k_metal_slot, nullptr, 1.0F, 3);
    QMatrix4x4 boss;
    boss.translate(face_point(0.0F, 0.0F));
    boss.scale(min_extent * 0.22F, min_extent * 0.22F, min_extent * 0.18F);
    builder.add_palette_mesh(get_unit_sphere(), boss, k_metal_slot, nullptr, 1.0F, 3);
  } else {
    builder.add_palette_mesh(
        get_unit_sphere(),
        sphere_at(shield_center +
                      QVector3D(0.0F, 0.0F, std::max(0.02F * k_scale_factor, dome)),
                  0.045F * k_scale_factor),
        k_metal_slot,
        nullptr,
        1.0F,
        config.material_id);
  }

  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(grip_center_local + QVector3D(-0.035F, 0.0F, 0.012F),
                       grip_center_local + QVector3D(0.035F, 0.0F, 0.012F),
                       0.010F),
      k_grip_slot,
      nullptr,
      1.0F,
      config.material_id);

  if (config.has_cross_decal) {
    QVector3D const center_front =
        shield_center + QVector3D(0.0F, 0.0F, plate_full * 0.5F + 0.0015F);
    float const bar_radius = min_extent * 0.10F;

    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(center_front + QVector3D(0.0F, shield_height * 0.90F, 0.0F),
                         center_front - QVector3D(0.0F, shield_height * 0.90F, 0.0F),
                         bar_radius),
        k_trim_slot,
        nullptr,
        1.0F,
        config.material_id);
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(center_front - QVector3D(shield_width * 0.90F, 0.0F, 0.0F),
                         center_front + QVector3D(shield_width * 0.90F, 0.0F, 0.0F),
                         bar_radius),
        k_trim_slot,
        nullptr,
        1.0F,
        config.material_id);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto shield_basis_transform(const QMatrix4x4& parent,
                            const AttachmentFrame& shield_frame) -> QMatrix4x4 {
  return parent * attachment_frame_transform(shield_frame);
}

auto shield_local_pose(const ShieldRenderConfig& config) -> QMatrix4x4 {
  QMatrix4x4 pose;
  pose.translate(shield_center_local(config));
  pose.rotate(90.0F, 0.0F, 1.0F, 0.0F);
  pose.translate(-shield_center_local(config));
  return pose;
}

} // namespace

ShieldRenderer::ShieldRenderer(ShieldRenderConfig config)
    : m_base(config) {
}

void ShieldRenderer::render(const DrawContext& ctx,
                            const BodyFrames& frames,
                            const HumanoidPalette& palette,
                            const HumanoidAnimationContext& anim,
                            EquipmentBatch& batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void ShieldRenderer::submit(const ShieldRenderConfig& m_config,
                            const DrawContext& ctx,
                            const BodyFrames& frames,
                            const HumanoidPalette& palette,
                            const HumanoidAnimationContext&,
                            EquipmentBatch& batch) {
  std::array<QVector3D, 6> const palette_slots{m_config.shield_color,
                                               palette.leather * 0.8F,
                                               m_config.trim_color * 0.95F,
                                               palette.leather * 0.90F,
                                               m_config.metal_color,
                                               palette.leather};
  AttachmentFrame const shield_frame = resolve_left_shield_frame(frames);
  QMatrix4x4 pose_adjustment;
  if (frames.shield_l.radius > 0.0F) {
    pose_adjustment = bind_left_shield_pose_calibration();
  }
  append_equipment_archetype(batch,
                             shield_archetype(m_config),
                             shield_basis_transform(ctx.model, shield_frame) *
                                 pose_adjustment * shield_local_pose(m_config),
                             palette_slots);
}

auto shield_fill_role_colors(const HumanoidPalette& palette,
                             const ShieldRenderConfig& config,
                             QVector3D* out,
                             std::size_t max) -> std::uint32_t {
  if (max < k_shield_role_count) {
    return 0;
  }
  out[k_shield_slot] = config.shield_color;
  out[k_back_slot] = palette.leather * 0.8F;
  out[k_trim_slot] = config.trim_color * 0.95F;
  out[k_inner_ring_slot] = palette.leather * 0.90F;
  out[k_metal_slot] = config.metal_color;
  out[k_grip_slot] = palette.leather;
  return k_shield_role_count;
}

auto shield_make_static_attachment(const ShieldRenderConfig& config,
                                   std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandL;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  auto const& bind_shield = Render::Humanoid::humanoid_bind_body_frames().shield_l;
  QMatrix4x4 const bind_socket = attachment_frame_transform(bind_shield);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &shield_archetype(config),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket =
          bind_left_shield_pose_calibration() * shield_local_pose(config),
  });
  spec.palette_role_remap[k_shield_slot] = base_role_byte;
  spec.palette_role_remap[k_back_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_trim_slot] = static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_inner_ring_slot] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  spec.palette_role_remap[k_metal_slot] =
      static_cast<std::uint8_t>(base_role_byte + 4U);
  spec.palette_role_remap[k_grip_slot] = static_cast<std::uint8_t>(base_role_byte + 5U);
  return spec;
}

} // namespace Render::GL
