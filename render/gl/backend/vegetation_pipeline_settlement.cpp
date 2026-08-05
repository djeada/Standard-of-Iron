#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../render_constants.h"
#include "gl/shader_cache.h"
#include "mesh_buffers.h"
#include "prop_mesh_builder.h"
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

  glGenVertexArrays(1, &m_firecamp_mesh.vao);
  glBindVertexArray(m_firecamp_mesh.vao);

  glGenBuffers(1, &m_firecamp_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_firecamp_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(FireCampVertex)),
               vertices.data(),
               GL_STATIC_DRAW);
  m_firecamp_mesh.vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(FireCampVertex),
                        reinterpret_cast<void*>(0));

  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(FireCampVertex),
                        reinterpret_cast<void*>(offsetof(FireCampVertex, tex_coord)));

  glGenBuffers(1, &m_firecamp_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_firecamp_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(),
               GL_STATIC_DRAW);
  m_firecamp_mesh.index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);

  glEnableVertexAttribArray(instance_scale);
  glVertexAttribDivisor(instance_scale, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::initialize_tent_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_tent_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  constexpr float H = 0.88F;
  constexpr float W = 0.62F;
  constexpr float Dp = 0.60F;

  const QVector3D A(-W, 0.0F, -Dp);
  const QVector3D B(W, 0.0F, -Dp);
  const QVector3D C(0.0F, H, -Dp);
  const QVector3D D(-W, 0.0F, Dp);
  const QVector3D E(W, 0.0F, Dp);
  const QVector3D F(0.0F, H, Dp);

  constexpr float inv_sqrt2 = 0.70711F;
  const QVector3D nL(-inv_sqrt2, inv_sqrt2, 0.0F);
  const QVector3D nR(inv_sqrt2, inv_sqrt2, 0.0F);

  using P = std::pair<QVector3D, QVector3D>;

  {
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{A, nL}, P{D, nL}, P{F, nL}, P{C, nL}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{B, nR}, P{C, nR}, P{F, nR}, P{E, nR}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    const QVector3D nF(0.0F, 0.0F, -1.0F);
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{A, nF}, P{C, nF}, P{B, nF}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }

  {
    const QVector3D nBk(0.0F, 0.0F, 1.0F);
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{D, nBk}, P{E, nBk}, P{F, nBk}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }

  append_box(verts, idx, {-W, -0.02F, -Dp}, {W, 0.00F, Dp});

  append_box(verts, idx, {-0.030F, 0.00F, -0.035F}, {0.030F, H * 0.90F, 0.035F});

  append_box(verts, idx, {-0.24F, 0.00F, -Dp - 0.02F}, {-0.16F, 0.44F, -Dp + 0.02F});
  append_box(verts, idx, {0.16F, 0.00F, -Dp - 0.02F}, {0.24F, 0.44F, -Dp + 0.02F});
  append_box(verts, idx, {-0.24F, 0.41F, -Dp - 0.02F}, {0.24F, 0.47F, -Dp + 0.02F});

  {
    constexpr float aw_ext = 0.30F;
    constexpr float aw_y = H * 0.46F;
    constexpr float inv_aw = 0.83205F;
    const QVector3D nAw(0.0F, inv_aw, -inv_aw);

    const QVector3D al(-W * 0.72F, aw_y, -Dp);
    const QVector3D ar(W * 0.72F, aw_y, -Dp);
    const QVector3D bl(-W * 0.72F, 0.04F, -Dp - aw_ext);
    const QVector3D br(W * 0.72F, 0.04F, -Dp - aw_ext);

    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{al, nAw}, P{ar, nAw}, P{br, nAw}, P{bl, nAw}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});

    const QVector3D nAwU(0.0F, -inv_aw, inv_aw);
    auto bu = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{bl, nAwU}, P{br, nAwU}, P{ar, nAwU}, P{al, nAwU}});
    idx.insert(idx.end(),
               {bu,
                uint16_t(bu + 1),
                uint16_t(bu + 2),
                bu,
                uint16_t(bu + 2),
                uint16_t(bu + 3)});

    append_box(verts,
               idx,
               {-W * 0.72F - 0.025F, 0.00F, -Dp - aw_ext},
               {-W * 0.72F + 0.025F, aw_y, -Dp - aw_ext + 0.025F});
    append_box(verts,
               idx,
               {W * 0.72F - 0.025F, 0.00F, -Dp - aw_ext},
               {W * 0.72F + 0.025F, aw_y, -Dp - aw_ext + 0.025F});
  }

  constexpr float sk = 0.07F;
  append_box(verts, idx, {-W - sk, 0.00F, -Dp - sk}, {-W, 0.07F, -Dp});
  append_box(verts, idx, {W, 0.00F, -Dp - sk}, {W + sk, 0.07F, -Dp});
  append_box(verts, idx, {-W - sk, 0.00F, Dp}, {-W, 0.07F, Dp + sk});
  append_box(verts, idx, {W, 0.00F, Dp}, {W + sk, 0.07F, Dp + sk});

  upload_prop_mesh_impl(verts, idx, m_tent_mesh);
}

