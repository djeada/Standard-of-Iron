#include "weapon_rack_mesh.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

#include "prop_mesh_builder.h"
#include "weapon_rack_parts.h"

namespace Render::GL::BackendPipelines {

namespace {

namespace M = WeaponRackMaterial;
using V3 = QVector3D;
using namespace WeaponRackParts;

constexpr float k_tau = 6.28318530F;

auto unit(const V3& v, const V3& fallback) -> V3 {
  const float len_sq = V3::dotProduct(v, v);
  if (len_sq < 1.0e-16F) {
    return fallback;
  }
  return v / std::sqrt(len_sq);
}

auto seed_of(int index) -> float {
  float const s = static_cast<float>(index) * 0.6180339F + 0.137F;
  return s - std::floor(s);
}

struct Ring {
  V3 center;
  float rx{0.0F};
  float ry{0.0F};
};

struct Profile {
  float h{0.0F};
  float rx{0.0F};
  float ry{0.0F};
};

struct ShieldFrame {
  V3 center;
  V3 right;
  V3 up;
  V3 fwd;
};

auto shield_frame(const V3& center, float lean, float yaw) -> ShieldFrame {
  V3 const fwd0 = unit(V3(0.0F, std::sin(lean), std::cos(lean)), V3(0.0F, 0.0F, 1.0F));
  V3 const fwd(fwd0.z() * std::sin(yaw), fwd0.y(), fwd0.z() * std::cos(yaw));
  V3 const right =
      unit(V3::crossProduct(V3(0.0F, 1.0F, 0.0F), fwd), V3(1.0F, 0.0F, 0.0F));
  V3 const up = unit(V3::crossProduct(fwd, right), V3(0.0F, 1.0F, 0.0F));
  return {center, right, up, fwd};
}

class RackBuilder {
public:
  explicit RackBuilder(WeaponRackMeshData& mesh)
      : m_mesh(mesh) {}

  [[nodiscard]] auto mark() const -> std::size_t { return m_mesh.vertices.size(); }

  void tag_projected(
      std::size_t first, float material, float seed, const V3& origin, const V3& axis) {
    V3 const along = unit(axis, V3(0.0F, 1.0F, 0.0F));
    V3 const reference =
        std::abs(along.y()) < 0.9F ? V3(0.0F, 1.0F, 0.0F) : V3(1.0F, 0.0F, 0.0F);
    V3 const side = unit(V3::crossProduct(reference, along), V3(1.0F, 0.0F, 0.0F));
    V3 const depth = V3::crossProduct(along, side);
    m_mesh.surface.resize(m_mesh.vertices.size());
    for (std::size_t i = first; i < m_mesh.vertices.size(); ++i) {
      V3 const d = m_mesh.vertices[i].first - origin;
      m_mesh.surface[i] =
          QVector4D(material,
                    V3::dotProduct(d, side) + 0.37F * V3::dotProduct(d, depth),
                    V3::dotProduct(d, along),
                    seed);
    }
  }

  void box(const V3& lo, const V3& hi, float material, float seed) {
    std::size_t const first = mark();
    append_box(m_mesh.vertices, m_mesh.indices, lo, hi);
    V3 const extent = hi - lo;
    V3 axis(1.0F, 0.0F, 0.0F);
    if (extent.y() >= extent.x() && extent.y() >= extent.z()) {
      axis = V3(0.0F, 1.0F, 0.0F);
    } else if (extent.z() >= extent.x()) {
      axis = V3(0.0F, 0.0F, 1.0F);
    }
    tag_projected(first, material, seed, lo, axis);
  }

  void beam(const V3& a, const V3& b, float hw, float hd, float material, float seed) {
    std::size_t const first = mark();
    append_prop_beam(m_mesh.vertices, m_mesh.indices, a, b, hw, hd);
    tag_projected(first, material, seed, a, b - a);
  }

  void taper(const PropTaperPart& part, float material, float seed) {
    std::size_t const first = mark();
    append_prop_taper(m_mesh.vertices,
                      m_mesh.indices,
                      part.cx,
                      part.y0,
                      part.cz,
                      part.r0,
                      part.r1,
                      part.height,
                      part.segments);
    tag_projected(
        first, material, seed, V3(part.cx, part.y0, part.cz), V3(0.0F, 1.0F, 0.0F));
  }

