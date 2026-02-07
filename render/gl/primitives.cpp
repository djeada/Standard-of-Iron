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

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_two_pi = 6.28318530718F;
constexpr float k_half_scalar = 0.5F;
constexpr float k_unit_radius = 1.0F;
constexpr float k_micro_noise_frequency = 12.9898F;
constexpr float k_micro_noise_scale = 43758.5453F;
constexpr float k_uv_center = 0.5F;
constexpr float k_uv_scale = 0.5F;
constexpr int k_indices_per_quad = 6;

auto create_unit_cylinder_mesh(int radial_segments) -> std::unique_ptr<Mesh> {
  const float radius = k_unit_radius;
  const float half_h = k_half_scalar;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= 1; ++y) {
    float py = (y != 0) ? half_h : -half_h;
    auto v_coord = float(y);
    for (int i = 0; i <= radial_segments; ++i) {
      float u = float(i) / float(radial_segments);
      float const ang = u * k_two_pi;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0F, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v_coord}});
    }
  }
  int const row = radial_segments + 1;
  for (int i = 0; i < radial_segments; ++i) {
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
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, half_h, pz},
                 {0.0F, 1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(base_top);
    idx.push_back(base_top + i);
    idx.push_back(base_top + i + 1);
  }

  int base_bot = (int)v.size();
  v.push_back(
      {{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, -half_h, pz},
                 {0.0F, -1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(base_bot);
    idx.push_back(base_bot + i + 1);
    idx.push_back(base_bot + i);
  }

  return std::make_unique<Mesh>(v, idx);
}

auto create_unit_sphere_mesh(int lat_segments,
                             int lon_segments) -> std::unique_ptr<Mesh> {
  const float r = k_unit_radius;
  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= lat_segments; ++y) {
    float vy = float(y) / float(lat_segments);
    float const phi = vy * k_pi;
    float py = r * std::cos(phi);
    float const pr = r * std::sin(phi);

    for (int x = 0; x <= lon_segments; ++x) {
      float vx = float(x) / float(lon_segments);
      float const theta = vx * k_two_pi;
      float px = pr * std::cos(theta);
      float pz = pr * std::sin(theta);

      QVector3D n(px, py, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {vx, vy}});
    }
  }

  int const row = lon_segments + 1;
  for (int y = 0; y < lat_segments; ++y) {
    for (int x = 0; x < lon_segments; ++x) {
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

  return std::make_unique<Mesh>(v, idx);
}

auto create_unit_cone_mesh(int radial_segments) -> std::unique_ptr<Mesh> {
  const float base_r = k_unit_radius;
  const float half_h = k_half_scalar;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  int apex_idx = 0;
  v.push_back({{0.0F, +half_h, 0.0F}, {0.0F, 1.0F, 0.0F}, {k_uv_center, 1.0F}});

  for (int i = 0; i <= radial_segments; ++i) {
    float u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float px = base_r * std::cos(ang);
    float pz = base_r * std::sin(ang);
    QVector3D n(px, base_r, pz);
    n.normalize();
    v.push_back({{px, -half_h, pz}, {n.x(), n.y(), n.z()}, {u, 0.0F}});
  }

  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(apex_idx);
    idx.push_back(i);
    idx.push_back(i + 1);
  }

  int base_center = (int)v.size();
  v.push_back(
      {{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  int const base_start = (int)v.size();
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float px = base_r * std::cos(ang);
    float pz = base_r * std::sin(ang);
    v.push_back({{px, -half_h, pz},
                 {0.0F, -1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 0; i < radial_segments; ++i) {
    idx.push_back(base_center);
    idx.push_back(base_start + i + 1);
    idx.push_back(base_start + i);
  }

  return std::make_unique<Mesh>(v, idx);
}

auto create_capsule_mesh(int radial_segments,
                         int height_segments) -> std::unique_ptr<Mesh> {
  constexpr float k_capsule_radius = 0.25F;
  const float radius = k_capsule_radius;
  const float half_h = k_half_scalar;

  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= height_segments; ++y) {
    float v = float(y) / float(height_segments);
    float py = -half_h + v * (2.0F * half_h);
    for (int i = 0; i <= radial_segments; ++i) {
      float u = float(i) / float(radial_segments);
      float const ang = u * k_two_pi;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0F, pz);
      n.normalize();
      verts.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v}});
    }
  }

  int const row = radial_segments + 1;
  for (int y = 0; y < height_segments; ++y) {
    for (int i = 0; i < radial_segments; ++i) {
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
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back({{px, half_h, pz},
                     {0.0F, 1.0F, 0.0F},
                     {k_uv_center + k_uv_scale * std::cos(ang),
                      k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(base_top);
    idx.push_back(base_top + i);
    idx.push_back(base_top + i + 1);
  }

  int base_bot = (int)verts.size();
  verts.push_back(
      {{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back({{px, -half_h, pz},
                     {0.0F, -1.0F, 0.0F},
                     {k_uv_center + k_uv_scale * std::cos(ang),
                      k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(base_bot);
    idx.push_back(base_bot + i + 1);
    idx.push_back(base_bot + i);
  }

  return std::make_unique<Mesh>(verts, idx);
}

auto simple_hash(float seed) -> float {
  float const x =
      std::sin(seed * k_micro_noise_frequency) * k_micro_noise_scale;
  return x - std::floor(x);
}

auto create_unit_torso_mesh(int radial_segments,
                            int height_segments) -> std::unique_ptr<Mesh> {
  const float half_h = k_half_scalar;

  constexpr float k_lower_extension = 0.05F;
  const float y_min = -half_h;
  const float y_max = half_h + k_lower_extension;
  const float y_span = y_max - y_min;

  const bool invert_profile = true;

  constexpr float k_band_epsilon = 1e-6F;
  constexpr float k_radius_epsilon = 1e-8F;

  constexpr float k_shoulder_dome_t_end = 0.10F;
  constexpr float k_shoulder_dome_height = 0.06F;
  constexpr float k_shoulder_dome_min_radius_scale = 0.06F;

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

  auto clamp_f = [](float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
  };
  auto smoothstep01 = [&](float x) {
    x = clamp_f(x, 0.0F, 1.0F);
    return x * x * (3.0F - 2.0F * x);
  };
  auto smooth_band = [&](float t, float a, float b) {
    float const enter = smoothstep01((t - a) / (b - a + k_band_epsilon));
    float const exit = smoothstep01((t - b) / (a - b - k_band_epsilon));
    float const v = enter < exit ? enter : exit;
    return clamp_f(v, 0.0F, 1.0F);
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
      {0.00F, {0.72F, 0.65F}}, {0.08F, {0.88F, 0.82F}}, {0.15F, {1.02F, 0.95F}},
      {0.22F, {0.98F, 0.92F}}, {0.45F, {0.76F, 0.70F}}, {0.65F, {1.12F, 1.06F}},
      {0.85F, {1.30F, 1.25F}}, {1.02F, {1.48F, 1.20F}}, {1.10F, {1.12F, 0.92F}},
  };
  constexpr int key_count = sizeof(keys) / sizeof(keys[0]);

  constexpr float k_shoulder_bulge_amp = 0.08F;
  constexpr float k_shoulder_bulge_start = 0.10F;
  constexpr float k_shoulder_bulge_end = 0.22F;

  constexpr float k_trap_slope_amp = 0.04F;
  constexpr float k_trap_slope_start = 0.00F;
  constexpr float k_trap_slope_end = 0.12F;

  auto cat_rom = [](float p0, float p1, float p2, float p3, float u) {
    return 0.5F * ((2.0F * p1) + (-p0 + p2) * u +
                   (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3) * u * u +
                   (-p0 + 3.0F * p1 - 3.0F * p2 + p3) * u * u * u);
  };

  auto sample_profile_axes = [&](float profile_t) -> Axes {
    profile_t = clamp_f(profile_t, 0.0F, 1.0F);
    int i = 0;
    while (i + 1 < key_count && profile_t > keys[i + 1].t) {
      ++i;
    }
    int const i0 = i > 0 ? i - 1 : 0;
    int const i1 = i;
    int const i2 = (i + 1 < key_count) ? i + 1 : key_count - 1;
    int const i3 = (i + 2 < key_count) ? i + 2 : key_count - 1;

    float const denom = (keys[i2].t - keys[i1].t);
    float u = denom > k_band_epsilon ? (profile_t - keys[i1].t) / denom : 0.0F;
    u = clamp_f(u, 0.0F, 1.0F);

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

  auto x_offset_at = [&](float profile_t) {
    float const forward =
        k_xforward_amp *
        smooth_band(profile_t, k_xforward_start, k_xforward_end);
    float const backward =
        k_xbackward_amp *
        smooth_band(profile_t, k_xbackward_start, k_xbackward_end);
    return forward + backward;
  };
  auto z_offset_at = [&](float profile_t) {
    float const lordosis =
        k_lordosis_amp *
        smooth_band(profile_t, k_lordosis_start, k_lordosis_end);
    float const chest_fwd =
        k_chest_forward_amp *
        smooth_band(profile_t, k_chest_forward_start, k_chest_forward_end);
    float const neck_back =
        k_neck_back_amp *
        smooth_band(profile_t, k_neck_back_start, k_neck_back_end);
    return lordosis + chest_fwd + neck_back;
  };
  auto twist_at = [&](float profile_t) {
    return k_twist_amplitude *
           smooth_band(profile_t, k_twist_start, k_twist_end);
  };

  auto theta_scale = [&](float profile_t, float ang) {
    float s = 0.0F;
    float const sin_a = std::sin(ang);
    float const cos_a = std::cos(ang);
    float const cos2 = cos_a * cos_a;
    s += k_theta_sin_pos_amp *
         smooth_band(profile_t, k_theta_sin_pos_start, k_theta_sin_pos_end) *
         std::max(0.0F, sin_a);
    s += k_theta_sin_neg_amp *
         smooth_band(profile_t, k_theta_sin_neg_start, k_theta_sin_neg_end) *
         std::max(0.0F, -sin_a);
    s += k_theta_cos_sq_amp *
         smooth_band(profile_t, k_theta_cos_sq_start, k_theta_cos_sq_end) *
         cos2;
    s += k_theta_cos_sq_neg_amp *
         smooth_band(profile_t, k_theta_cos_sq_neg_start,
                     k_theta_cos_sq_neg_end) *
         cos2;
    s += k_theta_cos_amp *
         smooth_band(profile_t, k_theta_cos_start, k_theta_cos_end) * cos_a;

    float const shoulder_band =
        smooth_band(profile_t, k_shoulder_bulge_start, k_shoulder_bulge_end);

    float const lateral_factor = std::abs(sin_a);
    s += k_shoulder_bulge_amp * shoulder_band * lateral_factor;

    float const trap_band =
        smooth_band(profile_t, k_trap_slope_start, k_trap_slope_end);

    float const trap_factor = (1.0F - std::abs(sin_a)) * 0.7F + 0.3F;
    s += k_trap_slope_amp * trap_band * trap_factor;

    return 1.0F + s;
  };

  auto micro = [&](float s) {
    float const f = std::sin(s * k_micro_noise_frequency) * k_micro_noise_scale;
    return f - std::floor(f);
  };

  auto sample_pos = [&](float t, float ang) -> QVector3D {
    float const profile_t = invert_profile ? (1.0F - t) : t;

    Axes const a = sample_profile_axes(profile_t);
    float const twist = twist_at(profile_t);
    float const th = ang + twist;

    float const radius = ellipse_radius(a.ax, a.az, th);
    float const s = theta_scale(profile_t, th);
    float r = radius * s;

    float py = y_min + t * y_span;

    if (t < k_shoulder_dome_t_end) {
      float const u = clamp_f(t / k_shoulder_dome_t_end, 0.0F, 1.0F);
      float const us = smoothstep01(u);

      float const sphere = std::sqrt(std::max(0.0F, (2.0F * us) - (us * us)));
      float const dome_radius_scale =
          std::max(sphere, k_shoulder_dome_min_radius_scale);
      r *= dome_radius_scale;

      py -= k_shoulder_dome_height * (1.0F - us);
    }

    float px = r * std::cos(th);
    float pz = r * std::sin(th);

    px += x_offset_at(profile_t);
    pz += z_offset_at(profile_t);

    float const s_value =
        (t * k_micro_temporal_frequency) + (ang * k_micro_angular_frequency);
    px += (micro(s_value) - k_micro_center) * k_micro_jitter;
    pz += (micro(s_value + k_micro_phase_offset) - k_micro_center) *
          k_micro_jitter;

    return {px, py, pz};
  };

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;
  v.reserve((radial_segments + 1) * (height_segments + 1) +
            (radial_segments + 1) * 2 + 2);
  idx.reserve(radial_segments * height_segments * k_indices_per_quad +
              radial_segments * k_indices_per_quad);

  for (int y = 0; y <= height_segments; ++y) {
    float const t = float(y) / float(height_segments);
    float const dt = 1.0F / float(height_segments);
    float v_coord = t;

    for (int i = 0; i <= radial_segments; ++i) {
      float u = float(i) / float(radial_segments);
      float const ang = u * k_two_pi;
      float const da = k_two_pi / float(radial_segments);

      QVector3D const p = sample_pos(t, ang);
      QVector3D const pu = sample_pos(t, ang + da);
      QVector3D const pv = sample_pos(clamp_f(t + dt, 0.0F, 1.0F), ang);

      QVector3D const du = pu - p;
      QVector3D const dv = pv - p;

      QVector3D n = QVector3D::crossProduct(du, dv);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      }

      v.push_back({{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {u, v_coord}});
    }
  }

  int const row = radial_segments + 1;
  for (int y = 0; y < height_segments; ++y) {
    for (int i = 0; i < radial_segments; ++i) {
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
    float const t_top = 1.0F;
    float const t_top_s = invert_profile ? (1.0F - t_top) : t_top;
    QVector3D const c_top(x_offset_at(t_top_s), y_max, z_offset_at(t_top_s));

    v.push_back({{c_top.x(), c_top.y(), c_top.z()},
                 {0, 1, 0},
                 {k_uv_center, k_uv_center}});
    for (int i = 0; i <= radial_segments; ++i) {
      float const u = float(i) / float(radial_segments);
      float const ang = u * k_two_pi;
      QVector3D const p = sample_pos(t_top, ang);
      v.push_back({{p.x(), p.y(), p.z()},
                   {0, 1, 0},
                   {k_uv_center + k_uv_scale * std::cos(ang),
                    k_uv_center + k_uv_scale * std::sin(ang)}});
    }
    for (int i = 1; i <= radial_segments; ++i) {
      idx.push_back(base_top);
      idx.push_back(base_top + i);
      idx.push_back(base_top + i + 1);
    }
  }
  {

    float const t_apex = 0.0F;
    float const ts_apex = invert_profile ? (1.0F - t_apex) : t_apex;
    float const apex_y = y_min - k_shoulder_dome_height;

    int const apex_idx = (int)v.size();
    QVector3D const apex(x_offset_at(ts_apex), apex_y, z_offset_at(ts_apex));
    v.push_back({{apex.x(), apex.y(), apex.z()},
                 {0, -1, 0},
                 {k_uv_center, k_uv_center}});

    for (int i = 0; i < radial_segments; ++i) {
      idx.push_back(apex_idx);
      idx.push_back(i + 1);
      idx.push_back(i);
    }
  }

  return std::make_unique<Mesh>(v, idx);
}

} // namespace

namespace {
const std::unique_ptr<Mesh> k_unit_cylinder_mesh =
    create_unit_cylinder_mesh(k_default_radial_segments);
const std::unique_ptr<Mesh> k_unit_cube_mesh = create_cube_mesh();
const std::unique_ptr<Mesh> k_unit_sphere_mesh =
    create_unit_sphere_mesh(k_default_latitude_segments,
                            k_default_radial_segments);
const std::unique_ptr<Mesh> k_unit_cone_mesh =
    create_unit_cone_mesh(k_default_radial_segments);
const std::unique_ptr<Mesh> k_unit_capsule_mesh =
    create_capsule_mesh(k_default_radial_segments,
                        k_default_capsule_height_segments);
const std::unique_ptr<Mesh> k_unit_torso_mesh =
    create_unit_torso_mesh(k_default_radial_segments,
                           k_default_torso_height_segments);
} // namespace

auto get_unit_cylinder(int radial_segments) -> Mesh * {
  (void)radial_segments;
  return k_unit_cylinder_mesh.get();
}

auto get_unit_cube() -> Mesh * {
  return k_unit_cube_mesh.get();
}

auto get_unit_sphere(int lat_segments, int lon_segments) -> Mesh * {
  (void)lat_segments;
  (void)lon_segments;
  return k_unit_sphere_mesh.get();
}

auto get_unit_cone(int radial_segments) -> Mesh * {
  (void)radial_segments;
  return k_unit_cone_mesh.get();
}

auto get_unit_capsule(int radial_segments, int height_segments) -> Mesh * {
  (void)radial_segments;
  (void)height_segments;
  return k_unit_capsule_mesh.get();
}

auto get_unit_torso(int radial_segments, int height_segments) -> Mesh * {
  (void)radial_segments;
  (void)height_segments;
  return k_unit_torso_mesh.get();
}

} // namespace Render::GL
