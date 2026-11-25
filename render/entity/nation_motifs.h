#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"

namespace Render::GL::Motif {

namespace Rome {
inline constexpr QVector3D k_tufa{0.40F, 0.36F, 0.31F};
inline constexpr QVector3D k_tufa_dark{0.29F, 0.26F, 0.23F};
inline constexpr QVector3D k_oxblood{0.50F, 0.15F, 0.12F};
inline constexpr QVector3D k_bronze{0.46F, 0.38F, 0.22F};
inline constexpr QVector3D k_verdigris{0.30F, 0.44F, 0.38F};
inline constexpr QVector3D k_ember{0.96F, 0.44F, 0.10F};
inline constexpr QVector3D k_soot{0.13F, 0.11F, 0.10F};
inline constexpr QVector3D k_bone{0.84F, 0.80F, 0.70F};
inline constexpr QVector3D k_raven{0.07F, 0.07F, 0.09F};
inline constexpr QVector3D k_terracotta{0.56F, 0.27F, 0.18F};
inline constexpr QVector3D k_travertine{0.90F, 0.86F, 0.76F};
} // namespace Rome

namespace Carthage {
inline constexpr QVector3D k_basalt{0.21F, 0.20F, 0.21F};
inline constexpr QVector3D k_basalt_light{0.31F, 0.29F, 0.30F};
inline constexpr QVector3D k_blue{0.17F, 0.31F, 0.56F};
inline constexpr QVector3D k_purple{0.40F, 0.14F, 0.38F};
inline constexpr QVector3D k_bronze{0.62F, 0.46F, 0.21F};
inline constexpr QVector3D k_ghost_fire{0.36F, 0.88F, 0.78F};
inline constexpr QVector3D k_bone{0.90F, 0.86F, 0.74F};
inline constexpr QVector3D k_cedar{0.30F, 0.20F, 0.15F};
inline constexpr QVector3D k_whitewash{0.93F, 0.90F, 0.82F};
} // namespace Carthage

inline constexpr auto k_intact = k_building_state_mask_intact;

inline constexpr std::string_view k_fire_label = "motif_fire";
inline constexpr std::string_view k_ghost_fire_label = "motif_fire_ghost";
inline constexpr float k_fire_radius = 0.050F;

[[nodiscard]] inline auto
surface_top(const BuildingArchetypeDesc& desc,
            float x,
            float z,
            float ceiling = std::numeric_limits<float>::max(),
            BuildingStateMask visible = BuildingStateMask::Normal) -> float {
  float best = 0.0F;
  const auto consider = [&](float y) {
    if (y <= ceiling && y > best) {
      best = y;
    }
  };
  for (const BuildingPartDesc& part : desc.parts()) {
    if ((part.states & visible) == BuildingStateMask::None) {
      continue;
    }
    switch (part.kind) {
    case BuildingPartKind::Box:
    case BuildingPartKind::PaletteBox:
      if (std::abs(x - part.point_a.x()) <= part.point_b.x() &&
          std::abs(z - part.point_a.z()) <= part.point_b.z()) {
        consider(part.point_a.y() + part.point_b.y());
      }
      break;
    case BuildingPartKind::RotatedBox:
    case BuildingPartKind::PaletteRotatedBox: {
      QMatrix4x4 turn;
      turn.rotate(part.euler_deg.z(), 0.0F, 0.0F, 1.0F);
      turn.rotate(part.euler_deg.y(), 0.0F, 1.0F, 0.0F);
      turn.rotate(part.euler_deg.x(), 1.0F, 0.0F, 0.0F);
      const QMatrix4x4 back = turn.transposed();
      const QVector3D origin =
          back.mapVector(QVector3D(x - part.point_a.x(), 0.0F, z - part.point_a.z()));
      const QVector3D up = back.mapVector(QVector3D(0.0F, 1.0F, 0.0F));
      float low = -std::numeric_limits<float>::max();
      float high = std::numeric_limits<float>::max();
      bool inside = true;
      for (int axis = 0; axis < 3 && inside; ++axis) {
        const float half = part.point_b[axis];
        if (std::abs(up[axis]) < 1.0e-5F) {
          inside = std::abs(origin[axis]) <= half;
          continue;
        }
        const float a = (-half - origin[axis]) / up[axis];
        const float b = (half - origin[axis]) / up[axis];
        low = std::max(low, std::min(a, b));
        high = std::min(high, std::max(a, b));
      }
      if (inside && low <= high) {
        consider(part.point_a.y() + high);
      }
      break;
    }
    case BuildingPartKind::Cylinder:
    case BuildingPartKind::PaletteCylinder:
    case BuildingPartKind::Cone: {
      const QVector3D axis = part.point_b - part.point_a;
      const float flat = (axis.x() * axis.x()) + (axis.z() * axis.z());
      float t = 0.0F;
      if (flat > 1.0e-8F) {
        t = std::clamp((((x - part.point_a.x()) * axis.x()) +
                        ((z - part.point_a.z()) * axis.z())) /
                           flat,
                       0.0F,
                       1.0F);
      }
      const QVector3D on_axis = part.point_a + (axis * t);
      if (std::hypot(x - on_axis.x(), z - on_axis.z()) <= part.radius) {
        consider(flat > (axis.y() * axis.y())
                     ? on_axis.y() + part.radius
                     : std::max(part.point_a.y(), part.point_b.y()));
      }
      break;
    }
    }
  }
  return best;
}

class Setter {
public:
  Setter(const BuildingArchetypeDesc& desc, BuildingState state)
      : m_desc(&desc)
      , m_visible(state == BuildingState::Normal
                      ? BuildingStateMask::Normal
                      : (state == BuildingState::Damaged
                             ? BuildingStateMask::Damaged
                             : BuildingStateMask::Destroyed)) {}

