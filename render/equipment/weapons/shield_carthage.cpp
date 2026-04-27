#include "shield_carthage.h"

#include "../../geom/transforms.h"
#include "../../gl/mesh.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/skeleton.h"
#include "../equipment_submit.h"

#include "../../render_archetype.h"
#include "../attachment_builder.h"

#include <array>
#include <cmath>
#include <deque>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace {

enum CarthageShieldPaletteSlot : std::uint8_t {
  k_shield_slot = 0U,
  k_trim_slot = 1U,
  k_metal_slot = 2U,
  k_grip_slot = 3U,
};

auto create_unit_hemisphere_mesh(int lat_segments = 12, int lon_segments = 32)
    -> std::unique_ptr<Mesh> {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve((lat_segments + 1) * (lon_segments + 1));

  for (int lat = 0; lat <= lat_segments; ++lat) {
    float const v = static_cast<float>(lat) / static_cast<float>(lat_segments);
    float const phi = v * (std::numbers::pi_v<float> * 0.5F);
    float const z = std::cos(phi);
    float const ring_r = std::sin(phi);

    for (int lon = 0; lon <= lon_segments; ++lon) {
      float const u =
          static_cast<float>(lon) / static_cast<float>(lon_segments);
      float const theta = u * 2.0F * std::numbers::pi_v<float>;
      float const x = ring_r * std::cos(theta);
      float const y = ring_r * std::sin(theta);

      QVector3D normal(x, y, z);
      normal.normalize();
      vertices.push_back(
          {{x, y, z}, {normal.x(), normal.y(), normal.z()}, {u, v}});
    }
  }

  int const row = lon_segments + 1;
  for (int lat = 0; lat < lat_segments; ++lat) {
    for (int lon = 0; lon < lon_segments; ++lon) {
      int const a = lat * row + lon;
      int const b = a + 1;
      int const c = (lat + 1) * row + lon + 1;
      int const d = (lat + 1) * row + lon;

      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(c);
      indices.push_back(c);
      indices.push_back(d);
      indices.push_back(a);
    }
  }

  return std::make_unique<Mesh>(vertices, indices);
}

auto get_unit_hemisphere_mesh() -> Mesh * {
  static std::unique_ptr<Mesh> const mesh = create_unit_hemisphere_mesh();
  return mesh.get();
}

constexpr float k_base_shield_diameter = 1.08F;

auto shield_center_local(float shield_radius) -> QVector3D {
  return {-shield_radius * 0.45F, -0.02F, 0.05F};
}

auto carthage_shield_archetype(float scale_multiplier)
    -> const RenderArchetype & {
  struct CachedArchetype {
    int scale_key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const scale_key = std::lround(scale_multiplier * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.scale_key == scale_key) {
      return entry.archetype;
    }
  }

  float const shield_radius =
      (k_base_shield_diameter * 0.5F) * scale_multiplier;
  float const dome_depth = shield_radius * 0.48F;
  QVector3D const shield_center = shield_center_local(shield_radius);

  RenderArchetypeBuilder builder{"carthage_shield_" +
                                 std::to_string(scale_key)};

  QMatrix4x4 dome;
  dome.translate(shield_center);
  dome.scale(shield_radius, shield_radius, dome_depth);
  builder.add_palette_mesh(get_unit_hemisphere_mesh(), dome, k_shield_slot,
                           nullptr, 1.0F, 4);

  constexpr int rim_segments = 24;
  for (int i = 0; i < rim_segments; ++i) {
    float const a0 =
        (static_cast<float>(i) / static_cast<float>(rim_segments)) * 2.0F *
        std::numbers::pi_v<float>;
    float const a1 =
        (static_cast<float>(i + 1) / static_cast<float>(rim_segments)) * 2.0F *
        std::numbers::pi_v<float>;

    QVector3D const p0 =
        shield_center + QVector3D(shield_radius * std::cos(a0),
                                  shield_radius * std::sin(a0), 0.0F);
    QVector3D const p1 =
        shield_center + QVector3D(shield_radius * std::cos(a1),
                                  shield_radius * std::sin(a1), 0.0F);
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(p0, p1, 0.012F), k_trim_slot,
                             nullptr, 1.0F, 4);
  }

  QVector3D const emblem_plane =
      shield_center + QVector3D(0.0F, 0.0F, dome_depth * 0.92F);
  QMatrix4x4 medallion;
  medallion.translate(emblem_plane);
  medallion.scale(shield_radius * 0.34F, shield_radius * 0.34F,
                  shield_radius * 0.08F);
  builder.add_palette_mesh(get_unit_cylinder(), medallion, k_trim_slot, nullptr,
                           1.0F, 4);

  QVector3D const emblem_body_top =
      emblem_plane + QVector3D(0.0F, shield_radius * 0.14F, 0.0F);
  QVector3D const emblem_body_bot =
      emblem_plane - QVector3D(0.0F, shield_radius * 0.08F, 0.0F);
  float const emblem_radius = shield_radius * 0.028F;

  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(emblem_body_bot, emblem_body_top, emblem_radius),
      k_metal_slot, nullptr, 1.0F, 4);
  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(emblem_plane + QVector3D(-shield_radius * 0.22F,
                                                shield_radius * 0.02F, 0.0F),
                       emblem_plane + QVector3D(shield_radius * 0.22F,
                                                shield_radius * 0.02F, 0.0F),
                       emblem_radius * 0.75F),
      k_metal_slot, nullptr, 1.0F, 4);
  builder.add_palette_mesh(
      get_unit_sphere(),
      sphere_at(emblem_body_top + QVector3D(0.0F, shield_radius * 0.05F, 0.0F),
                emblem_radius * 1.4F),
      k_metal_slot, nullptr, 1.0F, 4);
  builder.add_palette_mesh(
      get_unit_cone(),
      cone_from_to(emblem_body_bot -
                       QVector3D(0.0F, shield_radius * 0.04F, 0.0F),
                   emblem_plane - QVector3D(0.0F, shield_radius * 0.22F, 0.0F),
                   emblem_radius * 1.6F),
      k_metal_slot, nullptr, 1.0F, 4);

  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(shield_center + QVector3D(-0.035F, 0.0F, -0.030F),
                       shield_center + QVector3D(0.035F, 0.0F, -0.030F),
                       0.010F),
      k_grip_slot, nullptr, 1.0F, 4);

  cache.push_back({scale_key, std::move(builder).build()});
  return cache.back().archetype;
}

