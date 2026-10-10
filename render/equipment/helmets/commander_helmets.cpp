#include "commander_helmets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "helmet_alignment.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_archetype_helpers.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/helmets/commander_helmet_parts.h"
#include "render/equipment/humanoid_attachment_archetype.h"

namespace Render::GL {
namespace {

using namespace Render::GL::CommanderHelmetParts;

using Primitive = GeneratedEquipmentPrimitive;

void add_lobed_mass(std::vector<Primitive>& primitives,
                    std::span<const QVector3D> spine,
                    float root_radius,
                    float tip_radius,
                    std::uint8_t slot,
                    float lateral_scale = 1.0F) {
  if (spine.size() < 2U) {
    return;
  }
  float total = 0.0F;
  for (std::size_t i = 1U; i < spine.size(); ++i) {
    total += (spine[i] - spine[i - 1U]).length();
  }
  if (total <= 1.0e-4F) {
    return;
  }

  std::vector<float> stops;
  stops.reserve(32U);
  for (float travelled = 0.0F; travelled < total && stops.size() < 26U;) {
    float const t = travelled / total;
    stops.push_back(t);
    travelled +=
        std::max(0.03F, 0.42F * (root_radius + ((tip_radius - root_radius) * t)));
  }
  stops.push_back(1.0F);
  for (float const t : stops) {
    float remaining = t * total;
    QVector3D point = spine.back();
    for (std::size_t k = 1U; k < spine.size(); ++k) {
      float const segment = (spine[k] - spine[k - 1U]).length();
      if (remaining <= segment || k + 1U == spine.size()) {
        float const u =
            segment > 1.0e-4F ? std::clamp(remaining / segment, 0.0F, 1.0F) : 0.0F;
        point = spine[k - 1U] + ((spine[k] - spine[k - 1U]) * u);
        break;
      }
      remaining -= segment;
    }
    float const r = root_radius + ((tip_radius - root_radius) * t);
    primitives.push_back(
        generated_ellipsoid(point, QVector3D(r * lateral_scale, r, r), slot, 1.0F, 0));
  }
}

void add_base_helmet(std::vector<Primitive>& primitives, bool face_guard) {
  append_specs(primitives, std::span{k_base_helmet_primitives});

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    primitives.push_back(generated_cylinder(QVector3D(s * 0.52F, -0.24F, 1.22F),
                                            QVector3D(s * 1.16F, -0.40F, 0.34F),
                                            0.11F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
  }

  primitives.push_back(generated_cylinder(QVector3D(0.0F, -0.06F, 1.60F),
                                          QVector3D(0.0F, -0.80F, 1.44F),
                                          face_guard ? 0.16F : 0.09F,
                                          k_accent_slot,
                                          1.0F,
                                          2));

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    primitives.push_back(generated_cylinder(QVector3D(s * 1.14F, -0.30F, 0.32F),
                                            QVector3D(s * 1.32F, -0.35F, 0.27F),
                                            0.60F,
                                            k_metal_slot,
                                            1.0F,
                                            2));
    primitives.push_back(generated_cylinder(QVector3D(s * 1.02F, -0.92F, 0.48F),
                                            QVector3D(s * 1.20F, -0.97F, 0.43F),
                                            0.46F,
                                            k_metal_slot,
                                            1.0F,
                                            2));
    primitives.push_back(generated_cone(QVector3D(s * 0.94F, -1.18F, 0.56F),
                                        QVector3D(s * 0.70F, -1.88F, 0.62F),
                                        0.30F,
                                        k_metal_slot,
                                        1.0F,
                                        2));
    primitives.push_back(generated_sphere(
        QVector3D(s * 1.36F, -0.18F, 0.26F), 0.16F, k_accent_slot, 1.0F, 2));
    primitives.push_back(generated_sphere(
        QVector3D(s * 1.24F, -0.74F, 0.46F), 0.12F, k_dark_slot, 1.0F, 2));
  }
}

void add_roman_base_helmet(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_roman_base_helmet_primitives});

