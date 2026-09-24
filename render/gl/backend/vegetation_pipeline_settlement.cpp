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

#include "gl/shader_cache.h"
#include "mesh_buffers.h"
#include "prop_mesh_builder.h"
#include "render/gl/backend/abandoned_home_parts.h"
#include "render/gl/backend/cursed_gold_vein_mesh.h"
#include "render/gl/backend/magic_shrine_parts.h"
#include "render/gl/backend/prop_parts.h"
#include "render/gl/backend/ruins_parts.h"
#include "render/gl/backend/static_mesh_upload.h"
#include "render/gl/backend/statue_parts.h"
#include "render/gl/backend/supply_cart_parts.h"
#include "render/gl/backend/tent_mesh.h"
#include "render/gl/backend/weapon_rack_mesh.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "vegetation_pipeline.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

void VegetationPipeline::initialize_fire_camp_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_firecamp_mesh);

  struct FireCampVertex {
    QVector3D position;
    QVector2D tex_coord;
  };

  constexpr std::size_t k_firecamp_vertex_reserve = 12;
  constexpr std::size_t k_firecamp_index_reserve = 18;
  std::vector<FireCampVertex> vertices;
  vertices.reserve(k_firecamp_vertex_reserve);
  std::vector<unsigned short> indices;
  indices.reserve(k_firecamp_index_reserve);

  auto append_plane = [&](float plane_index) {
    auto const base = static_cast<unsigned short>(vertices.size());
    vertices.push_back({QVector3D(-1.0F, 0.0F, plane_index), QVector2D(0.0F, 0.0F)});
    vertices.push_back({QVector3D(1.0F, 0.0F, plane_index), QVector2D(1.0F, 0.0F)});
    vertices.push_back({QVector3D(1.0F, 2.0F, plane_index), QVector2D(1.0F, 1.0F)});
    vertices.push_back({QVector3D(-1.0F, 2.0F, plane_index), QVector2D(0.0F, 1.0F)});

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
  };

  append_plane(0.0F);
  append_plane(1.0F);
  append_plane(2.0F);

  constexpr std::array<VertexAttributeLayout, 2> k_firecamp_attributes{{
      {0, vec3, offsetof(FireCampVertex, position)},
      {1, vec2, offsetof(FireCampVertex, tex_coord)},
  }};
  constexpr std::array<GLuint, 2> k_firecamp_instance_locations{3, 4};
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
  release_mesh_buffers(*this, m_tent_mesh);

  PropMeshData mesh = build_tent_mesh();
  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_tent_mesh);
}

void VegetationPipeline::initialize_supply_cart_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_supply_cart_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::SupplyCartParts;

  append_parts(verts, idx, std::span{k_supply_cart_boxes});
  append_parts(verts, idx, std::span{k_supply_cart_beams});
  append_parts(verts, idx, std::span{k_supply_cart_tapers});

  constexpr int k_wheel_sides = 20;
  constexpr float k_front_wheel_r = 0.26F;
  constexpr float k_rear_wheel_r = 0.34F;
  constexpr float k_front_wheel_t = 0.070F;
  constexpr float k_rear_wheel_t = 0.090F;

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
    append_prop_taper(verts, idx, cx, y0, cz, r * 0.86F, r, height * 0.34F, 12);
    append_prop_taper(
        verts, idx, cx, y0 + height * 0.34F, cz, r, r, height * 0.32F, 12);
    append_prop_taper(
        verts, idx, cx, y0 + height * 0.66F, cz, r, r * 0.86F, height * 0.34F, 12);
    for (float const t : {0.10F, 0.50F, 0.90F}) {
      float const hoop_r = r * (t > 0.05F && t < 0.95F ? 1.045F : 0.90F);
      append_prop_taper(
          verts, idx, cx, y0 + height * t, cz, hoop_r, hoop_r, height * 0.055F, 12);
    }
    append_prop_taper(
        verts, idx, cx, y0 + height, cz, r * 0.86F, r * 0.80F, 0.020F, 12);
  };

  add_barrel(-0.28F, -0.14F, 0.185F, 0.62F);
  add_barrel(0.22F, 0.06F, 0.170F, 0.56F);

  auto add_sack = [&](float cx, float cy, float cz, float r, float height) {
    append_prop_taper(verts, idx, cx, cy, cz, r * 0.80F, r, height * 0.42F, 10);
    append_prop_taper(
        verts, idx, cx, cy + height * 0.42F, cz, r, r * 0.62F, height * 0.44F, 10);
    append_prop_taper(verts,
                      idx,
                      cx,
                      cy + height * 0.86F,
                      cz,
                      r * 0.62F,
                      r * 0.30F,
                      height * 0.14F,
                      10);
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

  for (int lash = 0; lash < 3; ++lash) {
    float const x = -0.32F + 0.32F * static_cast<float>(lash);
    append_prop_beam(
        verts, idx, {x, 1.108F, 0.10F}, {x, 0.912F, 0.10F}, 0.020F, 0.092F);
  }

  for (int hoop = 0; hoop < 2; ++hoop) {
    float const z = 0.10F + 0.34F * static_cast<float>(hoop);
    constexpr int k_arc = 5;
    for (int seg = 0; seg < k_arc; ++seg) {
      float const a0 =
          3.14159265F * static_cast<float>(seg) / static_cast<float>(k_arc);
      float const a1 =
          3.14159265F * static_cast<float>(seg + 1) / static_cast<float>(k_arc);
      append_prop_beam(verts,
                       idx,
                       {-std::cos(a0) * 0.58F, 0.96F + std::sin(a0) * 0.40F, z},
                       {-std::cos(a1) * 0.58F, 0.96F + std::sin(a1) * 0.40F, z},
                       0.030F,
                       0.032F);
    }
  }

  upload_prop_mesh_impl(verts, idx, m_supply_cart_mesh);
}