  [[nodiscard]] auto
  at(float x,
     float z,
     float ceiling = std::numeric_limits<float>::max()) const -> QVector3D {
    return {x, surface_top(*m_desc, x, z, ceiling, m_visible), z};
  }

  [[nodiscard]] auto
  highest(float x0, float x1, float z0, float z1, int steps = 12) const -> QVector3D {
    QVector3D best(0.5F * (x0 + x1), 0.0F, 0.5F * (z0 + z1));
    for (int ix = 0; ix <= steps; ++ix) {
      for (int iz = 0; iz <= steps; ++iz) {
        const QVector3D point =
            at(x0 + ((x1 - x0) * static_cast<float>(ix) / static_cast<float>(steps)),
               z0 + ((z1 - z0) * static_cast<float>(iz) / static_cast<float>(steps)));
        if (point.y() > best.y() + 1.0e-4F) {
          best = point;
        }
      }
    }
    return best;
  }

private:
  const BuildingArchetypeDesc* m_desc;
  BuildingStateMask m_visible;
};

inline void add_plinth(BuildingArchetypeDesc& desc,
                       float x0,
                       float x1,
                       float z0,
                       float z1,
                       float foot,
                       float height,
                       const QVector3D& stone,
                       float proud = 0.035F) {
  BuildingPartMaterial const material(desc, k_building_material_stone);
  const float half_y = height * 0.5F;
  const float mid_y = foot + half_y;
  const float mid_x = (x0 + x1) * 0.5F;
  const float mid_z = (z0 + z1) * 0.5F;
  const float half_x = (x1 - x0) * 0.5F + proud;
  const float half_z = (z1 - z0) * 0.5F + proud;
  const float band = proud + 0.03F;
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(mid_x, mid_y, mid_z + (side * (half_z - band * 0.5F))),
                 QVector3D(half_x, half_y, band * 0.5F),
                 weathered(stone, side > 0.0F ? 3 : 7, 0.06F));
    desc.add_box(QVector3D(mid_x + (side * (half_x - band * 0.5F)), mid_y, mid_z),
                 QVector3D(band * 0.5F, half_y * 0.985F, half_z - band),
                 weathered(stone, side > 0.0F ? 11 : 13, 0.06F));
  }
}

