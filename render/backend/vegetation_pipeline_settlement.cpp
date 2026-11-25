#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "render/backend/abandoned_home_parts.h"
#include "render/backend/cursed_gold_vein_mesh.h"
#include "render/backend/magic_shrine_parts.h"
#include "render/backend/mesh_buffers.h"
#include "render/backend/prop_mesh_builder.h"
#include "render/backend/prop_parts.h"
#include "render/backend/ruins_parts.h"
#include "render/backend/static_mesh_upload.h"
#include "render/backend/statue_parts.h"
#include "render/backend/supply_cart_parts.h"
#include "render/backend/tent_mesh.h"
#include "render/backend/vegetation_pipeline.h"
#include "render/backend/weapon_rack_mesh.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

void VegetationPipeline::initialize_fire_camp_pipeline() {
  initializeOpenGLFunctions();
  m_firecamp_mesh.reset();

  struct FireCampVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal{};
  };

  constexpr float k_flame_part = 0.0F;
  constexpr float k_hearth_part = 3.0F;
  constexpr float k_smoke_part = 4.0F;
  constexpr float k_stone_part = 5.0F;
  constexpr int k_hearth_segments = 18;
  constexpr int k_stones = 9;
  constexpr int k_stone_sides = 6;

  std::vector<FireCampVertex> vertices;
  vertices.reserve(8 + 1 + k_hearth_segments + k_stones * k_stone_sides * 9);
  std::vector<unsigned short> indices;
  indices.reserve(12 + k_hearth_segments * 3 + k_stones * k_stone_sides * 9);

  auto append_card = [&](float part) {
    auto const base = static_cast<unsigned short>(vertices.size());
    vertices.push_back({QVector3D(-1.0F, 0.0F, part), QVector2D(0.0F, 0.0F)});
    vertices.push_back({QVector3D(1.0F, 0.0F, part), QVector2D(1.0F, 0.0F)});
    vertices.push_back({QVector3D(1.0F, 1.0F, part), QVector2D(1.0F, 1.0F)});
    vertices.push_back({QVector3D(-1.0F, 1.0F, part), QVector2D(0.0F, 1.0F)});
    for (int const corner : {0, 1, 2, 0, 2, 3}) {
      indices.push_back(static_cast<unsigned short>(base + corner));
    }
  };

  auto const hearth_centre = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 0.0F, k_hearth_part), QVector2D(0.0F, 0.0F)});
  for (int segment = 0; segment < k_hearth_segments; ++segment) {
    float const angle = MathConstants::k_two_pi * static_cast<float>(segment) /
                        static_cast<float>(k_hearth_segments);
    vertices.push_back({QVector3D(0.0F, 0.0F, k_hearth_part),
                        QVector2D(std::cos(angle), std::sin(angle))});
    indices.push_back(hearth_centre);
    indices.push_back(static_cast<unsigned short>(hearth_centre + 1 + segment));
    indices.push_back(static_cast<unsigned short>(hearth_centre + 1 +
                                                  ((segment + 1) % k_hearth_segments)));
  }
  // Local XZ is stored in the same coordinates as the ash disc. A buried
  // rim and an offset shoulder give each fieldstone a flat, chipped crown.
  for (int stone = 0; stone < k_stones; ++stone) {
    float const bearing = MathConstants::k_two_pi * stone / k_stones;
    QVector3D const radial(std::cos(bearing), 0.0F, std::sin(bearing));
    QVector3D const tangent(-radial.z(), 0.0F, radial.x());
    auto const centre = radial * (0.77F + 0.025F * std::sin(stone * 2.4F));
    float const height = 0.12F + 0.032F * std::cos(stone * 3.1F);
    auto const crown = centre + radial * 0.035F + QVector3D(0.0F, height * 1.2F, 0.0F);
    auto rim = [&](int side, bool upper) {
      float const a = MathConstants::k_two_pi * side / k_stone_sides + 0.17F * stone;
      auto const offset = tangent * (0.25F * std::cos(a)) + radial * (0.16F * std::sin(a));
      return centre + offset * (upper ? 0.73F : 1.0F) +
             QVector3D(0.0F, upper ? height : -0.038F, 0.0F);
    };
    auto face = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c) {
      auto const normal = QVector3D::crossProduct(b - a, c - a).normalized();
      for (auto const& point : {a, b, c}) {
        indices.push_back(static_cast<unsigned short>(vertices.size()));
        vertices.push_back({{0.0F, point.y(), k_stone_part}, {point.x(), point.z()}, normal});
      }
    };
    for (int side = 0; side < k_stone_sides; ++side) {
      auto const a = rim(side, false);
      auto const b = rim(side + 1, false);
      auto const c = rim(side + 1, true);
      auto const d = rim(side, true);
      face(a, b, c);
      face(a, c, d);
      face(d, c, crown);
    }
  }
  append_card(k_smoke_part);
  append_card(k_flame_part);

  constexpr std::array<VertexAttributeLayout, 3> k_firecamp_attributes{{
      {0, vec3, offsetof(FireCampVertex, position)},
      {1, vec2, offsetof(FireCampVertex, tex_coord)},
      {2, vec3, offsetof(FireCampVertex, normal)},
  }};
  constexpr std::array<GLuint, 3> k_firecamp_instance_locations{3, 4, 5};
  upload_static_instanced_mesh(*this,
                               m_firecamp_mesh,
                               vertices.data(),
                               vertices.size(),
                               sizeof(FireCampVertex),
                               k_firecamp_attributes,
                               indices.data(),
                               indices.size(),
                               k_firecamp_instance_locations);
}

