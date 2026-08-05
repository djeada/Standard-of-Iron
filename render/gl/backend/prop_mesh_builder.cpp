#include "prop_mesh_builder.h"

#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace Render::GL::BackendPipelines {

void append_box(PropMeshVerts& verts,
                PropMeshIndices& idx,
                const QVector3D& lo,
                const QVector3D& hi) {

  using F = std::pair<QVector3D, QVector3D>;
  const float x0 = lo.x();
  const float y0 = lo.y();
  const float z0 = lo.z();
  const float x1 = hi.x();
  const float y1 = hi.y();
  const float z1 = hi.z();

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, 0, 1);
    verts.insert(verts.end(),
                 {F{{x0, y0, z1}, n},
                  F{{x1, y0, z1}, n},
                  F{{x1, y1, z1}, n},
                  F{{x0, y1, z1}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, 0, -1);
    verts.insert(verts.end(),
                 {F{{x1, y0, z0}, n},
                  F{{x0, y0, z0}, n},
                  F{{x0, y1, z0}, n},
                  F{{x1, y1, z0}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(1, 0, 0);
    verts.insert(verts.end(),
                 {F{{x1, y0, z0}, n},
                  F{{x1, y0, z1}, n},
                  F{{x1, y1, z1}, n},
                  F{{x1, y1, z0}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(-1, 0, 0);
    verts.insert(verts.end(),
                 {F{{x0, y0, z1}, n},
                  F{{x0, y0, z0}, n},
                  F{{x0, y1, z0}, n},
                  F{{x0, y1, z1}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, 1, 0);
    verts.insert(verts.end(),
                 {F{{x0, y1, z0}, n},
                  F{{x0, y1, z1}, n},
                  F{{x1, y1, z1}, n},
                  F{{x1, y1, z0}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, -1, 0);
    verts.insert(verts.end(),
                 {F{{x0, y0, z1}, n},
                  F{{x0, y0, z0}, n},
                  F{{x1, y0, z0}, n},
                  F{{x1, y0, z1}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }
}

void append_quad(PropMeshVerts& verts,
                 PropMeshIndices& idx,
                 const QVector3D& p0,
                 const QVector3D& p1,
                 const QVector3D& p2,
                 const QVector3D& p3,
                 const QVector3D& n) {
  using F = std::pair<QVector3D, QVector3D>;
  auto b = static_cast<uint16_t>(verts.size());
  verts.insert(verts.end(), {F{p0, n}, F{p1, n}, F{p2, n}, F{p3, n}});
  idx.insert(
      idx.end(),
      {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
}

void append_oriented_box(PropMeshVerts& verts,
                         PropMeshIndices& idx,
                         const QVector3D& a,
                         const QVector3D& b,
                         float half_width,
                         float half_depth) {
  QVector3D dir = b - a;
  if (dir.lengthSquared() < 1.0e-8F) {
    dir = {0.0F, 1.0F, 0.0F};
  } else {
    dir.normalize();
  }

  const QVector3D reference = std::abs(dir.y()) < 0.9F ? QVector3D(0.0F, 1.0F, 0.0F)
                                                       : QVector3D(1.0F, 0.0F, 0.0F);
  QVector3D side = QVector3D::crossProduct(reference, dir);
  if (side.lengthSquared() < 1.0e-8F) {
    side = {1.0F, 0.0F, 0.0F};
  } else {
    side.normalize();
  }
  QVector3D depth = QVector3D::crossProduct(dir, side);
  if (depth.lengthSquared() < 1.0e-8F) {
    depth = {0.0F, 0.0F, 1.0F};
  } else {
    depth.normalize();
  }

  side *= half_width;
  depth *= half_depth;

  const QVector3D a0 = a - side - depth;
  const QVector3D a1 = a + side - depth;
  const QVector3D a2 = a + side + depth;
  const QVector3D a3 = a - side + depth;
  const QVector3D b0 = b - side - depth;
  const QVector3D b1 = b + side - depth;
  const QVector3D b2 = b + side + depth;
  const QVector3D b3 = b - side + depth;

  auto face = [&](const QVector3D& p0,
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

  face(a0, b0, b1, a1);
  face(a3, a2, b2, b3);
  face(a1, b1, b2, a2);
  face(a0, a3, b3, b0);
  face(a0, a1, a2, a3);
  face(b0, b3, b2, b1);
}

void append_prop_taper(PropMeshVerts& verts,
                       PropMeshIndices& idx,
                       float cx,
                       float y0,
                       float cz,
                       float r0,
                       float r1,
                       float height,
                       int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float y1 = y0 + height;
  const float slope = (r0 - r1) / std::max(height, 1.0e-4F);

  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    float const a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    float const amid = (a0 + a1) * 0.5F;
    QVector3D normal(std::cos(amid), slope, std::sin(amid));
    normal.normalize();
    append_quad(verts,
                idx,
                {cx + r0 * std::cos(a0), y0, cz + r0 * std::sin(a0)},
                {cx + r0 * std::cos(a1), y0, cz + r0 * std::sin(a1)},
                {cx + r1 * std::cos(a1), y1, cz + r1 * std::sin(a1)},
                {cx + r1 * std::cos(a0), y1, cz + r1 * std::sin(a0)},
                normal);
  }

  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const down(0.0F, -1.0F, 0.0F);
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    float const a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    auto top = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{{cx, y1, cz}, up},
                  F{{cx + r1 * std::cos(a0), y1, cz + r1 * std::sin(a0)}, up},
                  F{{cx + r1 * std::cos(a1), y1, cz + r1 * std::sin(a1)}, up}});
    idx.insert(idx.end(), {top, uint16_t(top + 1), uint16_t(top + 2)});

    auto bottom = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{{cx, y0, cz}, down},
                  F{{cx + r0 * std::cos(a1), y0, cz + r0 * std::sin(a1)}, down},
                  F{{cx + r0 * std::cos(a0), y0, cz + r0 * std::sin(a0)}, down}});
    idx.insert(idx.end(), {bottom, uint16_t(bottom + 1), uint16_t(bottom + 2)});
  }
}

void append_prop_beam(PropMeshVerts& verts,
                      PropMeshIndices& idx,
                      const QVector3D& a,
                      const QVector3D& b,
                      float half_width,
                      float half_depth) {
  QVector3D dir = b - a;
  if (dir.lengthSquared() < 1.0e-8F) {
    dir = {0.0F, 1.0F, 0.0F};
  } else {
    dir.normalize();
  }
  const QVector3D reference = std::abs(dir.y()) < 0.9F ? QVector3D(0.0F, 1.0F, 0.0F)
                                                       : QVector3D(1.0F, 0.0F, 0.0F);
  QVector3D side = QVector3D::crossProduct(reference, dir);
  if (side.lengthSquared() < 1.0e-8F) {
    side = {1.0F, 0.0F, 0.0F};
  } else {
    side.normalize();
  }
  QVector3D depth = QVector3D::crossProduct(dir, side);
  if (depth.lengthSquared() < 1.0e-8F) {
    depth = {0.0F, 0.0F, 1.0F};
  } else {
    depth.normalize();
  }
  side *= half_width;
  depth *= half_depth;

  const QVector3D center = (a + b) * 0.5F;
  const std::array<QVector3D, 8> corner{a - side - depth,
                                        a + side - depth,
                                        a + side + depth,
                                        a - side + depth,
                                        b - side - depth,
                                        b + side - depth,
                                        b + side + depth,
                                        b - side + depth};

  auto face = [&](int i0, int i1, int i2, int i3) {
    const QVector3D& p0 = corner[i0];
    const QVector3D& p1 = corner[i1];
    const QVector3D& p2 = corner[i2];
    const QVector3D& p3 = corner[i3];
    QVector3D normal = QVector3D::crossProduct(p1 - p0, p3 - p0);
    if (normal.lengthSquared() > 1.0e-8F) {
      normal.normalize();
    } else {
      normal = {0.0F, 1.0F, 0.0F};
    }
    const QVector3D face_center = (p0 + p1 + p2 + p3) * 0.25F;
    if (QVector3D::dotProduct(normal, face_center - center) < 0.0F) {
      normal = -normal;
    }
    append_quad(verts, idx, p0, p1, p2, p3, normal);
  };

  face(0, 4, 5, 1);
  face(3, 2, 6, 7);
  face(1, 5, 6, 2);
  face(0, 3, 7, 4);
  face(0, 1, 2, 3);
  face(4, 7, 6, 5);
}

void append_prop_slab(PropMeshVerts& verts,
                      PropMeshIndices& idx,
                      float y0,
                      float y1,
                      float half_bottom,
                      float half_top) {
  const float dy = std::max(y1 - y0, 1.0e-4F);
  const float batter = half_bottom - half_top;

  const QVector3D nx = QVector3D(dy, batter, 0.0F).normalized();
  const QVector3D nz = QVector3D(0.0F, batter, dy).normalized();

  append_quad(verts,
              idx,
              {half_bottom, y0, -half_bottom},
              {half_bottom, y0, half_bottom},
              {half_top, y1, half_top},
              {half_top, y1, -half_top},
              nx);
  append_quad(verts,
              idx,
              {-half_bottom, y0, half_bottom},
              {-half_bottom, y0, -half_bottom},
              {-half_top, y1, -half_top},
              {-half_top, y1, half_top},
              {-nx.x(), nx.y(), nx.z()});
  append_quad(verts,
              idx,
              {-half_bottom, y0, half_bottom},
              {half_bottom, y0, half_bottom},
              {half_top, y1, half_top},
              {-half_top, y1, half_top},
              nz);
  append_quad(verts,
              idx,
              {half_bottom, y0, -half_bottom},
              {-half_bottom, y0, -half_bottom},
              {-half_top, y1, -half_top},
              {half_top, y1, -half_top},
              {nz.x(), nz.y(), -nz.z()});
  append_quad(verts,
              idx,
              {-half_top, y1, -half_top},
              {-half_top, y1, half_top},
              {half_top, y1, half_top},
              {half_top, y1, -half_top},
              {0.0F, 1.0F, 0.0F});
  append_quad(verts,
              idx,
              {-half_bottom, y0, half_bottom},
              {-half_bottom, y0, -half_bottom},
              {half_bottom, y0, -half_bottom},
              {half_bottom, y0, half_bottom},
              {0.0F, -1.0F, 0.0F});
}

void append_prop_frustum(PropMeshVerts& verts,
                         PropMeshIndices& idx,
                         float cx,
                         float y0,
                         float cz,
                         float rx0,
                         float rz0,
                         float rx1,
                         float rz1,
                         float height,
                         int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float y1 = y0 + height;
  const float dx = (rx1 - rx0) / std::max(height, 1.0e-4F);
  const float dz = (rz1 - rz0) / std::max(height, 1.0e-4F);

  auto ring_normal = [&](float angle) {
    const float cs = std::cos(angle);
    const float sn = std::sin(angle);
    QVector3D n(rz0 * cs, -(rz0 * dx * cs * cs + rx0 * dz * sn * sn), rx0 * sn);
    if (n.lengthSquared() < 1.0e-10F) {
      return QVector3D(cs, 0.0F, sn);
    }
    return n.normalized();
  };

  for (int i = 0; i < segs; ++i) {
    const float a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    const float a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    append_quad(verts,
                idx,
                {cx + rx0 * std::cos(a0), y0, cz + rz0 * std::sin(a0)},
                {cx + rx0 * std::cos(a1), y0, cz + rz0 * std::sin(a1)},
                {cx + rx1 * std::cos(a1), y1, cz + rz1 * std::sin(a1)},
                {cx + rx1 * std::cos(a0), y1, cz + rz1 * std::sin(a0)},
                ring_normal((a0 + a1) * 0.5F));
  }

  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D down(0.0F, -1.0F, 0.0F);
  for (int i = 0; i < segs; ++i) {
    const float a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    const float a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    auto top = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{{cx, y1, cz}, up},
                  F{{cx + rx1 * std::cos(a0), y1, cz + rz1 * std::sin(a0)}, up},
                  F{{cx + rx1 * std::cos(a1), y1, cz + rz1 * std::sin(a1)}, up}});
    idx.insert(idx.end(), {top, uint16_t(top + 1), uint16_t(top + 2)});

    auto bottom = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{{cx, y0, cz}, down},
                  F{{cx + rx0 * std::cos(a1), y0, cz + rz0 * std::sin(a1)}, down},
                  F{{cx + rx0 * std::cos(a0), y0, cz + rz0 * std::sin(a0)}, down}});
    idx.insert(idx.end(), {bottom, uint16_t(bottom + 1), uint16_t(bottom + 2)});
  }
}

void append_prop_limb(PropMeshVerts& verts,
                      PropMeshIndices& idx,
                      const QVector3D& a,
                      const QVector3D& b,
                      float r0,
                      float r1,
                      int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;

  QVector3D axis = b - a;
  const float length = axis.length();
  if (length < 1.0e-6F) {
    return;
  }
  axis /= length;

  const QVector3D reference = std::abs(axis.y()) < 0.9F ? QVector3D(0.0F, 1.0F, 0.0F)
                                                        : QVector3D(1.0F, 0.0F, 0.0F);
  QVector3D u = QVector3D::crossProduct(reference, axis);
  if (u.lengthSquared() < 1.0e-8F) {
    u = {1.0F, 0.0F, 0.0F};
  } else {
    u.normalize();
  }
  const QVector3D w = QVector3D::crossProduct(axis, u).normalized();
  const float slope = (r0 - r1) / length;

  auto radial = [&](float angle) {
    return u * std::cos(angle) + w * std::sin(angle);
  };

  for (int i = 0; i < segs; ++i) {
    const float a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    const float a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    const QVector3D d0 = radial(a0);
    const QVector3D d1 = radial(a1);
    const QVector3D n = (radial((a0 + a1) * 0.5F) + axis * slope).normalized();
    append_quad(verts, idx, a + d0 * r0, a + d1 * r0, b + d1 * r1, b + d0 * r1, n);
  }

  for (int i = 0; i < segs; ++i) {
    const float a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    const float a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    auto cap_b = static_cast<uint16_t>(verts.size());
    verts.insert(
        verts.end(),
        {F{b, axis}, F{b + radial(a0) * r1, axis}, F{b + radial(a1) * r1, axis}});
    idx.insert(idx.end(), {cap_b, uint16_t(cap_b + 1), uint16_t(cap_b + 2)});

    auto cap_a = static_cast<uint16_t>(verts.size());
    verts.insert(
        verts.end(),
        {F{a, -axis}, F{a + radial(a1) * r0, -axis}, F{a + radial(a0) * r0, -axis}});
    idx.insert(idx.end(), {cap_a, uint16_t(cap_a + 1), uint16_t(cap_a + 2)});
  }
}

void append_disc_xaxis(PropMeshVerts& verts,
                       PropMeshIndices& idx,
                       float cx,
                       float cy,
                       float cz,
                       float r,
                       float hthk,
                       int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float xa = cx - hthk;
  const float xb = cx + hthk;
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    float const y0 = cy + r * std::sin(a0);
    float const z0_ = cz + r * std::cos(a0);
    float const y1 = cy + r * std::sin(a1);
    float const z1_ = cz + r * std::cos(a1);
    float const amid = (a0 + a1) * 0.5F;
    QVector3D const n(0.0F, std::sin(amid), std::cos(amid));
    append_quad(
        verts, idx, {xa, y0, z0_}, {xa, y1, z1_}, {xb, y1, z1_}, {xb, y0, z0_}, n);
  }

  QVector3D const ncf(1, 0, 0);
  QVector3D const cf{xb, cy, cz};
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{cf, ncf},
                  F{{xb, cy + r * std::sin(a0), cz + r * std::cos(a0)}, ncf},
                  F{{xb, cy + r * std::sin(a1), cz + r * std::cos(a1)}, ncf}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }

  QVector3D const ncb(-1, 0, 0);
  QVector3D const cb{xa, cy, cz};
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{cb, ncb},
                  F{{xa, cy + r * std::sin(a1), cz + r * std::cos(a1)}, ncb},
                  F{{xa, cy + r * std::sin(a0), cz + r * std::cos(a0)}, ncb}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }
}

void append_spoked_wheel_xaxis(PropMeshVerts& verts,
                               PropMeshIndices& idx,
                               float cx,
                               float cy,
                               float cz,
                               float r,
                               float hthk,
                               int segs,
                               int spokes) {
  constexpr float k_tau = 6.28318530F;
  const float xa = cx - hthk;
  const float xb = cx + hthk;
  const float rim_inner = r * 0.70F;
  const float hub_r = std::min(0.085F, rim_inner * 0.55F);

  auto ring_point = [&](float x, float radius, float angle) {
    return QVector3D(x, cy + radius * std::sin(angle), cz + radius * std::cos(angle));
  };

  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * static_cast<float>(i) / static_cast<float>(segs);
    float const a1 = k_tau * static_cast<float>(i + 1) / static_cast<float>(segs);
    float const amid = (a0 + a1) * 0.5F;
    QVector3D const outward(0.0F, std::sin(amid), std::cos(amid));

    append_quad(verts,
                idx,
                ring_point(xa, r, a0),
                ring_point(xa, r, a1),
                ring_point(xb, r, a1),
                ring_point(xb, r, a0),
                outward);
    append_quad(verts,
                idx,
                ring_point(xb, rim_inner, a0),
                ring_point(xb, rim_inner, a1),
                ring_point(xa, rim_inner, a1),
                ring_point(xa, rim_inner, a0),
                -outward);
    append_quad(verts,
                idx,
                ring_point(xb, rim_inner, a0),
                ring_point(xb, r, a0),
                ring_point(xb, r, a1),
                ring_point(xb, rim_inner, a1),
                {1.0F, 0.0F, 0.0F});
    append_quad(verts,
                idx,
                ring_point(xa, rim_inner, a1),
                ring_point(xa, r, a1),
                ring_point(xa, r, a0),
                ring_point(xa, rim_inner, a0),
                {-1.0F, 0.0F, 0.0F});
  }

  append_disc_xaxis(verts, idx, cx, cy, cz, hub_r, hthk * 1.45F, 8);

  float const spoke_half_thickness = hthk * 0.42F;
  float const spoke_half_width = std::max(0.016F, r * 0.070F);
  for (int i = 0; i < spokes; ++i) {
    float const angle = k_tau * static_cast<float>(i) / static_cast<float>(spokes);
    append_prop_beam(verts,
                     idx,
                     ring_point(cx, hub_r * 0.85F, angle),
                     ring_point(cx, rim_inner + 0.008F, angle),
                     spoke_half_width,
                     spoke_half_thickness);
  }
}

void append_vert_prism(PropMeshVerts& verts,
                       PropMeshIndices& idx,
                       float cx,
                       float y0,
                       float cz,
                       float r,
                       float height,
                       int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float y1 = y0 + height;
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    float const x0 = cx + r * std::cos(a0);
    float const z0_ = cz + r * std::sin(a0);
    float const x1 = cx + r * std::cos(a1);
    float const z1_ = cz + r * std::sin(a1);
    float const amid = (a0 + a1) * 0.5F;
    QVector3D const n(std::cos(amid), 0.0F, std::sin(amid));
    append_quad(
        verts, idx, {x0, y0, z0_}, {x1, y0, z1_}, {x1, y1, z1_}, {x0, y1, z0_}, n);
  }

  QVector3D const tn(0, 1, 0);
  QVector3D const tc{cx, y1, cz};
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{tc, tn},
                  F{{cx + r * std::cos(a0), y1, cz + r * std::sin(a0)}, tn},
                  F{{cx + r * std::cos(a1), y1, cz + r * std::sin(a1)}, tn}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }
}