  for (int side : {-1, 1}) {
    float const sign = static_cast<float>(side);

    primitives.push_back(generated_ellipsoid(
        {sign * 1.20F, -0.69F, 0.46F}, {0.13F, 0.75F, 0.46F}, k_metal_slot, 1.0F, 2));
    primitives.push_back(generated_cylinder({sign * 1.27F, -0.32F, 0.84F},
                                            {sign * 1.19F, -1.22F, 0.68F},
                                            0.052F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
    primitives.push_back(
        generated_sphere({sign * 1.35F, -0.15F, 0.40F}, 0.10F, k_accent_slot, 1.0F, 2));

    for (int leaf = 0; leaf < 5; ++leaf) {
      float const t = static_cast<float>(leaf) / 4.0F;
      primitives.push_back(generated_ellipsoid(
          {sign * (0.22F + 0.98F * t), 0.25F + 0.28F * t, 1.48F - 0.57F * t},
          {0.17F, 0.095F, 0.065F},
          k_accent_slot,
          1.0F,
          2));
    }
  }
  primitives.push_back(generated_ellipsoid(
      {0.0F, -0.16F, 1.43F}, {1.06F, 0.07F, 0.12F}, k_accent_slot, 1.0F, 2));
}

void add_crest_hair(std::vector<Primitive>& primitives, bool transverse) {
  for (int strand = 0; strand < 27; ++strand) {
    float const t = static_cast<float>(strand) / 26.0F;
    float const along = (2.0F * t - 1.0F) * 1.48F;
    float const height = 1.75F + 1.04F * std::sin(t * 3.14159265F);
    QVector3D root = transverse ? QVector3D(along * 0.91F, height - 0.34F, -0.04F)
                                : QVector3D(0.0F, height - 0.34F, along - 0.05F);
    QVector3D tip = transverse ? QVector3D(along, height + 0.09F, -0.04F)
                               : QVector3D(0.0F, height + 0.09F, along - 0.05F);
    primitives.push_back(generated_cone(root, tip, 0.082F, k_plume_slot, 1.0F, 0));
  }
}

void add_fabius_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_fabius_crest_primitives});
  add_crest_hair(primitives, false);
}

void add_scipio_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_scipio_crest_primitives});

  add_crest_hair(primitives, true);
}

void add_marcellus_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_marcellus_crest_primitives});
  std::array<QVector3D, 6> const tail{{
      {0.0F, 1.70F, 0.36F},
      {0.0F, 2.02F, -0.04F},
      {0.0F, 2.06F, -0.56F},
      {0.0F, 1.88F, -1.08F},
      {0.0F, 1.56F, -1.54F},
      {0.0F, 1.14F, -1.90F},
  }};
  add_lobed_mass(primitives, tail, 0.46F, 0.20F, k_plume_slot, 0.44F);
}

void add_hanno_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_hanno_crest_primitives});

  std::array<QVector3D, 7> const centre{{
      {0.0F, 1.42F, -0.06F},
      {0.0F, 1.78F, -0.09F},
      {0.0F, 2.14F, -0.12F},
      {0.0F, 2.48F, -0.15F},
      {0.0F, 2.78F, -0.18F},
      {0.0F, 2.94F, -0.21F},
      {0.0F, 3.10F, -0.24F},
  }};
  add_lobed_mass(primitives, centre, 0.58F, 0.26F, k_plume_slot, 0.42F);
  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    std::array<QVector3D, 6> const wing{{
        {s * 0.56F, 1.34F, -0.06F},
        {s * 0.68F, 1.68F, -0.10F},
        {s * 0.80F, 2.00F, -0.14F},
        {s * 0.92F, 2.28F, -0.18F},
        {s * 1.02F, 2.50F, -0.22F},
        {s * 1.10F, 2.68F, -0.26F},
    }};
    add_lobed_mass(primitives, wing, 0.48F, 0.22F, k_accent_slot, 0.42F);
  }

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    primitives.push_back(generated_sphere(
        QVector3D(s * 1.32F, 0.16F, 0.44F), 0.24F, k_accent_slot, 1.0F, 2));
    primitives.push_back(generated_ellipsoid(QVector3D(s * 0.74F, 0.30F, 1.16F),
                                             QVector3D(0.44F, 0.12F, 0.20F),
                                             k_plume_slot,
                                             1.0F,
                                             2));
  }
}