void VegetationPipeline::initialize_supply_cart_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_supply_cart_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  constexpr int k_wheel_sides = 20;
  constexpr float k_front_wheel_r = 0.26F;
  constexpr float k_rear_wheel_r = 0.34F;
  constexpr float k_front_wheel_t = 0.070F;
  constexpr float k_rear_wheel_t = 0.090F;

  append_box(verts, idx, {-0.58F, 0.36F, -0.60F}, {-0.44F, 0.45F, 0.56F});
  append_box(verts, idx, {0.44F, 0.36F, -0.60F}, {0.58F, 0.45F, 0.56F});
  append_box(verts, idx, {-0.56F, 0.30F, -0.58F}, {0.56F, 0.39F, -0.42F});
  append_box(verts, idx, {-0.60F, 0.34F, 0.34F}, {0.60F, 0.43F, 0.52F});

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

  append_box(verts, idx, {-0.86F, 0.225F, -0.555F}, {-0.72F, 0.295F, -0.445F});
  append_box(verts, idx, {0.72F, 0.225F, -0.555F}, {0.86F, 0.295F, -0.445F});
  append_box(verts, idx, {-0.90F, 0.30F, 0.385F}, {-0.75F, 0.38F, 0.495F});
  append_box(verts, idx, {0.75F, 0.30F, 0.385F}, {0.90F, 0.38F, 0.495F});

  append_box(verts, idx, {-0.56F, 0.45F, -0.56F}, {0.56F, 0.54F, 0.54F});
  for (int plank = 0; plank < 6; ++plank) {
    float const z = -0.545F + 0.180F * static_cast<float>(plank);
    append_box(verts, idx, {-0.545F, 0.540F, z}, {0.545F, 0.556F, z + 0.150F});
  }

  append_box(verts, idx, {-0.64F, 0.45F, -0.60F}, {-0.52F, 0.94F, 0.58F});
  append_box(verts, idx, {0.52F, 0.45F, -0.60F}, {0.64F, 0.94F, 0.58F});
  append_box(verts, idx, {-0.56F, 0.45F, -0.64F}, {0.56F, 0.88F, -0.52F});
  append_box(verts, idx, {-0.56F, 0.45F, 0.48F}, {0.56F, 0.78F, 0.60F});
  append_box(verts, idx, {-0.66F, 0.88F, -0.62F}, {-0.50F, 0.98F, 0.60F});
  append_box(verts, idx, {0.50F, 0.88F, -0.62F}, {0.66F, 0.98F, 0.60F});
  for (float const side : {-1.0F, 1.0F}) {
    for (float const z : {-0.54F, -0.06F, 0.42F}) {
      append_box(verts,
                 idx,
                 {side * 0.58F - 0.055F, 0.45F, z - 0.045F},
                 {side * 0.58F + 0.055F, 1.02F, z + 0.045F});
    }
  }

  append_box(verts, idx, {-0.20F, 0.32F, -0.62F}, {-0.09F, 0.41F, -1.34F});
  append_box(verts, idx, {0.09F, 0.32F, -0.62F}, {0.20F, 0.41F, -1.34F});
  append_box(verts, idx, {-0.22F, 0.30F, -1.24F}, {0.22F, 0.39F, -1.14F});
  append_box(verts, idx, {-0.46F, 0.31F, -1.40F}, {0.46F, 0.40F, -1.30F});
  append_box(verts, idx, {-0.50F, 0.36F, -1.42F}, {-0.36F, 0.46F, -1.28F});
  append_box(verts, idx, {0.36F, 0.36F, -1.42F}, {0.50F, 0.46F, -1.28F});
  append_prop_beam(
      verts, idx, {-0.16F, 0.40F, -1.30F}, {-0.16F, 0.56F, -1.06F}, 0.030F, 0.028F);
  append_prop_beam(
      verts, idx, {0.16F, 0.40F, -1.30F}, {0.16F, 0.56F, -1.06F}, 0.030F, 0.028F);

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

  append_prop_taper(verts, idx, -0.06F, 0.556F, -0.42F, 0.075F, 0.135F, 0.20F, 10);
  append_prop_taper(verts, idx, -0.06F, 0.756F, -0.42F, 0.135F, 0.060F, 0.24F, 10);
  append_prop_taper(verts, idx, -0.06F, 0.996F, -0.42F, 0.052F, 0.072F, 0.10F, 10);
  for (float const side : {-1.0F, 1.0F}) {
    append_prop_beam(verts,
                     idx,
                     {-0.06F + side * 0.052F, 1.030F, -0.42F},
                     {-0.06F + side * 0.130F, 0.900F, -0.42F},
                     0.020F,
                     0.020F);
  }

  append_prop_beam(
      verts, idx, {-0.50F, 1.010F, 0.10F}, {0.50F, 1.010F, 0.10F}, 0.090F, 0.085F);
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
  append_prop_beam(
      verts, idx, {0.0F, 1.352F, 0.06F}, {0.0F, 1.352F, 0.48F}, 0.034F, 0.030F);

  upload_prop_mesh_impl(verts, idx, m_supply_cart_mesh);
}

