#include "shield_carthage.h"
#include "../../geom/transforms.h"
#include "../../gl/mesh.h"
#include "../../gl/primitives.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace {

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

} // namespace

CarthageShieldRenderer::CarthageShieldRenderer(float scale_multiplier)
    : m_scale_multiplier(scale_multiplier) {
  ShieldRenderConfig config;
  config.shield_color = {0.20F, 0.46F, 0.62F};
  config.trim_color = {0.76F, 0.68F, 0.42F};
  config.metal_color = {0.70F, 0.68F, 0.52F};
  config.shield_radius = 0.18F * 0.9F;
  config.shield_aspect = 1.0F;
  config.has_cross_decal = false;

  set_config(config);
}

void CarthageShieldRenderer::render(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &,
                                    ISubmitter &submitter) {
  constexpr float k_shield_yaw_degrees = -70.0F;
  constexpr float k_scale_factor = 2.5F;

  QMatrix4x4 rot;
  rot.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);

  const QVector3D n = rot.map(QVector3D(0.0F, 0.0F, 1.0F));
  const QVector3D axis_x = rot.map(QVector3D(1.0F, 0.0F, 0.0F));
  const QVector3D axis_y = rot.map(QVector3D(0.0F, 1.0F, 0.0F));

  float const shield_radius =
      0.18F * 0.9F * k_scale_factor * m_scale_multiplier;

  QVector3D shield_center = frames.hand_l.origin +
                            axis_x * (-shield_radius * 0.35F) +
                            axis_y * (-0.05F) + n * (0.06F);

  QVector3D const shield_color{0.20F, 0.46F, 0.62F};
  QVector3D const trim_color{0.76F, 0.68F, 0.42F};
  QVector3D const metal_color{0.70F, 0.68F, 0.52F};

  float const dome_depth = shield_radius * 0.55F;
  {
    QMatrix4x4 m = ctx.model;
    m.translate(shield_center);
    m.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
    m.scale(shield_radius, shield_radius, dome_depth);

    submitter.mesh(get_unit_hemisphere_mesh(), m, shield_color, nullptr, 1.0F,
                   4);
  }

  constexpr int rim_segments = 24;
  for (int i = 0; i < rim_segments; ++i) {
    float const a0 =
        (static_cast<float>(i) / static_cast<float>(rim_segments)) * 2.0F *
        std::numbers::pi_v<float>;
    float const a1 =
        (static_cast<float>(i + 1) / static_cast<float>(rim_segments)) * 2.0F *
        std::numbers::pi_v<float>;

    QVector3D const p0 = shield_center +
                         axis_x * (shield_radius * std::cos(a0)) +
                         axis_y * (shield_radius * std::sin(a0));
    QVector3D const p1 = shield_center +
                         axis_x * (shield_radius * std::cos(a1)) +
                         axis_y * (shield_radius * std::sin(a1));

    submitter.mesh(get_unit_cylinder(),
                   cylinder_between(ctx.model, p0, p1, 0.012F), trim_color,
                   nullptr, 1.0F, 4);
  }

  QVector3D const emblem_plane = shield_center + n * (dome_depth * 0.92F);
  {
    QMatrix4x4 medallion = ctx.model;
    medallion.translate(emblem_plane);
    medallion.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
    medallion.scale(shield_radius * 0.34F, shield_radius * 0.34F,
                    shield_radius * 0.08F);
    submitter.mesh(get_unit_cylinder(), medallion, trim_color * 0.95F, nullptr,
                   1.0F, 4);
  }

  QVector3D const emblem_body_top =
      emblem_plane + axis_y * (shield_radius * 0.14F);
  QVector3D const emblem_body_bot =
      emblem_plane - axis_y * (shield_radius * 0.08F);
  float const emblem_radius = shield_radius * 0.028F;

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, emblem_body_bot, emblem_body_top,
                                  emblem_radius),
                 metal_color, nullptr, 1.0F, 4);

  QVector3D const emblem_arm_height =
      emblem_plane + axis_y * (shield_radius * 0.02F);
  QVector3D const emblem_arm_left =
      emblem_arm_height - axis_x * (shield_radius * 0.22F);
  QVector3D const emblem_arm_right =
      emblem_arm_height + axis_x * (shield_radius * 0.22F);

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, emblem_arm_left, emblem_arm_right,
                                  emblem_radius * 0.75F),
                 metal_color, nullptr, 1.0F, 4);

  submitter.mesh(get_unit_sphere(),
                 sphere_at(ctx.model,
                           emblem_body_top + axis_y * (shield_radius * 0.05F),
                           emblem_radius * 1.4F),
                 metal_color, nullptr, 1.0F, 4);

  submitter.mesh(
      get_unit_cone(),
      cone_from_to(ctx.model,
                   emblem_body_bot - axis_y * (shield_radius * 0.04F),
                   emblem_plane - axis_y * (shield_radius * 0.22F),
                   emblem_radius * 1.6F),
      metal_color, nullptr, 1.0F, 4);

  QVector3D const grip_a = shield_center - axis_x * 0.035F - n * 0.030F;
  QVector3D const grip_b = shield_center + axis_x * 0.035F - n * 0.030F;
  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, grip_a, grip_b, 0.010F),
                 palette.leather, nullptr, 1.0F, 4);
}

} // namespace Render::GL
