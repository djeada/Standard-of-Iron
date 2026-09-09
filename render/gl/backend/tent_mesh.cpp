#include "tent_mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

#include "prop_mesh_builder.h"
#include "tent_parts.h"

namespace {

using Render::GL::BackendPipelines::PropMeshData;
using Render::GL::BackendPipelines::PropMeshIndices;
using Render::GL::BackendPipelines::PropMeshVerts;
using V3 = QVector3D;
using namespace Render::GL::BackendPipelines::TentParts;

constexpr float k_pi = 3.14159265F;

constexpr float H = k_ridge_height;
constexpr float Dp = k_half_depth;
constexpr float Ex = k_eave_half_width;
constexpr float Ey = k_eave_height;
constexpr float Ww = k_wall_half_width;
constexpr float Wh = k_wall_height;
constexpr float Dw = k_door_half_width;
constexpr float Dh = k_door_height;

constexpr float k_lining_gap = 0.007F;
constexpr float k_rope_half = 0.006F;
constexpr float k_pole_radius = 0.020F;

auto fract(float v) -> float {
  return v - std::floor(v);
}

auto unit(const V3& v, const V3& fallback) -> V3 {
  const float len_sq = V3::dotProduct(v, v);
  if (len_sq < 1.0e-20F) {
    return fallback;
  }
  return v / std::sqrt(len_sq);
}

auto hash01(int x, int y) -> float {
  auto h =
      static_cast<uint32_t>(x) * 374761393U + static_cast<uint32_t>(y) * 668265263U;
  h = (h ^ (h >> 13U)) * 1274126177U;
  return static_cast<float>(h ^ (h >> 16U)) / 4294967295.0F;
}

auto value_noise(float x, float y) -> float {
  const float fx = std::floor(x);
  const float fy = std::floor(y);
  const int ix = static_cast<int>(fx);
  const int iy = static_cast<int>(fy);
  float tx = x - fx;
  float ty = y - fy;
  tx = tx * tx * (3.0F - 2.0F * tx);
  ty = ty * ty * (3.0F - 2.0F * ty);
  const float a = hash01(ix, iy);
  const float b = hash01(ix + 1, iy);
  const float c = hash01(ix, iy + 1);
  const float d = hash01(ix + 1, iy + 1);
  return std::lerp(std::lerp(a, b, tx), std::lerp(c, d, tx), ty);
}

auto crease(float u, float v, float seed) -> float {
  return (value_noise(u * 9.0F + seed, v * 6.0F + seed * 0.7F) - 0.5F) * 2.0F;
}

struct Surface {
  int nu{8};
  int nv{8};
  std::function<V3(float, float)> at;
  V3 outward;
};

void append_surface(PropMeshData& mesh, const Surface& s, int side, float material) {
  auto normal_at = [&](float u, float v) {
    constexpr float e = 1.0e-3F;
    const V3 du = s.at(std::min(u + e, 1.0F), v) - s.at(std::max(u - e, 0.0F), v);
    const V3 dv = s.at(u, std::min(v + e, 1.0F)) - s.at(u, std::max(v - e, 0.0F));
    const V3 n = unit(V3::crossProduct(du, dv), s.outward);
    return V3::dotProduct(n, s.outward) < 0.0F ? -n : n;
  };

  const auto base = static_cast<uint16_t>(mesh.vertices.size());
  const float offset = side < 0 ? k_lining_gap : 0.0F;
  for (int j = 0; j <= s.nv; ++j) {
    for (int i = 0; i <= s.nu; ++i) {
      const float u = static_cast<float>(i) / static_cast<float>(s.nu);
      const float v = static_cast<float>(j) / static_cast<float>(s.nv);
      const V3 n = normal_at(u, v);
      mesh.vertices.push_back(
          {s.at(u, v) - n * offset, n * (static_cast<float>(side) * material)});
    }
  }

  const int stride = s.nu + 1;
  auto tri = [&](uint16_t a, uint16_t b, uint16_t c) {
    const V3& pa = mesh.vertices[a].first;
    const V3 g =
        V3::crossProduct(mesh.vertices[b].first - pa, mesh.vertices[c].first - pa);
    if (V3::dotProduct(g, mesh.vertices[a].second) >= 0.0F) {
      mesh.indices.insert(mesh.indices.end(), {a, b, c});
    } else {
      mesh.indices.insert(mesh.indices.end(), {a, c, b});
    }
  };
  for (int j = 0; j < s.nv; ++j) {
    for (int i = 0; i < s.nu; ++i) {
      const auto a = static_cast<uint16_t>(base + j * stride + i);
      const auto b = static_cast<uint16_t>(a + 1);
      const auto c = static_cast<uint16_t>(a + stride);
      const auto d = static_cast<uint16_t>(c + 1);
      tri(a, b, c);
      tri(b, d, c);
    }
  }
}

void append_canvas(PropMeshData& mesh, const Surface& s) {
  append_surface(mesh, s, +1, k_material_canvas);
  append_surface(mesh, s, -1, k_material_lining);
}

template <class Fn>
void tagged(PropMeshData& mesh, float material, Fn&& fn) {
  const size_t from = mesh.vertices.size();
  fn(mesh.vertices, mesh.indices);
  for (size_t i = from; i < mesh.vertices.size(); ++i) {
    auto& n = mesh.vertices[i].second;
    n = unit(n, V3(0.0F, 1.0F, 0.0F)) * material;
  }
}

void add_rope(PropMeshData& mesh, const V3& a, const V3& b) {
  tagged(mesh, k_material_rope, [&](PropMeshVerts& v, PropMeshIndices& i) {
    Render::GL::BackendPipelines::append_prop_beam(
        v, i, a, b, k_rope_half, k_rope_half);
  });
}

auto add_peg(PropMeshData& mesh, float x, float z, float lean_x, float lean_z) -> V3 {
  const V3 foot(x - lean_x * 0.35F, -0.02F, z - lean_z * 0.35F);
  const V3 head(x + lean_x * 0.65F, 0.062F, z + lean_z * 0.65F);
  tagged(mesh, k_material_wood, [&](PropMeshVerts& v, PropMeshIndices& i) {
    Render::GL::BackendPipelines::append_prop_limb(v, i, foot, head, 0.011F, 0.014F, 6);
  });
  return V3(x + lean_x * 0.45F, 0.045F, z + lean_z * 0.45F);
}

void add_pole(PropMeshData& mesh, const V3& a, const V3& b, float r0, float r1) {
  tagged(mesh, k_material_wood, [&](PropMeshVerts& v, PropMeshIndices& i) {
    Render::GL::BackendPipelines::append_prop_limb(v, i, a, b, r0, r1, 9);
  });
}

auto roof_outward(float s) -> V3 {
  return unit(V3(s * (H - Ey), Ex, 0.0F), V3(0.0F, 1.0F, 0.0F));
}

auto roof_point(float s, float u, float v) -> V3 {
  const float z = -Dp + u * 2.0F * Dp;
  const float between = std::sin(k_pi * fract(u * 2.0F));
  const float belly = std::sin(k_pi * v);
  const float sag = belly * (0.016F + 0.020F * between) + v * v * between * 0.014F;
  const float lift = v * v * between * 0.028F;
  const float wrinkle = crease(u, v, s * 3.0F) * 0.006F * belly;
  const V3 p(s * v * Ex, H - v * (H - Ey) + lift, z);
  return p - roof_outward(s) * (sag + wrinkle);
}

auto wall_point(float s, float u, float v) -> V3 {
  const float z = -Dp + u * 2.0F * Dp;
  const float between = std::sin(k_pi * fract(u * 4.0F));
  const float bow = std::sin(k_pi * v);
  const float x = Ww + 0.012F * v + bow * (0.008F + 0.010F * between) +
                  crease(u + 5.0F, v, s * 7.0F) * 0.004F * bow;
  return V3(s * x, Wh * (1.0F - v), z);
}

auto gable_top(float x) -> float {
  return H - (std::abs(x) / Ex) * (H - Ey) - 0.022F;
}

auto gable_piece(float e, float xa, float xb, float y_lo) -> Surface {
  Surface s;
  s.nu = std::max(2, static_cast<int>(std::round((xb - xa) / 0.065F)));
  s.nv = 8;
  s.outward = V3(0.0F, 0.0F, e);
  s.at = [=](float u, float v) {
    const float x = xa + (xb - xa) * u;
    const float top = gable_top(x);
    const float y = y_lo + (top - y_lo) * v;
    const float across = std::sin(k_pi * (x + Ww) / (2.0F * Ww));
    const float rise = std::sin(k_pi * std::clamp(y / top, 0.0F, 1.0F));
    const float bow =
        across * rise * 0.010F + crease(x * 1.5F, y * 1.4F, e * 11.0F) * 0.004F * rise;
    return V3(x, y, e * (Dp + bow));
  };
  return s;
}

auto roof_panel(float s) -> Surface {
  Surface r;
  r.nu = 18;
  r.nv = 9;
  r.outward = roof_outward(s);
  r.at = [s](float u, float v) {
    return roof_point(s, u, v);
  };
  return r;
}

auto side_wall(float s) -> Surface {
  Surface w;
  w.nu = 18;
  w.nv = 4;
  w.outward = V3(s, 0.0F, 0.0F);
  w.at = [s](float u, float v) {
    return wall_point(s, u, v);
  };
  return w;
}

auto awning() -> Surface {
  constexpr float Aw = k_awning_half_width;
  constexpr float Ae = k_awning_extent;
  constexpr float Ap = k_awning_pole_x;
  Surface a;
  a.nu = 16;
  a.nv = 6;
  a.outward = unit(V3(0.0F, Ae, -(k_awning_root_height - k_awning_edge_height)),
                   V3(0.0F, 1.0F, 0.0F));
  a.at = [=](float u, float v) {
    const float x = (-Aw + 2.0F * Aw * u) * (1.0F - 0.05F * v);
    const float z = -Dp - v * Ae;
    const float inside = std::cos(0.5F * k_pi * std::clamp(x / Ap, -1.0F, 1.0F));
    const float corner = std::max(std::abs(x) / Ap - 1.0F, 0.0F) / (Aw / Ap - 1.0F);
    const float reach = std::pow(v, 1.5F);
    const float sag = reach * (0.030F * inside * inside + 0.050F * corner) +
                      std::sin(k_pi * u) * std::sin(k_pi * v) * 0.012F +
                      crease(u * 1.3F, v, 17.0F) * 0.005F * std::sin(k_pi * v);
    const float y =
        k_awning_root_height - v * (k_awning_root_height - k_awning_edge_height);
    return V3(x, y - sag, z);
  };
  return a;
}

auto hanging_flap() -> Surface {
  constexpr float width = 0.16F;
  constexpr float swing = 1.05F;
  Surface f;
  f.nu = 4;
  f.nv = 8;
  f.outward = V3(std::sin(swing + 0.15F), 0.0F, -std::cos(swing + 0.15F));
  f.at = [=](float u, float v) {
    const float theta = swing + 0.30F * v + std::sin(k_pi * v) * 0.06F;
    const float w = u * width * (1.0F + 0.08F * v);
    const float y = Dh - 0.012F - v * (Dh - 0.022F) + u * u * v * 0.03F;
    const float ripple = crease(u * 2.0F, v * 1.5F, 23.0F) * 0.006F * u;
    return V3(Dw - w * std::cos(theta) + ripple * std::sin(theta),
              y,
              -Dp - w * std::sin(theta) - ripple * std::cos(theta));
  };
  return f;
}

} // namespace