void VegetationPipeline::initialize_tent_pipeline() {
  initializeOpenGLFunctions();
  m_tent_mesh.reset();

  PropMeshData mesh = build_tent_mesh();
  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_tent_mesh);
}

void VegetationPipeline::initialize_supply_cart_pipeline() {
  initializeOpenGLFunctions();
  m_supply_cart_mesh.reset();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::SupplyCartParts;

  append_parts(verts, idx, std::span{k_supply_cart_boxes});
  append_parts(verts, idx, std::span{k_supply_cart_beams});
  append_parts(verts, idx, std::span{k_supply_cart_tapers});

  constexpr int k_wheel_sides = 24;
  constexpr float k_front_wheel_r = 0.26F;
  constexpr float k_rear_wheel_r = 0.34F;
  constexpr float k_front_wheel_t = 0.052F;
  constexpr float k_rear_wheel_t = 0.052F;

  append_spoked_wheel_xaxis(verts,
                            idx,
                            -0.78F,
                            0.26F,
                            -0.50F,
                            k_front_wheel_r,
                            k_front_wheel_t,
                            k_wheel_sides,
                            8);
  append_spoked_wheel_xaxis(verts,
                            idx,
                            0.78F,
                            0.26F,
                            -0.50F,
                            k_front_wheel_r,
                            k_front_wheel_t,
                            k_wheel_sides,
                            8);
  append_spoked_wheel_xaxis(verts,
                            idx,
                            -0.82F,
                            0.34F,
                            0.44F,
                            k_rear_wheel_r,
                            k_rear_wheel_t,
                            k_wheel_sides,
                            10);
  append_spoked_wheel_xaxis(verts,
                            idx,
                            0.82F,
                            0.34F,
                            0.44F,
                            k_rear_wheel_r,
                            k_rear_wheel_t,
                            k_wheel_sides,
                            10);

  for (int plank = 0; plank < 6; ++plank) {
    float const z = -0.545F + 0.180F * static_cast<float>(plank);
    append_box(verts, idx, {-0.545F, 0.540F, z}, {0.545F, 0.556F, z + 0.150F});
  }

  for (float const side : {-1.0F, 1.0F}) {
    for (float const z : {-0.54F, -0.06F, 0.42F}) {
      append_box(verts,
                 idx,
                 {side * 0.58F - 0.055F, 0.45F, z - 0.045F},
                 {side * 0.58F + 0.055F, 1.02F, z + 0.045F});
    }
  }

  auto add_barrel = [&](float cx, float cz, float r, float height) {
    float const y0 = 0.556F;
    constexpr int k_profile_steps = 8;
    for (int step = 0; step < k_profile_steps; ++step) {
      float const t0 = static_cast<float>(step) / k_profile_steps;
      float const t1 = static_cast<float>(step + 1) / k_profile_steps;
      float const r0 = r * (0.86F + 0.14F * std::sin(t0 * 3.14159265F));
      float const r1 = r * (0.86F + 0.14F * std::sin(t1 * 3.14159265F));
      append_prop_taper(
          verts, idx, cx, y0 + height * t0, cz, r0, r1, height * (t1 - t0), 16);
    }
    for (float const t : {0.10F, 0.50F, 0.90F}) {
      float const hoop_r = r * (0.86F + 0.14F * std::sin(t * 3.14159265F)) + 0.004F;
      float const hoop_top_r =
          r * (0.86F + 0.14F * std::sin((t + 0.055F) * 3.14159265F)) + 0.004F;
      append_prop_taper(
          verts, idx, cx, y0 + height * t, cz, hoop_r, hoop_top_r, height * 0.055F, 16);
    }
    append_prop_taper(
        verts, idx, cx, y0 + height, cz, r * 0.86F, r * 0.80F, 0.020F, 16);
  };

  add_barrel(-0.28F, -0.14F, 0.185F, 0.62F);
  add_barrel(0.22F, 0.06F, 0.170F, 0.56F);

  auto add_sack = [&](float cx, float cy, float cz, float r, float height) {
    constexpr std::array<float, 7> profile{
        0.72F, 0.94F, 1.0F, 0.91F, 0.70F, 0.34F, 0.22F};
    for (std::size_t i = 0; i + 1 < profile.size(); ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(profile.size() - 1);
      append_prop_taper(verts,
                        idx,
                        cx,
                        cy + height * t,
                        cz,
                        r * profile[i],
                        r * profile[i + 1],
                        height / static_cast<float>(profile.size() - 1),
                        16);
    }
    append_prop_beam(verts,
                     idx,
                     {cx - r * 0.34F, cy + height * 1.02F, cz},
                     {cx + r * 0.34F, cy + height * 1.06F, cz},
                     0.030F,
                     0.026F);
  };

  add_sack(-0.24F, 0.556F, 0.36F, 0.185F, 0.40F);
  add_sack(0.20F, 0.556F, 0.44F, 0.165F, 0.34F);
  add_sack(0.30F, 0.556F, -0.36F, 0.150F, 0.30F);

  for (float const side : {-1.0F, 1.0F}) {
    append_prop_beam(verts,
                     idx,
                     {-0.06F + side * 0.052F, 1.030F, -0.42F},
                     {-0.06F + side * 0.130F, 0.900F, -0.42F},
                     0.020F,
                     0.020F);
  }

  for (int hoop = 0; hoop < 2; ++hoop) {
    float const z = -0.09F + 0.53F * static_cast<float>(hoop);
    constexpr int k_arc = 10;
    for (int seg = 0; seg < k_arc; ++seg) {
      float const a0 =
          3.14159265F * static_cast<float>(seg) / static_cast<float>(k_arc);
      float const a1 =
          3.14159265F * static_cast<float>(seg + 1) / static_cast<float>(k_arc);
      append_prop_beam(verts,
                       idx,
                       {-std::cos(a0) * 0.58F, 0.96F + std::sin(a0) * 0.38F, z},
                       {-std::cos(a1) * 0.58F, 0.96F + std::sin(a1) * 0.38F, z},
                       0.030F,
                       0.032F);
    }
  }

  // Stretch a short tilt over the rear hoops, leaving the forward load visible.
  // Bow the fabric between supports and let the hem droop at each end.
  constexpr int k_panels = 12;
  auto cover_point = [](float angle, float z, float sag) {
    return QVector3D(-std::cos(angle) * 0.612F,
                     0.96F + std::sin(angle) * (0.425F - sag), z);
  };
  constexpr std::array<float, 5> k_cover_z{-0.14F, -0.09F, 0.175F, 0.44F, 0.50F};
  constexpr std::array<float, 5> k_cover_sag{0.014F, 0.0F, 0.035F, 0.0F, 0.016F};
  for (int panel = 0; panel < k_panels; ++panel) {
    float const a = 3.14159265F * panel / k_panels;
    float const b = 3.14159265F * (panel + 1) / k_panels;
    for (std::size_t row = 1; row < k_cover_z.size(); ++row) {
      auto const p0 = cover_point(a, k_cover_z[row - 1], k_cover_sag[row - 1]);
      auto const p1 = cover_point(a, k_cover_z[row], k_cover_sag[row]);
      auto const p2 = cover_point(b, k_cover_z[row], k_cover_sag[row]);
      auto const p3 = cover_point(b, k_cover_z[row - 1], k_cover_sag[row - 1]);
      auto const normal = QVector3D::crossProduct(p1 - p0, p3 - p0).normalized();
      // Half-length normals select the existing canvas material on both sides.
      append_quad(verts, idx, p0, p1, p2, p3, normal * 0.5F);
      QVector3D const lining(0.0F, 0.008F, 0.0F);
      append_quad(verts, idx, p3 - lining, p2 - lining, p1 - lining, p0 - lining,
                  normal * -0.5F);
    }
  }

  upload_prop_mesh_impl(verts, idx, m_supply_cart_mesh);
}

