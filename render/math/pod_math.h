#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace Render::Math {

struct alignas(16) Vec3 {
  float x, y, z, w;

  Vec3() noexcept : x(0), y(0), z(0), w(0) {}
  Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_), w(0) {}

  auto operator+(const Vec3 &o) const noexcept -> Vec3 {
    return {x + o.x, y + o.y, z + o.z};
  }

  auto operator-(const Vec3 &o) const noexcept -> Vec3 {
    return {x - o.x, y - o.y, z - o.z};
  }

  auto operator*(float s) const noexcept -> Vec3 {
    return {x * s, y * s, z * s};
  }

  [[nodiscard]] auto dot(const Vec3 &o) const noexcept -> float {
    return x * o.x + y * o.y + z * o.z;
  }

  [[nodiscard]] auto cross(const Vec3 &o) const noexcept -> Vec3 {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }

  [[nodiscard]] auto lengthSquared() const noexcept -> float {
    return x * x + y * y + z * z;
  }

  [[nodiscard]] auto length() const noexcept -> float {
    return std::sqrt(lengthSquared());
  }

  [[nodiscard]] auto normalized() const noexcept -> Vec3 {
    float const len = length();
    if (len < 1e-6F) {
      return {0, 1, 0};
    }
    float const inv_len = 1.0F / len;
    return {x * inv_len, y * inv_len, z * inv_len};
  }

  void normalize() noexcept {
    float const len = length();
    if (len > 1e-6F) {
      float const inv_len = 1.0F / len;
      x *= inv_len;
      y *= inv_len;
      z *= inv_len;
    }
  }
};

struct alignas(16) Mat3x4 {
  std::array<std::array<float, 4>, 3> m{};

  Mat3x4() noexcept {
    for (auto &row : m) {
      row.fill(0.0F);
    }
    m[0][0] = m[1][1] = m[2][2] = 1.0F;
  }

  static auto TRS(const Vec3 &translation,
                  const std::array<std::array<float, 3>, 3> &rotation,
                  float scale_x, float scaleY,
                  float scale_z) noexcept -> Mat3x4 {
    Mat3x4 result;
    for (int row = 0; row < 3; ++row) {
      result.m[row][0] = rotation[row][0] * scale_x;
      result.m[row][1] = rotation[row][1] * scaleY;
      result.m[row][2] = rotation[row][2] * scale_z;
      result.m[row][3] = (&translation.x)[row];
    }
    return result;
  }

  [[nodiscard]] auto transform_point(const Vec3 &p) const noexcept -> Vec3 {
    return {m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
            m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
            m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]};
  }

  [[nodiscard]] auto transform_vector(const Vec3 &v) const noexcept -> Vec3 {
    return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
  }

  auto operator*(const Mat3x4 &o) const noexcept -> Mat3x4 {
    Mat3x4 result;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        result.m[row][col] = m[row][0] * o.m[0][col] + m[row][1] * o.m[1][col] +
                             m[row][2] * o.m[2][col];
      }
      result.m[row][3] = m[row][0] * o.m[0][3] + m[row][1] * o.m[1][3] +
                         m[row][2] * o.m[2][3] + m[row][3];
    }
    return result;
  }

  void set_translation(const Vec3 &t) noexcept {
    m[0][3] = t.x;
    m[1][3] = t.y;
    m[2][3] = t.z;
  }

  [[nodiscard]] auto get_translation() const noexcept -> Vec3 {
    return {m[0][3], m[1][3], m[2][3]};
  }
};

struct CylinderTransform {
  Vec3 center;
  Vec3 axis;
  Vec3 tangent;
  Vec3 bitangent;
  float length{};
  float radius{};

  static auto fromPoints(const Vec3 &start, const Vec3 &end,
                         float radius) noexcept -> CylinderTransform {
    CylinderTransform ct;
    ct.radius = radius;

    Vec3 const diff = end - start;
    float const len_sq = diff.lengthSquared();

    if (len_sq < 1e-10F) {

      ct.center = start;
      ct.axis = Vec3(0, 1, 0);
      ct.tangent = Vec3(1, 0, 0);
      ct.bitangent = Vec3(0, 0, 1);
      ct.length = 0.0F;
      return ct;
    }

    ct.length = std::sqrt(len_sq);
    ct.center = Vec3((start.x + end.x) * 0.5F, (start.y + end.y) * 0.5F,
                     (start.z + end.z) * 0.5F);
    ct.axis = diff * (1.0F / ct.length);

    Vec3 const up =
        (std::abs(ct.axis.y) < 0.999F) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    ct.tangent = up.cross(ct.axis).normalized();
    ct.bitangent = ct.axis.cross(ct.tangent).normalized();

    return ct;
  }