namespace Render::GL::BackendPipelines {

auto build_tent_mesh() -> PropMeshData {
  PropMeshData mesh;
  mesh.vertices.reserve(4096);
  mesh.indices.reserve(12288);

  for (const float s : {-1.0F, 1.0F}) {
    append_canvas(mesh, roof_panel(s));
    append_canvas(mesh, side_wall(s));
  }

  append_canvas(mesh, gable_piece(1.0F, -Ww, Ww, 0.0F));
  append_canvas(mesh, gable_piece(-1.0F, -Ww, -Dw, 0.0F));
  append_canvas(mesh, gable_piece(-1.0F, Dw, Ww, 0.0F));
  append_canvas(mesh, gable_piece(-1.0F, -Dw, Dw, Dh));

  append_canvas(mesh, awning());
  append_canvas(mesh, hanging_flap());

  {
    const V3 top(-Dw - 0.020F, Dh - 0.015F, -Dp - 0.022F);
    const V3 foot(-Dw - 0.046F, 0.030F, -Dp - 0.048F);
    tagged(mesh, k_material_canvas, [&](PropMeshVerts& v, PropMeshIndices& i) {
      append_prop_limb(v, i, top, foot, 0.022F, 0.031F, 9);
    });
    const V3 tie = top + (foot - top) * 0.48F;
    tagged(mesh, k_material_rope, [&](PropMeshVerts& v, PropMeshIndices& i) {
      append_prop_taper(
          v, i, tie.x(), tie.y() - 0.008F, tie.z(), 0.034F, 0.034F, 0.016F, 9);
    });
  }

  tagged(mesh, k_material_floor, [&](PropMeshVerts& v, PropMeshIndices& i) {
    append_box(
        v, i, {-Ww + 0.01F, 0.004F, -Dp + 0.01F}, {Ww - 0.01F, 0.014F, Dp - 0.01F});
  });

  constexpr float ridge_over = 0.055F;
  add_pole(mesh,
           V3(0.0F, H - 0.014F, -Dp - ridge_over),
           V3(0.0F, H - 0.014F, Dp + ridge_over),
           k_pole_radius,
           k_pole_radius);
  for (const float e : {-1.0F, 1.0F}) {
    tagged(mesh, k_material_wood, [&](PropMeshVerts& v, PropMeshIndices& i) {
      append_prop_taper(
          v, i, 0.0F, H - 0.030F, e * (Dp + ridge_over), 0.027F, 0.010F, 0.088F, 8);
    });
    add_pole(mesh,
             V3(0.0F, -0.01F, e * (Dp - 0.03F)),
             V3(0.0F, H - 0.028F, e * (Dp - 0.03F)),
             k_pole_radius * 1.1F,
             k_pole_radius);
  }

  {
    constexpr float pole_z = -Dp - k_awning_extent + 0.012F;
    for (const float s : {-1.0F, 1.0F}) {
      const V3 foot(s * k_awning_pole_x, -0.01F, pole_z);
      const V3 head(
          s * (k_awning_pole_x + 0.008F), k_awning_edge_height + 0.004F, pole_z);
      add_pole(mesh, foot, head, 0.015F, 0.012F);
      const V3 peg =
          add_peg(mesh, s * k_awning_peg_x, -k_awning_peg_z, s * 0.018F, -0.010F);
      add_rope(mesh, head - V3(0.0F, 0.012F, 0.0F), peg);
    }
  }

  for (const float s : {-1.0F, 1.0F}) {
    for (const float u : {0.0F, 0.5F, 1.0F}) {
      const V3 eave = roof_point(s, u, 1.0F);
      const V3 peg = add_peg(mesh, s * k_side_peg_x, eave.z(), s * 0.020F, 0.0F);
      add_rope(mesh, eave, peg);
    }
  }

  for (const float s : {-1.0F, 1.0F}) {
    const V3 peg = add_peg(mesh, s * 0.26F, k_rear_peg_z, s * 0.008F, 0.018F);
    add_rope(mesh, V3(0.0F, H - 0.020F, Dp + ridge_over - 0.01F), peg);
  }

  for (const float s : {-1.0F, 1.0F}) {
    for (const float z : {-0.44F, -0.15F, 0.15F, 0.44F}) {
      add_peg(mesh, s * (Ww + 0.030F), z, s * 0.012F, 0.0F);
    }
    add_peg(mesh, s * 0.30F, Dp + 0.030F, 0.0F, 0.012F);
  }

  return mesh;
}

} // namespace Render::GL::BackendPipelines
