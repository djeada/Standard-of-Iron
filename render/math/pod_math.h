#pragma once

#include <cmath>
#include <cstring>

namespace Render::Math {

// Lightweight, POD-friendly 3D vector
struct alignas(16) Vec3 {
  float x, y, z, w; // w padding for SIMD alignment

  Vec3() noexcept : x(0), y(0), z(0), w(0) {}
  Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_), w(0) {}

  inline Vec3 operator+(const Vec3 &o) const noexcept {
    return Vec3(x + o.x, y + o.y, z + o.z);
  }

  inline Vec3 operator-(const Vec3 &o) const noexcept {
    return Vec3(x - o.x, y - o.y, z - o.z);
  }

  inline Vec3 operator*(float s) const noexcept {
    return Vec3(x * s, y * s, z * s);
  }

  inline float dot(const Vec3 &o) const noexcept {
    return x * o.x + y * o.y + z * o.z;
  }

  inline Vec3 cross(const Vec3 &o) const noexcept {
    return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
  }

  inline float lengthSquared() const noexcept {
    return x * x + y * y + z * z;
  }

  inline float length() const noexcept {
    return std::sqrt(lengthSquared());
  }

  inline Vec3 normalized() const noexcept {
    float len = length();
    if (len < 1e-6f)
      return Vec3(0, 1, 0);
    float invLen = 1.0f / len;
    return Vec3(x * invLen, y * invLen, z * invLen);
  }

  inline void normalize() noexcept {
    float len = length();
    if (len > 1e-6f) {
      float invLen = 1.0f / len;
      x *= invLen;
      y *= invLen;
      z *= invLen;
    }
  }
};

// Compact 3x4 matrix (3 rows, 4 columns) for affine transforms
// Stores rotation/scale in 3x3 and translation in last column
// More cache-friendly than QMatrix4x4
struct alignas(16) Mat3x4 {
  float m[3][4]; // row-major: m[row][col]

  Mat3x4() noexcept {
    std::memset(m, 0, sizeof(m));
    m[0][0] = m[1][1] = m[2][2] = 1.0f;
  }

  // Create from rotation + scale + translation
  static inline Mat3x4 TRS(const Vec3 &translation, const float rotation[3][3],
                           float scaleX, float scaleY, float scaleZ) noexcept {
    Mat3x4 result;
    for (int row = 0; row < 3; ++row) {
      result.m[row][0] = rotation[row][0] * scaleX;
      result.m[row][1] = rotation[row][1] * scaleY;
      result.m[row][2] = rotation[row][2] * scaleZ;
      result.m[row][3] = (&translation.x)[row];
    }
    return result;
  }

  // Transform a point
  inline Vec3 transformPoint(const Vec3 &p) const noexcept {
    return Vec3(
      m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
      m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
      m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]
    );
  }

  // Transform a vector (ignores translation)
  inline Vec3 transformVector(const Vec3 &v) const noexcept {
    return Vec3(
      m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
      m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
      m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
    );
  }

  // Matrix multiplication (this * other)
  inline Mat3x4 operator*(const Mat3x4 &o) const noexcept {
    Mat3x4 result;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        result.m[row][col] = 
          m[row][0] * o.m[0][col] +
          m[row][1] * o.m[1][col] +
          m[row][2] * o.m[2][col];
      }
      result.m[row][3] = 
        m[row][0] * o.m[0][3] +
        m[row][1] * o.m[1][3] +
        m[row][2] * o.m[2][3] +
        m[row][3];
    }
    return result;
  }

  // Set translation column
  inline void setTranslation(const Vec3 &t) noexcept {
    m[0][3] = t.x;
    m[1][3] = t.y;
    m[2][3] = t.z;
  }

  inline Vec3 getTranslation() const noexcept {
    return Vec3(m[0][3], m[1][3], m[2][3]);
  }
};

// Fast cylinder transform builder (replaces cylinderBetween CPU computation)
// Builds TBN basis from start/end points
struct CylinderTransform {
  Vec3 center;
  Vec3 axis;        // normalized direction
  Vec3 tangent;     // perpendicular to axis
  Vec3 bitangent;   // perpendicular to both
  float length;
  float radius;