void VegetationPipeline::initialize_weapon_rack_pipeline() {
  initializeOpenGLFunctions();
  m_weapon_rack_mesh.reset();

  WeaponRackMeshData const mesh = build_weapon_rack_mesh();
  upload_prop_mesh_with_surface_impl(
      mesh.vertices, mesh.surface, mesh.indices, m_weapon_rack_mesh);
}

void VegetationPipeline::initialize_ruins_pipeline() {
  initializeOpenGLFunctions();
  m_ruins_mesh.reset();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::RuinsParts;

  append_parts(verts, idx, std::span{k_ruins_boxes});
  append_parts(verts, idx, std::span{k_ruins_prisms});
  append_parts(verts, idx, std::span{k_ruins_oriented_boxes});

  upload_prop_mesh_impl(verts, idx, m_ruins_mesh);
}

void VegetationPipeline::initialize_abandoned_home_pipeline() {
  initializeOpenGLFunctions();
  m_abandoned_home_mesh.reset();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::AbandonedHomeParts;

  append_parts(verts, idx, std::span{k_abandoned_home_boxes});
  auto const timber_start = verts.size();
  append_parts(verts, idx, std::span{k_abandoned_home_oriented_boxes});
  // As with tent materials, normal length tags the surface before normalization.
  for (auto i = timber_start; i < verts.size(); ++i) {
    verts[i].second *= 0.5F;
  }

  upload_prop_mesh_impl(verts, idx, m_abandoned_home_mesh);
}