void VegetationPipeline::initialize_weapon_rack_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_weapon_rack_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  auto append_blade_tip = [&](float cx,
                              float y0,
                              float z,
                              float half_width,
                              float height,
                              float half_depth) {
    const QVector3D base_l(cx - half_width, y0, z - half_depth);
    const QVector3D base_r(cx + half_width, y0, z - half_depth);
    const QVector3D base_rf(cx + half_width, y0, z + half_depth);
    const QVector3D base_lf(cx - half_width, y0, z + half_depth);
    const QVector3D tip(cx, y0 + height, z);

    auto tri = [&](const QVector3D& p0, const QVector3D& p1, const QVector3D& p2) {
      QVector3D n = QVector3D::crossProduct(p1 - p0, p2 - p0);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      auto base = static_cast<uint16_t>(verts.size());
      verts.insert(verts.end(), {{p0, n}, {p1, n}, {p2, n}});
      idx.insert(idx.end(), {base, uint16_t(base + 1), uint16_t(base + 2)});
    };

    tri(base_l, base_r, tip);
    tri(base_r, base_rf, tip);
    tri(base_rf, base_lf, tip);
    tri(base_lf, base_l, tip);
  };

  auto append_leaf_blade = [&](float cx,
                               float y0,
                               float z,
                               float half_width,
                               float height,
                               float half_depth) {
    const QVector3D base(cx, y0, z);
    const QVector3D shoulder_l(cx - half_width, y0 + height * 0.34F, z);
    const QVector3D shoulder_r(cx + half_width, y0 + height * 0.34F, z);
    const QVector3D waist_l(cx - half_width * 0.46F, y0 + height * 0.72F, z);
    const QVector3D waist_r(cx + half_width * 0.46F, y0 + height * 0.72F, z);
    const QVector3D tip(cx, y0 + height, z);
    const QVector3D ridge_f(cx, y0 + height * 0.48F, z + half_depth);
    const QVector3D ridge_b(cx, y0 + height * 0.48F, z - half_depth);

    auto tri = [&](const QVector3D& p0, const QVector3D& p1, const QVector3D& p2) {
      QVector3D n = QVector3D::crossProduct(p1 - p0, p2 - p0);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      auto base_idx = static_cast<uint16_t>(verts.size());
      verts.insert(verts.end(), {{p0, n}, {p1, n}, {p2, n}});
      idx.insert(idx.end(), {base_idx, uint16_t(base_idx + 1), uint16_t(base_idx + 2)});
    };

    tri(base, shoulder_l, ridge_f);
    tri(base, ridge_f, shoulder_r);
    tri(shoulder_l, waist_l, ridge_f);
    tri(shoulder_r, ridge_f, waist_r);
    tri(waist_l, tip, ridge_f);
    tri(waist_r, ridge_f, tip);

    tri(base, ridge_b, shoulder_l);
    tri(base, shoulder_r, ridge_b);
    tri(shoulder_l, ridge_b, waist_l);
    tri(shoulder_r, waist_r, ridge_b);
    tri(waist_l, ridge_b, tip);
    tri(waist_r, tip, ridge_b);
  };

  auto append_sword_blade = [&](const QVector3D& base,
                                const QVector3D& tip,
                                float base_half_width,
                                float tip_half_width,
                                float half_depth) {
    QVector3D const axis = tip - base;
    QVector3D side(-axis.y(), axis.x(), 0.0F);
    if (side.lengthSquared() < 1.0e-8F) {
      side = {1.0F, 0.0F, 0.0F};
    } else {
      side.normalize();
    }
    const QVector3D mid = base + axis * 0.58F;
    const QVector3D base_l = base - side * base_half_width;
    const QVector3D base_r = base + side * base_half_width;
    const QVector3D mid_l = mid - side * (base_half_width * 0.72F);
    const QVector3D mid_r = mid + side * (base_half_width * 0.72F);
    const QVector3D tip_l = tip - side * tip_half_width;
    const QVector3D tip_r = tip + side * tip_half_width;
    const QVector3D ridge_f = mid + QVector3D(0.0F, 0.0F, half_depth);
    const QVector3D ridge_b = mid - QVector3D(0.0F, 0.0F, half_depth);

    auto quad = [&](const QVector3D& p0,
                    const QVector3D& p1,
                    const QVector3D& p2,
                    const QVector3D& p3) {
      QVector3D n = QVector3D::crossProduct(p1 - p0, p3 - p0);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      append_quad(verts, idx, p0, p1, p2, p3, n);
    };

    quad(base_l, mid_l, ridge_f, base);
    quad(base, ridge_f, mid_r, base_r);
    quad(mid_l, tip_l, tip, ridge_f);
    quad(ridge_f, tip, tip_r, mid_r);

    quad(base, ridge_b, mid_l, base_l);
    quad(base_r, mid_r, ridge_b, base);
    quad(ridge_b, tip, tip_l, mid_l);
    quad(mid_r, tip_r, tip, ridge_b);
  };

  append_box(verts, idx, {-0.78F, 0.00F, -0.13F}, {-0.64F, 1.42F, 0.03F});
  append_box(verts, idx, {0.64F, 0.00F, -0.13F}, {0.78F, 1.42F, 0.03F});
  append_box(verts, idx, {-0.88F, 0.00F, 0.13F}, {-0.66F, 0.18F, 0.33F});
  append_box(verts, idx, {0.66F, 0.00F, 0.13F}, {0.88F, 0.18F, 0.33F});
  append_box(verts, idx, {-0.86F, 0.34F, -0.10F}, {0.86F, 0.48F, 0.08F});
  append_box(verts, idx, {-0.84F, 0.96F, -0.12F}, {0.84F, 1.10F, 0.06F});
  append_box(verts, idx, {-0.74F, 1.28F, -0.10F}, {0.74F, 1.40F, 0.04F});
  append_prop_beam(
      verts, idx, {-0.70F, 0.16F, -0.08F}, {-0.18F, 0.96F, -0.08F}, 0.045F, 0.055F);
  append_prop_beam(
      verts, idx, {0.70F, 0.16F, -0.08F}, {0.18F, 0.96F, -0.08F}, 0.045F, 0.055F);

  for (float const x : {-0.58F, -0.30F, 0.00F, 0.30F, 0.58F}) {
    append_box(verts, idx, {x - 0.050F, 0.46F, 0.08F}, {x + 0.050F, 0.58F, 0.24F});
    append_box(verts, idx, {x - 0.045F, 1.10F, 0.06F}, {x + 0.045F, 1.22F, 0.20F});
  }

  append_prop_beam(
      verts, idx, {-0.64F, 0.05F, 0.17F}, {-0.50F, 1.72F, 0.09F}, 0.032F, 0.034F);
  append_prop_beam(
      verts, idx, {-0.515F, 1.54F, 0.099F}, {-0.503F, 1.68F, 0.092F}, 0.046F, 0.047F);
  append_leaf_blade(-0.485F, 1.62F, 0.09F, 0.115F, 0.42F, 0.035F);
  append_box(verts, idx, {-0.675F, 0.00F, 0.13F}, {-0.595F, 0.12F, 0.21F});

  append_box(verts, idx, {-0.335F, 0.02F, 0.12F}, {-0.210F, 0.15F, 0.24F});
  append_prop_beam(
      verts, idx, {-0.280F, 0.12F, 0.17F}, {-0.235F, 0.38F, 0.12F}, 0.044F, 0.034F);
  append_prop_beam(
      verts, idx, {-0.525F, 0.34F, 0.13F}, {0.030F, 0.41F, 0.13F}, 0.042F, 0.048F);
  append_sword_blade(
      {-0.250F, 0.42F, 0.12F}, {-0.335F, 1.82F, 0.07F}, 0.080F, 0.018F, 0.030F);
  append_blade_tip(-0.345F, 1.72F, 0.07F, 0.070F, 0.24F, 0.026F);

  append_box(verts, idx, {0.030F, 0.03F, 0.14F}, {0.140F, 0.14F, 0.25F});
  append_prop_beam(
      verts, idx, {0.085F, 0.12F, 0.18F}, {0.080F, 0.34F, 0.13F}, 0.036F, 0.030F);
  append_prop_beam(
      verts, idx, {-0.075F, 0.30F, 0.14F}, {0.285F, 0.37F, 0.14F}, 0.038F, 0.044F);
  append_sword_blade(
      {0.085F, 0.38F, 0.13F}, {0.180F, 1.56F, 0.08F}, 0.065F, 0.015F, 0.026F);
  append_blade_tip(0.185F, 1.46F, 0.08F, 0.058F, 0.22F, 0.024F);

  append_prop_beam(
      verts, idx, {0.54F, 0.05F, 0.16F}, {0.68F, 1.70F, 0.08F}, 0.030F, 0.032F);
  append_prop_beam(
      verts, idx, {0.665F, 1.52F, 0.089F}, {0.677F, 1.66F, 0.082F}, 0.044F, 0.045F);
  append_leaf_blade(0.690F, 1.60F, 0.08F, 0.105F, 0.40F, 0.033F);
  append_box(verts, idx, {0.505F, 0.00F, 0.12F}, {0.585F, 0.12F, 0.20F});

  {
    constexpr float k_depth = 0.026F;
    struct Pt {
      float x;
      float y;
      float z;
    };
    static constexpr Pt pts[] = {
        {0.25F, 0.06F, -0.20F},
        {0.43F, 0.38F, -0.23F},
        {0.56F, 0.74F, -0.26F},
        {0.60F, 1.06F, -0.26F},
        {0.55F, 1.42F, -0.24F},
        {0.40F, 1.72F, -0.21F},
        {0.22F, 1.94F, -0.19F},
    };
    for (int i = 0; i < 6; ++i) {
      append_prop_beam(verts,
                       idx,
                       {pts[i].x, pts[i].y, pts[i].z},
                       {pts[i + 1].x, pts[i + 1].y, pts[i + 1].z},
                       0.026F,
                       k_depth);
    }
    append_prop_beam(
        verts, idx, {0.225F, 0.10F, -0.20F}, {0.205F, 1.88F, -0.19F}, 0.006F, 0.006F);
    append_box(verts, idx, {0.520F, 0.86F, -0.29F}, {0.625F, 1.12F, -0.16F});
  }

  append_box(verts, idx, {-0.70F, 0.48F, -0.115F}, {0.70F, 0.94F, -0.055F});
  for (int plank = 0; plank < 4; ++plank) {
    float const y = 0.495F + 0.112F * static_cast<float>(plank);
    append_box(verts, idx, {-0.705F, y, -0.125F}, {0.705F, y + 0.016F, -0.108F});
  }
  append_box(verts, idx, {-0.72F, 0.46F, -0.130F}, {-0.66F, 0.96F, -0.050F});
  append_box(verts, idx, {0.66F, 0.46F, -0.130F}, {0.72F, 0.96F, -0.050F});

  auto add_scutum = [&](float cx, float lean_x, float front_z, float back_z) {
    const QVector3D foot(cx, 0.020F, front_z);
    const QVector3D crown(cx + lean_x, 0.980F, back_z);
    append_prop_beam(verts, idx, foot, crown, 0.230F, 0.032F);
    append_prop_beam(verts,
                     idx,
                     {cx - 0.238F, 0.055F, front_z - 0.030F},
                     {cx + lean_x - 0.238F, 0.945F, back_z - 0.030F},
                     0.052F,
                     0.028F);
    append_prop_beam(verts,
                     idx,
                     {cx + 0.238F, 0.055F, front_z - 0.030F},
                     {cx + lean_x + 0.238F, 0.945F, back_z - 0.030F},
                     0.052F,
                     0.028F);
    append_prop_beam(verts,
                     idx,
                     {cx - 0.245F, 0.060F, front_z + 0.004F},
                     {cx + 0.245F, 0.060F, front_z + 0.004F},
                     0.026F,
                     0.040F);
    append_prop_beam(verts,
                     idx,
                     {cx + lean_x - 0.245F, 0.940F, back_z + 0.004F},
                     {cx + lean_x + 0.245F, 0.940F, back_z + 0.004F},
                     0.026F,
                     0.040F);
    const QVector3D boss = foot * 0.5F + crown * 0.5F;
    append_prop_taper(verts,
                      idx,
                      boss.x() - 0.005F,
                      boss.y() - 0.052F,
                      boss.z() + 0.030F,
                      0.082F,
                      0.062F,
                      0.045F,
                      10);
    append_prop_beam(verts,
                     idx,
                     {boss.x() - 0.150F, boss.y() + 0.006F, boss.z() + 0.036F},
                     {boss.x() + 0.150F, boss.y() + 0.006F, boss.z() + 0.036F},
                     0.020F,
                     0.022F);
  };

  add_scutum(-0.905F, 0.075F, 0.360F, 0.190F);
  add_scutum(0.930F, -0.070F, 0.330F, 0.165F);

  append_prop_taper(verts, idx, 0.315F, 0.020F, 0.400F, 0.135F, 0.115F, 0.360F, 10);
  append_prop_taper(verts, idx, 0.315F, 0.380F, 0.400F, 0.115F, 0.128F, 0.030F, 10);
  for (int shaft = 0; shaft < 4; ++shaft) {
    float const angle = 1.20F + 0.34F * static_cast<float>(shaft);
    append_prop_beam(
        verts,
        idx,
        {0.315F + std::cos(angle) * 0.055F, 0.395F, 0.400F + std::sin(angle) * 0.055F},
        {0.315F + std::cos(angle) * 0.155F, 1.150F, 0.400F + std::sin(angle) * 0.155F},
        0.019F,
        0.019F);
  }

  upload_prop_mesh_impl(verts, idx, m_weapon_rack_mesh);
}