void append_barrel_yaxis(PropMeshVerts& verts,
                         PropMeshIndices& idx,
                         float cx,
                         float y0,
                         float cz,
                         float r,
                         float height,
                         int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  static constexpr float k_profile_t[] = {0.0F, 0.26F, 0.5F, 0.74F, 1.0F};
  static constexpr float k_profile_r[] = {0.81F, 0.96F, 1.0F, 0.96F, 0.81F};
  constexpr int k_stations = static_cast<int>(std::size(k_profile_t));

  auto station_point = [&](int station, int side) {
    float const angle = k_tau * static_cast<float>(side) / static_cast<float>(segs);
    float const radius = r * k_profile_r[station];
    return QVector3D(cx + radius * std::cos(angle),
                     y0 + height * k_profile_t[station],
                     cz + radius * std::sin(angle));
  };

  for (int station = 0; station + 1 < k_stations; ++station) {
    for (int side = 0; side < segs; ++side) {
      int const next = (side + 1) % segs;
      QVector3D const lower_a = station_point(station, side);
      QVector3D const lower_b = station_point(station, next);
      QVector3D const upper_b = station_point(station + 1, next);
      QVector3D const upper_a = station_point(station + 1, side);
      QVector3D n = QVector3D::crossProduct(upper_a - lower_a, lower_b - lower_a);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      append_quad(verts, idx, lower_a, lower_b, upper_b, upper_a, n);
    }
  }

  QVector3D const top_normal(0.0F, 1.0F, 0.0F);
  QVector3D const top_center(cx, y0 + height, cz);
  QVector3D const bottom_normal(0.0F, -1.0F, 0.0F);
  QVector3D const bottom_center(cx, y0, cz);
  for (int side = 0; side < segs; ++side) {
    int const next = (side + 1) % segs;
    auto top_base = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{top_center, top_normal},
                  F{station_point(k_stations - 1, side), top_normal},
                  F{station_point(k_stations - 1, next), top_normal}});
    idx.insert(idx.end(), {top_base, uint16_t(top_base + 1), uint16_t(top_base + 2)});

    auto bottom_base = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{bottom_center, bottom_normal},
                  F{station_point(0, next), bottom_normal},
                  F{station_point(0, side), bottom_normal}});
    idx.insert(idx.end(),
               {bottom_base, uint16_t(bottom_base + 1), uint16_t(bottom_base + 2)});
  }
}
} // namespace Render::GL::BackendPipelines