inline void add_roman_tripod_fire(BuildingArchetypeDesc& desc,
                                  const QVector3D& base,
                                  float scale = 1.0F) {
  BuildingPartMaterial const metal(desc, k_building_material_metal);
  const float bowl_y = 0.30F * scale;
  for (int leg = 0; leg < 3; ++leg) {
    const float angle = (2.0943951F * static_cast<float>(leg)) + 0.5F;
    const QVector3D foot(
        std::cos(angle) * 0.085F * scale, 0.0F, std::sin(angle) * 0.085F * scale);
    desc.add_cylinder(base + foot,
                      base + (foot * 0.42F) + QVector3D(0.0F, bowl_y, 0.0F),
                      0.012F * scale,
                      Rome::k_bronze,
                      k_intact);
  }
  desc.add_cylinder(base + QVector3D(0.0F, bowl_y * 0.52F, 0.0F),
                    base + QVector3D(0.0F, (bowl_y * 0.52F) + (0.012F * scale), 0.0F),
                    0.060F * scale,
                    Rome::k_verdigris,
                    k_intact);
  desc.add_cone(base + QVector3D(0.0F, bowl_y + (0.045F * scale), 0.0F),
                base + QVector3D(0.0F, bowl_y - (0.035F * scale), 0.0F),
                0.090F * scale,
                Rome::k_verdigris,
                k_intact);
  desc.add_cylinder(base + QVector3D(0.0F, bowl_y + (0.040F * scale), 0.0F),
                    base + QVector3D(0.0F, bowl_y + (0.058F * scale), 0.0F),
                    0.094F * scale,
                    Rome::k_bronze,
                    k_intact);
  desc.add_cylinder(base + QVector3D(0.0F, bowl_y + (0.050F * scale), 0.0F),
                    base + QVector3D(0.0F, bowl_y + (0.066F * scale), 0.0F),
                    0.074F * scale,
                    Rome::k_soot,
                    k_intact);

  BuildingPartLabel const fire(desc, std::string(k_fire_label));
  desc.add_cone(base + QVector3D(0.0F, bowl_y + (0.060F * scale), 0.0F),
                base + QVector3D(0.0F, bowl_y + (0.200F * scale), 0.0F),
                k_fire_radius * scale,
                Rome::k_ember,
                BuildingStateMask::Normal);
}

inline void add_roman_trophy(BuildingArchetypeDesc& desc,
                             const QVector3D& base,
                             float scale = 1.0F,
                             int salt = 0) {
  const float s = scale;
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.74F * s, 0.0F),
                      0.028F * s,
                      Rome::k_tufa_dark * 1.2F,
                      k_intact);
    desc.add_cylinder(base + QVector3D(-0.20F * s, 0.52F * s, 0.0F),
                      base + QVector3D(0.20F * s, 0.52F * s, 0.0F),
                      0.020F * s,
                      Rome::k_tufa_dark * 1.2F,
                      k_intact);

    for (const float side : {-1.0F, 1.0F}) {
      desc.add_rotated_box(base + QVector3D(0.0F, 0.50F * s, -0.03F * s),
                           QVector3D(0.008F, 0.52F, 0.008F) * s,
                           QVector3D(0.0F, 0.0F, side * 24.0F),
                           Rome::k_tufa_dark * 1.5F,
                           k_intact);
    }
  }
  {
    BuildingPartMaterial const metal(desc, k_building_material_metal);

    desc.add_box(base + QVector3D(0.0F, 0.44F * s, 0.012F * s),
                 QVector3D(0.075F, 0.105F, 0.045F) * s,
                 Rome::k_bronze,
                 k_intact);
    desc.add_box(base + QVector3D(0.0F, 0.325F * s, 0.012F * s),
                 QVector3D(0.082F, 0.022F, 0.050F) * s,
                 Rome::k_oxblood,
                 k_intact);
    desc.add_cylinder(base + QVector3D(0.0F, 0.72F * s, 0.0F),
                      base + QVector3D(0.0F, 0.80F * s, 0.0F),
                      0.052F * s,
                      Rome::k_verdigris,
                      k_intact);
    desc.add_cone(base + QVector3D(0.0F, 0.80F * s, 0.0F),
                  base + QVector3D(0.0F, 0.86F * s, 0.0F),
                  0.052F * s,
                  Rome::k_verdigris,
                  k_intact);
    desc.add_box(base + QVector3D(0.0F, 0.89F * s, 0.0F),
                 QVector3D(0.010F, 0.045F, 0.060F) * s,
                 Rome::k_oxblood,
                 k_intact);
  }

  for (const float side : {-1.0F, 1.0F}) {
    const QVector3D boss = base + QVector3D(side * 0.19F * s, 0.44F * s, 0.02F * s);
    desc.add_cylinder(boss,
                      boss + QVector3D(0.0F, 0.0F, 0.018F * s),
                      0.098F * s,
                      side > 0.0F ? Rome::k_oxblood : Rome::k_tufa * 1.25F,
                      k_intact);
    desc.add_cylinder(boss + QVector3D(0.0F, 0.0F, 0.008F * s),
                      boss + QVector3D(0.0F, 0.0F, 0.032F * s),
                      0.030F * s,
                      Rome::k_bronze,
                      k_intact);
  }

  const QVector3D skull =
      base +
      QVector3D((0.10F + (0.04F * decay_hash(salt + 5))) * s, 0.035F * s, 0.07F * s);
  desc.add_box(skull, QVector3D(0.034F, 0.030F, 0.038F) * s, Rome::k_bone, k_intact);
  desc.add_box(skull + QVector3D(0.0F, -0.022F * s, 0.012F * s),
               QVector3D(0.024F, 0.012F, 0.022F) * s,
               Rome::k_bone * 0.9F,
               k_intact);
  desc.add_box(skull + QVector3D(0.0F, 0.004F * s, 0.036F * s),
               QVector3D(0.022F, 0.008F, 0.004F) * s,
               Rome::k_soot,
               k_intact);
}