  [[nodiscard]] auto to_matrix() const noexcept -> Mat3x4 {
    Mat3x4 m;

    m.m[0][0] = tangent.x * radius;
    m.m[1][0] = tangent.y * radius;
    m.m[2][0] = tangent.z * radius;

    m.m[0][1] = axis.x * length;
    m.m[1][1] = axis.y * length;
    m.m[2][1] = axis.z * length;

    m.m[0][2] = bitangent.x * radius;
    m.m[1][2] = bitangent.y * radius;
    m.m[2][2] = bitangent.z * radius;

    m.m[0][3] = center.x;
    m.m[1][3] = center.y;
    m.m[2][3] = center.z;

    return m;
  }
};

inline auto cylinder_between_fast(const Vec3 &a, const Vec3 &b,
                                  float radius) noexcept -> Mat3x4 {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float dz = b.z - a.z;
  const float len_sq = dx * dx + dy * dy + dz * dz;

  constexpr float k_epsilonSq = 1e-12F;
  constexpr float k_rad_to_deg = 57.2957795131F;

  Vec3 const center((a.x + b.x) * 0.5F, (a.y + b.y) * 0.5F, (a.z + b.z) * 0.5F);

  if (len_sq < k_epsilonSq) {

    Mat3x4 m;
    m.m[0][0] = radius;
    m.m[0][1] = 0;
    m.m[0][2] = 0;
    m.m[1][0] = 0;
    m.m[1][1] = 1.0F;
    m.m[1][2] = 0;
    m.m[2][0] = 0;
    m.m[2][1] = 0;
    m.m[2][2] = radius;
    m.set_translation(center);
    return m;
  }

  const float len = std::sqrt(len_sq);
  const float inv_len = 1.0F / len;

  const float ndx = dx * inv_len;
  const float ndy = dy * inv_len;
  const float ndz = dz * inv_len;

  const float axis_x = ndz;
  const float axis_z = -ndx;
  const float axis_len_sq = axis_x * axis_x + axis_z * axis_z;

  std::array<std::array<float, 3>, 3> rot{};

  if (axis_len_sq < k_epsilonSq) {

    if (ndy < 0.0F) {

      rot[0][0] = 1;
      rot[0][1] = 0;
      rot[0][2] = 0;
      rot[1][0] = 0;
      rot[1][1] = -1;
      rot[1][2] = 0;
      rot[2][0] = 0;
      rot[2][1] = 0;
      rot[2][2] = -1;
    } else {

      rot[0][0] = 1;
      rot[0][1] = 0;
      rot[0][2] = 0;
      rot[1][0] = 0;
      rot[1][1] = 1;
      rot[1][2] = 0;
      rot[2][0] = 0;
      rot[2][1] = 0;
      rot[2][2] = 1;
    }
  } else {

    const float axis_inv_len = 1.0F / std::sqrt(axis_len_sq);
    const float ax = axis_x * axis_inv_len;
    const float az = axis_z * axis_inv_len;

    const float dot = std::clamp(ndy, -1.0F, 1.0F);
    const float angle = std::acos(dot);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0F - c;

    rot[0][0] = t * ax * ax + c;
    rot[0][1] = t * ax * 0;
    rot[0][2] = t * ax * az - s * 0;

    rot[1][0] = t * 0 * ax + s * az;
    rot[1][1] = t * 0 * 0 + c;
    rot[1][2] = t * 0 * az - s * ax;

    rot[2][0] = t * az * ax + s * 0;
    rot[2][1] = t * az * 0 - s * ax;
    rot[2][2] = t * az * az + c;
  }

  Mat3x4 result = Mat3x4::TRS(center, rot, radius, len, radius);
  return result;
}

inline auto sphere_at_fast(const Vec3 &pos, float radius) noexcept -> Mat3x4 {
  Mat3x4 m;
  m.m[0][0] = radius;
  m.m[0][1] = 0;
  m.m[0][2] = 0;
  m.m[1][0] = 0;
  m.m[1][1] = radius;
  m.m[1][2] = 0;
  m.m[2][0] = 0;
  m.m[2][1] = 0;
  m.m[2][2] = radius;
  m.set_translation(pos);
  return m;
}

inline auto cylinder_between_fast(const Mat3x4 &parent, const Vec3 &a,
                                  const Vec3 &b,
                                  float radius) noexcept -> Mat3x4 {
  Mat3x4 const local = cylinder_between_fast(a, b, radius);
  return parent * local;
}

inline auto sphere_at_fast(const Mat3x4 &parent, const Vec3 &pos,
                           float radius) noexcept -> Mat3x4 {
  Mat3x4 const local = sphere_at_fast(pos, radius);
  return parent * local;
}

} // namespace Render::Math