void VegetationPipeline::initialize_ruins_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_ruins_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  auto add_rubble = [&](const QVector3D& lo, const QVector3D& hi) {
    append_box(verts, idx, lo, hi);
  };

  append_box(verts, idx, {-0.92F, -0.02F, -0.70F}, {0.90F, 0.06F, 0.68F});
  append_box(verts, idx, {-0.76F, 0.06F, -0.56F}, {0.76F, 0.12F, 0.54F});

  append_box(verts, idx, {-0.78F, 0.00F, -0.26F}, {-0.48F, 0.14F, 0.06F});
  append_vert_prism(verts, idx, -0.63F, 0.14F, -0.10F, 0.13F, 1.34F, 10);
  append_box(verts, idx, {-0.80F, 1.48F, -0.28F}, {-0.46F, 1.66F, 0.08F});
  append_box(verts, idx, {-0.74F, 1.66F, -0.24F}, {-0.50F, 1.78F, 0.02F});
  append_oriented_box(
      verts, idx, {-0.74F, 0.24F, -0.34F}, {-0.52F, 1.10F, -0.28F}, 0.05F, 0.05F);

  append_box(verts, idx, {0.46F, 0.00F, -0.20F}, {0.74F, 0.12F, 0.10F});
  append_vert_prism(verts, idx, 0.60F, 0.12F, -0.05F, 0.12F, 0.88F, 10);
  append_box(verts, idx, {0.44F, 1.00F, -0.24F}, {0.76F, 1.16F, 0.12F});
  append_oriented_box(
      verts, idx, {0.70F, 0.16F, 0.16F}, {0.48F, 0.86F, 0.20F}, 0.04F, 0.05F);

  append_box(verts, idx, {-0.34F, 0.00F, 0.26F}, {-0.06F, 0.14F, 0.56F});
  append_vert_prism(verts, idx, -0.20F, 0.14F, 0.40F, 0.12F, 1.42F, 10);
  append_box(verts, idx, {-0.38F, 1.56F, 0.22F}, {0.00F, 1.74F, 0.60F});
  append_oriented_box(
      verts, idx, {-0.06F, 0.22F, 0.52F}, {0.18F, 0.86F, 0.46F}, 0.05F, 0.05F);

  append_box(verts, idx, {-0.62F, 0.12F, -0.52F}, {0.02F, 0.72F, -0.32F});
  append_box(verts, idx, {-0.62F, 0.74F, -0.50F}, {-0.18F, 1.04F, -0.30F});
  append_box(verts, idx, {-0.06F, 0.12F, -0.48F}, {0.24F, 0.52F, -0.30F});
  append_box(verts, idx, {-0.58F, 1.08F, -0.46F}, {-0.24F, 1.24F, -0.26F});
  append_box(verts, idx, {-0.18F, 1.02F, -0.42F}, {0.12F, 1.16F, -0.24F});

  append_box(verts, idx, {0.12F, 0.12F, 0.14F}, {0.52F, 0.86F, 0.48F});
  append_box(verts, idx, {0.08F, 0.88F, 0.18F}, {0.46F, 1.08F, 0.52F});
  append_box(verts, idx, {0.30F, 0.18F, -0.56F}, {0.60F, 0.42F, -0.34F});

  append_oriented_box(
      verts, idx, {-0.42F, 1.46F, -0.10F}, {0.18F, 1.22F, -0.08F}, 0.10F, 0.08F);
  append_oriented_box(
      verts, idx, {0.18F, 1.22F, -0.08F}, {0.54F, 1.08F, -0.04F}, 0.08F, 0.07F);
  append_oriented_box(
      verts, idx, {-0.26F, 1.60F, 0.30F}, {0.10F, 1.30F, 0.26F}, 0.08F, 0.08F);

  append_vert_prism(verts, idx, -0.46F, 0.10F, 0.03F, 0.13F, 1.72F, 7);
  append_vert_prism(verts, idx, 0.46F, 0.10F, 0.03F, 0.13F, 1.46F, 7);
  append_oriented_box(
      verts, idx, {-0.48F, 1.78F, 0.03F}, {-0.06F, 2.04F, 0.03F}, 0.11F, 0.10F);
  append_oriented_box(
      verts, idx, {-0.06F, 2.04F, 0.03F}, {0.34F, 1.76F, 0.03F}, 0.10F, 0.09F);
  append_oriented_box(
      verts, idx, {0.34F, 1.76F, 0.03F}, {0.51F, 1.58F, 0.03F}, 0.075F, 0.08F);

  append_oriented_box(
      verts, idx, {-0.86F, 0.12F, 0.48F}, {-0.74F, 0.94F, 0.42F}, 0.075F, 0.08F);
  append_oriented_box(
      verts, idx, {0.78F, 0.10F, 0.42F}, {0.66F, 0.80F, 0.36F}, 0.07F, 0.075F);

  add_rubble({-0.82F, 0.02F, -0.64F}, {-0.54F, 0.16F, -0.44F});
  add_rubble({-0.40F, 0.02F, -0.64F}, {-0.08F, 0.18F, -0.46F});
  add_rubble({0.06F, 0.02F, -0.66F}, {0.34F, 0.12F, -0.46F});
  add_rubble({0.42F, 0.02F, -0.62F}, {0.76F, 0.18F, -0.40F});
  add_rubble({-0.64F, 0.02F, 0.44F}, {-0.28F, 0.16F, 0.64F});
  add_rubble({-0.14F, 0.02F, 0.50F}, {0.20F, 0.14F, 0.66F});
  add_rubble({0.28F, 0.02F, 0.36F}, {0.64F, 0.16F, 0.62F});
  add_rubble({-0.18F, 0.02F, -0.08F}, {0.10F, 0.10F, 0.14F});

  append_oriented_box(
      verts, idx, {-0.28F, 0.14F, -0.24F}, {-0.02F, 0.08F, 0.02F}, 0.07F, 0.08F);
  append_oriented_box(
      verts, idx, {0.22F, 0.12F, 0.04F}, {0.52F, 0.06F, 0.28F}, 0.06F, 0.08F);
  append_oriented_box(
      verts, idx, {-0.52F, 0.12F, 0.18F}, {-0.28F, 0.06F, 0.34F}, 0.06F, 0.06F);

  upload_prop_mesh_impl(verts, idx, m_ruins_mesh);
}