void add_hasdrubal_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_hasdrubal_crest_primitives});

  std::array<QVector3D, 7> const tail{{
      {0.0F, 1.50F, 0.40F},
      {0.0F, 1.86F, 0.02F},
      {0.0F, 1.98F, -0.50F},
      {0.0F, 1.94F, -1.04F},
      {0.0F, 1.76F, -1.52F},
      {0.0F, 1.48F, -1.92F},
      {0.0F, 1.12F, -2.20F},
  }};
  add_lobed_mass(primitives, tail, 0.52F, 0.24F, k_plume_slot, 0.45F);
}

void add_hannibal_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_hannibal_crest_primitives});

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    std::array<QVector3D, 7> const ridge{{
        {s * 0.20F, 1.36F, 0.76F},
        {s * 0.21F, 1.96F, 0.40F},
        {s * 0.23F, 2.22F, -0.14F},
        {s * 0.26F, 2.26F, -0.70F},
        {s * 0.30F, 2.06F, -1.20F},
        {s * 0.34F, 1.74F, -1.62F},
        {s * 0.38F, 1.32F, -1.96F},
    }};
    add_lobed_mass(primitives, ridge, 0.46F, 0.24F, k_plume_slot, 0.46F);
  }
}

void add_side_feathers(std::vector<Primitive>& primitives) {
  for (int side : {-1, 1}) {
    float const s = static_cast<float>(side);
    primitives.push_back(generated_cylinder(QVector3D(s * 1.02F, 0.78F, 0.04F),
                                            QVector3D(s * 1.08F, 1.42F, 0.00F),
                                            0.10F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
    std::array<QVector3D, 6> const feather{{
        {s * 1.08F, 1.30F, 0.02F},
        {s * 1.16F, 1.86F, -0.04F},
        {s * 1.24F, 2.40F, -0.12F},
        {s * 1.30F, 2.90F, -0.24F},
        {s * 1.32F, 3.30F, -0.40F},
        {s * 1.30F, 3.58F, -0.58F},
    }};
    add_lobed_mass(primitives, feather, 0.40F, 0.17F, k_plume_slot, 0.55F);
  }
  primitives.push_back(
      generated_sphere(QVector3D(0.0F, 1.80F, -0.06F), 0.26F, k_accent_slot, 1.0F, 2));
}

void add_triple_feathers(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_box(QVector3D(0.0F, 1.66F, -0.06F),
                                     QVector3D(0.62F, 0.16F, 0.24F),
                                     k_accent_slot,
                                     1.0F,
                                     2));
  for (int index = -1; index <= 1; ++index) {
    float const x = 0.40F * static_cast<float>(index);
    std::array<QVector3D, 6> const feather{{
        {x, 1.72F, -0.06F},
        {x * 1.12F, 2.26F, -0.10F},
        {x * 1.24F, 2.80F, -0.16F},
        {x * 1.36F, 3.28F, -0.26F},
        {x * 1.46F, 3.66F, -0.40F},
        {x * 1.52F, 3.92F, -0.56F},
    }};
    add_lobed_mass(primitives, feather, 0.26F, 0.11F, k_plume_slot, 0.55F);
  }
}

void add_forward_arch_crest(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_box(QVector3D(0.0F, 1.56F, -0.04F),
                                     QVector3D(0.16F, 0.30F, 1.20F),
                                     k_accent_slot,
                                     1.0F,
                                     2));
  std::array<QVector3D, 8> const arch{{
      {0.0F, 1.04F, -1.72F},
      {0.0F, 1.84F, -1.32F},
      {0.0F, 2.42F, -0.70F},
      {0.0F, 2.70F, 0.00F},
      {0.0F, 2.66F, 0.70F},
      {0.0F, 2.38F, 1.34F},
      {0.0F, 1.94F, 1.82F},
      {0.0F, 1.46F, 2.08F},
  }};
  add_lobed_mass(primitives, arch, 0.70F, 0.38F, k_plume_slot, 0.46F);
}