  void sweep(std::span<const Ring> rings,
             int segs,
             bool faceted,
             const V3& side_hint,
             float material,
             float seed,
             bool normalize_v = false) {
    std::size_t const n = rings.size();
    if (n < 2 || segs < 3) {
      return;
    }
    std::size_t const first = mark();

    std::vector<V3> tangent(n);
    std::vector<V3> side(n);
    std::vector<V3> depth(n);
    std::vector<float> arc(n, 0.0F);
    for (std::size_t i = 0; i < n; ++i) {
      V3 const prev = rings[i > 0 ? i - 1 : 0].center;
      V3 const next = rings[std::min(i + 1, n - 1)].center;
      tangent[i] = unit(next - prev, V3(0.0F, 1.0F, 0.0F));
      if (i > 0) {
        arc[i] = arc[i - 1] + (rings[i].center - rings[i - 1].center).length();
      }
    }
    for (std::size_t i = 0; i < n; ++i) {
      V3 const carried = i == 0 ? side_hint : side[i - 1];
      V3 s = carried - tangent[i] * V3::dotProduct(carried, tangent[i]);
      if (s.lengthSquared() < 1.0e-10F) {
        V3 const reference = std::abs(tangent[i].y()) < 0.9F ? V3(0.0F, 1.0F, 0.0F)
                                                             : V3(1.0F, 0.0F, 0.0F);
        s = V3::crossProduct(reference, tangent[i]);
      }
      side[i] = unit(s, V3(1.0F, 0.0F, 0.0F));
      depth[i] = V3::crossProduct(tangent[i], side[i]);
    }

    float const total = std::max(arc.back(), 1.0e-5F);
    auto v_at = [&](std::size_t i) {
      return normalize_v ? arc[i] / total : arc[i];
    };
    auto point = [&](std::size_t i, float a) {
      return rings[i].center + side[i] * (rings[i].rx * std::cos(a)) +
             depth[i] * (rings[i].ry * std::sin(a));
    };
    auto slope_at = [&](std::size_t i) {
      std::size_t const p = i > 0 ? i - 1 : 0;
      std::size_t const q = std::min(i + 1, n - 1);
      float const dr = (rings[q].rx + rings[q].ry - rings[p].rx - rings[p].ry) * 0.5F;
      float const ds = std::max(arc[q] - arc[p], 1.0e-5F);
      return dr / ds;
    };
    auto smooth_normal = [&](std::size_t i, float a) {
      V3 const radial =
          unit(side[i] * (std::cos(a) / std::max(rings[i].rx, 1.0e-4F)) +
                   depth[i] * (std::sin(a) / std::max(rings[i].ry, 1.0e-4F)),
               side[i]);
      return unit(radial - tangent[i] * slope_at(i), radial);
    };

    auto push = [&](const V3& p, const V3& nrm, float u, float v) {
      m_mesh.vertices.emplace_back(p, nrm);
      m_mesh.surface.emplace_back(material, u, v, seed);
    };
    m_mesh.surface.resize(first);

    if (!faceted) {
      for (std::size_t i = 0; i < n; ++i) {
        for (int s = 0; s <= segs; ++s) {
          float const a = k_tau * static_cast<float>(s) / static_cast<float>(segs);
          push(point(i, a),
               smooth_normal(i, a),
               static_cast<float>(s) / static_cast<float>(segs),
               v_at(i));
        }
      }
      auto const stride = static_cast<std::size_t>(segs + 1);
      for (std::size_t i = 0; i + 1 < n; ++i) {
        for (int s = 0; s < segs; ++s) {
          auto const b0 = static_cast<std::uint16_t>(first + i * stride +
                                                     static_cast<std::size_t>(s));
          auto const b1 = static_cast<std::uint16_t>(b0 + 1);
          auto const c0 = static_cast<std::uint16_t>(b0 + stride);
          auto const c1 = static_cast<std::uint16_t>(c0 + 1);
          m_mesh.indices.insert(m_mesh.indices.end(), {b0, b1, c1, b0, c1, c0});
        }
      }
    } else {
      for (std::size_t i = 0; i + 1 < n; ++i) {
        for (int s = 0; s < segs; ++s) {
          float const a0 = k_tau * static_cast<float>(s) / static_cast<float>(segs);
          float const a1 = k_tau * static_cast<float>(s + 1) / static_cast<float>(segs);
          V3 const p0 = point(i, a0);
          V3 const p1 = point(i, a1);
          V3 const p2 = point(i + 1, a1);
          V3 const p3 = point(i + 1, a0);
          V3 nrm = V3::crossProduct(p1 - p0, p3 - p0);
          if (nrm.lengthSquared() < 1.0e-14F) {
            nrm = V3::crossProduct(p2 - p1, p0 - p1);
          }
          nrm = unit(nrm, side[i]);
          V3 const mid = (p0 + p1 + p2 + p3) * 0.25F;
          V3 const axis_mid = (rings[i].center + rings[i + 1].center) * 0.5F;
          if (V3::dotProduct(nrm, mid - axis_mid) < 0.0F) {
            nrm = -nrm;
          }
          auto const base = static_cast<std::uint16_t>(m_mesh.vertices.size());
          float const u0 = static_cast<float>(s) / static_cast<float>(segs);
          float const u1 = static_cast<float>(s + 1) / static_cast<float>(segs);
          push(p0, nrm, u0, v_at(i));
          push(p1, nrm, u1, v_at(i));
          push(p2, nrm, u1, v_at(i + 1));
          push(p3, nrm, u0, v_at(i + 1));
          m_mesh.indices.insert(m_mesh.indices.end(),
                                {base,
                                 static_cast<std::uint16_t>(base + 1),
                                 static_cast<std::uint16_t>(base + 2),
                                 base,
                                 static_cast<std::uint16_t>(base + 2),
                                 static_cast<std::uint16_t>(base + 3)});
        }
      }
    }

    auto cap = [&](std::size_t i, const V3& outward) {
      if (rings[i].rx < 1.0e-4F && rings[i].ry < 1.0e-4F) {
        return;
      }
      auto const hub = static_cast<std::uint16_t>(m_mesh.vertices.size());
      push(rings[i].center, outward, 0.5F, v_at(i));
      for (int s = 0; s <= segs; ++s) {
        float const a = k_tau * static_cast<float>(s) / static_cast<float>(segs);
        push(point(i, a),
             outward,
             static_cast<float>(s) / static_cast<float>(segs),
             v_at(i));
      }
      for (int s = 0; s < segs; ++s) {
        m_mesh.indices.insert(m_mesh.indices.end(),
                              {hub,
                               static_cast<std::uint16_t>(hub + 1 + s),
                               static_cast<std::uint16_t>(hub + 2 + s)});
      }
    };
    cap(0, -tangent.front());
    cap(n - 1, tangent.back());
  }