void VegetationPipeline::initialize_statue_pipeline() {
  initializeOpenGLFunctions();
  m_statue_mesh.reset();

  using namespace Render::GL::BackendPipelines::StatueParts;

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  append_parts(verts, idx, std::span{k_statue_slabs});
  append_parts(verts, idx, std::span{k_statue_beams});
  append_parts(verts, idx, std::span{k_statue_limbs});
  append_parts(verts, idx, std::span{k_statue_frustums});

  for (int strip = 0; strip < 11; ++strip) {
    float const angle = 6.28318530F * (static_cast<float>(strip) + 0.5F) / 11.0F;
    float const cs = std::cos(angle);
    float const sn = std::sin(angle);
    float const drop = 0.074F + 0.026F * std::cos(angle * 2.0F);
    append_prop_limb(verts,
                     idx,
                     {0.002F + 0.122F * cs, 1.842F, 0.160F * sn},
                     {0.002F + 0.132F * cs, 1.842F - drop, 0.174F * sn},
                     0.032F,
                     0.026F,
                     6);
  }

  for (int leaf = 0; leaf < 13; ++leaf) {
    float const angle = 6.28318530F * static_cast<float>(leaf) / 13.0F;
    float const next = 6.28318530F * static_cast<float>(leaf + 1) / 13.0F;
    append_prop_limb(verts,
                     idx,
                     {0.008F + 0.076F * std::cos(angle),
                      2.404F + 0.010F * std::sin(angle * 3.0F),
                      0.079F * std::sin(angle)},
                     {0.008F + 0.076F * std::cos(next),
                      2.404F + 0.010F * std::sin(next * 3.0F),
                      0.079F * std::sin(next)},
                     0.019F,
                     0.016F,
                     6);
  }

  for (int fold = 0; fold < 5; ++fold) {
    float const t = (static_cast<float>(fold) - 2.0F) * 0.5F;
    float const z = t * 0.115F;
    float const belly = 0.012F * (1.0F - std::fabs(t));
    float const bottom =
        1.502F + 0.064F * std::fabs(t) + 0.030F * static_cast<float>(fold % 2);
    append_prop_beam(verts,
                     idx,
                     {-0.150F - belly, 2.150F, z + 0.028F},
                     {-0.196F - belly, bottom, z + 0.018F},
                     0.034F,
                     0.024F);
  }

  upload_prop_mesh_impl(verts, idx, m_statue_mesh);
}

void VegetationPipeline::initialize_magic_shrine_pipeline() {
  initializeOpenGLFunctions();
  m_magic_shrine_mesh.reset();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::MagicShrineParts;

  append_parts(verts, idx, std::span{k_magic_shrine_boxes});
  append_parts(verts, idx, std::span{k_magic_shrine_tapers});
  append_parts(verts, idx, std::span{k_magic_shrine_limbs});
  append_parts(verts, idx, std::span{k_magic_shrine_beams});

  upload_prop_mesh_impl(verts, idx, m_magic_shrine_mesh);
}

void VegetationPipeline::initialize_cursed_gold_vein_pipeline() {
  initializeOpenGLFunctions();
  m_cursed_gold_vein_mesh.reset();

  PropMeshData mesh = build_cursed_gold_vein_mesh();
  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_cursed_gold_vein_mesh);
}

} // namespace Render::GL::BackendPipelines