auto hand_basis_transform(const QMatrix4x4 &parent,
                          const AttachmentFrame &hand) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(hand.right, 0.0F));
  local.setColumn(1, QVector4D(hand.up, 0.0F));
  local.setColumn(2, QVector4D(hand.forward, 0.0F));
  local.setColumn(3, QVector4D(hand.origin, 1.0F));
  return parent * local;
}

auto carthage_shield_local_pose(float scale_multiplier) -> QMatrix4x4 {
  float const shield_radius =
      (k_base_shield_diameter * 0.5F) * scale_multiplier;
  QVector3D const shield_center = shield_center_local(shield_radius);
  QMatrix4x4 pose;
  pose.translate(shield_center);
  pose.rotate(90.0F, 0.0F, 1.0F, 0.0F);
  pose.translate(-shield_center);
  return pose;
}

} // namespace

CarthageShieldRenderer::CarthageShieldRenderer(float scale_multiplier)
    : ShieldRenderer([]() {
        ShieldRenderConfig config;
        config.shield_color = {0.20F, 0.46F, 0.62F};
        config.trim_color = {0.76F, 0.68F, 0.42F};
        config.metal_color = {0.70F, 0.68F, 0.52F};
        config.shield_radius = k_base_shield_diameter * 0.5F;
        config.shield_aspect = 1.0F;
        config.has_cross_decal = false;
        return config;
      }()),
      m_scale_multiplier(scale_multiplier) {}

void CarthageShieldRenderer::render(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &anim,
                                    EquipmentBatch &batch) {
  submit({m_scale_multiplier}, ctx, frames, palette, anim, batch);
}

void CarthageShieldRenderer::submit(const CarthageShieldConfig &config,
                                    const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &,
                                    EquipmentBatch &batch) {
  AttachmentFrame const grip =
      frames.grip_l.radius > 0.0F
          ? frames.grip_l
          : Render::Humanoid::socket_attachment_frame(
                frames.hand_l, Render::Humanoid::HumanoidSocket::GripL);
  std::array<QVector3D, 4> const palette_slots{
      QVector3D(0.20F, 0.46F, 0.62F), QVector3D(0.76F, 0.68F, 0.42F),
      QVector3D(0.70F, 0.68F, 0.52F), palette.leather};
  append_equipment_archetype(
      batch, carthage_shield_archetype(config.scale_multiplier),
      hand_basis_transform(ctx.model, grip) *
          carthage_shield_local_pose(config.scale_multiplier),
      palette_slots);
}

auto carthage_shield_fill_role_colors(const HumanoidPalette &palette,
                                      QVector3D *out,
                                      std::size_t max) -> std::uint32_t {
  if (max < kCarthageShieldRoleCount) {
    return 0;
  }
  out[k_shield_slot] = QVector3D(0.20F, 0.46F, 0.62F);
  out[k_trim_slot] = QVector3D(0.76F, 0.68F, 0.42F);
  out[k_metal_slot] = QVector3D(0.70F, 0.68F, 0.52F);
  out[k_grip_slot] = palette.leather;
  return kCarthageShieldRoleCount;
}

auto carthage_shield_make_static_attachment(const CarthageShieldConfig &config,
                                            std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandL;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          k_bone)];
  auto const &bind_grip = Render::Humanoid::humanoid_bind_body_frames().grip_l;
  QMatrix4x4 const bind_socket = hand_basis_transform(QMatrix4x4{}, bind_grip);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &carthage_shield_archetype(config.scale_multiplier),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = carthage_shield_local_pose(config.scale_multiplier),
  });
  spec.palette_role_remap[k_shield_slot] = base_role_byte;
  spec.palette_role_remap[k_trim_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_metal_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_grip_slot] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  return spec;
}

} // namespace Render::GL