void VegetationPipeline::initialize_weapon_rack_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_weapon_rack_mesh);

  WeaponRackMeshData const mesh = build_weapon_rack_mesh();
  upload_prop_mesh_with_surface_impl(
      mesh.vertices, mesh.surface, mesh.indices, m_weapon_rack_mesh);
}

void VegetationPipeline::initialize_ruins_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_ruins_mesh);

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
  release_mesh_buffers(*this, m_abandoned_home_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::AbandonedHomeParts;

  append_parts(verts, idx, std::span{k_abandoned_home_boxes});
  append_parts(verts, idx, std::span{k_abandoned_home_oriented_boxes});

  for (int course = 0; course < 5; ++course) {
    float const y = 0.24F + 0.20F * static_cast<float>(course);
    append_box(verts, idx, {-0.885F, y, 0.58F}, {0.80F, y + 0.022F, 0.585F});
    append_box(verts, idx, {-0.895F, y, -0.70F}, {-0.885F, y + 0.022F, 0.70F});
  }

  for (int course = 0; course < 5; ++course) {
    float const step = static_cast<float>(course);
    float const z0 = -0.76F + 0.148F * step;
    float const y0 = 1.06F + 0.092F * step;
    append_box(verts, idx, {-0.58F, y0, z0}, {0.88F, y0 + 0.092F, z0 + 0.148F});
  }

  upload_prop_mesh_impl(verts, idx, m_abandoned_home_mesh);
}

void VegetationPipeline::initialize_statue_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_statue_mesh);

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
  release_mesh_buffers(*this, m_magic_shrine_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  using namespace Render::GL::BackendPipelines::MagicShrineParts;

  append_parts(verts, idx, std::span{k_magic_shrine_boxes});
  append_parts(verts, idx, std::span{k_magic_shrine_prisms});
  append_parts(verts, idx, std::span{k_magic_shrine_oriented_boxes});

  auto add_rune_stone = [&](const QVector3D& center, float rotation) {
    constexpr float half_extent = 0.08F;
    append_box(verts,
               idx,
               {center.x() - half_extent, 0.02F, center.z() - half_extent},
               {center.x() + half_extent, 0.18F, center.z() + half_extent});
    append_oriented_box(verts,
                        idx,
                        {center.x(), 0.18F, center.z()},
                        {center.x() + std::cos(rotation) * 0.05F,
                         0.34F,
                         center.z() + std::sin(rotation) * 0.05F},
                        0.045F,
                        0.05F);
  };

  auto add_obelisk = [&](float x, float z) {
    append_box(
        verts, idx, {x - 0.18F, 0.08F, z - 0.18F}, {x + 0.18F, 0.18F, z + 0.18F});
    append_box(
        verts, idx, {x - 0.14F, 0.18F, z - 0.14F}, {x + 0.14F, 0.26F, z + 0.14F});
    append_vert_prism(verts, idx, x, 0.26F, z, 0.085F, 0.78F, 6);
    append_vert_prism(verts, idx, x, 1.04F, z, 0.060F, 0.16F, 6);
    append_box(
        verts, idx, {x - 0.10F, 1.20F, z - 0.10F}, {x + 0.10F, 1.28F, z + 0.10F});
  };

  add_obelisk(-0.54F, -0.54F);
  add_obelisk(0.54F, -0.54F);
  add_obelisk(-0.54F, 0.54F);
  add_obelisk(0.54F, 0.54F);

  for (int i = 0; i < 8; ++i) {
    float const angle = static_cast<float>(i) * 0.78539816F;
    float const radius = (i % 2 == 0) ? 0.88F : 0.94F;
    float const x = std::cos(angle) * radius;
    float const z = std::sin(angle) * radius;
    append_oriented_box(verts,
                        idx,
                        {x, 0.08F, z},
                        {x * 0.88F, 0.60F + 0.08F * float(i % 3), z * 0.88F},
                        0.045F,
                        0.055F);
  }

  add_rune_stone({-0.72F, 0.0F, -0.06F}, 0.6F);
  add_rune_stone({0.72F, 0.0F, 0.08F}, 2.5F);
  add_rune_stone({-0.06F, 0.0F, 0.72F}, 1.3F);
  add_rune_stone({0.10F, 0.0F, -0.74F}, -1.2F);
  add_rune_stone({-0.62F, 0.0F, 0.58F}, 0.9F);
  add_rune_stone({0.62F, 0.0F, -0.60F}, -0.4F);

  upload_prop_mesh_impl(verts, idx, m_magic_shrine_mesh);
}

void VegetationPipeline::initialize_cursed_gold_vein_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_cursed_gold_vein_mesh);

  PropMeshData mesh = build_cursed_gold_vein_mesh();
  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_cursed_gold_vein_mesh);
}

} // namespace Render::GL::BackendPipelines