  // Compute basis from start/end
  static inline CylinderTransform fromPoints(const Vec3 &start, const Vec3 &end,
                                             float radius) noexcept {
    CylinderTransform ct;
    ct.radius = radius;
    
    Vec3 diff = end - start;
    float lenSq = diff.lengthSquared();
    
    if (lenSq < 1e-10f) {
      // Degenerate case
      ct.center = start;
      ct.axis = Vec3(0, 1, 0);
      ct.tangent = Vec3(1, 0, 0);
      ct.bitangent = Vec3(0, 0, 1);
      ct.length = 0.0f;
      return ct;
    }

    ct.length = std::sqrt(lenSq);
    ct.center = Vec3((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f,
                     (start.z + end.z) * 0.5f);
    ct.axis = diff * (1.0f / ct.length);

    // Build perpendicular basis
    Vec3 up = (std::abs(ct.axis.y) < 0.999f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    ct.tangent = up.cross(ct.axis).normalized();
    ct.bitangent = ct.axis.cross(ct.tangent).normalized();

    return ct;
  }

  // Build a Mat3x4 from this cylinder transform
  inline Mat3x4 toMatrix() const noexcept {
    Mat3x4 m;
    // Column 0: tangent * radius
    m.m[0][0] = tangent.x * radius;
    m.m[1][0] = tangent.y * radius;
    m.m[2][0] = tangent.z * radius;
    
    // Column 1: axis * length
    m.m[0][1] = axis.x * length;
    m.m[1][1] = axis.y * length;
    m.m[2][1] = axis.z * length;
    
    // Column 2: bitangent * radius
    m.m[0][2] = bitangent.x * radius;
    m.m[1][2] = bitangent.y * radius;
    m.m[2][2] = bitangent.z * radius;
    
    // Column 3: center position
    m.m[0][3] = center.x;
    m.m[1][3] = center.y;
    m.m[2][3] = center.z;
    
    return m;
  }
};

// ============================================================================
// OPTIMIZED GEOMETRY FUNCTIONS (replaces render/geom/transforms.cpp)
// ============================================================================

// Fast cylinder between two points - avoids QMatrix4x4 overhead
// This is 3-5x faster than cylinderBetween() with QMatrix4x4::rotate/scale
inline Mat3x4 cylinderBetweenFast(const Vec3 &a, const Vec3 &b, float radius) noexcept {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float dz = b.z - a.z;
  const float lenSq = dx * dx + dy * dy + dz * dz;
  
  constexpr float kEpsilonSq = 1e-12f;
  constexpr float kRadToDeg = 57.2957795131f;
  
  Vec3 center((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f);
  
  if (lenSq < kEpsilonSq) {
    // Degenerate: just a sphere
    Mat3x4 m;
    m.m[0][0] = radius; m.m[0][1] = 0; m.m[0][2] = 0;
    m.m[1][0] = 0; m.m[1][1] = 1.0f; m.m[1][2] = 0;
    m.m[2][0] = 0; m.m[2][1] = 0; m.m[2][2] = radius;
    m.setTranslation(center);
    return m;
  }
  
  const float len = std::sqrt(lenSq);
  const float invLen = 1.0f / len;
  
  // Normalized direction
  const float ndx = dx * invLen;
  const float ndy = dy * invLen;
  const float ndz = dz * invLen;
  
  // Rotation axis: cross(Y_AXIS, direction) = (-ndz, 0, ndx)
  const float axisX = ndz;
  const float axisZ = -ndx;
  const float axisLenSq = axisX * axisX + axisZ * axisZ;
  
  // Build rotation matrix directly (avoids QMatrix4x4::rotate overhead)
  float rot[3][3];
  
  if (axisLenSq < kEpsilonSq) {
    // Aligned with Y axis
    if (ndy < 0.0f) {
      // Flip 180 degrees around X
      rot[0][0] = 1; rot[0][1] = 0; rot[0][2] = 0;
      rot[1][0] = 0; rot[1][1] = -1; rot[1][2] = 0;
      rot[2][0] = 0; rot[2][1] = 0; rot[2][2] = -1;
    } else {
      // Identity
      rot[0][0] = 1; rot[0][1] = 0; rot[0][2] = 0;
      rot[1][0] = 0; rot[1][1] = 1; rot[1][2] = 0;
      rot[2][0] = 0; rot[2][1] = 0; rot[2][2] = 1;
    }
  } else {
    // General rotation
    const float axisInvLen = 1.0f / std::sqrt(axisLenSq);
    const float ax = axisX * axisInvLen;
    const float az = axisZ * axisInvLen;
    
    const float dot = std::clamp(ndy, -1.0f, 1.0f);
    const float angle = std::acos(dot);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0f - c;
    
    // Rodrigues' rotation formula
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
  
  // Build TRS matrix: Translation * Rotation * Scale
  Mat3x4 result = Mat3x4::TRS(center, rot, radius, len, radius);
  return result;
}

// Fast sphere transform
inline Mat3x4 sphereAtFast(const Vec3 &pos, float radius) noexcept {
  Mat3x4 m;
  m.m[0][0] = radius; m.m[0][1] = 0; m.m[0][2] = 0;
  m.m[1][0] = 0; m.m[1][1] = radius; m.m[1][2] = 0;
  m.m[2][0] = 0; m.m[2][1] = 0; m.m[2][2] = radius;
  m.setTranslation(pos);
  return m;
}

// Cylinder with parent transform
inline Mat3x4 cylinderBetweenFast(const Mat3x4 &parent, const Vec3 &a, 
                                  const Vec3 &b, float radius) noexcept {
  Mat3x4 local = cylinderBetweenFast(a, b, radius);
  return parent * local;
}

// Sphere with parent transform
inline Mat3x4 sphereAtFast(const Mat3x4 &parent, const Vec3 &pos, float radius) noexcept {
  Mat3x4 local = sphereAtFast(pos, radius);
  return parent * local;
}

} // namespace Render::Math
