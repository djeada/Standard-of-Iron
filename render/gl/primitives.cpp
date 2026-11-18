#include "primitives.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "gl/mesh.h"
#include "render/gl/garment_shell.h"
#include "render/gl/shared_geometry_cache.h"

namespace Render::GL {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_two_pi = 6.28318530718F;
constexpr float k_half_scalar = 0.5F;
constexpr float k_unit_radius = 1.0F;
constexpr float k_uv_center = 0.5F;
constexpr float k_uv_scale = 0.5F;

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

  int base_top = static_cast<int>(v.size());
  v.push_back({{0.0F, half_h, 0.0F}, {0.0F, 1.0F, 0.0F}, {k_uv_center, k_uv_center}});
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

  int base_bot = static_cast<int>(v.size());
  v.push_back({{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
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

auto create_unit_tapered_cylinder_mesh(float anchor_radius_scale,
                                       float tail_radius_scale,
                                       int radial_segments) -> std::unique_ptr<Mesh> {
  const float half_h = k_half_scalar;
  const float anchor_radius = std::max(anchor_radius_scale, 0.001F);
  const float tail_radius = std::max(tail_radius_scale, 0.001F);
  const float slope = anchor_radius - tail_radius;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  auto append_ring = [&](float py, float radius, float v_coord) {
    for (int i = 0; i <= radial_segments; ++i) {
      float const u = float(i) / float(radial_segments);
      float const ang = u * k_two_pi;
      float const px = radius * std::cos(ang);
      float const pz = radius * std::sin(ang);
      QVector3D n(std::cos(ang), slope, std::sin(ang));
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v_coord}});
    }
  };

  append_ring(-half_h, anchor_radius, 0.0F);
  append_ring(half_h, tail_radius, 1.0F);

  int const row = radial_segments + 1;
  for (int i = 0; i < radial_segments; ++i) {
    int const a = i;
    int const b = i + 1;
    int const c = row + i + 1;
    int const d = row + i;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(c);
    idx.push_back(d);
    idx.push_back(a);
  }

  int const base_anchor = static_cast<int>(v.size());
  v.push_back({{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float const px = anchor_radius * std::cos(ang);
    float const pz = anchor_radius * std::sin(ang);
    v.push_back({{px, -half_h, pz},
                 {0.0F, -1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(base_anchor);
    idx.push_back(base_anchor + i + 1);
    idx.push_back(base_anchor + i);
  }

  int const base_tail = static_cast<int>(v.size());
  v.push_back({{0.0F, half_h, 0.0F}, {0.0F, 1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  for (int i = 0; i <= radial_segments; ++i) {
    float const u = float(i) / float(radial_segments);
    float const ang = u * k_two_pi;
    float const px = tail_radius * std::cos(ang);
    float const pz = tail_radius * std::sin(ang);
    v.push_back({{px, half_h, pz},
                 {0.0F, 1.0F, 0.0F},
                 {k_uv_center + k_uv_scale * std::cos(ang),
                  k_uv_center + k_uv_scale * std::sin(ang)}});
  }
  for (int i = 1; i <= radial_segments; ++i) {
    idx.push_back(base_tail);
    idx.push_back(base_tail + i);
    idx.push_back(base_tail + i + 1);
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

  int base_center = static_cast<int>(v.size());
  v.push_back({{0.0F, -half_h, 0.0F}, {0.0F, -1.0F, 0.0F}, {k_uv_center, k_uv_center}});
  int const base_start = static_cast<int>(v.size());
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
  const float radius = k_unit_radius;
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

  int base_top = static_cast<int>(verts.size());
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

  int base_bot = static_cast<int>(verts.size());
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

auto create_unit_torso_mesh(int radial_segments,
                            int height_segments) -> std::unique_ptr<Mesh> {
  // Armor attachments run from shoulder to waist along local +Y, and
  // align_torso_mesh_forward maps the local Z diameter across the shoulders.
  return make_garment_shell({{-0.53F, 0.54F, 0.53F},
                             {-0.46F, 0.87F, 0.91F},
                             {-0.29F, 1.10F, 1.13F},
                             {-0.08F, 1.08F, 1.09F},
                             {0.17F, 0.98F, 0.98F},
                             {0.40F, 0.93F, 0.90F},
                             {0.51F, 0.96F, 0.92F}},
                            radial_segments,
                            std::max(1, height_segments / 4),
                            true);
}

} // namespace

namespace {

auto create_orientation_arrow_mesh() -> std::unique_ptr<Mesh> {
  constexpr float sw = 0.075F;
  constexpr float hw = 0.28F;
  constexpr float sl = 1.08F;
  constexpr float hl = 0.48F;
  constexpr float tl = sl + hl;
  constexpr float hh = 0.028F;
  constexpr float nd = 0.16F;

  struct P2D {
    float x;
    float z;
  };
  const P2D outline[8] = {
      {sw, 0.0F},
      {sw, -sl},
      {hw, -sl},
      {0.0F, -tl},
      {-hw, -sl},
      {-sw, -sl},
      {-sw, 0.0F},
      {0.0F, -nd},
  };
  constexpr int N = 8;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  auto push_top = [&](const P2D& p) {
    v.push_back({{p.x, hh, p.z}, {0.0F, 1.0F, 0.0F}, {0.5F + p.x, -p.z}});
  };
  auto push_bot = [&](const P2D& p) {
    v.push_back({{p.x, -hh, p.z}, {0.0F, -1.0F, 0.0F}, {0.5F + p.x, -p.z}});
  };

  int const top_base = static_cast<int>(v.size());
  for (int i = 0; i < N; ++i) {
    push_top(outline[i]);
  }

  idx.push_back(top_base + 7);
  idx.push_back(top_base + 0);
  idx.push_back(top_base + 1);
  idx.push_back(top_base + 7);
  idx.push_back(top_base + 1);
  idx.push_back(top_base + 5);
  idx.push_back(top_base + 5);
  idx.push_back(top_base + 6);
  idx.push_back(top_base + 7);

  idx.push_back(top_base + 1);
  idx.push_back(top_base + 2);
  idx.push_back(top_base + 3);
  idx.push_back(top_base + 1);
  idx.push_back(top_base + 3);
  idx.push_back(top_base + 5);
  idx.push_back(top_base + 3);
  idx.push_back(top_base + 4);
  idx.push_back(top_base + 5);

  int const bot_base = static_cast<int>(v.size());
  for (int i = 0; i < N; ++i) {
    push_bot(outline[i]);
  }
  idx.push_back(bot_base + 7);
  idx.push_back(bot_base + 1);
  idx.push_back(bot_base + 0);
  idx.push_back(bot_base + 7);
  idx.push_back(bot_base + 5);
  idx.push_back(bot_base + 1);
  idx.push_back(bot_base + 5);
  idx.push_back(bot_base + 7);
  idx.push_back(bot_base + 6);
  idx.push_back(bot_base + 1);
  idx.push_back(bot_base + 3);
  idx.push_back(bot_base + 2);
  idx.push_back(bot_base + 1);
  idx.push_back(bot_base + 5);
  idx.push_back(bot_base + 3);
  idx.push_back(bot_base + 3);
  idx.push_back(bot_base + 5);
  idx.push_back(bot_base + 4);

  for (int i = 0; i < N; ++i) {
    int const next = (i + 1) % N;
    const P2D& a = outline[i];
    const P2D& b = outline[next];

    float const ex = b.x - a.x;
    float const ez = b.z - a.z;
    QVector3D n(-ez, 0.0F, ex);
    n.normalize();

    int const vi = static_cast<int>(v.size());
    v.push_back({{a.x, -hh, a.z}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
    v.push_back({{b.x, -hh, b.z}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    v.push_back({{b.x, +hh, b.z}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
    v.push_back({{a.x, +hh, a.z}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});

    idx.push_back(vi + 0);
    idx.push_back(vi + 1);
    idx.push_back(vi + 2);
    idx.push_back(vi + 2);
    idx.push_back(vi + 3);
    idx.push_back(vi + 0);
  }

  return std::make_unique<Mesh>(v, idx);
}

enum class UnitPrimitiveKind : std::uint8_t {
  Sphere,
  Cylinder,
  TaperedCylinder,
  Cone,
  Capsule,
};

struct UnitPrimitive {
  UnitPrimitiveKind kind{UnitPrimitiveKind::Sphere};
  int radial_segments{0};
  int lat_segments{0};
  int height_segments{0};
  float anchor_radius_scale{1.0F};
  float tail_radius_scale{1.0F};
};

struct UnitPrimitiveRegistry {
  std::mutex mutex;
  std::unordered_map<const Mesh*, UnitPrimitive> primitives;
};

auto unit_primitive_registry() -> UnitPrimitiveRegistry& {
  static UnitPrimitiveRegistry registry;
  return registry;
}

auto registered(std::unique_ptr<Mesh> mesh,
                const UnitPrimitive& primitive) -> std::unique_ptr<Mesh> {
  auto& registry = unit_primitive_registry();
  const std::lock_guard<std::mutex> lock(registry.mutex);
  registry.primitives[mesh.get()] = primitive;
  return mesh;
}

auto unit_primitive_of(const Mesh* mesh) -> const UnitPrimitive* {
  auto& registry = unit_primitive_registry();
  const std::lock_guard<std::mutex> lock(registry.mutex);
  const auto it = registry.primitives.find(mesh);
  return it != registry.primitives.end() ? &it->second : nullptr;
}

auto segments_for_radius(float radius, int authored, int minimum) -> int {
  if (!(radius > k_bake_chord_tolerance)) {
    return std::min(authored, minimum);
  }
  const double half_angle =
      std::acos(1.0 - static_cast<double>(k_bake_chord_tolerance) / radius);
  const int needed = static_cast<int>(std::ceil(std::numbers::pi / half_angle));
  return std::clamp(needed, std::min(minimum, authored), authored);
}

auto keep_axis_extremes(int segments, int authored) -> int {
  const int rounded = ((segments + 3) / 4) * 4;
  return rounded <= authored ? rounded : authored;
}

auto axis_length(const QMatrix4x4& model, int column) -> float {
  return model.column(column).toVector3D().length();
}

} // namespace

auto unit_solid_of(const Mesh* mesh) -> UnitSolid {
  const UnitPrimitive* primitive = unit_primitive_of(mesh);
  if (primitive == nullptr) {
    return {};
  }
  if (primitive->kind == UnitPrimitiveKind::Cylinder) {
    return {.kind = UnitSolidKind::Prism,
            .inradius = k_unit_radius *
                        std::cos(std::numbers::pi_v<float> /
                                 static_cast<float>(primitive->radial_segments))};
  }
  if (primitive->kind != UnitPrimitiveKind::Sphere) {
    return {};
  }
  auto const& vertices = mesh->get_vertices();
  auto const& indices = mesh->get_indices();
  auto position = [&](unsigned int i) {
    auto const& p = vertices[i].position;
    return QVector3D(p[0], p[1], p[2]);
  };
  float inradius = k_unit_radius;
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    QVector3D const a = position(indices[i]);
    QVector3D const normal = QVector3D::crossProduct(position(indices[i + 1]) - a,
                                                     position(indices[i + 2]) - a);
    if (normal.lengthSquared() < 1.0e-12F) {
      continue;
    }
    inradius =
        std::min(inradius, std::abs(QVector3D::dotProduct(normal.normalized(), a)));
  }
  return {.kind = UnitSolidKind::Ball, .inradius = inradius};
}

auto bake_radial_segments(float radius, int authored) -> int {
  return keep_axis_extremes(
      segments_for_radius(radius, authored, k_min_bake_radial_segments), authored);
}

auto get_unit_cylinder(int radial_segments) -> Mesh* {
  radial_segments = std::max(radial_segments, 3);
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/unit_cylinder", static_cast<std::uint64_t>(radial_segments)),
      [radial_segments] {
        return registered(
            create_unit_cylinder_mesh(radial_segments),
            {.kind = UnitPrimitiveKind::Cylinder, .radial_segments = radial_segments});
      });
}

auto get_unit_tapered_cylinder(float anchor_radius_scale,
                               float tail_radius_scale,
                               int radial_segments) -> Mesh* {
  radial_segments = std::max(radial_segments, 3);
  anchor_radius_scale = std::max(anchor_radius_scale, 0.001F);
  tail_radius_scale = std::max(tail_radius_scale, 0.001F);

  auto const quantize = [](float value) -> std::uint64_t {
    return static_cast<std::uint64_t>(std::lround(value * 1000.0F));
  };
  std::uint64_t const variant = (quantize(anchor_radius_scale) << 32) |
                                (quantize(tail_radius_scale) << 16) |
                                static_cast<std::uint64_t>(radial_segments);

  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/unit_tapered_cylinder", variant),
      [anchor_radius_scale, tail_radius_scale, radial_segments] {
        return registered(create_unit_tapered_cylinder_mesh(
                              anchor_radius_scale, tail_radius_scale, radial_segments),
                          {.kind = UnitPrimitiveKind::TaperedCylinder,
                           .radial_segments = radial_segments,
                           .anchor_radius_scale = anchor_radius_scale,
                           .tail_radius_scale = tail_radius_scale});
      });
}

auto get_unit_cube() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(geometry_key("gl/unit_cube"),
                                                      create_cube_mesh);
}

auto get_unit_sphere(int lat_segments, int lon_segments) -> Mesh* {
  lat_segments = std::max(lat_segments, 3);
  lon_segments = std::max(lon_segments, 3);
  std::uint64_t const variant = (static_cast<std::uint64_t>(lat_segments) << 32U) |
                                static_cast<std::uint64_t>(lon_segments);
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/unit_sphere", variant), [lat_segments, lon_segments] {
        return registered(create_unit_sphere_mesh(lat_segments, lon_segments),
                          {.kind = UnitPrimitiveKind::Sphere,
                           .radial_segments = lon_segments,
                           .lat_segments = lat_segments});
      });
}

auto get_unit_cone(int radial_segments) -> Mesh* {
  radial_segments = std::max(radial_segments, 3);
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/unit_cone", static_cast<std::uint64_t>(radial_segments)),
      [radial_segments] {
        return registered(
            create_unit_cone_mesh(radial_segments),
            {.kind = UnitPrimitiveKind::Cone, .radial_segments = radial_segments});
      });
}

auto get_unit_capsule(int radial_segments, int height_segments) -> Mesh* {
  radial_segments = std::max(radial_segments, 3);
  height_segments = std::max(height_segments, 1);
  std::uint64_t const variant = (static_cast<std::uint64_t>(radial_segments) << 32U) |
                                static_cast<std::uint64_t>(height_segments);
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/unit_capsule", variant), [radial_segments, height_segments] {
        return registered(create_capsule_mesh(radial_segments, height_segments),
                          {.kind = UnitPrimitiveKind::Capsule,
                           .radial_segments = radial_segments,
                           .height_segments = height_segments});
      });
}

auto get_unit_torso(int radial_segments, int height_segments) -> Mesh* {
  (void)radial_segments;
  (void)height_segments;
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/unit_torso"), [] {
        return create_unit_torso_mesh(k_default_radial_segments,
                                      k_default_torso_height_segments);
      });
}

auto coarse_unit_mesh_for(Mesh* mesh) -> Mesh* {
  if (mesh == nullptr) {
    return nullptr;
  }

  if (mesh == get_unit_sphere()) {
    return get_unit_sphere(k_coarse_latitude_segments, k_coarse_radial_segments);
  }
  if (mesh == get_unit_cylinder()) {
    return get_unit_cylinder(k_coarse_radial_segments);
  }
  if (mesh == get_unit_cone()) {
    return get_unit_cone(k_coarse_radial_segments);
  }
  if (mesh == get_unit_capsule()) {
    return get_unit_capsule(k_coarse_radial_segments);
  }
  return mesh;
}

auto bake_tessellated_mesh(Mesh* mesh, const QMatrix4x4& model) -> Mesh* {
  const UnitPrimitive* primitive = unit_primitive_of(mesh);
  if (primitive == nullptr) {
    return mesh;
  }

  const float radial = std::max(axis_length(model, 0), axis_length(model, 2));
  switch (primitive->kind) {
  case UnitPrimitiveKind::Sphere: {
    const float radius = std::max(radial, axis_length(model, 1));
    const int lon = bake_radial_segments(radius, primitive->radial_segments);

    const int lat = std::min(((lon / 2) + 1) / 2 * 2, primitive->lat_segments);
    return get_unit_sphere(std::max(lat, std::min(4, primitive->lat_segments)), lon);
  }
  case UnitPrimitiveKind::Cylinder:
    return get_unit_cylinder(bake_radial_segments(radial, primitive->radial_segments));
  case UnitPrimitiveKind::Cone:
    return get_unit_cone(bake_radial_segments(radial, primitive->radial_segments));
  case UnitPrimitiveKind::Capsule:
    return get_unit_capsule(bake_radial_segments(radial, primitive->radial_segments),
                            primitive->height_segments);
  case UnitPrimitiveKind::TaperedCylinder:
    return get_unit_tapered_cylinder(
        primitive->anchor_radius_scale,
        primitive->tail_radius_scale,
        bake_radial_segments(radial * std::max(primitive->anchor_radius_scale,
                                               primitive->tail_radius_scale),
                             primitive->radial_segments));
  }
  return mesh;
}

auto unit_surface_shape(const Mesh* mesh) -> UnitSurfaceShape {
  const UnitPrimitive* primitive = unit_primitive_of(mesh);
  if (primitive == nullptr) {
    return UnitSurfaceShape::Flat;
  }
  switch (primitive->kind) {
  case UnitPrimitiveKind::Cylinder:
  case UnitPrimitiveKind::TaperedCylinder:
  case UnitPrimitiveKind::Capsule:
    return UnitSurfaceShape::Round;
  case UnitPrimitiveKind::Cone:
    return UnitSurfaceShape::Pointed;
  case UnitPrimitiveKind::Sphere:
    return UnitSurfaceShape::Flat;
  }
  return UnitSurfaceShape::Flat;
}

auto get_orientation_arrow() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("gl/orientation_arrow"), create_orientation_arrow_mesh);
}

} // namespace Render::GL