  void lathe(const V3& base,
             const V3& axis,
             std::initializer_list<Profile> profile,
             int segs,
             bool faceted,
             const V3& side_hint,
             float material,
             float seed,
             bool normalize_v = false) {
    V3 const dir = unit(axis, V3(0.0F, 1.0F, 0.0F));
    std::vector<Ring> rings;
    rings.reserve(profile.size());
    for (const Profile& p : profile) {
      rings.push_back({base + dir * p.h, p.rx, p.ry});
    }
    sweep(rings, segs, faceted, side_hint, material, seed, normalize_v);
  }

  void tube(const V3& a,
            const V3& b,
            float r0,
            float r1,
            int segs,
            float material,
            float seed,
            const V3& side_hint = V3(1.0F, 0.0F, 0.0F)) {
    std::array<Ring, 2> const rings{{{a, r0, r0}, {b, r1, r1}}};
    sweep(rings, segs, false, side_hint, material, seed);
  }

  void scutum(const ShieldFrame& f,
              float radius,
              float half_angle,
              float half_height,
              float thickness,
              float seed) {
    constexpr int k_cols = 16;
    constexpr int k_rows = 18;
    constexpr float k_super = 2.4F;
    auto width_at = [&](float v) {
      float const e = 1.0F - std::pow(std::min(std::abs(v), 1.0F), k_super);
      return std::pow(std::max(e, 0.0F), 1.0F / k_super);
    };
    auto front = [&](float u, float v) {
      float const theta = u * half_angle * width_at(v);
      return f.center + f.right * (radius * std::sin(theta)) +
             f.up * (half_height * v) + f.fwd * (radius * std::cos(theta) - radius);
    };
    auto outward = [&](float u, float v) {
      float const theta = u * half_angle * width_at(v);
      return unit(f.right * std::sin(theta) + f.fwd * std::cos(theta), f.fwd);
    };

    for (int side = 0; side < 2; ++side) {
      bool const is_front = side == 0;
      std::size_t const first = mark();
      m_mesh.surface.resize(first);
      for (int r = 0; r <= k_rows; ++r) {
        float const v = -1.0F + 2.0F * static_cast<float>(r) / k_rows;
        for (int c = 0; c <= k_cols; ++c) {
          float const u = -1.0F + 2.0F * static_cast<float>(c) / k_cols;
          V3 const n = outward(u, v);
          V3 const p = is_front ? front(u, v) : front(u, v) - n * thickness;
          m_mesh.vertices.emplace_back(p, is_front ? n : -n);
          m_mesh.surface.emplace_back(is_front ? M::k_shield_paint : M::k_shield_back,
                                      u * width_at(v),
                                      v,
                                      seed);
        }
      }
      for (int r = 0; r < k_rows; ++r) {
        for (int c = 0; c < k_cols; ++c) {
          auto const b0 = static_cast<std::uint16_t>(
              first + static_cast<std::size_t>(r * (k_cols + 1) + c));
          auto const b1 = static_cast<std::uint16_t>(b0 + 1);
          auto const c0 = static_cast<std::uint16_t>(b0 + k_cols + 1);
          auto const c1 = static_cast<std::uint16_t>(c0 + 1);
          m_mesh.indices.insert(m_mesh.indices.end(), {b0, b1, c1, b0, c1, c0});
        }
      }
    }

    std::vector<Ring> rim;
    for (int r = 0; r <= k_rows; ++r) {
      float const v = -1.0F + 2.0F * static_cast<float>(r) / k_rows;
      rim.push_back({front(1.0F, v) - outward(1.0F, v) * (thickness * 0.5F),
                     thickness * 0.95F,
                     thickness * 0.95F});
    }
    for (int r = k_rows - 1; r >= 0; --r) {
      float const v = -1.0F + 2.0F * static_cast<float>(r) / k_rows;
      rim.push_back({front(-1.0F, v) - outward(-1.0F, v) * (thickness * 0.5F),
                     thickness * 0.95F,
                     thickness * 0.95F});
    }
    sweep(rim, 7, false, f.fwd, M::k_leather, seed + 0.31F);

    std::vector<Ring> spina;
    for (int r = 1; r < k_rows; ++r) {
      float const v = -1.0F + 2.0F * static_cast<float>(r) / k_rows;
      float const swell = 1.0F - 0.62F * std::abs(v);
      spina.push_back({front(0.0F, v) + f.fwd * 0.004F, 0.036F * swell, 0.016F});
    }
    std::size_t const spina_first = mark();
    sweep(spina, 8, false, f.right, M::k_shield_paint, seed);

    float const face_half_width = radius * std::sin(half_angle);
    for (std::size_t i = spina_first; i < m_mesh.vertices.size(); ++i) {
      V3 const d = m_mesh.vertices[i].first - f.center;
      m_mesh.surface[i] = QVector4D(M::k_shield_paint,
                                    V3::dotProduct(d, f.right) / face_half_width,
                                    V3::dotProduct(d, f.up) / half_height,
                                    seed);
    }

    lathe(front(0.0F, 0.0F) - f.fwd * 0.004F,
          f.fwd,
          {{0.000F, 0.112F, 0.084F},
           {0.010F, 0.112F, 0.084F},
           {0.016F, 0.098F, 0.074F},
           {0.040F, 0.078F, 0.060F},
           {0.062F, 0.044F, 0.036F},
           {0.071F, 0.014F, 0.012F},
           {0.073F, 0.000F, 0.000F}},
          16,
          false,
          f.up,
          M::k_bronze,
          seed + 0.17F);
  }