void VegetationPipeline::initialize_abandoned_home_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_abandoned_home_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  append_box(verts, idx, {-0.98F, -0.02F, -0.82F}, {0.98F, 0.07F, 0.82F});
  append_box(verts, idx, {-0.90F, 0.07F, -0.74F}, {0.90F, 0.14F, 0.74F});

  append_box(verts, idx, {-0.88F, 0.12F, -0.72F}, {-0.70F, 1.16F, 0.72F});
  append_box(verts, idx, {-0.90F, 1.16F, -0.72F}, {-0.68F, 1.26F, 0.16F});
  append_box(verts, idx, {-0.88F, 1.16F, 0.34F}, {-0.70F, 1.21F, 0.72F});

  append_box(verts, idx, {-0.88F, 0.12F, 0.56F}, {0.86F, 1.14F, 0.72F});
  append_box(verts, idx, {-0.30F, 1.14F, 0.56F}, {0.36F, 1.30F, 0.72F});
  append_box(verts, idx, {0.06F, 0.52F, 0.545F}, {0.44F, 0.86F, 0.735F});
  append_box(verts, idx, {0.10F, 0.56F, 0.535F}, {0.40F, 0.82F, 0.745F});

  append_box(verts, idx, {-0.88F, 0.12F, -0.72F}, {-0.30F, 0.96F, -0.56F});
  append_box(verts, idx, {-0.88F, 0.96F, -0.72F}, {-0.56F, 1.20F, -0.56F});
  append_box(verts, idx, {0.06F, 0.12F, -0.72F}, {0.50F, 0.88F, -0.56F});
  append_box(verts, idx, {-0.30F, 0.80F, -0.72F}, {0.06F, 0.96F, -0.56F});
  append_box(verts, idx, {-0.32F, 0.94F, -0.74F}, {0.10F, 1.02F, -0.54F});
  append_box(verts, idx, {0.50F, 0.12F, -0.72F}, {0.86F, 0.44F, -0.56F});

  append_box(verts, idx, {0.70F, 0.12F, -0.56F}, {0.86F, 0.92F, 0.30F});
  append_box(verts, idx, {0.70F, 0.92F, -0.30F}, {0.86F, 1.06F, 0.30F});

  for (int course = 0; course < 5; ++course) {
    float const y = 0.24F + 0.20F * static_cast<float>(course);
    append_box(verts, idx, {-0.885F, y, 0.58F}, {0.80F, y + 0.022F, 0.585F});
    append_box(verts, idx, {-0.895F, y, -0.70F}, {-0.885F, y + 0.022F, 0.70F});
  }

  append_box(verts, idx, {-0.16F, 0.12F, -0.60F}, {0.10F, 0.16F, -0.44F});

  for (int course = 0; course < 5; ++course) {
    float const step = static_cast<float>(course);
    float const z0 = -0.76F + 0.148F * step;
    float const y0 = 1.06F + 0.092F * step;
    append_box(verts, idx, {-0.58F, y0, z0}, {0.88F, y0 + 0.092F, z0 + 0.148F});
  }
  append_box(verts, idx, {-0.60F, 1.52F, -0.06F}, {0.90F, 1.63F, 0.08F});
  append_box(verts, idx, {-0.62F, 1.44F, -0.80F}, {-0.52F, 1.60F, 0.10F});

  append_oriented_box(
      verts, idx, {-0.24F, 1.54F, 0.05F}, {-0.10F, 1.20F, 0.44F}, 0.055F, 0.048F);
  append_oriented_box(
      verts, idx, {0.30F, 1.54F, 0.05F}, {0.24F, 1.30F, 0.34F}, 0.052F, 0.046F);
  append_oriented_box(
      verts, idx, {0.66F, 1.53F, 0.05F}, {0.72F, 1.10F, 0.56F}, 0.052F, 0.046F);
  append_oriented_box(
      verts, idx, {0.02F, 1.55F, 0.04F}, {0.10F, 0.62F, 0.30F}, 0.048F, 0.042F);

  append_box(verts, idx, {0.42F, 0.14F, 0.08F}, {0.70F, 1.42F, 0.36F});
  append_box(verts, idx, {0.38F, 1.42F, 0.04F}, {0.74F, 1.56F, 0.40F});
  append_box(verts, idx, {0.44F, 1.56F, 0.10F}, {0.68F, 1.62F, 0.34F});

  append_box(verts, idx, {-0.34F, 0.12F, -0.14F}, {0.04F, 0.24F, 0.24F});
  append_box(verts, idx, {-0.30F, 0.24F, -0.08F}, {-0.04F, 0.32F, 0.16F});
  append_box(verts, idx, {0.16F, 0.12F, 0.02F}, {0.44F, 0.20F, 0.32F});
  append_box(verts, idx, {-0.62F, 0.12F, 0.10F}, {-0.38F, 0.19F, 0.40F});
  append_box(verts, idx, {0.26F, 0.11F, -0.46F}, {0.54F, 0.18F, -0.22F});

  append_box(verts, idx, {0.92F, 0.06F, -0.78F}, {1.16F, 0.16F, -0.48F});
  append_box(verts, idx, {0.96F, 0.06F, 0.34F}, {1.14F, 0.13F, 0.60F});
  append_box(verts, idx, {-1.14F, 0.06F, 0.38F}, {-0.92F, 0.15F, 0.64F});
  append_box(verts, idx, {-1.10F, 0.05F, -0.62F}, {-0.94F, 0.11F, -0.42F});

  append_oriented_box(
      verts, idx, {-0.24F, 0.14F, -0.62F}, {-0.02F, 0.74F, -0.40F}, 0.13F, 0.030F);

  upload_prop_mesh_impl(verts, idx, m_abandoned_home_mesh);
}

