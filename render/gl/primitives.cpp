#include "primitives.h"
#include "gl/mesh.h"
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <qvectornd.h>
#include <vector>

namespace Render::GL {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float k_two_pi = 6.28318530718F;
constexpr float k_half_scalar = 0.5F;
constexpr float k_unit_radius = 1.0F;
constexpr float k_micro_noise_frequency = 12.9898F;
constexpr float k_micro_noise_scale = 43758.5453F;
constexpr float k_uv_center = 0.5F;
constexpr float k_uv_scale = 0.5F;
constexpr int k_indices_per_quad = 6;

auto createUnitCylinderMesh(int radialSegments) -> Mesh * {
  const float radius = k_unit_radius;
  const float half_h = k_half_scalar;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= 1; ++y) {
    float py = (y != 0) ? half_h : -half_h;
    auto v_coord = float(y);
    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float const ang = u * k_two_pi;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0F, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v_coord}});
    }
  }
  int const row = radialSegments + 1;
  for (int i = 0; i < radialSegments; ++i) {
    int a = 0 * row + i;
    int b = 0 * row + i + 1;
    int c = 1 * row + i + 1;
    int d = 1 * row + i;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(c);
    idx.push_back(d);
    idx.push_back(a);
  }

  int base_top = (int)v.size();
  v.push_back(
      {{0.0F, half_h, 0.0F}, {0.0F, 1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radialSegments; ++i) {
    float const u = float(i) / float(radialSegments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, half_h, pz},
                 {0.0F, 1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(base_top);
    idx.push_back(base_top + i);
    idx.push_back(base_top + i + 1);
  }

  int base_bot = (int)v.size();
  v.push_back(
      {{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radialSegments; ++i) {
    float const u = float(i) / float(radialSegments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, -half_h, pz},
                 {0.0F, -1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(base_bot);
    idx.push_back(base_bot + i + 1);
    idx.push_back(base_bot + i);
  }

  return new Mesh(v, idx);
}

auto createUnitSphereMesh(int latSegments, int lonSegments) -> Mesh * {
  const float r = k_unit_radius;
  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= latSegments; ++y) {
    float vy = float(y) / float(latSegments);
    float const phi = vy * kPi;
    float py = r * std::cos(phi);
    float const pr = r * std::sin(phi);

    for (int x = 0; x <= lonSegments; ++x) {
      float vx = float(x) / float(lonSegments);
      float const theta = vx * k_two_pi;
      float px = pr * std::cos(theta);
      float pz = pr * std::sin(theta);

      QVector3D n(px, py, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {vx, vy}});
    }
  }

  int const row = lonSegments + 1;
  for (int y = 0; y < latSegments; ++y) {
    for (int x = 0; x < lonSegments; ++x) {
      int a = y * row + x;
      int b = a + 1;
      int c = (y + 1) * row + x + 1;
      int d = (y + 1) * row + x;
      idx.push_back(a);
      idx.push_back(b);
      idx.push_back(c);
      idx.push_back(c);
      idx.push_back(d);
      idx.push_back(a);
    }
  }

  return new Mesh(v, idx);
}

auto createUnitConeMesh(int radialSegments) -> Mesh * {
  const float base_r = k_unit_radius;
  const float half_h = k_half_scalar;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  int apex_idx = 0;
  v.push_back({{0.0F, +half_h, 0.0F}, {0.0F, 1.0F, 0.0F}, {k_uv_center, 1.0F}});

  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float const ang = u * k_two_pi;
    float px = base_r * std::cos(ang);
    float pz = base_r * std::sin(ang);
    QVector3D n(px, base_r, pz);
    n.normalize();
    v.push_back({{px, -half_h, pz}, {n.x(), n.y(), n.z()}, {u, 0.0F}});
  }

  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(apex_idx);
    idx.push_back(i);
    idx.push_back(i + 1);
  }

  int base_center = (int)v.size();
  v.push_back(
      {{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  int const base_start = (int)v.size();
  for (int i = 0; i <= radialSegments; ++i) {
    float const u = float(i) / float(radialSegments);
    float const ang = u * k_two_pi;
    float px = base_r * std::cos(ang);
    float pz = base_r * std::sin(ang);
    v.push_back({{px, -half_h, pz},
                 {0.0F, -1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 0; i < radialSegments; ++i) {
    idx.push_back(base_center);
    idx.push_back(base_start + i + 1);
    idx.push_back(base_start + i);
  }

  return new Mesh(v, idx);
}

auto createCapsuleMesh(int radialSegments, int heightSegments) -> Mesh * {
  constexpr float k_capsule_radius = 0.25F;
  const float radius = k_capsule_radius;
  const float half_h = k_half_scalar;

  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= heightSegments; ++y) {
    float v = float(y) / float(heightSegments);
    float py = -half_h + v * (2.0F * half_h);
    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float const ang = u * k_two_pi;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0F, pz);
      n.normalize();
      verts.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v}});
    }
  }

  int const row = radialSegments + 1;
  for (int y = 0; y < heightSegments; ++y) {
    for (int i = 0; i < radialSegments; ++i) {
      int a = y * row + i;
      int b = y * row + i + 1;
      int c = (y + 1) * row + i + 1;
      int d = (y + 1) * row + i;
      idx.push_back(a);
      idx.push_back(b);
      idx.push_back(c);
      idx.push_back(c);
      idx.push_back(d);
      idx.push_back(a);
    }
  }

  int base_top = (int)verts.size();
  verts.push_back(
      {{0.0F, half_h, 0.0F}, {0.0F, 1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radialSegments; ++i) {
    float const u = float(i) / float(radialSegments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back({{px, half_h, pz},
                     {0.0F, 1.0F, 0.0F},
                     {k_uv_center + k_uv_scale * std::cos(ang),
                      k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(base_top);
    idx.push_back(base_top + i);
    idx.push_back(base_top + i + 1);
  }

  int base_bot = (int)verts.size();
  verts.push_back(
      {{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radialSegments; ++i) {
    float const u = float(i) / float(radialSegments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back({{px, -half_h, pz},
                     {0.0F, -1.0F, 0.0F},
                     {k_uv_center + k_uv_scale * std::cos(ang),
                      k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(base_bot);
    idx.push_back(base_bot + i + 1);
    idx.push_back(base_bot + i);
  }

  return new Mesh(verts, idx);
}

auto simpleHash(float seed) -> float {
  float const x =
      std::sin(seed * k_micro_noise_frequency) * k_micro_noise_scale;
  return x - std::floor(x);
}

auto createUnitTorsoMesh(int radialSegments, int heightSegments) -> Mesh * {
  const float half_h = k_half_scalar;

  const bool invert_profile = true;

  constexpr float k_band_epsilon = 1e-6F;
  constexpr float k_radius_epsilon = 1e-8F;

  constexpr float k_xforward_amp = 0.02F;
  constexpr float k_xforward_start = 0.6F;
  constexpr float k_xforward_end = 0.95F;
  constexpr float k_xbackward_amp = -0.01F;
  constexpr float k_xbackward_start = 0.0F;
  constexpr float k_xbackward_end = 0.2F;

  constexpr float k_lordosis_amp = -0.03F;
  constexpr float k_lordosis_start = 0.15F;
  constexpr float k_lordosis_end = 0.40F;
  constexpr float k_chest_forward_amp = 0.035F;
  constexpr float k_chest_forward_start = 0.65F;
  constexpr float k_chest_forward_end = 0.85F;
  constexpr float k_neck_back_amp = -0.015F;
  constexpr float k_neck_back_start = 0.90F;
  constexpr float k_neck_back_end = 1.0F;

  constexpr float k_twist_amplitude = 0.10F;
  constexpr float k_twist_start = 0.55F;
  constexpr float k_twist_end = 0.95F;

  constexpr float k_theta_sin_pos_amp = 0.07F;
  constexpr float k_theta_sin_pos_start = 0.68F;
  constexpr float k_theta_sin_pos_end = 0.88F;
  constexpr float k_theta_sin_neg_amp = -0.03F;
  constexpr float k_theta_sin_neg_start = 0.65F;
  constexpr float k_theta_sin_neg_end = 0.90F;
  constexpr float k_theta_cos_sq_amp = 0.06F;
  constexpr float k_theta_cos_sq_start = 0.55F;
  constexpr float k_theta_cos_sq_end = 0.75F;
  constexpr float k_theta_cos_sq_neg_amp = -0.02F;
  constexpr float k_theta_cos_sq_neg_start = 0.40F;
  constexpr float k_theta_cos_sq_neg_end = 0.55F;
  constexpr float k_theta_cos_amp = 0.015F;
  constexpr float k_theta_cos_start = 0.70F;
  constexpr float k_theta_cos_end = 0.95F;

  constexpr float k_micro_temporal_frequency = 37.0F;
  constexpr float k_micro_angular_frequency = 3.0F;
  constexpr float k_micro_phase_offset = 1.23F;
  constexpr float k_micro_center = 0.5F;
  constexpr float k_micro_jitter = 0.004F;

  auto clampf = [](float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
  };
  auto smoothstep01 = [&](float x) {
    x = clampf(x, 0.0F, 1.0F);
    return x * x * (3.0F - 2.0F * x);
  };
  auto smooth_band = [&](float t, float a, float b) {
    float const enter = smoothstep01((t - a) / (b - a + k_band_epsilon));
    float const exit = smoothstep01((t - b) / (a - b - k_band_epsilon));
    float const v = enter < exit ? enter : exit;
    return clampf(v, 0.0F, 1.0F);
  };

  struct Axes {
    float ax;
    float az;
  };
  struct Key {
    float t;
    Axes A;
  };

  const Key keys[] = {
      {0.10F, {0.98F, 0.92F}}, {0.20F, {1.02F, 0.96F}}, {0.45F, {0.82F, 0.78F}},
      {0.65F, {1.20F, 1.04F}}, {0.85F, {1.42F, 1.18F}}, {1.02F, {1.60F, 1.06F}},
      {1.10F, {1.20F, 0.96F}},
  };
  constexpr int key_count = sizeof(keys) / sizeof(keys[0]);

  auto cat_rom = [](float p0, float p1, float p2, float p3, float u) {
    return 0.5F * ((2.0F * p1) + (-p0 + p2) * u +
                   (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3) * u * u +
                   (-p0 + 3.0F * p1 - 3.0F * p2 + p3) * u * u * u);
  };

  auto sample_axes = [&](float t) -> Axes {
    t = clampf(t, 0.0F, 1.0F);
    int i = 0;
    while (i + 1 < key_count && t > keys[i + 1].t) {
      ++i;
    }
    int const i0 = i > 0 ? i - 1 : 0;
    int const i1 = i;
    int const i2 = (i + 1 < key_count) ? i + 1 : key_count - 1;
    int const i3 = (i + 2 < key_count) ? i + 2 : key_count - 1;

    float const denom = (keys[i2].t - keys[i1].t);
    float u = denom > k_band_epsilon ? (t - keys[i1].t) / denom : 0.0F;
    u = clampf(u, 0.0F, 1.0F);

    float const ax =
        cat_rom(keys[i0].A.ax, keys[i1].A.ax, keys[i2].A.ax, keys[i3].A.ax, u);
    float const az =
        cat_rom(keys[i0].A.az, keys[i1].A.az, keys[i2].A.az, keys[i3].A.az, u);
    return {ax, az};
  };

  auto ellipse_radius = [&](float a, float b, float ang) {
    float const c = std::cos(ang);
    float const s = std::sin(ang);
    float const denom = std::sqrt((b * b * c * c) + (a * a * s * s));
    return (a * b) / (denom + k_radius_epsilon);
  };

  auto x_offset_at = [&](float t) {
    float const forward =
        k_xforward_amp * smooth_band(t, k_xforward_start, k_xforward_end);
    float const backward =
        k_xbackward_amp * smooth_band(t, k_xbackward_start, k_xbackward_end);
    return forward + backward;
  };
  auto z_offset_at = [&](float t) {
    float const lordosis =
        k_lordosis_amp * smooth_band(t, k_lordosis_start, k_lordosis_end);
    float const chest_fwd =
        k_chest_forward_amp *
        smooth_band(t, k_chest_forward_start, k_chest_forward_end);
    float const neck_back =
        k_neck_back_amp * smooth_band(t, k_neck_back_start, k_neck_back_end);
    return lordosis + chest_fwd + neck_back;
  };
  auto twist_at = [&](float t) {
    return k_twist_amplitude * smooth_band(t, k_twist_start, k_twist_end);
  };

  auto theta_scale = [&](float t, float ang) {
    float s = 0.0F;
    float const sinA = std::sin(ang);
    float const cosA = std::cos(ang);
    float const cos2 = cosA * cosA;
    s += k_theta_sin_pos_amp *
         smooth_band(t, k_theta_sin_pos_start, k_theta_sin_pos_end) *
         std::max(0.0F, sinA);
    s += k_theta_sin_neg_amp *
         smooth_band(t, k_theta_sin_neg_start, k_theta_sin_neg_end) *
         std::max(0.0F, -sinA);
    s += k_theta_cos_sq_amp *
         smooth_band(t, k_theta_cos_sq_start, k_theta_cos_sq_end) * cos2;
    s += k_theta_cos_sq_neg_amp *
         smooth_band(t, k_theta_cos_sq_neg_start, k_theta_cos_sq_neg_end) *
         cos2;
    s += k_theta_cos_amp * smooth_band(t, k_theta_cos_start, k_theta_cos_end) *
         cosA;
    return 1.0F + s;
  };

  auto micro = [&](float s) {
    float const f = std::sin(s * k_micro_noise_frequency) * k_micro_noise_scale;
    return f - std::floor(f);
  };

  auto sample_pos = [&](float t, float ang) -> QVector3D {
    float const ts = invert_profile ? (1.0F - t) : t;

    Axes const A = sample_axes(ts);
    float const twist = twist_at(ts);
    float const th = ang + twist;

    float const R = ellipse_radius(A.ax, A.az, th);
    float const S = theta_scale(ts, th);
    float const r = R * S;

    float px = r * std::cos(th);
    float pz = r * std::sin(th);

    px += x_offset_at(ts);
    pz += z_offset_at(ts);

    float const py = -half_h + t * (2.0F * half_h);

    float const s_value =
        (t * k_micro_temporal_frequency) + (ang * k_micro_angular_frequency);
    px += (micro(s_value) - k_micro_center) * k_micro_jitter;
    pz += (micro(s_value + k_micro_phase_offset) - k_micro_center) *
          k_micro_jitter;

    return {px, py, pz};
  };

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;
  v.reserve((radialSegments + 1) * (heightSegments + 1) +
            (radialSegments + 1) * 2 + 2);
  idx.reserve(radialSegments * heightSegments * k_indices_per_quad +
              radialSegments * k_indices_per_quad);

  for (int y = 0; y <= heightSegments; ++y) {
    float const t = float(y) / float(heightSegments);
    float const dt = 1.0F / float(heightSegments);
    float v_coord = t;

    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float const ang = u * k_two_pi;
      float const da = k_two_pi / float(radialSegments);

      QVector3D const p = sample_pos(t, ang);
      QVector3D const pu = sample_pos(t, ang + da);
      QVector3D const pv = sample_pos(clampf(t + dt, 0.0F, 1.0F), ang);

      QVector3D const du = pu - p;
      QVector3D const dv = pv - p;

      QVector3D n = QVector3D::crossProduct(du, dv);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      }

      v.push_back({{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {u, v_coord}});
    }
  }

  int const row = radialSegments + 1;
  for (int y = 0; y < heightSegments; ++y) {
    for (int i = 0; i < radialSegments; ++i) {
      int a = y * row + i;
      int b = y * row + i + 1;
      int c = (y + 1) * row + i + 1;
      int d = (y + 1) * row + i;

      idx.push_back(a);
      idx.push_back(b);
      idx.push_back(c);
      idx.push_back(c);
      idx.push_back(d);
      idx.push_back(a);
    }
  }

  {

    int base_top = (int)v.size();
    float const tTop = 1.0F;
    float const t_top_s = invert_profile ? (1.0F - tTop) : tTop;
    QVector3D const cTop(x_offset_at(t_top_s), half_h, z_offset_at(t_top_s));
    v.push_back({{cTop.x(), cTop.y(), cTop.z()},
                 {0, 1, 0},
                 {k_uv_center, k_uv_center}});
    for (int i = 0; i <= radialSegments; ++i) {
      float const u = float(i) / float(radialSegments);
      float const ang = u * k_two_pi;
      QVector3D const p = sample_pos(tTop, ang);
      v.push_back({{p.x(), p.y(), p.z()},
                   {0, 1, 0},
                   {k_uv_center + k_uv_scale * std::cos(ang),
                    k_uv_center + k_uv_scale * std::sin(ang)}});
    }
    for (int i = 1; i <= radialSegments; ++i) {
      idx.push_back(base_top);
      idx.push_back(base_top + i);
      idx.push_back(base_top + i + 1);
    }
  }
  {

    int base_bot = (int)v.size();
    float const tBot = 0.0F;
    float const t_bot_s = invert_profile ? (1.0F - tBot) : tBot;
    QVector3D const cBot(x_offset_at(t_bot_s), -half_h, z_offset_at(t_bot_s));
    v.push_back({{cBot.x(), cBot.y(), cBot.z()},
                 {0, -1, 0},
                 {k_uv_center, k_uv_center}});
    for (int i = 0; i <= radialSegments; ++i) {
      float const u = float(i) / float(radialSegments);
      float const ang = u * k_two_pi;
      QVector3D const p = sample_pos(tBot, ang);
      v.push_back({{p.x(), p.y(), p.z()},
                   {0, -1, 0},
                   {k_uv_center + k_uv_scale * std::cos(ang),
                    k_uv_center + k_uv_scale * std::sin(ang)}});
    }
    for (int i = 1; i <= radialSegments; ++i) {
      idx.push_back(base_bot);
      idx.push_back(base_bot + i + 1);
      idx.push_back(base_bot + i);
    }
  }

  return new Mesh(v, idx);
}

} // namespace

auto getUnitCylinder(int radialSegments) -> Mesh * {
  static std::unique_ptr<Mesh> const s_mesh(
      createUnitCylinderMesh(radialSegments));
  return s_mesh.get();
}

auto getUnitSphere(int latSegments, int lonSegments) -> Mesh * {
  static std::unique_ptr<Mesh> const s_mesh(
      createUnitSphereMesh(latSegments, lonSegments));
  return s_mesh.get();
}

auto getUnitCone(int radialSegments) -> Mesh * {
  static std::unique_ptr<Mesh> const s_mesh(createUnitConeMesh(radialSegments));
  return s_mesh.get();
}

auto getUnitCapsule(int radialSegments, int heightSegments) -> Mesh * {
  static std::unique_ptr<Mesh> const s_mesh(
      createCapsuleMesh(radialSegments, heightSegments));
  return s_mesh.get();
}

auto getUnitTorso(int radialSegments, int heightSegments) -> Mesh * {
  static std::unique_ptr<Mesh> const s_mesh(
      createUnitTorsoMesh(radialSegments, heightSegments));
  return s_mesh.get();
}

} // namespace Render::GL