inline void add_roman_acroterion(BuildingArchetypeDesc& desc,
                                 const QVector3D& base,
                                 float scale = 1.0F,
                                 bool along_x = true) {
  BuildingPartMaterial const stone(desc, k_building_material_stone);
  const float s = scale;
  desc.add_box(base + QVector3D(0.0F, 0.020F * s, 0.0F),
               QVector3D(0.050F, 0.020F, 0.050F) * s,
               Rome::k_tufa,
               k_intact);
  const auto leaf = [&](float lean, float height, const QVector3D& colour) {
    const QVector3D across =
        along_x ? QVector3D(0.0F, 0.0F, 1.0F) : QVector3D(1.0F, 0.0F, 0.0F);
    const float rad = lean * 0.0174533F;
    desc.add_rotated_box(
        base + (across * (std::sin(rad) * height * 0.5F * s)) +
            QVector3D(0.0F, (0.04F + (std::cos(rad) * height * 0.5F)) * s, 0.0F),
        (along_x ? QVector3D(0.014F, height * 0.5F, 0.030F)
                 : QVector3D(0.030F, height * 0.5F, 0.014F)) *
            s,
        along_x ? QVector3D(lean, 0.0F, 0.0F) : QVector3D(0.0F, 0.0F, -lean),
        colour,
        k_intact);
  };
  leaf(0.0F, 0.20F, Rome::k_travertine);
  leaf(32.0F, 0.15F, Rome::k_travertine * 0.93F);
  leaf(-32.0F, 0.15F, Rome::k_travertine * 0.93F);
  desc.add_box(base + QVector3D(0.0F, 0.075F * s, 0.0F),
               QVector3D(0.026F, 0.026F, 0.026F) * s,
               Rome::k_oxblood,
               k_intact);
}