void VegetationPipeline::initialize_statue_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_statue_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  constexpr float k_plinth_top = 1.175F;
  constexpr int k_limb_segs = 9;
  constexpr int k_body_segs = 12;

  append_prop_slab(verts, idx, -0.030F, 0.070F, 0.545F, 0.536F);
  append_prop_slab(verts, idx, 0.070F, 0.140F, 0.478F, 0.470F);
  append_prop_slab(verts, idx, 0.140F, 0.208F, 0.412F, 0.404F);

  append_prop_slab(verts, idx, 0.208F, 0.266F, 0.362F, 0.358F);
  append_prop_slab(verts, idx, 0.266F, 0.316F, 0.358F, 0.320F);
  append_prop_slab(verts, idx, 0.316F, 0.348F, 0.320F, 0.306F);

  append_prop_slab(verts, idx, 0.348F, 0.920F, 0.304F, 0.290F);

  append_prop_slab(verts, idx, 0.920F, 0.954F, 0.290F, 0.320F);
  append_prop_slab(verts, idx, 0.954F, 0.998F, 0.320F, 0.362F);
  append_prop_slab(verts, idx, 0.998F, 1.030F, 0.362F, 0.356F);
  append_prop_slab(verts, idx, 1.030F, 1.066F, 0.356F, 0.312F);

  append_prop_slab(verts, idx, 1.066F, 1.122F, 0.292F, 0.286F);
  append_prop_slab(verts, idx, 1.122F, k_plinth_top, 0.266F, 0.260F);

  append_prop_beam(
      verts, idx, {-0.055F, 1.202F, 0.086F}, {0.128F, 1.202F, 0.100F}, 0.050F, 0.027F);
  append_prop_beam(verts,
                   idx,
                   {-0.140F, 1.200F, -0.088F},
                   {0.030F, 1.200F, -0.160F},
                   0.048F,
                   0.025F);

  append_prop_limb(verts,
                   idx,
                   {0.014F, 1.229F, 0.092F},
                   {0.010F, 1.370F, 0.096F},
                   0.044F,
                   0.068F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.010F, 1.370F, 0.096F},
                   {0.016F, 1.545F, 0.086F},
                   0.068F,
                   0.057F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.016F, 1.512F, 0.086F},
                   {0.016F, 1.578F, 0.086F},
                   0.062F,
                   0.060F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.016F, 1.545F, 0.086F},
                   {0.004F, 1.850F, 0.098F},
                   0.062F,
                   0.098F,
                   k_limb_segs);

  append_prop_limb(verts,
                   idx,
                   {-0.056F, 1.226F, -0.124F},
                   {-0.020F, 1.372F, -0.110F},
                   0.042F,
                   0.064F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {-0.020F, 1.372F, -0.110F},
                   {0.046F, 1.522F, -0.096F},
                   0.064F,
                   0.054F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.044F, 1.492F, -0.096F},
                   {0.048F, 1.556F, -0.095F},
                   0.058F,
                   0.056F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.046F, 1.522F, -0.096F},
                   {0.002F, 1.846F, -0.100F},
                   0.058F,
                   0.094F,
                   k_limb_segs);

  append_prop_frustum(verts,
                      idx,
                      0.0F,
                      1.796F,
                      0.0F,
                      0.108F,
                      0.150F,
                      0.100F,
                      0.138F,
                      0.154F,
                      k_body_segs);

  append_prop_frustum(verts,
                      idx,
                      0.002F,
                      1.828F,
                      0.0F,
                      0.128F,
                      0.168F,
                      0.116F,
                      0.154F,
                      0.078F,
                      k_body_segs);
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

  append_prop_frustum(verts,
                      idx,
                      0.004F,
                      1.902F,
                      0.0F,
                      0.126F,
                      0.164F,
                      0.100F,
                      0.130F,
                      0.052F,
                      k_body_segs);
  append_prop_frustum(verts,
                      idx,
                      0.004F,
                      1.938F,
                      0.0F,
                      0.100F,
                      0.130F,
                      0.120F,
                      0.158F,
                      0.150F,
                      k_body_segs);
  append_prop_frustum(verts,
                      idx,
                      0.004F,
                      2.088F,
                      0.0F,
                      0.120F,
                      0.158F,
                      0.110F,
                      0.170F,
                      0.126F,
                      k_body_segs);

  append_prop_limb(verts,
                   idx,
                   {0.086F, 2.108F, 0.062F},
                   {0.104F, 2.176F, 0.055F},
                   0.058F,
                   0.044F,
                   8);
  append_prop_limb(verts,
                   idx,
                   {0.086F, 2.108F, -0.062F},
                   {0.104F, 2.176F, -0.055F},
                   0.058F,
                   0.044F,
                   8);

  append_prop_limb(verts,
                   idx,
                   {0.004F, 2.150F, 0.148F},
                   {0.000F, 2.226F, 0.140F},
                   0.072F,
                   0.062F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.004F, 2.150F, -0.148F},
                   {0.000F, 2.226F, -0.140F},
                   0.072F,
                   0.062F,
                   k_limb_segs);

  append_prop_frustum(
      verts, idx, 0.012F, 2.196F, 0.0F, 0.054F, 0.060F, 0.048F, 0.052F, 0.078F, 10);

  append_prop_frustum(verts,
                      idx,
                      0.014F,
                      2.274F,
                      0.0F,
                      0.050F,
                      0.052F,
                      0.068F,
                      0.070F,
                      0.062F,
                      k_body_segs);
  append_prop_frustum(verts,
                      idx,
                      0.012F,
                      2.336F,
                      0.0F,
                      0.068F,
                      0.070F,
                      0.070F,
                      0.072F,
                      0.058F,
                      k_body_segs);
  append_prop_frustum(verts,
                      idx,
                      0.006F,
                      2.394F,
                      0.0F,
                      0.070F,
                      0.072F,
                      0.030F,
                      0.032F,
                      0.068F,
                      k_body_segs);
  append_prop_limb(
      verts, idx, {0.068F, 2.374F, 0.0F}, {0.092F, 2.352F, 0.0F}, 0.020F, 0.012F, 6);
  append_prop_frustum(
      verts, idx, -0.020F, 2.372F, 0.0F, 0.070F, 0.074F, 0.040F, 0.044F, 0.078F, 10);

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

  append_prop_limb(verts,
                   idx,
                   {0.000F, 2.196F, -0.150F},
                   {0.052F, 2.108F, -0.286F},
                   0.062F,
                   0.048F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.052F, 2.108F, -0.286F},
                   {0.128F, 2.318F, -0.318F},
                   0.050F,
                   0.034F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.128F, 2.318F, -0.318F},
                   {0.156F, 2.392F, -0.322F},
                   0.036F,
                   0.030F,
                   8);
  append_prop_beam(
      verts, idx, {0.150F, 2.376F, -0.321F}, {0.172F, 2.452F, -0.324F}, 0.036F, 0.014F);

  append_prop_limb(verts,
                   idx,
                   {0.000F, 2.196F, 0.152F},
                   {-0.022F, 1.972F, 0.196F},
                   0.062F,
                   0.046F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {-0.022F, 1.972F, 0.196F},
                   {0.038F, 1.812F, 0.206F},
                   0.046F,
                   0.034F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {0.038F, 1.812F, 0.206F},
                   {0.062F, 1.766F, 0.210F},
                   0.036F,
                   0.028F,
                   8);

  append_prop_limb(verts,
                   idx,
                   {0.052F, k_plinth_top, 0.216F},
                   {0.046F, 2.482F, 0.212F},
                   0.026F,
                   0.022F,
                   8);
  append_prop_limb(verts,
                   idx,
                   {0.046F, 2.482F, 0.212F},
                   {0.045F, 2.516F, 0.212F},
                   0.032F,
                   0.026F,
                   8);
  append_prop_limb(verts,
                   idx,
                   {0.045F, 2.512F, 0.212F},
                   {0.044F, 2.616F, 0.212F},
                   0.036F,
                   0.003F,
                   8);

  append_prop_limb(verts,
                   idx,
                   {-0.030F, 2.246F, 0.118F},
                   {-0.086F, 2.062F, 0.186F},
                   0.078F,
                   0.086F,
                   k_limb_segs);
  append_prop_limb(verts,
                   idx,
                   {-0.070F, 2.090F, 0.180F},
                   {0.060F, 1.928F, 0.020F},
                   0.068F,
                   0.058F,
                   8);
  append_prop_limb(verts,
                   idx,
                   {0.060F, 1.928F, 0.020F},
                   {0.020F, 1.876F, -0.150F},
                   0.058F,
                   0.062F,
                   8);
  append_prop_limb(verts,
                   idx,
                   {0.020F, 1.876F, -0.150F},
                   {-0.120F, 1.860F, -0.060F},
                   0.062F,
                   0.070F,
                   8);

  append_prop_beam(
      verts, idx, {-0.128F, 2.180F, 0.030F}, {-0.176F, 1.520F, 0.010F}, 0.150F, 0.030F);
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
  append_prop_beam(
      verts, idx, {-0.176F, 1.700F, 0.140F}, {-0.128F, 1.436F, 0.226F}, 0.075F, 0.026F);

  upload_prop_mesh_impl(verts, idx, m_statue_mesh);
}