  void
  parma(const ShieldFrame& f, float radius, float dome, float thickness, float seed) {
    constexpr int k_rings = 9;
    constexpr int k_segs = 28;
    auto front = [&](float rho, float phi) {
      V3 const radial = f.right * std::cos(phi) + f.up * std::sin(phi);
      return f.center + radial * (rho * radius) + f.fwd * (dome * (1.0F - rho * rho));
    };
    auto outward = [&](float rho, float phi) {
      V3 const radial = f.right * std::cos(phi) + f.up * std::sin(phi);
      return unit(f.fwd + radial * (2.0F * dome * rho / radius), f.fwd);
    };
    for (int side = 0; side < 2; ++side) {
      bool const is_front = side == 0;
      std::size_t const first = mark();
      m_mesh.surface.resize(first);
      for (int r = 0; r <= k_rings; ++r) {
        float const rho = static_cast<float>(r) / k_rings;
        for (int s = 0; s <= k_segs; ++s) {
          float const phi = k_tau * static_cast<float>(s) / k_segs;
          V3 const n = outward(rho, phi);
          V3 const p = is_front ? front(rho, phi) : front(rho, phi) - n * thickness;
          m_mesh.vertices.emplace_back(p, is_front ? n : -n);
          m_mesh.surface.emplace_back(is_front ? M::k_shield_paint : M::k_shield_back,
                                      rho * std::cos(phi),
                                      rho * std::sin(phi),
                                      seed);
        }
      }
      for (int r = 0; r < k_rings; ++r) {
        for (int s = 0; s < k_segs; ++s) {
          auto const b0 = static_cast<std::uint16_t>(
              first + static_cast<std::size_t>(r * (k_segs + 1) + s));
          auto const b1 = static_cast<std::uint16_t>(b0 + 1);
          auto const c0 = static_cast<std::uint16_t>(b0 + k_segs + 1);
          auto const c1 = static_cast<std::uint16_t>(c0 + 1);
          m_mesh.indices.insert(m_mesh.indices.end(), {b0, b1, c1, b0, c1, c0});
        }
      }
    }

    std::vector<Ring> rim;
    for (int s = 0; s <= k_segs; ++s) {
      float const phi = k_tau * static_cast<float>(s) / k_segs;
      rim.push_back({front(1.0F, phi) - f.fwd * (thickness * 0.5F),
                     thickness * 0.9F,
                     thickness * 0.9F});
    }
    sweep(rim, 7, false, f.fwd, M::k_iron, seed + 0.23F);

    lathe(front(0.0F, 0.0F) - f.fwd * 0.004F,
          f.fwd,
          {{0.000F, 0.074F, 0.074F},
           {0.008F, 0.074F, 0.074F},
           {0.013F, 0.062F, 0.062F},
           {0.034F, 0.048F, 0.048F},
           {0.050F, 0.024F, 0.024F},
           {0.056F, 0.000F, 0.000F}},
          16,
          false,
          f.up,
          M::k_bronze,
          seed + 0.41F);
  }

  void gladius_hilt(const V3& base, const V3& up, const V3& side_hint, float seed) {
    lathe(base,
          up,
          {{-0.004F, 0.060F, 0.024F}, {0.004F, 0.060F, 0.024F}},
          12,
          false,
          side_hint,
          M::k_bronze,
          seed);
    lathe(base,
          up,
          {{0.004F, 0.056F, 0.028F},
           {0.010F, 0.066F, 0.033F},
           {0.026F, 0.062F, 0.031F},
           {0.034F, 0.040F, 0.024F}},
          12,
          false,
          side_hint,
          M::k_bone,
          seed + 0.11F);
    lathe(base,
          up,
          {{0.034F, 0.019F, 0.016F},
           {0.052F, 0.022F, 0.019F},
           {0.066F, 0.018F, 0.016F},
           {0.080F, 0.022F, 0.019F},
           {0.094F, 0.018F, 0.016F},
           {0.108F, 0.022F, 0.019F},
           {0.122F, 0.019F, 0.016F}},
          10,
          false,
          side_hint,
          M::k_bone,
          seed + 0.19F);
    lathe(base,
          up,
          {{0.120F, 0.024F, 0.021F},
           {0.134F, 0.040F, 0.032F},
           {0.156F, 0.045F, 0.036F},
           {0.176F, 0.034F, 0.027F},
           {0.186F, 0.014F, 0.012F}},
          12,
          false,
          side_hint,
          M::k_bone,
          seed + 0.27F);
    lathe(
        base,
        up,
        {{0.184F, 0.012F, 0.012F}, {0.192F, 0.011F, 0.011F}, {0.198F, 0.000F, 0.000F}},
        8,
        false,
        side_hint,
        M::k_bronze,
        seed + 0.33F);
  }