inline void add_roman_raven(BuildingArchetypeDesc& desc,
                            const QVector3D& base,
                            float yaw_degrees = 0.0F,
                            float scale = 1.0F) {
  const float s = scale;
  const float rad = yaw_degrees * 0.0174533F;
  const QVector3D fwd(std::sin(rad), 0.0F, std::cos(rad));
  desc.add_rotated_box(base + QVector3D(0.0F, 0.050F * s, 0.0F),
                       QVector3D(0.026F, 0.030F, 0.052F) * s,
                       QVector3D(-18.0F, yaw_degrees, 0.0F),
                       Rome::k_raven,
                       k_intact);
  desc.add_box(base + (fwd * (0.050F * s)) + QVector3D(0.0F, 0.088F * s, 0.0F),
               QVector3D(0.020F, 0.020F, 0.022F) * s,
               Rome::k_raven,
               k_intact);
  desc.add_cone(base + (fwd * (0.068F * s)) + QVector3D(0.0F, 0.086F * s, 0.0F),
                base + (fwd * (0.110F * s)) + QVector3D(0.0F, 0.078F * s, 0.0F),
                0.010F * s,
                Rome::k_tufa_dark,
                k_intact);
  desc.add_rotated_box(base - (fwd * (0.062F * s)) + QVector3D(0.0F, 0.040F * s, 0.0F),
                       QVector3D(0.016F, 0.008F, 0.038F) * s,
                       QVector3D(14.0F, yaw_degrees, 0.0F),
                       Rome::k_raven,
                       k_intact);
}

inline void add_roman_bucranium(BuildingArchetypeDesc& desc,
                                const QVector3D& center,
                                float scale = 1.0F,
                                float facing = 1.0F) {
  const float s = scale;
  const float out = facing * 0.012F * s;
  desc.add_box(center + QVector3D(0.0F, 0.0F, out),
               QVector3D(0.040F, 0.050F, 0.012F) * s,
               Rome::k_bone,
               k_intact);
  desc.add_box(center + QVector3D(0.0F, -0.062F * s, out),
               QVector3D(0.022F, 0.026F, 0.010F) * s,
               Rome::k_bone * 0.92F,
               k_intact);
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(center + QVector3D(side * 0.070F * s, 0.052F * s, out),
                         QVector3D(0.042F, 0.010F, 0.008F) * s,
                         QVector3D(0.0F, 0.0F, side * 28.0F),
                         Rome::k_bone * 0.85F,
                         k_intact);
    desc.add_box(center + QVector3D(side * 0.016F * s, 0.012F * s, out * 2.1F),
                 QVector3D(0.008F, 0.010F, 0.003F) * s,
                 Rome::k_soot,
                 k_intact);

    desc.add_rotated_box(center + QVector3D(side * 0.150F * s, -0.020F * s, out * 0.8F),
                         QVector3D(0.075F, 0.016F, 0.008F) * s,
                         QVector3D(0.0F, 0.0F, side * -20.0F),
                         Rome::k_oxblood,
                         k_intact);
  }
}

inline void dress_roman_barracks(BuildingArchetypeDesc& desc, BuildingState state) {
  const Setter set(desc, state);
  for (const float side : {-1.0F, 1.0F}) {
    add_roman_trophy(
        desc, set.at(side * 0.95F, 1.70F, 0.4F), 1.45F, side > 0.0F ? 1 : 2);
    add_roman_tripod_fire(desc, set.at(side * 0.42F, 1.74F, 0.4F), 1.05F);
  }
  add_roman_raven(desc, set.highest(-1.6F, 1.6F, -1.6F, 1.4F), -30.0F, 1.5F);
}

inline void add_punic_horns(BuildingArchetypeDesc& desc,
                            const QVector3D& base,
                            float scale = 1.0F,
                            const QVector3D& wash = Carthage::k_whitewash) {
  BuildingPartMaterial const stone(desc, k_building_material_stone);
  const float s = scale;
  desc.add_box(base + QVector3D(0.0F, 0.035F * s, 0.0F),
               QVector3D(0.085F, 0.035F, 0.085F) * s,
               wash,
               k_intact);
  for (const float sx : {-1.0F, 1.0F}) {
    for (const float sz : {-1.0F, 1.0F}) {
      desc.add_cone(base + QVector3D(sx * 0.052F * s, 0.068F * s, sz * 0.052F * s),
                    base + QVector3D(sx * 0.088F * s, 0.215F * s, sz * 0.088F * s),
                    0.034F * s,
                    wash * 0.97F,
                    k_intact);
    }
  }
  desc.add_box(base + QVector3D(0.0F, 0.074F * s, 0.0F),
               QVector3D(0.090F, 0.008F, 0.090F) * s,
               Carthage::k_blue,
               k_intact);
}