void VegetationPipeline::initialize_magic_shrine_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_magic_shrine_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

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

  append_box(verts, idx, {-0.86F, -0.02F, -0.86F}, {0.86F, 0.08F, 0.86F});
  append_box(verts, idx, {-0.68F, 0.08F, -0.68F}, {0.68F, 0.16F, 0.68F});
  append_box(verts, idx, {-0.82F, 0.08F, -0.24F}, {0.82F, 0.15F, 0.24F});
  append_box(verts, idx, {-0.24F, 0.08F, -0.82F}, {0.24F, 0.15F, 0.82F});
  append_box(verts, idx, {-0.48F, 0.16F, -0.48F}, {0.48F, 0.24F, 0.48F});

  append_box(verts, idx, {-0.26F, 0.24F, -0.26F}, {0.26F, 0.72F, 0.26F});
  append_box(verts, idx, {-0.32F, 0.72F, -0.32F}, {0.32F, 0.80F, 0.32F});
  append_box(verts, idx, {-0.26F, 0.80F, -0.26F}, {-0.08F, 0.92F, 0.26F});
  append_box(verts, idx, {0.08F, 0.80F, -0.26F}, {0.26F, 0.92F, 0.26F});
  append_box(verts, idx, {-0.08F, 0.80F, -0.26F}, {0.08F, 0.92F, -0.08F});
  append_box(verts, idx, {-0.08F, 0.80F, 0.08F}, {0.08F, 0.92F, 0.26F});
  append_box(verts, idx, {-0.16F, 0.92F, -0.16F}, {0.16F, 1.00F, 0.16F});

  append_vert_prism(verts, idx, 0.0F, 0.96F, 0.0F, 0.10F, 0.82F, 6);
  append_vert_prism(verts, idx, 0.0F, 1.78F, 0.0F, 0.065F, 0.24F, 5);
  append_oriented_box(
      verts, idx, {-0.05F, 1.52F, 0.0F}, {-0.42F, 1.76F, 0.0F}, 0.055F, 0.065F);
  append_oriented_box(
      verts, idx, {0.05F, 1.52F, 0.0F}, {0.42F, 1.76F, 0.0F}, 0.055F, 0.065F);
  append_oriented_box(
      verts, idx, {-0.42F, 1.76F, 0.0F}, {-0.52F, 1.96F, 0.0F}, 0.045F, 0.055F);
  append_oriented_box(
      verts, idx, {0.42F, 1.76F, 0.0F}, {0.52F, 1.96F, 0.0F}, 0.045F, 0.055F);

  append_oriented_box(
      verts, idx, {-0.58F, 0.16F, -0.12F}, {-0.26F, 0.60F, -0.12F}, 0.055F, 0.06F);
  append_oriented_box(
      verts, idx, {0.58F, 0.16F, 0.12F}, {0.26F, 0.60F, 0.12F}, 0.055F, 0.06F);
  append_oriented_box(
      verts, idx, {-0.12F, 0.16F, -0.58F}, {-0.12F, 0.58F, -0.24F}, 0.055F, 0.07F);
  append_oriented_box(
      verts, idx, {0.12F, 0.16F, 0.58F}, {0.12F, 0.58F, 0.24F}, 0.055F, 0.07F);

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

  append_box(verts, idx, {-0.58F, 1.12F, -0.60F}, {0.58F, 1.20F, -0.44F});
  append_box(verts, idx, {-0.58F, 1.12F, 0.44F}, {0.58F, 1.20F, 0.60F});
  append_box(verts, idx, {-0.60F, 1.12F, -0.58F}, {-0.44F, 1.20F, 0.58F});
  append_box(verts, idx, {0.44F, 1.12F, -0.58F}, {0.60F, 1.20F, 0.58F});

  add_rune_stone({-0.72F, 0.0F, -0.06F}, 0.6F);
  add_rune_stone({0.72F, 0.0F, 0.08F}, 2.5F);
  add_rune_stone({-0.06F, 0.0F, 0.72F}, 1.3F);
  add_rune_stone({0.10F, 0.0F, -0.74F}, -1.2F);
  add_rune_stone({-0.62F, 0.0F, 0.58F}, 0.9F);
  add_rune_stone({0.62F, 0.0F, -0.60F}, -0.4F);

  upload_prop_mesh_impl(verts, idx, m_magic_shrine_mesh);
}

} // namespace Render::GL::BackendPipelines