  void gladius_blade(const V3& guard, const V3& tip, const V3& side_hint, float seed) {
    V3 const axis = tip - guard;
    float const length = axis.length();
    lathe(guard,
          axis,
          {{0.00F * length, 0.050F, 0.010F},
           {0.08F * length, 0.049F, 0.011F},
           {0.36F * length, 0.044F, 0.010F},
           {0.62F * length, 0.050F, 0.010F},
           {0.78F * length, 0.042F, 0.009F},
           {0.92F * length, 0.018F, 0.006F},
           {1.00F * length, 0.000F, 0.000F}},
          4,
          true,
          side_hint,
          M::k_steel,
          seed,
          true);
  }

  void spear_butt(const V3& butt, const V3& dir, float seed) {
    lathe(butt,
          dir,
          {{0.000F, 0.000F, 0.000F},
           {0.012F, 0.007F, 0.007F},
           {0.060F, 0.016F, 0.016F},
           {0.078F, 0.020F, 0.020F},
           {0.090F, 0.020F, 0.020F}},
          8,
          false,
          V3(1.0F, 0.0F, 0.0F),
          M::k_iron,
          seed);
  }

  void hasta(const V3& butt, const V3& rest, float length, float seed) {
    V3 const dir = unit(rest - butt, V3(0.0F, 1.0F, 0.0F));
    spear_butt(butt, dir, seed);
    tube(butt + dir * 0.086F,
         butt + dir * (length - 0.300F),
         0.0175F,
         0.0150F,
         9,
         M::k_ash,
         seed);
    tube(butt + dir * 0.93F,
         butt + dir * 1.10F,
         0.0192F,
         0.0190F,
         9,
         M::k_leather,
         seed);
    for (float const h : {0.93F, 1.10F}) {
      lathe(butt + dir * h,
            dir,
            {{-0.006F, 0.0200F, 0.0200F}, {0.006F, 0.0200F, 0.0200F}},
            9,
            false,
            V3(1.0F, 0.0F, 0.0F),
            M::k_linen,
            seed + 0.5F);
    }
    lathe(butt + dir * (length - 0.310F),
          dir,
          {{0.000F, 0.0215F, 0.0215F},
           {0.012F, 0.0215F, 0.0215F},
           {0.016F, 0.0190F, 0.0190F},
           {0.100F, 0.0150F, 0.0150F},
           {0.122F, 0.0125F, 0.0125F}},
          9,
          false,
          V3(1.0F, 0.0F, 0.0F),
          M::k_iron,
          seed + 0.07F);
    lathe(butt + dir * (length - 0.196F),
          dir,
          {{0.000F, 0.011F, 0.009F},
           {0.030F, 0.040F, 0.010F},
           {0.070F, 0.053F, 0.009F},
           {0.115F, 0.042F, 0.008F},
           {0.165F, 0.016F, 0.005F},
           {0.196F, 0.000F, 0.000F}},
          4,
          true,
          V3(1.0F, 0.0F, 0.0F),
          M::k_steel,
          seed + 0.13F,
          true);
  }

  void pilum(const V3& butt, const V3& rest, float length, float seed) {
    V3 const dir = unit(rest - butt, V3(0.0F, 1.0F, 0.0F));
    float const shaft_end = length * 0.64F;
    spear_butt(butt, dir, seed);
    tube(butt + dir * 0.086F,
         butt + dir * shaft_end,
         0.0180F,
         0.0170F,
         9,
         M::k_ash,
         seed);
    lathe(butt + dir * shaft_end,
          dir,
          {{0.000F, 0.022F, 0.022F},
           {0.018F, 0.031F, 0.031F},
           {0.070F, 0.027F, 0.027F},
           {0.098F, 0.016F, 0.016F}},
          4,
          true,
          V3(1.0F, 0.0F, 1.0F),
          M::k_oak,
          seed + 0.21F);
    for (float const h : {0.004F, 0.066F}) {
      lathe(butt + dir * (shaft_end + h),
            dir,
            {{0.000F, 0.030F, 0.030F}, {0.012F, 0.031F, 0.031F}},
            4,
            true,
            V3(1.0F, 0.0F, 1.0F),
            M::k_iron,
            seed + 0.29F);
    }
    tube(butt + dir * (shaft_end + 0.090F),
         butt + dir * (length - 0.050F),
         0.0072F,
         0.0060F,
         6,
         M::k_iron,
         seed + 0.37F);
    lathe(butt + dir * (length - 0.056F),
          dir,
          {{0.000F, 0.0065F, 0.0065F},
           {0.012F, 0.0175F, 0.0175F},
           {0.056F, 0.0000F, 0.0000F}},
          4,
          true,
          V3(1.0F, 0.0F, 1.0F),
          M::k_steel,
          seed + 0.43F,
          true);
  }