inline void add_punic_standard(BuildingArchetypeDesc& desc,
                               const QVector3D& base,
                               float scale = 1.0F) {
  const float s = scale;
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.70F * s, 0.0F),
                      0.014F * s,
                      Carthage::k_cedar,
                      k_intact);
  }
  {
    BuildingPartMaterial const metal(desc, k_building_material_metal);
    const QVector3D head = base + QVector3D(0.0F, 0.80F * s, 0.0F);
    for (int step = -3; step <= 3; ++step) {
      const float angle = static_cast<float>(step) * 0.36F;
      desc.add_rotated_box(head + QVector3D(std::sin(angle) * 0.105F * s,
                                            (0.105F - (std::cos(angle) * 0.105F)) * s,
                                            0.0F),
                           QVector3D(0.022F, 0.014F, 0.012F) * s,
                           QVector3D(0.0F, 0.0F, angle * 57.29578F),
                           Carthage::k_bronze,
                           k_intact);
    }
    desc.add_cylinder(head + QVector3D(0.0F, 0.085F * s, -0.008F * s),
                      head + QVector3D(0.0F, 0.085F * s, 0.008F * s),
                      0.050F * s,
                      Carthage::k_bronze * 1.15F,
                      k_intact);
    desc.add_box(head + QVector3D(0.0F, -0.045F * s, 0.0F),
                 QVector3D(0.050F, 0.010F, 0.010F) * s,
                 Carthage::k_bronze,
                 k_intact);
  }
  BuildingPartMaterial const cloth(desc, k_building_material_cloth);
  desc.add_box(base + QVector3D(0.035F * s, 0.56F * s, 0.0F),
               QVector3D(0.030F, 0.110F, 0.004F) * s,
               Carthage::k_purple,
               BuildingStateMask::Normal);
}

inline void add_punic_incense_stand(BuildingArchetypeDesc& desc,
                                    const QVector3D& base,
                                    float scale = 1.0F) {
  const float s = scale;
  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    desc.add_box(base + QVector3D(0.0F, 0.020F * s, 0.0F),
                 QVector3D(0.060F, 0.020F, 0.060F) * s,
                 Carthage::k_basalt,
                 k_intact);
  }
  BuildingPartMaterial const metal(desc, k_building_material_metal);
  desc.add_cylinder(base + QVector3D(0.0F, 0.040F * s, 0.0F),
                    base + QVector3D(0.0F, 0.40F * s, 0.0F),
                    0.016F * s,
                    Carthage::k_bronze,
                    k_intact);

  for (const float y : {0.20F, 0.31F}) {
    desc.add_cone(base + QVector3D(0.0F, (y + 0.035F) * s, 0.0F),
                  base + QVector3D(0.0F, (y - 0.030F) * s, 0.0F),
                  0.052F * s,
                  Carthage::k_bronze * 0.9F,
                  k_intact);
  }
  desc.add_cone(base + QVector3D(0.0F, 0.455F * s, 0.0F),
                base + QVector3D(0.0F, 0.385F * s, 0.0F),
                0.070F * s,
                Carthage::k_bronze,
                k_intact);
  desc.add_cylinder(base + QVector3D(0.0F, 0.450F * s, 0.0F),
                    base + QVector3D(0.0F, 0.462F * s, 0.0F),
                    0.058F * s,
                    Carthage::k_basalt,
                    k_intact);
  BuildingPartLabel const fire(desc, std::string(k_ghost_fire_label));
  desc.add_cone(base + QVector3D(0.0F, 0.458F * s, 0.0F),
                base + QVector3D(0.0F, 0.640F * s, 0.0F),
                k_fire_radius * s,
                Carthage::k_ghost_fire,
                BuildingStateMask::Normal);
}

