#include "render/humanoid/asset/humanoid_beard_mesh.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "render/gl/mesh.h"
#include "render/humanoid/asset/humanoid_spec.h"

namespace Render::Humanoid {

namespace {

using Render::GL::Vertex;

constexpr float k_theta_max = 1.85F;
constexpr float k_head_center_y = -0.05F;
constexpr float k_face_depth_scale = 0.97F;
constexpr float k_jaw_center_y = -0.90F;
constexpr float k_chest_clearance_z = 0.82F;
constexpr float k_chest_top_y = -1.25F;

struct BeardShape {
  float coverage;
  float top_center_y;
  float side_rise;
  float tip_y;
  float tip_z;
  float bottom_half_width;
  float face_thickness;
  float hang_depth;
  float lock_count;
  float lock_relief;
  int hang_rows;
  bool mustache;
  float bulge;
};

auto shape_for(HumanoidBodyVariant variant) -> BeardShape {
  switch (variant) {
  case HumanoidBodyVariant::ShortBeard:
    return {1.0F,
            -0.64F,
            0.36F,
            -1.10F,
            0.90F,
            0.46F,
            0.08F,
            0.20F,
            7.0F,
            0.024F,
            4,
            false,
            0.06F};
  case HumanoidBodyVariant::FullBeard:
    return {1.0F,
            -0.64F,
            0.38F,
            -1.95F,
            1.16F,
            0.22F,
            0.09F,
            0.42F,
            9.0F,
            0.045F,
            12,
            true,
            0.16F};
  case HumanoidBodyVariant::LongGoatee:
    return {0.30F,
            -0.64F,
            0.0F,
            -1.60F,
            1.08F,
            0.08F,
            0.08F,
            0.24F,
            4.0F,
            0.030F,
            9,
            true,
            0.08F};
  case HumanoidBodyVariant::MustacheBeard:
    return {1.0F,
            -0.64F,
            0.36F,
            -1.24F,
            0.96F,
            0.36F,
            0.08F,
            0.24F,
            8.0F,
            0.028F,
            5,
            true,
            0.08F};
  case HumanoidBodyVariant::Clean:
  case HumanoidBodyVariant::Count:
    break;
  }
  return {};
}

auto smoothstep(float edge0, float edge1, float x) -> float {
  float const t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto face_point(float theta, float y) -> QVector3D {
  float const dy = y - k_head_center_y;
  float const ring = std::sqrt(std::max(0.12F, 1.0F - (dy * dy)));
  return {ring * std::sin(theta), y, ring * std::cos(theta) * k_face_depth_scale};
}

auto face_normal(const QVector3D& p) -> QVector3D {
  return QVector3D(p.x(),
                   p.y() - k_head_center_y,
                   p.z() / (k_face_depth_scale * k_face_depth_scale))
      .normalized();
}

auto hermite(const QVector3D& p0,
             const QVector3D& t0,
             const QVector3D& p1,
             const QVector3D& t1,
             float t) -> QVector3D {
  float const t2 = t * t;
  float const t3 = t2 * t;
  return (p0 * ((2.0F * t3) - (3.0F * t2) + 1.0F)) + (t0 * (t3 - (2.0F * t2) + t)) +
         (p1 * ((-2.0F * t3) + (3.0F * t2))) + (t1 * (t3 - t2));
}

struct Surface {
  std::vector<QVector3D> positions;
  std::vector<std::array<std::uint32_t, 3>> triangles;
};

void orient(Surface& surface,
            std::size_t first_triangle,
            const QVector3D& outward_hint,
            bool use_centroid,
            const QVector3D& centroid) {
  for (std::size_t i = first_triangle; i < surface.triangles.size(); ++i) {
    auto& tri = surface.triangles[i];
    QVector3D const a = surface.positions[tri[0]];
    QVector3D const b = surface.positions[tri[1]];
    QVector3D const c = surface.positions[tri[2]];
    QVector3D const n = QVector3D::crossProduct(b - a, c - a);
    QVector3D const hint =
        use_centroid ? ((a + b + c) / 3.0F) - centroid : outward_hint;
    if (QVector3D::dotProduct(n, hint) < 0.0F) {
      std::swap(tri[1], tri[2]);
    }
  }
}

void append_group(std::vector<Vertex>& vertices,
                  std::vector<unsigned int>& indices,
                  const Surface& surface) {
  std::vector<QVector3D> normals(surface.positions.size(), QVector3D());
  for (auto const& tri : surface.triangles) {
    QVector3D const a = surface.positions[tri[0]];
    QVector3D const b = surface.positions[tri[1]];
    QVector3D const c = surface.positions[tri[2]];
    QVector3D const n = QVector3D::crossProduct(b - a, c - a);
    for (auto const idx : tri) {
      normals[idx] += n;
    }
  }
  auto const base = static_cast<unsigned int>(vertices.size());
  for (std::size_t i = 0; i < surface.positions.size(); ++i) {
    Vertex v{};
    QVector3D const p = surface.positions[i];
    QVector3D n = normals[i];
    n = n.lengthSquared() > 1.0e-12F ? n.normalized() : QVector3D(0.0F, 0.0F, 1.0F);
    v.position = {p.x(), p.y(), p.z()};
    v.normal = {n.x(), n.y(), n.z()};
    v.color_role = k_humanoid_hair_role;
    vertices.push_back(v);
  }
  for (auto const& tri : surface.triangles) {
    indices.push_back(base + tri[0]);
    indices.push_back(base + tri[1]);
    indices.push_back(base + tri[2]);
  }
}

void add_grid_triangles(Surface& surface,
                        std::uint32_t base,
                        std::uint32_t columns,
                        std::uint32_t rows) {
  for (std::uint32_t r = 0; r + 1U < rows; ++r) {
    for (std::uint32_t c = 0; c + 1U < columns; ++c) {
      std::uint32_t const i0 = base + (r * columns) + c;
      std::uint32_t const i1 = i0 + 1U;
      std::uint32_t const i2 = i0 + columns;
      std::uint32_t const i3 = i2 + 1U;
      surface.triangles.push_back({i0, i2, i1});
      surface.triangles.push_back({i1, i2, i3});
    }
  }
}

struct Loft {
  std::uint32_t columns = 0;
  std::uint32_t rows = 0;
  std::vector<QVector3D> outer;
  std::vector<QVector3D> inner;

  [[nodiscard]] auto at(std::uint32_t row,
                        std::uint32_t column) const -> std::uint32_t {
    return (row * columns) + column;
  }
};

auto build_loft(const BeardShape& shape, bool detailed) -> Loft {
  Loft loft;
  int const face_rows = detailed ? 5 : 2;
  int const hang_rows = detailed ? shape.hang_rows : std::max(2, shape.hang_rows / 3);
  loft.columns = detailed ? 25U : 7U;
  loft.rows = static_cast<std::uint32_t>(face_rows + hang_rows + 1);
  loft.outer.resize(static_cast<std::size_t>(loft.columns) * loft.rows);
  loft.inner.resize(loft.outer.size());

  QVector3D const back_direction = QVector3D(0.0F, 0.18F, 1.0F).normalized();
  float const lock_relief = detailed ? shape.lock_relief : 0.0F;

  for (std::uint32_t c = 0; c < loft.columns; ++c) {
    float const s =
        ((2.0F * static_cast<float>(c)) / static_cast<float>(loft.columns - 1U)) - 1.0F;
    float const arc = s * shape.coverage;
    float const theta = arc * k_theta_max;
    float const edge = std::abs(s);
    float const y_top =
        shape.top_center_y + (shape.side_rise * smoothstep(0.25F, 1.0F, edge));
    float const y_jaw = k_jaw_center_y + (0.34F * arc * arc);

    float const lock_phase = s * shape.lock_count * std::numbers::pi_v<float> * 0.5F;
    float const ridge = std::pow(std::abs(std::cos(lock_phase)), 0.6F);

    QVector3D jaw_outer;
    for (int r = 0; r <= face_rows; ++r) {
      float const t = static_cast<float>(r) / static_cast<float>(face_rows);
      float const y = y_top + ((y_jaw - y_top) * t);
      QVector3D const p = face_point(theta, y);
      QVector3D const n = face_normal(p);
      float const relief = lock_relief * t * 0.5F * ridge;
      auto const idx = loft.at(static_cast<std::uint32_t>(r), c);
      loft.outer[idx] =
          p + (n * (shape.face_thickness * (0.55F + (0.45F * t)) + relief));
      loft.inner[idx] = p - (n * 0.10F);
      jaw_outer = loft.outer[idx];
    }

    float const bottom_span = std::max(0.0F, k_jaw_center_y - shape.tip_y);
    QVector3D bottom(s * shape.bottom_half_width,
                     shape.tip_y + (bottom_span * 0.30F * s * s) -
                         (lock_relief * 3.0F * ridge),
                     shape.tip_z - (0.16F * s * s));
    float const reach = (bottom - jaw_outer).length();
    QVector3D const jaw_tangent =
        QVector3D(jaw_outer.x() * -0.15F, -1.0F, 0.30F).normalized() * reach;
    QVector3D const tip_tangent =
        QVector3D(0.0F, -1.0F, 0.08F).normalized() * reach * 0.7F;

    for (int r = 1; r <= hang_rows; ++r) {
      float const t = static_cast<float>(r) / static_cast<float>(hang_rows);
      QVector3D front = hermite(jaw_outer, jaw_tangent, bottom, tip_tangent, t);
      float const depth = ((shape.face_thickness + 0.10F) +
                           ((shape.hang_depth - shape.face_thickness - 0.10F) *
                            smoothstep(0.0F, 0.35F, t))) *
                          (1.0F - (0.85F * t * t * t));
      QVector3D const lock_normal =
          QVector3D(front.x() * 0.6F, 0.0F, 1.0F).normalized();
      front += lock_normal * (lock_relief * ridge * std::min(1.0F, t * 2.0F));
      float const belly =
          std::sin(std::numbers::pi_v<float> * std::min(1.0F, t * 1.15F));
      front += QVector3D(0.0F, 0.0F, shape.bulge * (1.0F - (s * s)) * belly);
      QVector3D back = front - (back_direction * depth);
      if (back.y() < k_chest_top_y && back.z() < k_chest_clearance_z) {
        float const lift = k_chest_clearance_z - back.z();
        back.setZ(k_chest_clearance_z);
        front.setZ(std::max(front.z(), back.z() + 0.03F + (lift * 0.2F)));
      }
      auto const idx = loft.at(static_cast<std::uint32_t>(face_rows + r), c);
      loft.outer[idx] = front;
      loft.inner[idx] = back;
    }
  }
  return loft;
}

void append_loft(std::vector<Vertex>& vertices,
                 std::vector<unsigned int>& indices,
                 const Loft& loft) {
  QVector3D centroid;
  for (auto const& p : loft.outer) {
    centroid += p;
  }
  for (auto const& p : loft.inner) {
    centroid += p;
  }
  centroid /= static_cast<float>(loft.outer.size() + loft.inner.size());

  {
    Surface outer;
    outer.positions = loft.outer;
    add_grid_triangles(outer, 0U, loft.columns, loft.rows);
    for (auto& tri : outer.triangles) {
      QVector3D const a = outer.positions[tri[0]];
      QVector3D const b = outer.positions[tri[1]];
      QVector3D const c = outer.positions[tri[2]];
      QVector3D const out_dir = ((a - loft.inner[tri[0]]) + (b - loft.inner[tri[1]]) +
                                 (c - loft.inner[tri[2]]));
      if (QVector3D::dotProduct(QVector3D::crossProduct(b - a, c - a), out_dir) <
          0.0F) {
        std::swap(tri[1], tri[2]);
      }
    }
    append_group(vertices, indices, outer);
  }
  {
    Surface inner;
    inner.positions = loft.inner;
    add_grid_triangles(inner, 0U, loft.columns, loft.rows);
    for (auto& tri : inner.triangles) {
      QVector3D const a = inner.positions[tri[0]];
      QVector3D const b = inner.positions[tri[1]];
      QVector3D const c = inner.positions[tri[2]];
      QVector3D const in_dir = ((a - loft.outer[tri[0]]) + (b - loft.outer[tri[1]]) +
                                (c - loft.outer[tri[2]]));
      if (QVector3D::dotProduct(QVector3D::crossProduct(b - a, c - a), in_dir) < 0.0F) {
        std::swap(tri[1], tri[2]);
      }
    }
    append_group(vertices, indices, inner);
  }

  auto add_wall = [&](auto&& index_at, std::uint32_t count) {
    Surface wall;
    for (std::uint32_t i = 0; i < count; ++i) {
      auto const idx = index_at(i);
      wall.positions.push_back(loft.outer[idx]);
      wall.positions.push_back(loft.inner[idx]);
    }
    for (std::uint32_t i = 0; i + 1U < count; ++i) {
      std::uint32_t const o0 = i * 2U;
      std::uint32_t const i0 = o0 + 1U;
      std::uint32_t const o1 = o0 + 2U;
      std::uint32_t const i1 = o0 + 3U;
      wall.triangles.push_back({o0, i0, o1});
      wall.triangles.push_back({o1, i0, i1});
    }
    orient(wall, 0U, QVector3D(), true, centroid);
    append_group(vertices, indices, wall);
  };

  add_wall([&](std::uint32_t row) { return loft.at(row, 0U); }, loft.rows);
  add_wall([&](std::uint32_t row) { return loft.at(row, loft.columns - 1U); },
           loft.rows);
  add_wall([&](std::uint32_t column) { return loft.at(0U, column); }, loft.columns);
  add_wall([&](std::uint32_t column) { return loft.at(loft.rows - 1U, column); },
           loft.columns);
}

void append_mustache(std::vector<Vertex>& vertices,
                     std::vector<unsigned int>& indices) {
  constexpr std::array<std::array<float, 2>, 5> k_path{{
      {0.03F, -0.37F},
      {0.16F, -0.40F},
      {0.30F, -0.46F},
      {0.40F, -0.56F},
      {0.45F, -0.70F},
  }};
  constexpr int k_samples = 12;
  constexpr int k_sides = 10;

  for (float side : {-1.0F, 1.0F}) {
    std::vector<QVector3D> centres;
    std::vector<float> radii;
    for (int i = 0; i <= k_samples; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(k_samples);
      float const f = t * static_cast<float>(k_path.size() - 1U);
      auto const seg = std::min(static_cast<std::size_t>(f), k_path.size() - 2U);
      float const u = f - static_cast<float>(seg);
      float const x = k_path[seg][0] + ((k_path[seg + 1U][0] - k_path[seg][0]) * u);
      float const y = k_path[seg][1] + ((k_path[seg + 1U][1] - k_path[seg][1]) * u);
      float const radius = 0.070F * (1.0F - (0.85F * t * t)) + 0.006F;
      float const dy = y - k_head_center_y;
      float const ring = std::sqrt(std::max(0.12F, 1.0F - (dy * dy)));
      float const theta = std::asin(std::clamp(x / ring, -1.0F, 1.0F));
      QVector3D const surface = face_point(side * theta, y);
      centres.push_back(surface + (face_normal(surface) * (radius * 0.75F)));
      radii.push_back(radius);
    }

    auto const base = static_cast<unsigned int>(vertices.size());
    for (std::size_t i = 0; i < centres.size(); ++i) {
      QVector3D const prev = centres[i == 0 ? 0 : i - 1U];
      QVector3D const next = centres[std::min(i + 1U, centres.size() - 1U)];
      QVector3D const tangent = (next - prev).normalized();
      QVector3D const out = face_normal(centres[i]);
      QVector3D const binormal = QVector3D::crossProduct(tangent, out).normalized();
      QVector3D const normal = QVector3D::crossProduct(binormal, tangent).normalized();
      for (int k = 0; k < k_sides; ++k) {
        float const a = (2.0F * std::numbers::pi_v<float> * static_cast<float>(k)) /
                        static_cast<float>(k_sides);
        QVector3D const radial = (normal * std::cos(a)) + (binormal * std::sin(a));
        QVector3D const p = centres[i] + (radial * radii[i]);
        Vertex v{};
        v.position = {p.x(), p.y(), p.z()};
        v.normal = {radial.x(), radial.y(), radial.z()};
        v.color_role = k_humanoid_hair_role;
        vertices.push_back(v);
      }
    }
    for (std::size_t i = 0; i + 1U < centres.size(); ++i) {
      for (int k = 0; k < k_sides; ++k) {
        auto const k1 = static_cast<unsigned int>((k + 1) % k_sides);
        auto const row0 = base + static_cast<unsigned int>(i * k_sides);
        auto const row1 = row0 + static_cast<unsigned int>(k_sides);
        std::array<unsigned int, 6> quad{row0 + static_cast<unsigned int>(k),
                                         row1 + static_cast<unsigned int>(k),
                                         row0 + k1,
                                         row0 + k1,
                                         row1 + static_cast<unsigned int>(k),
                                         row1 + k1};
        for (std::size_t q = 0; q < quad.size(); q += 3U) {
          auto const& va = vertices[quad[q]];
          auto const& vb = vertices[quad[q + 1U]];
          auto const& vc = vertices[quad[q + 2U]];
          QVector3D const a(va.position[0], va.position[1], va.position[2]);
          QVector3D const b(vb.position[0], vb.position[1], vb.position[2]);
          QVector3D const c(vc.position[0], vc.position[1], vc.position[2]);
          QVector3D const n(va.normal[0], va.normal[1], va.normal[2]);
          bool const flip =
              QVector3D::dotProduct(QVector3D::crossProduct(b - a, c - a), n) < 0.0F;
          indices.push_back(quad[q]);
          indices.push_back(flip ? quad[q + 2U] : quad[q + 1U]);
          indices.push_back(flip ? quad[q + 1U] : quad[q + 2U]);
        }
      }
    }
  }
}

} // namespace

auto body_variant_for_facial_hair(Render::GL::FacialHairStyle style) noexcept
    -> HumanoidBodyVariant {
  using Render::GL::FacialHairStyle;
  switch (style) {
  case FacialHairStyle::ShortBeard:
    return HumanoidBodyVariant::ShortBeard;
  case FacialHairStyle::FullBeard:
  case FacialHairStyle::LongBeard:
    return HumanoidBodyVariant::FullBeard;
  case FacialHairStyle::Goatee:
    return HumanoidBodyVariant::LongGoatee;
  case FacialHairStyle::MustacheAndBeard:
    return HumanoidBodyVariant::MustacheBeard;
  case FacialHairStyle::None:
  case FacialHairStyle::Stubble:
  case FacialHairStyle::Mustache:
    break;
  }
  return HumanoidBodyVariant::Clean;
}

auto body_variant_name_suffix(HumanoidBodyVariant variant) noexcept
    -> std::string_view {
  switch (variant) {
  case HumanoidBodyVariant::ShortBeard:
    return "_short_beard";
  case HumanoidBodyVariant::FullBeard:
    return "_full_beard";
  case HumanoidBodyVariant::LongGoatee:
    return "_long_goatee";
  case HumanoidBodyVariant::MustacheBeard:
    return "_mustache_beard";
  case HumanoidBodyVariant::Clean:
  case HumanoidBodyVariant::Count:
    break;
  }
  return "";
}

auto build_humanoid_beard_mesh(HumanoidBodyVariant variant,
                               Render::Creature::CreatureLOD lod)
    -> std::unique_ptr<Render::GL::Mesh> {
  if (variant == HumanoidBodyVariant::Clean || variant == HumanoidBodyVariant::Count ||
      lod == Render::Creature::CreatureLOD::Culled) {
    return nullptr;
  }
  bool const detailed = lod == Render::Creature::CreatureLOD::Full;
  BeardShape const shape = shape_for(variant);

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  append_loft(vertices, indices, build_loft(shape, detailed));
  if (detailed && shape.mustache) {
    append_mustache(vertices, indices);
  }
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

} // namespace Render::Humanoid