  void peg(const V3& root, const V3& tip, float seed) {
    tube(root, tip, 0.0115F, 0.0105F, 8, M::k_oak, seed);
    lathe(
        tip,
        tip - root,
        {{0.000F, 0.0150F, 0.0150F}, {0.010F, 0.0150F, 0.0150F}, {0.014F, 0.0F, 0.0F}},
        8,
        false,
        V3(1.0F, 0.0F, 0.0F),
        M::k_oak,
        seed + 0.5F);
  }

  void nail(float x, float y, float face_z, float seed) {
    box(V3(x - 0.010F, y - 0.010F, face_z - 0.004F),
        V3(x + 0.010F, y + 0.010F, face_z + 0.007F),
        M::k_iron,
        seed);
  }

private:
  WeaponRackMeshData& m_mesh;
};

} // namespace

auto build_weapon_rack_mesh() -> WeaponRackMeshData {
  WeaponRackMeshData mesh;
  RackBuilder rack(mesh);
  int part = 0;

  for (const PropBoxPart& p : k_weapon_rack_boxes) {
    rack.box(V3(p.lo.x, p.lo.y, p.lo.z),
             V3(p.hi.x, p.hi.y, p.hi.z),
             M::k_oak,
             seed_of(part++));
  }
  for (const PropBeamPart& p : k_weapon_rack_beams) {
    rack.beam(V3(p.a.x, p.a.y, p.a.z),
              V3(p.b.x, p.b.y, p.b.z),
              p.half_width,
              p.half_depth,
              M::k_oak,
              seed_of(part++));
  }
  for (const PropTaperPart& p : k_weapon_rack_tapers) {
    rack.taper(p, M::k_oak, seed_of(part++));
  }

  for (float const sx : {-1.0F, 1.0F}) {
    float const x = sx * k_upright_x;
    for (float const y : {1.425F, 0.100F}) {
      rack.box(V3(x - 0.057F, y, -0.067F),
               V3(x + 0.057F, y + 0.048F, 0.047F),
               M::k_iron,
               seed_of(part++));
    }
    rack.box(V3(x - 0.057F, 0.955F, -0.067F),
             V3(x + 0.057F, 0.975F, 0.047F),
             M::k_iron,
             seed_of(part++));
    rack.nail(x, 1.510F, k_top_cap_front_z, seed_of(part++));
    rack.nail(x, 1.552F, k_top_cap_front_z, seed_of(part++));
    rack.nail(x, 1.020F, 0.040F, seed_of(part++));
    rack.nail(x, 0.240F, 0.255F, seed_of(part++));
  }

  for (float const x : {-0.545F, -0.395F, -0.175F, 0.125F, 0.310F, 0.505F, 0.665F}) {
    rack.peg(V3(x, 1.534F, 0.050F), V3(x, 1.548F, 0.140F), seed_of(part++));
  }

  auto shaft_top = [](float x) {
    return V3(x, k_top_cap_y, 0.085F);
  };
  auto shaft_foot = [](float x) {
    return V3(x, k_tray_top_y, 0.310F);
  };

  rack.hasta(shaft_foot(-0.600F), shaft_top(-0.612F), 2.05F, seed_of(part++));
  rack.pilum(shaft_foot(-0.470F), shaft_top(-0.462F), 1.98F, seed_of(part++));
  rack.hasta(shaft_foot(-0.330F), shaft_top(-0.296F), 1.92F, seed_of(part++));
  rack.pilum(shaft_foot(0.060F), shaft_top(0.048F), 1.98F, seed_of(part++));
  rack.hasta(shaft_foot(0.190F), shaft_top(0.222F), 2.08F, seed_of(part++));
  rack.hasta(shaft_foot(0.580F), shaft_top(0.598F), 1.96F, seed_of(part++));

  {
    V3 const tip(0.400F, 0.045F, 0.325F);
    V3 const guard(0.430F, 0.800F, 0.125F);
    float const seed = seed_of(part++);
    rack.gladius_blade(guard, tip, V3(1.0F, 0.0F, 0.0F), seed);
    rack.gladius_hilt(guard, guard - tip, V3(1.0F, 0.0F, 0.0F), seed);
  }

  {
    V3 const top(-0.060F, 0.900F, 0.100F);
    V3 const bottom(-0.085F, 0.300F, 0.115F);
    V3 const down = unit(bottom - top, V3(0.0F, -1.0F, 0.0F));
    float const length = (bottom - top).length();
    float const seed = seed_of(part++);
    V3 const flat(1.0F, 0.0F, 0.0F);
    rack.lathe(top,
               down,
               {{0.000F, 0.060F, 0.021F},
                {0.200F, 0.056F, 0.020F},
                {0.420F, 0.058F, 0.019F},
                {length - 0.090F, 0.042F, 0.016F},
                {length - 0.020F, 0.020F, 0.012F},
                {length, 0.000F, 0.000F}},
               12,
               false,
               flat,
               M::k_leather,
               seed,
               true);
    for (float const h : {0.000F, 0.150F, 0.290F}) {
      rack.lathe(top + down * h,
                 down,
                 {{0.000F, 0.064F, 0.025F},
                  {0.006F, 0.066F, 0.026F},
                  {0.030F, 0.066F, 0.026F},
                  {0.036F, 0.064F, 0.025F}},
                 12,
                 false,
                 flat,
                 M::k_bronze,
                 seed + 0.07F * h);
    }
    rack.lathe(top + down * (length - 0.130F),
               down,
               {{0.000F, 0.050F, 0.022F},
                {0.012F, 0.052F, 0.023F},
                {0.110F, 0.032F, 0.018F},
                {0.132F, 0.018F, 0.016F},
                {0.140F, 0.000F, 0.000F}},
               12,
               false,
               flat,
               M::k_bronze,
               seed + 0.19F);
    for (float const h : {0.020F, 0.170F}) {
      for (float const sx : {-1.0F, 1.0F}) {
        V3 const ring = top + down * h + flat * (sx * 0.070F);
        rack.tube(ring - down * 0.012F,
                  ring + down * 0.012F,
                  0.0085F,
                  0.0085F,
                  8,
                  M::k_bronze,
                  seed + 0.3F);
      }
    }
    rack.gladius_hilt(top, -down, flat, seed + 0.5F);

    V3 const peg_tip(-0.175F, 1.046F, 0.120F);
    rack.peg(V3(-0.175F, 1.032F, 0.030F), peg_tip, seed_of(part++));
    std::array<Ring, 4> const upper{
        {{top + down * 0.02F + flat * -0.078F, 0.013F, 0.0025F},
         {V3(-0.155F, 0.960F, 0.112F), 0.013F, 0.0025F},
         {V3(-0.172F, 1.040F, 0.112F), 0.013F, 0.0025F},
         {V3(-0.178F, 1.064F, 0.090F), 0.013F, 0.0025F}}};
    rack.sweep(upper, 6, false, V3(0.0F, 0.0F, 1.0F), M::k_leather, seed + 0.61F);
    std::array<Ring, 4> const lower{
        {{top + down * 0.17F + flat * 0.078F, 0.012F, 0.0025F},
         {V3(-0.020F, 0.880F, 0.145F), 0.012F, 0.0025F},
         {V3(-0.130F, 1.010F, 0.140F), 0.012F, 0.0025F},
         {V3(-0.168F, 1.062F, 0.100F), 0.012F, 0.0025F}}};
    rack.sweep(lower, 6, false, V3(0.0F, 0.0F, 1.0F), M::k_leather, seed + 0.67F);
  }

  {
    V3 const base(-0.740F, 1.598F, -0.010F);
    V3 const up = unit(V3(0.06F, 1.0F, 0.04F), V3(0.0F, 1.0F, 0.0F));
    float const seed = seed_of(part++);
    rack.lathe(base,
               up,
               {{0.000F, 0.112F, 0.120F},
                {0.008F, 0.116F, 0.124F},
                {0.014F, 0.104F, 0.112F},
                {0.030F, 0.100F, 0.107F},
                {0.080F, 0.097F, 0.104F},
                {0.120F, 0.085F, 0.091F},
                {0.155F, 0.062F, 0.067F},
                {0.178F, 0.036F, 0.039F},
                {0.190F, 0.018F, 0.019F},
                {0.194F, 0.014F, 0.014F},
                {0.202F, 0.018F, 0.018F},
                {0.216F, 0.018F, 0.018F},
                {0.228F, 0.010F, 0.010F},
                {0.234F, 0.000F, 0.000F}},
               18,
               false,
               V3(1.0F, 0.0F, 0.0F),
               M::k_bronze,
               seed);
    for (float const sz : {-1.0F, 1.0F}) {
      V3 const hinge = base + V3(0.005F, 0.012F, sz * 0.100F);
      rack.beam(hinge,
                hinge + V3(0.018F, -0.110F, sz * -0.006F),
                0.0045F,
                0.036F,
                M::k_bronze,
                seed + 0.2F);
    }
  }

  rack.scutum(shield_frame(V3(-0.500F, 0.513F, 0.346F), 0.170F, 0.20F),
              0.50F,
              0.62F,
              0.50F,
              0.020F,
              0.12F + 0.3F * seed_of(part++));
  rack.parma(shield_frame(V3(0.550F, 0.300F, 0.400F), 0.349F, -0.26F),
             0.290F,
             0.040F,
             0.018F,
             0.62F + 0.3F * seed_of(part++));

  {
    float const seed = seed_of(part++);
    std::vector<Ring> limbs;
    constexpr int k_steps = 24;
    for (int i = 0; i <= k_steps; ++i) {
      float const t = -1.0F + 2.0F * static_cast<float>(i) / k_steps;
      float const at = std::abs(t);
      float const recurve = std::clamp((at - 0.78F) / 0.22F, 0.0F, 1.0F);
      float const x = 0.300F + 0.190F * (1.0F - t * t) +
                      0.060F * recurve * recurve * (3.0F - 2.0F * recurve);
      float const r = 0.024F - 0.013F * std::pow(at, 1.3F);
      limbs.push_back({V3(x, 0.950F + 0.660F * t, -0.150F), r * 1.35F, r * 0.78F});
    }
    rack.sweep(limbs, 10, false, V3(0.0F, 0.0F, 1.0F), M::k_yew, seed);
    V3 const grip = limbs[k_steps / 2].center;
    rack.tube(grip - V3(0.0F, 0.070F, 0.0F),
              grip + V3(0.0F, 0.070F, 0.0F),
              0.0265F,
              0.0265F,
              10,
              M::k_leather,
              seed + 0.3F,
              V3(0.0F, 0.0F, 1.0F));
    V3 const nock_low = limbs.front().center;
    V3 const nock_high = limbs.back().center;
    rack.tube(nock_low, nock_high, 0.0035F, 0.0035F, 5, M::k_linen, seed + 0.5F);
    rack.peg(V3(0.360F, 1.534F, -0.075F), V3(0.360F, 1.548F, -0.175F), seed_of(part++));
  }

  {
    float const seed = seed_of(part++);
    V3 const foot(0.620F, 0.520F, -0.130F);
    V3 const mouth(0.650F, 1.180F, -0.136F);
    V3 const axis = unit(mouth - foot, V3(0.0F, 1.0F, 0.0F));
    float const length = (mouth - foot).length();
    rack.lathe(foot,
               axis,
               {{0.000F, 0.000F, 0.000F},
                {0.004F, 0.036F, 0.030F},
                {0.030F, 0.048F, 0.040F},
                {length * 0.5F, 0.052F, 0.043F},
                {length, 0.056F, 0.047F}},
               12,
               false,
               V3(1.0F, 0.0F, 0.0F),
               M::k_leather,
               seed);
    for (float const h : {0.050F, length - 0.030F}) {
      rack.lathe(foot + axis * h,
                 axis,
                 {{0.000F, 0.054F, 0.045F}, {0.026F, 0.058F, 0.049F}},
                 12,
                 false,
                 V3(1.0F, 0.0F, 0.0F),
                 M::k_bronze,
                 seed + 0.1F);
    }
    std::array<V3, 6> const arrows{{V3(-0.020F, 0.0F, -0.014F),
                                    V3(0.018F, 0.0F, -0.018F),
                                    V3(0.000F, 0.0F, 0.012F),
                                    V3(-0.026F, 0.0F, 0.016F),
                                    V3(0.026F, 0.0F, 0.010F),
                                    V3(0.004F, 0.0F, -0.004F)}};
    int arrow = 0;
    for (const V3& offset : arrows) {
      float const lean = 0.010F * static_cast<float>(arrow % 3 - 1);
      V3 const low = mouth + offset - axis * 0.220F;
      V3 const high = mouth + offset * 1.5F + V3(lean, 0.0F, 0.0F) +
                      axis * (0.150F + 0.012F * static_cast<float>(arrow % 2));
      V3 const shaft = unit(high - low, axis);
      float const arrow_seed = seed + 0.07F * static_cast<float>(arrow);
      rack.tube(low, high, 0.0048F, 0.0048F, 5, M::k_ash, arrow_seed);
      rack.lathe(high,
                 shaft,
                 {{0.000F, 0.0055F, 0.0055F},
                  {0.012F, 0.0050F, 0.0050F},
                  {0.014F, 0.0F, 0.0F}},
                 5,
                 false,
                 V3(1.0F, 0.0F, 0.0F),
                 M::k_bone,
                 arrow_seed);
      V3 const vane_dir = unit(V3::crossProduct(shaft, offset + V3(0.001F, 0.0F, 0.0F)),
                               V3(1.0F, 0.0F, 0.0F));
      for (float const s : {-1.0F, 1.0F}) {
        V3 const root = high - shaft * 0.105F + vane_dir * (s * 0.010F);
        rack.beam(root,
                  root + shaft * 0.085F + vane_dir * (s * 0.004F),
                  0.0012F,
                  0.0080F,
                  M::k_feather,
                  arrow_seed + 0.5F);
      }
      ++arrow;
    }
    rack.peg(V3(0.620F, 1.534F, -0.075F), V3(0.620F, 1.548F, -0.160F), seed_of(part++));
    std::array<Ring, 5> const strap{
        {{mouth + V3(-0.050F, -0.020F, 0.0F), 0.013F, 0.0025F},
         {V3(0.590F, 1.340F, -0.152F), 0.013F, 0.0025F},
         {V3(0.618F, 1.562F, -0.150F), 0.013F, 0.0025F},
         {V3(0.650F, 1.340F, -0.156F), 0.013F, 0.0025F},
         {mouth + V3(0.052F, -0.020F, 0.0F), 0.013F, 0.0025F}}};
    rack.sweep(strap, 6, false, V3(0.0F, 0.0F, 1.0F), M::k_leather, seed + 0.8F);
  }

  mesh.surface.resize(mesh.vertices.size());
  return mesh;
}

} // namespace Render::GL::BackendPipelines