inline void add_punic_stele(BuildingArchetypeDesc& desc,
                            const QVector3D& base,
                            float scale = 1.0F,
                            float facing = 1.0F,
                            int salt = 0,
                            bool faces_x = false) {
  BuildingPartMaterial const stone(desc, k_building_material_stone);
  const float s = scale;

  const auto turned = [&](const QVector3D& v) {
    return faces_x ? QVector3D(v.z(), v.y(), v.x()) : v;
  };
  const QVector3D slab = weathered(Carthage::k_basalt_light, salt + 3, 0.10F);
  desc.add_box(base + turned(QVector3D(0.0F, 0.018F * s, 0.0F)),
               turned(QVector3D(0.075F, 0.018F, 0.048F)) * s,
               Carthage::k_basalt,
               k_intact);
  desc.add_box(base + turned(QVector3D(0.0F, 0.150F * s, 0.0F)),
               turned(QVector3D(0.055F, 0.115F, 0.026F)) * s,
               slab,
               k_intact);
  desc.add_rotated_box(base + turned(QVector3D(0.0F, 0.262F * s, 0.0F)),
                       turned(QVector3D(0.039F, 0.039F, 0.026F)) * s,
                       faces_x ? QVector3D(45.0F, 0.0F, 0.0F)
                               : QVector3D(0.0F, 0.0F, 45.0F),
                       slab,
                       k_intact);
  const float out = facing * 0.028F * s;

  desc.add_box(base + turned(QVector3D(0.0F, 0.085F * s, out)),
               turned(QVector3D(0.032F, 0.012F, 0.004F)) * s,
               Carthage::k_whitewash,
               k_intact);
  desc.add_box(base + turned(QVector3D(0.0F, 0.112F * s, out)),
               turned(QVector3D(0.020F, 0.015F, 0.004F)) * s,
               Carthage::k_whitewash,
               k_intact);
  desc.add_box(base + turned(QVector3D(0.0F, 0.140F * s, out)),
               turned(QVector3D(0.036F, 0.006F, 0.004F)) * s,
               Carthage::k_whitewash,
               k_intact);
  desc.add_cylinder(
      base + turned(QVector3D(0.0F, 0.170F * s, out - (facing * 0.004F * s))),
      base + turned(QVector3D(0.0F, 0.170F * s, out + (facing * 0.004F * s))),
      0.016F * s,
      Carthage::k_whitewash,
      k_intact);
}

inline void add_punic_tusks(BuildingArchetypeDesc& desc,
                            const QVector3D& base,
                            float span,
                            float scale = 1.0F) {
  const float s = scale;
  constexpr int k_segments = 5;
  for (const float side : {-1.0F, 1.0F}) {
    for (int segment = 0; segment < k_segments; ++segment) {
      const float t0 = static_cast<float>(segment) / static_cast<float>(k_segments);
      const float t1 = static_cast<float>(segment + 1) / static_cast<float>(k_segments);
      const auto point = [&](float t) {
        const float angle = t * 1.75F;
        return base +
               QVector3D(side * ((span * 0.5F) - (std::sin(angle) * span * 0.62F)),
                         (1.0F - std::cos(angle)) * 0.36F * s,
                         side * 0.012F * s);
      };
      desc.add_cylinder(point(t0),
                        point(t1),
                        (0.040F - (0.030F * t1)) * s,
                        Carthage::k_bone * (1.0F - (0.10F * t0)),
                        k_intact);
    }
  }
}

inline void add_punic_beam_ends(BuildingArchetypeDesc& desc,
                                float x0,
                                float x1,
                                float y,
                                float z,
                                float facing,
                                float pitch = 0.20F) {
  BuildingPartMaterial const timber(desc, k_building_material_wood);
  const int count = std::max(2, static_cast<int>(std::floor((x1 - x0) / pitch)));
  const float step = (x1 - x0) / static_cast<float>(count);
  for (int beam = 0; beam < count; ++beam) {
    const float x = x0 + (step * (static_cast<float>(beam) + 0.5F));
    desc.add_cylinder(QVector3D(x, y, z - (facing * 0.02F)),
                      QVector3D(x, y, z + (facing * 0.055F)),
                      0.020F,
                      weathered(Carthage::k_cedar, beam + 17, 0.10F),
                      k_intact);
  }
}

} // namespace Render::GL::Motif