void add_crown_tail(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_cylinder(QVector3D(0.0F, 1.60F, -0.06F),
                                          QVector3D(0.0F, 2.02F, -0.10F),
                                          0.16F,
                                          k_accent_slot,
                                          1.0F,
                                          2));
  primitives.push_back(
      generated_sphere(QVector3D(0.0F, 2.10F, -0.10F), 0.24F, k_accent_slot, 1.0F, 2));
  std::array<QVector3D, 8> const tail{{
      {0.0F, 2.22F, -0.14F},
      {0.0F, 2.30F, -0.56F},
      {0.0F, 2.10F, -1.04F},
      {0.0F, 1.70F, -1.48F},
      {0.0F, 1.18F, -1.86F},
      {0.0F, 0.58F, -2.10F},
      {0.0F, -0.04F, -2.22F},
      {0.0F, -0.56F, -2.26F},
  }};
  add_lobed_mass(primitives, tail, 0.42F, 0.24F, k_plume_slot, 0.66F);
}

void add_fan_crest(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_box(QVector3D(0.0F, 1.52F, -0.05F),
                                     QVector3D(0.14F, 0.26F, 1.10F),
                                     k_accent_slot,
                                     1.0F,
                                     2));
  constexpr float k_pi = 3.14159265F;
  auto arc = [](float radius, float y_lift) {
    std::array<QVector3D, 9> points{};
    for (std::size_t i = 0; i < points.size(); ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(points.size() - 1U);
      float const angle = (0.10F + 0.80F * t) * k_pi;
      points[i] = QVector3D(0.0F,
                            1.50F + y_lift + radius * std::sin(angle),
                            -0.05F + radius * std::cos(angle));
    }
    return points;
  };
  auto const inner = arc(0.78F, 0.0F);
  add_lobed_mass(primitives, inner, 0.46F, 0.46F, k_plume_slot, 0.30F);
  auto const outer = arc(1.28F, 0.0F);
  add_lobed_mass(primitives, outer, 0.24F, 0.24F, k_accent_slot, 0.34F);
}

void add_tall_column_plume(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_cylinder(QVector3D(0.0F, 2.18F, -0.14F),
                                          QVector3D(0.0F, 2.62F, -0.18F),
                                          0.17F,
                                          k_accent_slot,
                                          1.0F,
                                          2));
  std::array<QVector3D, 7> const column{{
      {0.0F, 2.50F, -0.16F},
      {0.0F, 2.96F, -0.20F},
      {0.0F, 3.40F, -0.30F},
      {0.0F, 3.80F, -0.48F},
      {0.0F, 4.10F, -0.76F},
      {0.0F, 4.24F, -1.10F},
      {0.0F, 4.20F, -1.40F},
  }};
  add_lobed_mass(primitives, column, 0.46F, 0.24F, k_plume_slot, 0.62F);
}

void add_horsehair_ridge(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_box(QVector3D(0.0F, 1.80F, -0.10F),
                                     QVector3D(0.16F, 0.30F, 1.24F),
                                     k_accent_slot,
                                     1.0F,
                                     2));
  std::array<QVector3D, 8> const ridge{{
      {0.0F, 1.58F, 1.26F},
      {0.0F, 2.18F, 0.86F},
      {0.0F, 2.56F, 0.30F},
      {0.0F, 2.66F, -0.30F},
      {0.0F, 2.48F, -0.90F},
      {0.0F, 2.06F, -1.42F},
      {0.0F, 1.46F, -1.82F},
      {0.0F, 0.82F, -2.02F},
  }};
  add_lobed_mass(primitives, ridge, 0.48F, 0.34F, k_plume_slot, 0.50F);
}

void add_phrygian_peak(std::vector<Primitive>& primitives) {
  std::array<QVector3D, 6> const peak{{
      {0.0F, 2.04F, -0.12F},
      {0.0F, 2.48F, 0.06F},
      {0.0F, 2.70F, 0.44F},
      {0.0F, 2.62F, 0.86F},
      {0.0F, 2.34F, 1.18F},
      {0.0F, 2.00F, 1.30F},
  }};
  add_lobed_mass(primitives, peak, 0.62F, 0.22F, k_metal_slot, 0.86F);
  primitives.push_back(generated_ellipsoid(QVector3D(0.0F, 1.98F, 1.32F),
                                           QVector3D(0.24F, 0.20F, 0.20F),
                                           k_accent_slot,
                                           1.0F,
                                           2));
  std::array<QVector3D, 5> const plume{{
      {0.0F, 2.30F, -0.46F},
      {0.0F, 2.72F, -0.74F},
      {0.0F, 3.04F, -1.08F},
      {0.0F, 3.20F, -1.44F},
      {0.0F, 3.18F, -1.76F},
  }};
  add_lobed_mass(primitives, plume, 0.28F, 0.14F, k_plume_slot, 0.55F);
}

