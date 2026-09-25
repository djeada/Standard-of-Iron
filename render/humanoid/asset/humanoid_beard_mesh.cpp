#include "render/humanoid/asset/humanoid_beard_mesh.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/gl/mesh.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

namespace {

using Render::GL::Vertex;
using HP = Render::GL::HumanProportions;

constexpr float k_pi = std::numbers::pi_v<float>;

constexpr float k_rig_to_beard = HP::HEAD_RADIUS / k_beard_head_silhouette_radius;

const QVector3D k_cranium_center(
    0.0F, HP::HEAD_RADIUS * 0.06F / k_beard_head_silhouette_radius, 0.0F);
const QVector3D k_cranium_radii(0.80F * 2.0F * k_rig_to_beard,
                                0.98F * 2.0F * k_rig_to_beard,
                                0.88F * 2.0F * k_rig_to_beard);

constexpr float k_mouth_y = -0.52F;
constexpr float k_mouth_half_width = 0.24F;
constexpr float k_lip_clearance_top = k_mouth_y + 0.08F;
constexpr float k_lip_clearance_bottom = k_mouth_y - 0.09F;
constexpr float k_chin_y = -1.02F;
constexpr float k_chest_clearance_z = 0.84F;
constexpr float k_chest_top_y = -1.20F;

constexpr std::uint8_t k_head_bone = static_cast<std::uint8_t>(HumanoidBone::Head);
constexpr std::uint8_t k_chest_bone = static_cast<std::uint8_t>(HumanoidBone::Chest);

auto hash01(std::uint32_t n) -> float {
  n ^= n >> 16U;
  n *= 0x7feb352dU;
  n ^= n >> 15U;
  n *= 0x846ca68bU;
  n ^= n >> 16U;
  return static_cast<float>(n & 0xffffffU) / 16777216.0F;
}

auto smoothstep(float edge0, float edge1, float x) -> float {
  float const t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto skull_point(float azimuth, float elevation) -> QVector3D {
  return k_cranium_center +
         QVector3D(k_cranium_radii.x() * std::cos(elevation) * std::sin(azimuth),
                   k_cranium_radii.y() * std::sin(elevation),
                   k_cranium_radii.z() * std::cos(elevation) * std::cos(azimuth));
}

auto skull_normal(const QVector3D& p) -> QVector3D {
  QVector3D const d = p - k_cranium_center;
  return QVector3D(d.x() / (k_cranium_radii.x() * k_cranium_radii.x()),
                   d.y() / (k_cranium_radii.y() * k_cranium_radii.y()),
                   d.z() / (k_cranium_radii.z() * k_cranium_radii.z()))
      .normalized();
}

auto elevation_for_height(float y) -> float {
  return std::asin(
      std::clamp((y - k_cranium_center.y()) / k_cranium_radii.y(), -1.0F, 1.0F));
}

auto in_lip_zone(const QVector3D& p) -> bool {
  return p.z() > 0.0F && std::abs(p.x()) < k_mouth_half_width &&
         p.y() < k_lip_clearance_top && p.y() > k_lip_clearance_bottom;
}

auto chest_weight(float y) -> float {
  return 0.65F * smoothstep(k_chin_y, -1.9F, y);
}

struct MeshBuffers {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  void add(const QVector3D& p, const QVector3D& n, float tone) {
    Vertex v{};
    v.position = {p.x(), p.y(), p.z()};
    QVector3D const unit =
        n.lengthSquared() > 1.0e-12F ? n.normalized() : QVector3D(0, 0, 1);
    v.normal = {unit.x(), unit.y(), unit.z()};
    v.color_role = k_humanoid_hair_role;
    v.tex_coord = {tone, 0.0F};
    float const w = chest_weight(p.y());
    v.bone_indices = {k_head_bone, k_chest_bone, 0U, 0U};
    v.bone_weights = {1.0F - w, w, 0.0F, 0.0F};
    vertices.push_back(v);
  }
};

constexpr float k_band_tone = 0.70F;

auto tone_for(std::uint32_t seed) -> float {
  float const pick = hash01(seed * 7919U + 17U);
  if (pick < 0.30F) {
    return 0.70F;
  }
  if (pick > 0.78F) {
    return 1.22F;
  }
  return 0.92F + (0.16F * hash01(seed * 104729U + 3U));
}

struct BandShape {
  float azimuth_half_range;
  float center_top_y;
  float side_top_y;
  float bottom_elevation;
  float thickness;
};

void append_band(MeshBuffers& out,
                 const BandShape& band,
                 bool detailed,
                 std::uint32_t seed) {
  int const columns = detailed ? 28 : 8;
  int const rows = detailed ? 7 : 3;
  auto const base = static_cast<unsigned int>(out.vertices.size());
  std::vector<QVector3D> positions;
  positions.reserve(static_cast<std::size_t>((columns + 1) * (rows + 1)));
  for (int c = 0; c <= columns; ++c) {
    float const s =
        ((2.0F * static_cast<float>(c)) / static_cast<float>(columns)) - 1.0F;
    float const azimuth = s * band.azimuth_half_range;
    float const edge = std::abs(s);
    float top_y = band.center_top_y + ((band.side_top_y - band.center_top_y) *
                                       smoothstep(0.28F, 0.95F, edge));
    top_y += (hash01(seed + static_cast<std::uint32_t>(c) * 131U) - 0.5F) * 0.05F;
    float const top_elevation = elevation_for_height(top_y);
    float const bottom_elevation = band.bottom_elevation + (edge * 0.55F);
    for (int r = 0; r <= rows; ++r) {
      float const t = static_cast<float>(r) / static_cast<float>(rows);
      float const elevation = top_elevation + ((bottom_elevation - top_elevation) * t);
      QVector3D const p = skull_point(azimuth, elevation);
      float const taper = std::min(1.0F, std::min(t * 3.0F, (1.0F - edge) * 5.0F));
      positions.push_back(
          p + (skull_normal(p) * (band.thickness * (0.35F + 0.65F * taper))));
    }
  }
  for (auto const& p : positions) {
    out.add(p, skull_normal(p), k_band_tone);
  }
  auto const stride = static_cast<unsigned int>(rows + 1);
  for (int c = 0; c < columns; ++c) {
    for (int r = 0; r < rows; ++r) {
      unsigned int const i0 =
          base + (static_cast<unsigned int>(c) * stride) + static_cast<unsigned int>(r);
      unsigned int const i1 = i0 + 1U;
      unsigned int const i2 = i0 + stride;
      unsigned int const i3 = i2 + 1U;
      QVector3D const a = positions[i0 - base];
      QVector3D const b = positions[i1 - base];
      QVector3D const d = positions[i2 - base];
      bool const outward = QVector3D::dotProduct(QVector3D::crossProduct(b - a, d - a),
                                                 skull_normal(a)) > 0.0F;
      if (outward) {
        out.indices.insert(out.indices.end(), {i0, i1, i2, i2, i1, i3});
      } else {
        out.indices.insert(out.indices.end(), {i0, i2, i1, i2, i3, i1});
      }
    }
  }
}

struct Lock {
  QVector3D root;
  QVector3D root_normal;
  QVector3D tip;
  float width;
  float flatness;
  float droop;
  float tone;
};

auto bezier(const QVector3D& p0,
            const QVector3D& p1,
            const QVector3D& p2,
            const QVector3D& p3,
            float t) -> QVector3D {
  float const u = 1.0F - t;
  return (p0 * (u * u * u)) + (p1 * (3.0F * u * u * t)) + (p2 * (3.0F * u * t * t)) +
         (p3 * (t * t * t));
}

void append_lock(MeshBuffers& out, const Lock& lock, bool detailed) {
  int const segments = detailed ? 7 : 3;
  int const sides = detailed ? 6 : 4;
  float const length = (lock.tip - lock.root).length();
  QVector3D const c1 = lock.root + (lock.root_normal * (length * 0.30F)) +
                       QVector3D(0.0F, -length * 0.10F, 0.0F);
  QVector3D const c2 = lock.tip + QVector3D(0.0F, length * lock.droop, length * 0.06F);

  std::vector<QVector3D> spine;
  for (int i = 0; i <= segments; ++i) {
    QVector3D p = bezier(lock.root,
                         c1,
                         c2,
                         lock.tip,
                         static_cast<float>(i) / static_cast<float>(segments));
    if (p.y() < k_chest_top_y && p.z() < k_chest_clearance_z) {
      p.setZ(k_chest_clearance_z);
    }
    spine.push_back(p);
  }

  auto const base = static_cast<unsigned int>(out.vertices.size());
  for (int i = 0; i <= segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(segments);
    QVector3D const prev = spine[static_cast<std::size_t>(std::max(0, i - 1))];
    QVector3D const next = spine[static_cast<std::size_t>(std::min(segments, i + 1))];
    QVector3D const tangent = (next - prev).normalized();
    QVector3D outward = spine[static_cast<std::size_t>(i)] - k_cranium_center;
    outward.setY(0.0F);
    outward =
        (outward - (tangent * QVector3D::dotProduct(outward, tangent))).normalized();
    QVector3D const side = QVector3D::crossProduct(tangent, outward).normalized();
    float const half_width = lock.width * std::pow(1.0F - (t * 0.94F), 0.75F);
    float const half_depth = half_width * lock.flatness;
    for (int k = 0; k < sides; ++k) {
      float const a = (2.0F * k_pi * static_cast<float>(k)) / static_cast<float>(sides);
      QVector3D const radial = (side * std::cos(a)) + (outward * std::sin(a));
      QVector3D const p = spine[static_cast<std::size_t>(i)] +
                          (side * std::cos(a) * half_width) +
                          (outward * std::sin(a) * half_depth);
      QVector3D const n = (side * (std::cos(a) / std::max(half_width, 1.0e-4F))) +
                          (outward * (std::sin(a) / std::max(half_depth, 1.0e-4F)));
      out.add(p, n.lengthSquared() > 0.0F ? n : radial, lock.tone);
    }
  }
  auto const tip_index = static_cast<unsigned int>(out.vertices.size());
  out.add(lock.tip +
              ((lock.tip - spine[static_cast<std::size_t>(segments - 1)]).normalized() *
               lock.width * 0.4F),
          lock.tip - spine[static_cast<std::size_t>(segments - 1)],
          lock.tone);

  auto const ring = [&](int i, int k) {
    return base + static_cast<unsigned int>((i * sides) + (k % sides));
  };
  for (int i = 0; i < segments; ++i) {
    for (int k = 0; k < sides; ++k) {
      unsigned int const a = ring(i, k);
      unsigned int const b = ring(i, k + 1);
      unsigned int const c = ring(i + 1, k);
      unsigned int const d = ring(i + 1, k + 1);
      out.indices.insert(out.indices.end(), {a, c, b, b, c, d});
    }
  }
  for (int k = 0; k < sides; ++k) {
    out.indices.insert(out.indices.end(),
                       {ring(segments, k), tip_index, ring(segments, k + 1)});
  }

  for (std::size_t tri = out.indices.size() -
                         static_cast<std::size_t>(segments * sides * 6 + sides * 3);
       tri < out.indices.size();
       tri += 3U) {
    auto const& va = out.vertices[out.indices[tri]];
    auto const& vb = out.vertices[out.indices[tri + 1U]];
    auto const& vc = out.vertices[out.indices[tri + 2U]];
    QVector3D const pa(va.position[0], va.position[1], va.position[2]);
    QVector3D const pb(vb.position[0], vb.position[1], vb.position[2]);
    QVector3D const pc(vc.position[0], vc.position[1], vc.position[2]);
    QVector3D const na(va.normal[0], va.normal[1], va.normal[2]);
    if (QVector3D::dotProduct(QVector3D::crossProduct(pb - pa, pc - pa), na) < 0.0F) {
      std::swap(out.indices[tri + 1U], out.indices[tri + 2U]);
    }
  }
}

struct ClumpShape {
  int clumps;
  int locks_per_clump;
  float azimuth_half_range;
  float root_top_y;
  float root_bottom_y;
  float tip_center_y;
  float tip_side_rise;
  float tip_z;
  float convergence;
  float width;
  float flatness;
  float azimuth_min;
};

void append_clumps(MeshBuffers& out,
                   const ClumpShape& shape,
                   bool detailed,
                   std::uint32_t seed) {
  int const clumps = detailed ? shape.clumps : std::max(3, (shape.clumps + 1) / 2);
  int const per_clump = detailed ? shape.locks_per_clump : 1;
  for (int c = 0; c < clumps; ++c) {
    float const s =
        clumps > 1
            ? ((2.0F * static_cast<float>(c)) / static_cast<float>(clumps - 1)) - 1.0F
            : 0.0F;
    std::uint32_t const clump_seed = seed + (static_cast<std::uint32_t>(c) * 977U);
    float const clump_tip_y = shape.tip_center_y + (shape.tip_side_rise * s * s) +
                              ((hash01(clump_seed + 3U) - 0.5F) * 0.18F);
    for (int l = 0; l < per_clump; ++l) {
      std::uint32_t const lock_seed =
          clump_seed + (static_cast<std::uint32_t>(l) * 53U);
      float const spread =
          per_clump > 1
              ? ((static_cast<float>(l) / static_cast<float>(per_clump - 1)) - 0.5F)
              : 0.0F;
      float const side = s < 0.0F ? -1.0F : 1.0F;
      float const along =
          shape.azimuth_min +
          (std::abs(s) * (shape.azimuth_half_range - shape.azimuth_min));
      float const azimuth =
          (side * along) + (spread * (shape.azimuth_half_range - shape.azimuth_min) *
                            1.6F / static_cast<float>(clumps));
      float const root_y =
          shape.root_top_y +
          ((shape.root_bottom_y - shape.root_top_y) * hash01(lock_seed + 1U));
      QVector3D root = skull_point(azimuth, elevation_for_height(root_y));
      if (in_lip_zone(root)) {
        root =
            skull_point(azimuth, elevation_for_height(k_lip_clearance_bottom - 0.02F));
      }
      QVector3D const normal = skull_normal(root);
      root -= normal * 0.015F;

      float const tip_y = clump_tip_y + ((hash01(lock_seed + 5U) - 0.5F) * 0.14F);
      QVector3D const tip(root.x() * shape.convergence +
                              ((hash01(lock_seed + 7U) - 0.5F) * shape.width * 1.2F),
                          tip_y,
                          std::max(shape.tip_z - (0.12F * s * s) +
                                       ((hash01(lock_seed + 9U) - 0.5F) * 0.06F),
                                   root.z() * 0.5F));
      Lock lock{};
      lock.root = root;
      lock.root_normal = normal;
      lock.tip = tip;
      lock.width = shape.width * (0.75F + (0.5F * hash01(lock_seed + 11U)));
      lock.flatness = shape.flatness;
      lock.droop = 0.18F + (0.12F * hash01(lock_seed + 13U));
      lock.tone = tone_for(lock_seed);
      append_lock(out, lock, detailed);
    }
  }
}

struct BeardRecipe {
  BandShape band;
  ClumpShape clumps;
  ClumpShape cheeks;
  bool cheeks_enabled;
};

auto recipe_for(HumanoidBodyVariant variant) -> BeardRecipe {
  BeardRecipe r{};
  r.band = {1.30F, k_lip_clearance_bottom, -0.24F, -1.20F, 0.028F};
  r.cheeks_enabled = true;
  switch (variant) {
  case HumanoidBodyVariant::FullBeard:
    r.clumps = {
        9, 4, 0.95F, -0.66F, -1.02F, -1.88F, 0.55F, 1.02F, 0.48F, 0.088F, 0.40F, 0.0F};
    r.cheeks = {
        8, 3, 1.25F, -0.30F, -0.80F, -1.30F, 0.12F, 0.94F, 0.70F, 0.070F, 0.40F, 0.58F};
    break;
  case HumanoidBodyVariant::ShortBeard:
    r.clumps = {
        9, 3, 1.05F, -0.66F, -1.00F, -1.20F, 0.12F, 0.86F, 0.80F, 0.055F, 0.45F, 0.0F};
    r.cheeks = {
        6, 3, 1.25F, -0.30F, -0.80F, -1.05F, 0.10F, 0.84F, 0.85F, 0.045F, 0.45F, 0.62F};
    break;
  case HumanoidBodyVariant::MustacheBeard:
    r.clumps = {
        8, 3, 1.00F, -0.66F, -1.00F, -1.32F, 0.18F, 0.90F, 0.70F, 0.060F, 0.42F, 0.0F};
    r.cheeks = {
        6, 3, 1.25F, -0.30F, -0.80F, -1.10F, 0.10F, 0.86F, 0.80F, 0.048F, 0.42F, 0.62F};
    break;
  case HumanoidBodyVariant::LongGoatee:
    r.band = {0.42F, k_lip_clearance_bottom, k_lip_clearance_bottom, -1.20F, 0.024F};
    r.clumps = {
        3, 4, 0.20F, -0.68F, -1.00F, -1.62F, 0.10F, 0.98F, 0.30F, 0.060F, 0.45F, 0.0F};
    r.cheeks_enabled = false;
    break;
  case HumanoidBodyVariant::Clean:
  case HumanoidBodyVariant::Count:
    break;
  }
  return r;
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
  BeardRecipe const recipe = recipe_for(variant);
  auto const seed = static_cast<std::uint32_t>(variant) * 0x9E3779B9U;

  MeshBuffers out;
  append_band(out, recipe.band, detailed, seed);
  append_clumps(out, recipe.clumps, detailed, seed + 1U);
  if (recipe.cheeks_enabled) {
    append_clumps(out, recipe.cheeks, detailed, seed + 2U);
  }
  return std::make_unique<Render::GL::Mesh>(out.vertices, out.indices);
}

} // namespace Render::Humanoid