void add_horn_plumes(std::vector<Primitive>& primitives) {
  for (int side : {-1, 1}) {
    float const s = static_cast<float>(side);
    std::array<QVector3D, 7> const horn{{
        {s * 1.04F, 1.04F, -0.06F},
        {s * 1.40F, 1.54F, -0.10F},
        {s * 1.66F, 2.10F, -0.16F},
        {s * 1.76F, 2.66F, -0.24F},
        {s * 1.66F, 3.16F, -0.32F},
        {s * 1.42F, 3.52F, -0.40F},
        {s * 1.12F, 3.70F, -0.46F},
    }};
    add_lobed_mass(primitives, horn, 0.38F, 0.15F, k_plume_slot, 0.85F);
  }
}

void add_numidian_diadem(std::vector<Primitive>& primitives) {
  constexpr float k_pi = 3.14159265F;

  primitives.push_back(generated_ellipsoid(QVector3D(0.0F, 0.56F, -0.18F),
                                           QVector3D(1.30F, 1.02F, 1.44F),
                                           k_dark_slot,
                                           1.0F,
                                           0));
  constexpr int k_curl_rings = 3;
  for (int ring = 0; ring < k_curl_rings; ++ring) {
    float const elevation = (0.20F + 0.24F * static_cast<float>(ring)) * k_pi;
    int const curls = 14 - 4 * ring;
    for (int curl = 0; curl < curls; ++curl) {
      float const azimuth =
          (static_cast<float>(curl) + 0.5F * static_cast<float>(ring)) * 2.0F * k_pi /
          static_cast<float>(curls);
      float const horizontal = std::cos(elevation);
      primitives.push_back(
          generated_sphere(QVector3D(1.24F * horizontal * std::sin(azimuth),
                                     0.56F + 0.98F * std::sin(elevation),
                                     -0.18F + 1.38F * horizontal * std::cos(azimuth)),
                           0.26F,
                           k_dark_slot,
                           1.0F,
                           0));
    }
  }

  constexpr int k_band_beads = 30;
  for (int bead = 0; bead < k_band_beads; ++bead) {
    float const azimuth =
        static_cast<float>(bead) * 2.0F * k_pi / static_cast<float>(k_band_beads);
    QVector3D const centre(
        1.36F * std::sin(azimuth), 0.40F, -0.18F + 1.50F * std::cos(azimuth));
    primitives.push_back(generated_ellipsoid(
        centre, QVector3D(0.20F, 0.11F, 0.20F), k_plume_slot, 1.0F, 1));
    if (bead % 3 == 0) {
      primitives.push_back(generated_sphere(centre * QVector3D(1.04F, 1.0F, 1.0F) +
                                                QVector3D(0.0F, 0.0F, 0.04F),
                                            0.07F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
    }
  }
  primitives.push_back(generated_ellipsoid(QVector3D(0.0F, 0.42F, 1.36F),
                                           QVector3D(0.20F, 0.20F, 0.08F),
                                           k_accent_slot,
                                           1.0F,
                                           2));
  for (int side : {-1, 1}) {
    float const s = static_cast<float>(side);
    std::array<QVector3D, 5> const ribbon{{
        {s * 0.18F, 0.36F, -1.66F},
        {s * 0.26F, 0.02F, -1.78F},
        {s * 0.32F, -0.36F, -1.82F},
        {s * 0.36F, -0.74F, -1.80F},
        {s * 0.38F, -1.10F, -1.74F},
    }};
    add_lobed_mass(primitives, ribbon, 0.14F, 0.11F, k_plume_slot, 0.85F);
  }
}

auto is_roman_commander_helmet(CommanderHelmetStyle style) -> bool {
  switch (style) {
  case CommanderHelmetStyle::Fabius:
  case CommanderHelmetStyle::Scipio:
  case CommanderHelmetStyle::Marcellus:
  case CommanderHelmetStyle::Sempronius:
  case CommanderHelmetStyle::Flaminius:
  case CommanderHelmetStyle::Varro:
  case CommanderHelmetStyle::Paullus:
  case CommanderHelmetStyle::ScipioElder:
    return true;
  default:
    return false;
  }
}

auto build_commander_helmet(CommanderHelmetStyle style,
                            std::string_view debug_name) -> RenderArchetype {
  std::vector<Primitive> primitives;
  primitives.reserve(96U);
  if (style == CommanderHelmetStyle::Masinissa) {
    add_numidian_diadem(primitives);
  } else if (is_roman_commander_helmet(style)) {
    add_roman_base_helmet(primitives);
  } else {
    add_base_helmet(primitives,
                    style == CommanderHelmetStyle::Hannibal ||
                        style == CommanderHelmetStyle::HasdrubalCavalry);
  }
  switch (style) {
  case CommanderHelmetStyle::Fabius:
    add_fabius_crest(primitives);
    break;
  case CommanderHelmetStyle::Scipio:
    add_scipio_crest(primitives);
    break;
  case CommanderHelmetStyle::Marcellus:
    add_marcellus_crest(primitives);
    break;
  case CommanderHelmetStyle::Hanno:
    add_hanno_crest(primitives);
    break;
  case CommanderHelmetStyle::Hasdrubal:
    add_hasdrubal_crest(primitives);
    break;
  case CommanderHelmetStyle::Hannibal:
    add_hannibal_crest(primitives);
    break;
  case CommanderHelmetStyle::Sempronius:
    add_side_feathers(primitives);
    break;
  case CommanderHelmetStyle::Flaminius:
    add_triple_feathers(primitives);
    break;
  case CommanderHelmetStyle::Varro:
    add_forward_arch_crest(primitives);
    break;
  case CommanderHelmetStyle::Paullus:
    add_crown_tail(primitives);
    break;
  case CommanderHelmetStyle::ScipioElder:
    add_fan_crest(primitives);
    break;
  case CommanderHelmetStyle::Mago:
    add_tall_column_plume(primitives);
    break;
  case CommanderHelmetStyle::Maharbal:
    add_horsehair_ridge(primitives);
    break;
  case CommanderHelmetStyle::HannoBomilcar:
    add_phrygian_peak(primitives);
    break;
  case CommanderHelmetStyle::HasdrubalCavalry:
    add_horn_plumes(primitives);
    break;
  case CommanderHelmetStyle::Masinissa:
    break;
  }
  return build_generated_equipment_archetype(debug_name, primitives);
}

auto commander_colors(CommanderHelmetStyle style, const HumanoidPalette& palette)
    -> std::array<QVector3D, k_commander_helmet_role_count> {
  (void)palette;
  switch (style) {
  case CommanderHelmetStyle::Fabius:
    return {QVector3D(0.36F, 0.38F, 0.40F),
            QVector3D(0.17F, 0.18F, 0.21F),
            QVector3D(0.78F, 0.60F, 0.31F),
            QVector3D(0.40F, 0.025F, 0.035F)};
  case CommanderHelmetStyle::Scipio:
    return {QVector3D(0.58F, 0.49F, 0.31F),
            QVector3D(0.20F, 0.16F, 0.105F),
            QVector3D(0.90F, 0.66F, 0.22F),
            QVector3D(0.54F, 0.025F, 0.05F)};
  case CommanderHelmetStyle::Marcellus:
    return {QVector3D(0.29F, 0.31F, 0.34F),
            QVector3D(0.14F, 0.15F, 0.18F),
            QVector3D(0.70F, 0.50F, 0.24F),
            QVector3D(0.62F, 0.045F, 0.025F)};
  case CommanderHelmetStyle::Hanno:
    return {QVector3D(0.48F, 0.39F, 0.24F),
            QVector3D(0.16F, 0.13F, 0.095F),
            QVector3D(0.84F, 0.58F, 0.20F),
            QVector3D(0.29F, 0.045F, 0.31F)};
  case CommanderHelmetStyle::Hasdrubal:
    return {QVector3D(0.36F, 0.31F, 0.22F),
            QVector3D(0.10F, 0.16F, 0.17F),
            QVector3D(0.32F, 0.52F, 0.44F),
            QVector3D(0.19F, 0.04F, 0.24F)};
  case CommanderHelmetStyle::Hannibal:
    return {QVector3D(0.38F, 0.31F, 0.22F),
            QVector3D(0.075F, 0.08F, 0.095F),
            QVector3D(0.78F, 0.48F, 0.14F),
            QVector3D(0.055F, 0.055F, 0.070F)};
  case CommanderHelmetStyle::Sempronius:
    return {QVector3D(0.55F, 0.44F, 0.26F),
            QVector3D(0.18F, 0.14F, 0.10F),
            QVector3D(0.80F, 0.62F, 0.28F),
            QVector3D(0.92F, 0.90F, 0.84F)};
  case CommanderHelmetStyle::Flaminius:
    return {QVector3D(0.42F, 0.43F, 0.45F),
            QVector3D(0.12F, 0.12F, 0.13F),
            QVector3D(0.62F, 0.46F, 0.22F),
            QVector3D(0.07F, 0.05F, 0.09F)};
  case CommanderHelmetStyle::Varro:
    return {QVector3D(0.78F, 0.60F, 0.24F),
            QVector3D(0.22F, 0.16F, 0.08F),
            QVector3D(0.95F, 0.78F, 0.36F),
            QVector3D(0.40F, 0.06F, 0.46F)};
  case CommanderHelmetStyle::Paullus:
    return {QVector3D(0.66F, 0.67F, 0.70F),
            QVector3D(0.20F, 0.20F, 0.22F),
            QVector3D(0.85F, 0.85F, 0.88F),
            QVector3D(0.86F, 0.66F, 0.22F)};
  case CommanderHelmetStyle::ScipioElder:
    return {QVector3D(0.50F, 0.40F, 0.24F),
            QVector3D(0.15F, 0.12F, 0.09F),
            QVector3D(0.88F, 0.86F, 0.80F),
            QVector3D(0.06F, 0.06F, 0.07F)};
  case CommanderHelmetStyle::Mago:
    return {QVector3D(0.52F, 0.40F, 0.22F),
            QVector3D(0.14F, 0.10F, 0.07F),
            QVector3D(0.88F, 0.64F, 0.22F),
            QVector3D(0.62F, 0.04F, 0.06F)};
  case CommanderHelmetStyle::Maharbal:
    return {QVector3D(0.58F, 0.48F, 0.28F),
            QVector3D(0.15F, 0.12F, 0.08F),
            QVector3D(0.30F, 0.22F, 0.12F),
            QVector3D(0.80F, 0.32F, 0.08F)};
  case CommanderHelmetStyle::HannoBomilcar:
    return {QVector3D(0.46F, 0.40F, 0.30F),
            QVector3D(0.12F, 0.12F, 0.16F),
            QVector3D(0.76F, 0.74F, 0.72F),
            QVector3D(0.12F, 0.20F, 0.62F)};
  case CommanderHelmetStyle::HasdrubalCavalry:
    return {QVector3D(0.24F, 0.25F, 0.27F),
            QVector3D(0.08F, 0.08F, 0.09F),
            QVector3D(0.66F, 0.44F, 0.18F),
            QVector3D(0.90F, 0.88F, 0.82F)};
  case CommanderHelmetStyle::Masinissa:
    return {QVector3D(0.80F, 0.64F, 0.30F),
            QVector3D(0.045F, 0.035F, 0.030F),
            QVector3D(0.92F, 0.74F, 0.30F),
            QVector3D(0.93F, 0.92F, 0.88F)};
  }
  return {QVector3D(0.52F, 0.55F, 0.58F),
          QVector3D(0.20F, 0.20F, 0.22F),
          QVector3D(0.62F, 0.42F, 0.16F),
          QVector3D(0.55F, 0.10F, 0.09F)};
}

} // namespace

auto commander_helmet_archetype(CommanderHelmetStyle style) -> const RenderArchetype& {
  static const RenderArchetype fabius =
      build_commander_helmet(CommanderHelmetStyle::Fabius, "commander_helmet_fabius");
  static const RenderArchetype scipio =
      build_commander_helmet(CommanderHelmetStyle::Scipio, "commander_helmet_scipio");
  static const RenderArchetype marcellus = build_commander_helmet(
      CommanderHelmetStyle::Marcellus, "commander_helmet_marcellus");
  static const RenderArchetype hanno =
      build_commander_helmet(CommanderHelmetStyle::Hanno, "commander_helmet_hanno");
  static const RenderArchetype hasdrubal = build_commander_helmet(
      CommanderHelmetStyle::Hasdrubal, "commander_helmet_hasdrubal");
  static const RenderArchetype hannibal = build_commander_helmet(
      CommanderHelmetStyle::Hannibal, "commander_helmet_hannibal");
  static const RenderArchetype sempronius = build_commander_helmet(
      CommanderHelmetStyle::Sempronius, "commander_helmet_sempronius");
  static const RenderArchetype flaminius = build_commander_helmet(
      CommanderHelmetStyle::Flaminius, "commander_helmet_flaminius");
  static const RenderArchetype varro =
      build_commander_helmet(CommanderHelmetStyle::Varro, "commander_helmet_varro");
  static const RenderArchetype paullus =
      build_commander_helmet(CommanderHelmetStyle::Paullus, "commander_helmet_paullus");
  static const RenderArchetype scipio_elder = build_commander_helmet(
      CommanderHelmetStyle::ScipioElder, "commander_helmet_scipio_elder");
  static const RenderArchetype mago =
      build_commander_helmet(CommanderHelmetStyle::Mago, "commander_helmet_mago");
  static const RenderArchetype maharbal = build_commander_helmet(
      CommanderHelmetStyle::Maharbal, "commander_helmet_maharbal");
  static const RenderArchetype hanno_bomilcar = build_commander_helmet(
      CommanderHelmetStyle::HannoBomilcar, "commander_helmet_hanno_bomilcar");
  static const RenderArchetype hasdrubal_cavalry = build_commander_helmet(
      CommanderHelmetStyle::HasdrubalCavalry, "commander_helmet_hasdrubal_cavalry");
  static const RenderArchetype masinissa = build_commander_helmet(
      CommanderHelmetStyle::Masinissa, "commander_helmet_masinissa");
  switch (style) {
  case CommanderHelmetStyle::Fabius:
    return fabius;
  case CommanderHelmetStyle::Scipio:
    return scipio;
  case CommanderHelmetStyle::Marcellus:
    return marcellus;
  case CommanderHelmetStyle::Hanno:
    return hanno;
  case CommanderHelmetStyle::Hasdrubal:
    return hasdrubal;
  case CommanderHelmetStyle::Hannibal:
    return hannibal;
  case CommanderHelmetStyle::Sempronius:
    return sempronius;
  case CommanderHelmetStyle::Flaminius:
    return flaminius;
  case CommanderHelmetStyle::Varro:
    return varro;
  case CommanderHelmetStyle::Paullus:
    return paullus;
  case CommanderHelmetStyle::ScipioElder:
    return scipio_elder;
  case CommanderHelmetStyle::Mago:
    return mago;
  case CommanderHelmetStyle::Maharbal:
    return maharbal;
  case CommanderHelmetStyle::HannoBomilcar:
    return hanno_bomilcar;
  case CommanderHelmetStyle::HasdrubalCavalry:
    return hasdrubal_cavalry;
  case CommanderHelmetStyle::Masinissa:
    return masinissa;
  }
  return fabius;
}

auto commander_helmet_fill_role_colors(CommanderHelmetStyle style,
                                       const HumanoidPalette& palette,
                                       QVector3D* out,
                                       std::size_t max) -> std::uint32_t {
  if (out == nullptr || max < k_commander_helmet_role_count) {
    return 0U;
  }
  auto const colors = commander_colors(style, palette);
  for (std::size_t i = 0; i < colors.size(); ++i) {
    out[i] = colors[i];
  }
  return k_commander_helmet_role_count;
}

auto commander_helmet_make_static_attachment(CommanderHelmetStyle style,
                                             std::uint16_t socket_bone_index,
                                             std::uint8_t base_role_byte,
                                             const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float k_head_socket_radius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &commander_helmet_archetype(style),
      .socket_bone_index = socket_bone_index,
      .uniform_scale = k_helmet_uniform_scale,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = k_head_socket_radius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  fill_sequential_role_remap(spec, base_role_byte, k_commander_helmet_role_count);
  return spec;
}

} // namespace Render::GL
