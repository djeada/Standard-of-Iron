#include "temple_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"
#include "render/entity/temple_renderer_common.h"
#include "render/render_archetype.h"

namespace Render::GL::Roman {
namespace {

constexpr float k_pi = 3.14159265F;
constexpr float k_rad_to_deg = 180.0F / k_pi;

struct RomanTemplePalette {
  QVector3D marble{0.94F, 0.92F, 0.85F};
  QVector3D marble_shade{0.81F, 0.78F, 0.70F};
  QVector3D limestone{0.84F, 0.78F, 0.66F};
  QVector3D limestone_dark{0.50F, 0.46F, 0.38F};
  QVector3D mortar{0.56F, 0.52F, 0.45F};
  QVector3D terracotta{0.70F, 0.32F, 0.15F};
  QVector3D terracotta_light{0.81F, 0.45F, 0.24F};
  QVector3D terracotta_dark{0.44F, 0.17F, 0.085F};
  QVector3D cloth_red{0.53F, 0.075F, 0.052F};
  QVector3D gold{0.72F, 0.53F, 0.20F};
  QVector3D bronze{0.48F, 0.30F, 0.11F};
  QVector3D blue_accent{0.24F, 0.40F, 0.55F};
  QVector3D soot{0.16F, 0.14F, 0.12F};
  QVector3D flame{0.88F, 0.47F, 0.12F};
};

constexpr std::uint8_t k_temple_team_slot = 1;

constexpr float k_shaft_radius = 0.072F;
constexpr int k_flute_count = 8;

constexpr float k_super_x = 0.02F;
constexpr float k_pediment_half_z = 0.86F;
constexpr float k_pediment_rise = 0.40F;
constexpr float k_roof_half_span_x = 1.215F;
constexpr float k_roof_half_thick = 0.038F;
constexpr float k_roof_overhang = 0.030F;

auto shaft_radius_at(float t) -> float {
  float const taper = 1.0F - (0.155F * t * t);
  float const swell = 1.0F + (0.022F * std::sin(k_pi * t));
  return k_shaft_radius * taper * swell;
}

void add_dentil_row(BuildingArchetypeDesc& desc,
                    const QVector3D& start,
                    const QVector3D& step,
                    int count,
                    const QVector3D& half,
                    const QVector3D& color) {
  for (int i = 0; i < count; ++i) {
    desc.add_box(start + (step * static_cast<float>(i)),
                 half,
                 color,
                 k_building_state_mask_intact);
  }
}

void add_column_base(BuildingArchetypeDesc& desc,
                     const RomanTemplePalette& c,
                     float x,
                     float z,
                     float base_y) {
  desc.add_box(QVector3D(x, base_y + 0.024F, z),
               QVector3D(k_shaft_radius * 1.78F, 0.024F, k_shaft_radius * 1.78F),
               c.marble_shade);
  desc.add_cylinder(QVector3D(x, base_y + 0.048F, z),
                    QVector3D(x, base_y + 0.082F, z),
                    k_shaft_radius * 1.44F,
                    c.marble,
                    BuildingStateMask::All);
  desc.add_cylinder(QVector3D(x, base_y + 0.082F, z),
                    QVector3D(x, base_y + 0.108F, z),
                    k_shaft_radius * 1.18F,
                    c.marble_shade,
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(x, base_y + 0.108F, z),
                    QVector3D(x, base_y + 0.136F, z),
                    k_shaft_radius * 1.34F,
                    c.marble,
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(x, base_y + 0.136F, z),
                    QVector3D(x, base_y + 0.158F, z),
                    k_shaft_radius * 1.10F,
                    c.marble,
                    k_building_state_mask_intact);
}

void add_column_capital(BuildingArchetypeDesc& desc,
                        const RomanTemplePalette& c,
                        float x,
                        float z,
                        float cap_y) {
  desc.add_cylinder(QVector3D(x, cap_y - 0.022F, z),
                    QVector3D(x, cap_y, z),
                    k_shaft_radius * 1.02F,
                    c.marble_shade,
                    k_building_state_mask_intact);

  desc.add_cylinder(QVector3D(x, cap_y, z),
                    QVector3D(x, cap_y + 0.062F, z),
                    k_shaft_radius * 1.12F,
                    c.marble,
                    k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(x, cap_y + 0.062F, z),
                    QVector3D(x, cap_y + 0.112F, z),
                    k_shaft_radius * 1.34F,
                    c.marble,
                    k_building_state_mask_intact);

  for (int leaf = 0; leaf < 6; ++leaf) {
    float const angle = (k_pi * 2.0F * static_cast<float>(leaf)) / 6.0F;
    float const radius = k_shaft_radius * 1.20F;
    desc.add_rotated_box(QVector3D(x + (std::cos(angle) * radius),
                                   cap_y + 0.036F,
                                   z + (std::sin(angle) * radius)),
                         QVector3D(0.020F, 0.040F, 0.026F),
                         QVector3D(0.0F, -angle * k_rad_to_deg, 0.0F),
                         c.marble_shade,
                         k_building_state_mask_intact);
  }

  for (float const sx : {-1.0F, 1.0F}) {
    for (float const sz : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(x + (sx * k_shaft_radius * 1.26F),
                             cap_y + 0.098F,
                             z + (sz * k_shaft_radius * 1.26F)),
                   QVector3D(0.026F, 0.024F, 0.026F),
                   c.marble_shade,
                   k_building_state_mask_intact);
    }
  }

  desc.add_box(QVector3D(x, cap_y + 0.132F, z),
               QVector3D(k_shaft_radius * 1.58F, 0.020F, k_shaft_radius * 1.58F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(x, cap_y + 0.160F, z),
               QVector3D(k_shaft_radius * 1.66F, 0.014F, k_shaft_radius * 1.66F),
               c.marble_shade,
               k_building_state_mask_intact);
}

void add_fluted_column(BuildingArchetypeDesc& desc,
                       const RomanTemplePalette& c,
                       float x,
                       float z,
                       float base_y,
                       float height) {
  add_column_base(desc, c, x, z, base_y);

  float const shaft_bottom = base_y + 0.158F;
  float const shaft_top = base_y + height - 0.196F;
  float const shaft_span = std::max(shaft_top - shaft_bottom, 0.04F);

  constexpr int k_drums = 5;
  for (int drum = 0; drum < k_drums; ++drum) {
    float const t0 = static_cast<float>(drum) / k_drums;
    float const t1 = static_cast<float>(drum + 1) / k_drums;
    desc.add_cylinder(QVector3D(x, shaft_bottom + (shaft_span * t0), z),
                      QVector3D(x, shaft_bottom + (shaft_span * t1) + 0.004F, z),
                      shaft_radius_at((t0 + t1) * 0.5F),
                      c.marble);
  }

  for (int flute = 0; flute < k_flute_count; ++flute) {
    float const angle = (k_pi * 2.0F * static_cast<float>(flute)) / k_flute_count;
    float const radius = k_shaft_radius * 0.90F;
    desc.add_rotated_box(QVector3D(x + (std::cos(angle) * radius),
                                   shaft_bottom + (shaft_span * 0.5F),
                                   z + (std::sin(angle) * radius)),
                         QVector3D(0.018F, shaft_span * 0.47F, 0.011F),
                         QVector3D(0.0F, -angle * k_rad_to_deg, 0.0F),
                         c.marble_shade,
                         k_building_state_mask_intact);
  }

  add_column_capital(desc, c, x, z, shaft_top);
}

void add_votive_altar(BuildingArchetypeDesc& desc,
                      const RomanTemplePalette& c,
                      const QVector3D& base) {
  desc.add_box(base + QVector3D(0.0F, 0.030F, 0.0F),
               QVector3D(0.155F, 0.030F, 0.135F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.070F, 0.0F),
               QVector3D(0.128F, 0.014F, 0.110F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.125F, 0.0F),
               QVector3D(0.112F, 0.048F, 0.095F),
               c.marble_shade,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(base + QVector3D(side * 0.100F, 0.125F, 0.0F),
                 QVector3D(0.014F, 0.044F, 0.086F),
                 c.marble,
                 k_building_state_mask_intact);
  }
  desc.add_box(base + QVector3D(0.0F, 0.186F, 0.0F),
               QVector3D(0.150F, 0.020F, 0.130F),
               c.limestone,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.118F, 0.206F, 0.0F),
                      base + QVector3D(side * 0.118F, 0.244F, 0.0F),
                      0.030F,
                      c.marble,
                      k_building_state_mask_intact);
  }
  desc.add_box(base + QVector3D(0.0F, 0.214F, 0.0F),
               QVector3D(0.085F, 0.012F, 0.070F),
               c.soot,
               k_building_state_mask_intact);
  desc.add_cone(base + QVector3D(0.0F, 0.222F, 0.0F),
                base + QVector3D(0.0F, 0.330F, 0.0F),
                0.052F,
                c.flame,
                BuildingStateMask::Normal);
}

void add_roof_field(BuildingArchetypeDesc& desc,
                    const RomanTemplePalette& c,
                    float eave_y) {
  add_gable_roof_x(
      [&](const QVector3D& center,
          const QVector3D& scale,
          const QVector3D& euler,
          const QVector3D& color) {
        desc.add_rotated_box(center, scale, euler, color, k_building_state_mask_intact);
      },
      k_super_x,
      0.0F,
      eave_y,
      k_roof_half_span_x,
      k_pediment_half_z,
      k_pediment_rise,
      k_roof_half_thick,
      c.terracotta_dark,
      k_roof_overhang);

  float const theta = std::atan2(k_pediment_rise, k_pediment_half_z);
  float const theta_deg = theta * k_rad_to_deg;
  float const slope_half_len = (std::sqrt((k_pediment_half_z * k_pediment_half_z) +
                                          (k_pediment_rise * k_pediment_rise)) *
                                0.5F) +
                               k_roof_overhang;
  float const pan_lift = (k_roof_half_thick + 0.009F) / std::cos(theta);
  float const cover_lift = (k_roof_half_thick + 0.024F) / std::cos(theta);

  constexpr int k_pans = 19;
  constexpr float k_pan_step = 0.1280F;
  float const pan_start = k_super_x - (k_pan_step * (k_pans - 1) * 0.5F);

  for (float const side : {-1.0F, 1.0F}) {
    float const centre_z = side * k_pediment_half_z * 0.5F;
    float const pan_y = eave_y + (k_pediment_rise * 0.5F) + pan_lift;
    float const cover_y = eave_y + (k_pediment_rise * 0.5F) + cover_lift;

    for (int pan = 0; pan < k_pans; ++pan) {
      float const px = pan_start + (k_pan_step * static_cast<float>(pan));
      desc.add_rotated_box(QVector3D(px, pan_y, centre_z),
                           QVector3D(0.058F, 0.009F, slope_half_len),
                           QVector3D(side * theta_deg, 0.0F, 0.0F),
                           c.terracotta,
                           k_building_state_mask_intact);
    }
    for (int cover = 0; cover <= k_pans; ++cover) {
      float const px = pan_start + (k_pan_step * (static_cast<float>(cover) - 0.5F));
      desc.add_rotated_box(QVector3D(px, cover_y, centre_z),
                           QVector3D(0.018F, 0.017F, slope_half_len),
                           QVector3D(side * theta_deg, 0.0F, 0.0F),
                           c.terracotta_light,
                           k_building_state_mask_intact);
    }

    for (int course = 1; course < 5; ++course) {
      float const t = static_cast<float>(course) / 5.0F;
      float const cz = side * k_pediment_half_z * t;
      float const cy = eave_y + (k_pediment_rise * (1.0F - t)) + pan_lift + 0.006F;
      desc.add_rotated_box(
          QVector3D(k_super_x, cy, cz),
          QVector3D(k_roof_half_span_x + k_roof_overhang, 0.010F, 0.014F),
          QVector3D(side * theta_deg, 0.0F, 0.0F),
          c.terracotta_dark,
          k_building_state_mask_intact);
    }

    float const eave_z = side * (k_pediment_half_z + 0.046F);
    desc.add_box(
        QVector3D(k_super_x, eave_y - 0.008F, side * (k_pediment_half_z + 0.024F)),
        QVector3D(k_roof_half_span_x + k_roof_overhang, 0.022F, 0.024F),
        c.terracotta_dark,
        k_building_state_mask_intact);
    for (int antefix = 0; antefix < 9; ++antefix) {
      float const px = k_super_x - 1.140F + (0.285F * static_cast<float>(antefix));
      desc.add_box(QVector3D(px, eave_y + 0.030F, eave_z),
                   QVector3D(0.038F, 0.044F, 0.014F),
                   c.terracotta_light,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(px, eave_y + 0.062F, eave_z),
                   QVector3D(0.020F, 0.020F, 0.012F),
                   c.gold,
                   BuildingStateMask::Normal);
    }
  }

  float const ridge_y = eave_y + k_pediment_rise + 0.022F;
  desc.add_box(QVector3D(k_super_x, ridge_y, 0.0F),
               QVector3D(k_roof_half_span_x + k_roof_overhang, 0.024F, 0.046F),
               c.terracotta,
               k_building_state_mask_intact);
  for (int tile = 0; tile < k_pans; ++tile) {
    float const px = pan_start + (k_pan_step * static_cast<float>(tile));
    desc.add_cylinder(QVector3D(px - 0.052F, ridge_y + 0.030F, 0.0F),
                      QVector3D(px + 0.052F, ridge_y + 0.030F, 0.0F),
                      0.030F,
                      c.terracotta_light,
                      k_building_state_mask_intact);
  }
}

void add_entablature(BuildingArchetypeDesc& desc,
                     const RomanTemplePalette& c,
                     float base_y) {
  desc.add_box(QVector3D(k_super_x, base_y + 0.030F, 0.0F),
               QVector3D(1.215F, 0.030F, 0.835F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_super_x, base_y + 0.090F, 0.0F),
               QVector3D(1.228F, 0.030F, 0.848F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_super_x, base_y + 0.150F, 0.0F),
               QVector3D(1.241F, 0.030F, 0.861F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_super_x, base_y + 0.192F, 0.0F),
               QVector3D(1.248F, 0.012F, 0.868F),
               c.marble_shade,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(k_super_x, base_y + 0.246F, 0.0F),
               QVector3D(1.254F, 0.042F, 0.874F),
               c.cloth_red,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_super_x, base_y + 0.298F, 0.0F),
               QVector3D(1.262F, 0.010F, 0.882F),
               c.marble,
               k_building_state_mask_intact);

  for (int rosette = 0; rosette < 15; ++rosette) {
    float const px = k_super_x - 1.190F + (0.170F * static_cast<float>(rosette));
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(px, base_y + 0.246F, side * 0.880F),
                   QVector3D(0.034F, 0.026F, 0.010F),
                   c.gold,
                   k_building_state_mask_intact);
    }
  }
  for (int rosette = 0; rosette < 11; ++rosette) {
    float const pz = -0.800F + (0.160F * static_cast<float>(rosette));
    for (float const face : {-1.240F, 1.280F}) {
      desc.add_box(QVector3D(face, base_y + 0.246F, pz),
                   QVector3D(0.010F, 0.026F, 0.032F),
                   c.gold,
                   k_building_state_mask_intact);
    }
  }

  desc.add_box(QVector3D(k_super_x, base_y + 0.334F, 0.0F),
               QVector3D(1.256F, 0.026F, 0.876F),
               c.marble_shade,
               k_building_state_mask_intact);

  constexpr QVector3D k_dentil_x_half(0.022F, 0.024F, 0.026F);
  constexpr QVector3D k_dentil_z_half(0.026F, 0.024F, 0.022F);
  for (float const side : {-1.0F, 1.0F}) {
    add_dentil_row(desc,
                   QVector3D(k_super_x - 1.230F, base_y + 0.334F, side * 0.894F),
                   QVector3D(0.0888F, 0.0F, 0.0F),
                   29,
                   k_dentil_z_half,
                   c.marble);
  }
  for (float const face : {-1.254F, 1.294F}) {
    add_dentil_row(desc,
                   QVector3D(face, base_y + 0.334F, -0.816F),
                   QVector3D(0.0F, 0.0F, 0.0906F),
                   19,
                   k_dentil_x_half,
                   c.marble);
  }

  desc.add_box(QVector3D(k_super_x, base_y + 0.386F, 0.0F),
               QVector3D(1.290F, 0.026F, 0.910F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(k_super_x, base_y + 0.424F, 0.0F),
               QVector3D(1.305F, 0.012F, 0.925F),
               c.marble_shade,
               k_building_state_mask_intact);
}

void add_tympanum(BuildingArchetypeDesc& desc,
                  const RomanTemplePalette& c,
                  float pediment_y,
                  float face_x,
                  float face_dir,
                  const QVector3D& field) {
  auto add_triangle = [&](float x,
                          float half_thick,
                          float base_inset,
                          float z_inset,
                          const QVector3D& color) {
    constexpr int k_bands = 22;
    float const usable_rise = k_pediment_rise - (base_inset * 2.0F);
    for (int band = 0; band < k_bands; ++band) {
      float const base =
          base_inset + (usable_rise * static_cast<float>(band) / k_bands);
      float const top =
          base_inset + (usable_rise * static_cast<float>(band + 1) / k_bands);
      float const half_z =
          (k_pediment_half_z * (1.0F - (base / k_pediment_rise))) - z_inset;
      if (half_z <= 0.02F) {
        continue;
      }
      desc.add_box(QVector3D(x, pediment_y + ((base + top) * 0.5F), 0.0F),
                   QVector3D(half_thick, ((top - base) * 0.5F) + 0.002F, half_z),
                   color,
                   k_building_state_mask_intact);
    }
  };

  add_triangle(face_x, 0.034F, 0.0F, 0.0F, c.marble);
  add_triangle(face_x + (face_dir * 0.028F), 0.008F, 0.100F, 0.320F, field);

  float const rake_theta =
      std::atan2(k_pediment_rise, k_pediment_half_z) * k_rad_to_deg;
  float const rake_half_len = std::sqrt((k_pediment_half_z * k_pediment_half_z) +
                                        (k_pediment_rise * k_pediment_rise)) *
                              0.5F;
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(QVector3D(face_x + (face_dir * 0.030F),
                                   pediment_y + (k_pediment_rise * 0.5F) + 0.020F,
                                   side * k_pediment_half_z * 0.5F),
                         QVector3D(0.032F, 0.038F, rake_half_len + 0.032F),
                         QVector3D(side * rake_theta, 0.0F, 0.0F),
                         c.marble,
                         k_building_state_mask_intact);
    desc.add_rotated_box(QVector3D(face_x + (face_dir * 0.046F),
                                   pediment_y + (k_pediment_rise * 0.5F) + 0.062F,
                                   side * k_pediment_half_z * 0.5F),
                         QVector3D(0.020F, 0.018F, rake_half_len + 0.038F),
                         QVector3D(side * rake_theta, 0.0F, 0.0F),
                         c.marble_shade,
                         k_building_state_mask_intact);
  }
  desc.add_box(QVector3D(face_x + (face_dir * 0.026F), pediment_y - 0.012F, 0.0F),
               QVector3D(0.028F, 0.024F, k_pediment_half_z + 0.024F),
               c.marble,
               k_building_state_mask_intact);
}

void add_cella(BuildingArchetypeDesc& desc,
               const RomanTemplePalette& c,
               float podium_y,
               float cella_h) {
  desc.add_box(QVector3D(0.52F, podium_y + (cella_h * 0.5F), 0.0F),
               QVector3D(0.70F, cella_h * 0.5F, 0.60F),
               c.marble_shade);
  desc.add_box(QVector3D(0.52F, podium_y + 0.030F, 0.0F),
               QVector3D(0.73F, 0.030F, 0.63F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.52F, podium_y + 0.066F, 0.0F),
               QVector3D(0.715F, 0.014F, 0.615F),
               c.marble_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.52F, podium_y + cella_h + 0.024F, 0.0F),
               QVector3D(0.72F, 0.024F, 0.62F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.52F, podium_y + cella_h + 0.060F, 0.0F),
               QVector3D(0.74F, 0.016F, 0.64F),
               c.marble_shade,
               k_building_state_mask_intact);

  for (int course = 1; course < 7; ++course) {
    float const course_y = podium_y + (cella_h * static_cast<float>(course) / 7.0F);
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.52F, course_y, side * 0.605F),
                   QVector3D(0.68F, 0.007F, 0.006F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
    desc.add_box(QVector3D(1.225F, course_y, 0.0F),
                 QVector3D(0.006F, 0.007F, 0.58F),
                 c.mortar,
                 k_building_state_mask_intact);
  }

  for (float const px : {-0.06F, 0.32F, 0.70F, 1.08F}) {
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(px, podium_y + (cella_h * 0.5F), side * 0.615F),
                   QVector3D(0.046F, cella_h * 0.47F, 0.020F),
                   c.marble,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(px, podium_y + (cella_h * 0.965F), side * 0.624F),
                   QVector3D(0.060F, cella_h * 0.030F, 0.028F),
                   c.marble,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(px, podium_y + (cella_h * 0.055F), side * 0.624F),
                   QVector3D(0.060F, cella_h * 0.026F, 0.028F),
                   c.marble_shade,
                   k_building_state_mask_intact);
    }
  }
  for (float const pz : {-0.40F, 0.0F, 0.40F}) {
    desc.add_box(QVector3D(1.235F, podium_y + (cella_h * 0.5F), pz),
                 QVector3D(0.020F, cella_h * 0.47F, 0.046F),
                 c.marble,
                 k_building_state_mask_intact);
  }

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(0.52F, podium_y + (cella_h * 0.895F), side * 0.612F),
                 QVector3D(0.665F, cella_h * 0.040F, 0.012F),
                 c.cloth_red,
                 BuildingStateMask::Normal);
  }
  desc.add_box(QVector3D(1.232F, podium_y + (cella_h * 0.895F), 0.0F),
               QVector3D(0.012F, cella_h * 0.040F, 0.565F),
               c.cloth_red,
               BuildingStateMask::Normal);
}

void add_cella_doorway(BuildingArchetypeDesc& desc,
                       const RomanTemplePalette& c,
                       float podium_y,
                       float cella_h) {
  float const jamb_h = cella_h * 0.36F;

  desc.add_box(QVector3D(-0.192F, podium_y + jamb_h, 0.0F),
               QVector3D(0.026F, jamb_h + 0.030F, 0.360F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.206F, podium_y + jamb_h, 0.0F),
               QVector3D(0.016F, jamb_h + 0.010F, 0.330F),
               c.marble_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.198F, podium_y + (jamb_h * 2.0F) + 0.052F, 0.0F),
               QVector3D(0.034F, 0.026F, 0.400F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.206F, podium_y + (jamb_h * 2.0F) + 0.092F, 0.0F),
               QVector3D(0.042F, 0.020F, 0.300F),
               c.marble_shade,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(-0.185F, podium_y + (jamb_h * 0.98F), 0.0F),
               QVector3D(0.030F, jamb_h * 0.98F, 0.290F),
               c.soot,
               k_building_state_mask_intact);
  for (float const leaf : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.200F, podium_y + (jamb_h * 0.94F), leaf * 0.132F),
                 QVector3D(0.020F, jamb_h * 0.94F, 0.116F),
                 c.bronze,
                 k_building_state_mask_intact);
    for (int panel = 0; panel < 3; ++panel) {
      float const panel_y =
          podium_y + (jamb_h * (0.32F + (0.60F * static_cast<float>(panel))));
      desc.add_box(QVector3D(-0.222F, panel_y, leaf * 0.132F),
                   QVector3D(0.008F, 0.052F, 0.086F),
                   c.gold,
                   k_building_state_mask_intact);
    }
    desc.add_box(QVector3D(-0.224F, podium_y + (jamb_h * 0.94F), leaf * 0.026F),
                 QVector3D(0.008F, jamb_h * 0.90F, 0.012F),
                 c.gold,
                 k_building_state_mask_intact);
  }

  for (int step = 0; step < 2; ++step) {
    float const t = static_cast<float>(step);
    desc.add_box(QVector3D(-0.230F - (0.052F * t),
                           podium_y + 0.012F + (0.026F * (1.0F - t)),
                           0.0F),
                 QVector3D(0.052F, 0.014F, 0.330F + (0.020F * t)),
                 c.marble,
                 k_building_state_mask_intact);
  }
}

void add_roman_temple_ruin(BuildingArchetypeDesc& desc,
                           const RomanTemplePalette& c,
                           float podium_y,
                           float cella_h) {
  constexpr auto k_ruin = BuildingStateMask::Destroyed;

  struct Drum {
    float x;
    float z;
    float yaw;
    float len;
  };
  constexpr std::array<Drum, 6> k_drums{Drum{-0.86F, 0.42F, 18.0F, 0.30F},
                                        Drum{-0.34F, -0.58F, 74.0F, 0.24F},
                                        Drum{0.28F, 0.66F, 122.0F, 0.27F},
                                        Drum{0.94F, -0.30F, 41.0F, 0.22F},
                                        Drum{-1.28F, -0.70F, 96.0F, 0.26F},
                                        Drum{1.18F, 0.52F, 8.0F, 0.20F}};
  for (const auto& drum : k_drums) {
    float const rad = drum.yaw * k_pi / 180.0F;
    QVector3D const axis(std::cos(rad) * drum.len, 0.0F, std::sin(rad) * drum.len);
    QVector3D const mid(drum.x, podium_y + 0.074F, drum.z);
    desc.add_cylinder(mid - axis, mid + axis, 0.070F, c.marble_shade, k_ruin);
  }

  struct Block {
    float x;
    float y;
    float z;
    float yaw;
    float roll;
    float sx;
    float sy;
    float sz;
  };

  constexpr std::array<Block, 7> k_blocks{
      Block{-1.44F, 0.10F, 0.34F, 22.0F, 9.0F, 0.16F, 0.075F, 0.11F},
      Block{-0.62F, 0.09F, -1.16F, 58.0F, -6.0F, 0.13F, 0.065F, 0.13F},
      Block{0.44F, 0.10F, 1.22F, 12.0F, 14.0F, 0.18F, 0.070F, 0.10F},
      Block{1.42F, 0.09F, -0.86F, 81.0F, -11.0F, 0.14F, 0.060F, 0.12F},
      Block{0.10F, 0.09F, -0.34F, 34.0F, 7.0F, 0.15F, 0.070F, 0.12F},
      Block{-0.96F, 0.08F, 0.88F, 66.0F, -8.0F, 0.12F, 0.058F, 0.10F},
      Block{1.02F, 0.10F, 0.16F, 5.0F, 16.0F, 0.19F, 0.080F, 0.13F}};
  int block_index = 0;
  for (const auto& block : k_blocks) {
    bool const on_podium = block_index++ >= 4;
    desc.add_rotated_box(
        QVector3D(block.x, block.y + (on_podium ? podium_y : 0.0F), block.z),
        QVector3D(block.sx, block.sy, block.sz),
        QVector3D(block.roll, block.yaw, 0.0F),
        (block.yaw > 40.0F) ? c.limestone : c.marble_shade,
        k_ruin);
  }

  float const stub_top = podium_y + cella_h;
  constexpr std::array<float, 5> k_stub_z{-0.52F, -0.24F, 0.06F, 0.32F, 0.54F};
  for (std::size_t i = 0; i < k_stub_z.size(); ++i) {
    float const rise = 0.06F + (0.055F * static_cast<float>((i * 3) % 4));
    desc.add_box(QVector3D(1.20F, stub_top + (rise * 0.5F), k_stub_z[i]),
                 QVector3D(0.055F, rise * 0.5F, 0.11F),
                 c.marble_shade,
                 k_ruin);
  }
  for (float const side : {-1.0F, 1.0F}) {
    for (int i = 0; i < 4; ++i) {
      float const px = -0.02F + (0.36F * static_cast<float>(i));
      float const rise = 0.05F + (0.05F * static_cast<float>((i + 1) % 3));
      desc.add_box(QVector3D(px, stub_top + (rise * 0.5F), side * 0.58F),
                   QVector3D(0.13F, rise * 0.5F, 0.052F),
                   c.marble_shade,
                   k_ruin);
    }
  }

  QVector3D const ash = (c.soot * 0.45F) + (c.limestone * 0.55F);
  QVector3D const ash_dark = (c.soot * 0.72F) + (c.limestone * 0.28F);

  desc.add_box(QVector3D(0.52F, stub_top + 0.010F, 0.0F),
               QVector3D(0.62F, 0.010F, 0.52F),
               ash,
               k_ruin);
  struct Debris {
    float x;
    float z;
    float yaw;
    float sx;
    float sy;
    float sz;
  };
  constexpr std::array<Debris, 5> k_debris{
      Debris{0.24F, -0.22F, 27.0F, 0.15F, 0.055F, 0.12F},
      Debris{0.72F, 0.28F, 63.0F, 0.13F, 0.070F, 0.11F},
      Debris{1.00F, -0.30F, 14.0F, 0.11F, 0.048F, 0.14F},
      Debris{0.36F, 0.34F, 88.0F, 0.14F, 0.062F, 0.10F},
      Debris{0.86F, 0.02F, 45.0F, 0.10F, 0.052F, 0.12F}};
  for (const auto& piece : k_debris) {
    desc.add_rotated_box(QVector3D(piece.x, stub_top + piece.sy + 0.010F, piece.z),
                         QVector3D(piece.sx, piece.sy, piece.sz),
                         QVector3D(0.0F, piece.yaw, 0.0F),
                         (piece.yaw > 50.0F) ? ash_dark : c.marble_shade,
                         k_ruin);
  }

  constexpr std::array<Debris, 6> k_roof_shards{
      Debris{-0.94F, 0.62F, 34.0F, 0.14F, 0.016F, 0.10F},
      Debris{-0.28F, -0.86F, 71.0F, 0.12F, 0.016F, 0.12F},
      Debris{0.62F, 0.94F, 19.0F, 0.15F, 0.016F, 0.09F},
      Debris{1.24F, -0.42F, 55.0F, 0.11F, 0.016F, 0.11F},
      Debris{-1.36F, -0.24F, 8.0F, 0.13F, 0.016F, 0.10F},
      Debris{0.16F, 1.08F, 88.0F, 0.10F, 0.016F, 0.12F}};
  for (const auto& shard : k_roof_shards) {
    desc.add_rotated_box(QVector3D(shard.x, shard.sy + 0.006F, shard.z),
                         QVector3D(shard.sx, shard.sy, shard.sz),
                         QVector3D(0.0F, shard.yaw, 0.0F),
                         (shard.yaw > 50.0F) ? c.terracotta_dark : c.terracotta,
                         k_ruin);
  }

  for (float const sx : {-1.10F, -0.20F, 0.86F}) {
    desc.add_box(QVector3D(sx, podium_y + 0.010F, 0.30F),
                 QVector3D(0.26F, 0.008F, 0.22F),
                 ash,
                 k_ruin);
  }
}

auto build_temple_archetype(BuildingState state) -> RenderArchetype {
  RomanTemplePalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.74F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.42F;
  }

  BuildingArchetypeDesc desc("roman_temple");

  desc.add_box(
      QVector3D(0.0F, 0.055F, 0.0F), QVector3D(1.38F, 0.055F, 1.10F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.195F, 0.0F), QVector3D(1.33F, 0.085F, 1.05F), c.limestone);
  desc.add_box(
      QVector3D(0.0F, 0.345F, 0.0F), QVector3D(1.29F, 0.065F, 1.01F), c.limestone);
  desc.add_box(
      QVector3D(0.0F, 0.428F, 0.0F), QVector3D(1.25F, 0.030F, 0.97F), c.marble_shade);
  desc.add_box(
      QVector3D(0.0F, 0.466F, 0.0F), QVector3D(1.30F, 0.014F, 1.02F), c.marble);
  desc.add_box(
      QVector3D(0.0F, 0.484F, 0.0F), QVector3D(1.27F, 0.008F, 0.99F), c.marble);

  float const podium_y = 0.492F;

  for (int course = 0; course < 4; ++course) {
    float const course_y = 0.110F + (0.048F * static_cast<float>(course));
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.0F, course_y, side * 1.035F),
                   QVector3D(1.29F, 0.006F, 0.006F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
    for (float const face : {-1.315F, 1.315F}) {
      desc.add_box(QVector3D(face, course_y, 0.0F),
                   QVector3D(0.006F, 0.006F, 1.03F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
  }
  for (float const joint_x : {-0.90F, -0.30F, 0.30F, 0.90F}) {
    desc.add_box(QVector3D(joint_x, 0.170F, 0.0F),
                 QVector3D(0.007F, 0.052F, 1.035F),
                 c.mortar,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(joint_x, 0.290F, 0.0F),
                 QVector3D(0.007F, 0.046F, 1.035F),
                 c.mortar,
                 k_building_state_mask_intact);
  }
  for (float const joint_z : {-0.62F, 0.0F, 0.62F}) {
    for (float const face : {-1.315F, 1.315F}) {
      desc.add_box(QVector3D(face, 0.170F, joint_z),
                   QVector3D(0.006F, 0.052F, 0.007F),
                   c.mortar,
                   k_building_state_mask_intact);
    }
  }

  for (int step = 0; step < 6; ++step) {
    float const step_index = static_cast<float>(step);
    float const top = 0.082F * (step_index + 1.0F);
    desc.add_box(QVector3D(-1.700F + (0.078F * step_index), top * 0.5F, 0.0F),
                 QVector3D(0.041F, top * 0.5F, 0.80F),
                 (step % 2 == 0) ? c.marble_shade : c.limestone,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.700F + (0.078F * step_index), top - 0.006F, 0.0F),
                 QVector3D(0.043F, 0.006F, 0.805F),
                 c.marble,
                 k_building_state_mask_intact);
  }
  for (float const cheek : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-1.50F, 0.24F, cheek * 0.855F),
                 QVector3D(0.26F, 0.24F, 0.055F),
                 c.limestone,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.50F, 0.492F, cheek * 0.855F),
                 QVector3D(0.27F, 0.014F, 0.062F),
                 c.marble,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.50F, 0.052F, cheek * 0.862F),
                 QVector3D(0.27F, 0.052F, 0.062F),
                 c.limestone_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-1.50F, 0.320F, cheek * 0.868F),
                 QVector3D(0.26F, 0.010F, 0.010F),
                 c.mortar,
                 k_building_state_mask_intact);
  }

  float const column_height = 1.52F * height_multiplier;
  bool const ruined = state == BuildingState::Destroyed;

  int column_index = 0;
  auto snapped_height = [&](int index) {
    if (!ruined) {
      return column_height;
    }
    constexpr std::array<float, 7> k_breaks{
        1.24F, 0.46F, 0.88F, 0.31F, 1.05F, 0.62F, 0.78F};
    return column_height * k_breaks[static_cast<std::size_t>(index) % k_breaks.size()];
  };

  for (float const cz : {-0.76F, -0.26F, 0.26F, 0.76F}) {
    add_fluted_column(desc, c, -1.10F, cz, podium_y, snapped_height(column_index++));
    add_fluted_column(desc, c, -0.56F, cz, podium_y, snapped_height(column_index++));
  }
  for (float const cx : {-0.02F, 0.52F, 1.06F}) {
    for (float const cz : {-0.76F, 0.76F}) {
      add_fluted_column(desc, c, cx, cz, podium_y, snapped_height(column_index++));
    }
  }

  float const cella_h = 1.12F * height_multiplier;
  add_cella(desc, c, podium_y, cella_h);
  add_cella_doorway(desc, c, podium_y, cella_h);

  float const entablature_y = podium_y + column_height + 0.014F;
  add_entablature(desc, c, entablature_y);

  float const pediment_y = entablature_y + 0.436F;

  add_tympanum(desc, c, pediment_y, -1.235F, -1.0F, c.cloth_red);
  add_tympanum(desc, c, pediment_y, 1.275F, 1.0F, c.blue_accent);

  add_roof_field(desc, c, pediment_y);

  for (float const corner : {-1.0F, 1.0F}) {
    for (float const face : {-1.235F, 1.275F}) {
      desc.add_box(QVector3D(face, pediment_y + 0.026F, corner * 0.885F),
                   QVector3D(0.064F, 0.052F, 0.064F),
                   c.marble,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(face, pediment_y + 0.070F, corner * 0.885F),
                   QVector3D(0.048F, 0.014F, 0.048F),
                   c.marble_shade,
                   k_building_state_mask_intact);
      desc.add_cone(QVector3D(face, pediment_y + 0.082F, corner * 0.885F),
                    QVector3D(face, pediment_y + 0.235F, corner * 0.885F),
                    0.058F,
                    c.gold,
                    BuildingStateMask::Normal);
    }
  }

  add_votive_altar(desc, c, QVector3D(-1.44F, 0.0F, 0.92F));
  add_votive_altar(desc, c, QVector3D(-1.44F, 0.0F, -0.92F));

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(
        QVector3D(-1.33F, podium_y, side * 0.60F),
        QVector3D(-1.33F, podium_y + (0.72F * height_multiplier), side * 0.60F),
        0.024F,
        c.bronze,
        k_building_state_mask_intact);
    desc.add_palette_box(
        QVector3D(-1.33F, podium_y + (0.52F * height_multiplier), side * 0.60F),
        QVector3D(0.012F, 0.17F, 0.12F),
        k_temple_team_slot,
        BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(
        QVector3D(-1.33F, podium_y + (0.75F * height_multiplier), side * 0.60F),
        QVector3D(0.032F, 0.032F, 0.032F),
        c.gold,
        BuildingStateMask::Normal);
  }

  desc.add_palette_box(QVector3D(0.52F, podium_y + (cella_h * 0.74F), 0.615F),
                       QVector3D(0.36F, 0.15F, 0.014F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_palette_box(QVector3D(0.52F, podium_y + (cella_h * 0.74F), -0.615F),
                       QVector3D(0.36F, 0.15F, 0.014F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  add_roman_aquila_relief(
      desc,
      QVector3D(-1.287F, pediment_y + (k_pediment_rise * 0.40F), 0.0F),
      BuildingFacadePlane::ZY,
      0.46F,
      c.gold,
      c.cloth_red);
  add_roman_roof_standard(
      desc,
      QVector3D(k_super_x, pediment_y + k_pediment_rise + 0.070F, 0.0F),
      0.68F,
      c.gold,
      c.cloth_red);

  if (ruined) {
    add_roman_temple_ruin(desc, c, podium_y, cella_h);
  }

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(1.18F, 0.0F, 0.92F),
                                 .stone = c.limestone,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.limestone_dark * 0.5F,
                                 .ground_y = 0.29F,
                                 .scale = 1.15F,
                                 .seed = 269});

  desc.scale_uniformly(k_temple_mesh_scale);

  return build_building_archetype(desc, state);
}

auto temple_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_temple_archetype);
  return k_set.for_state(state);
}

} // namespace

void register_temple_renderer(EntityRendererRegistry& registry) {
  register_temple_renderer_variant(
      registry,
      TempleRendererConfig{.nation_slug = "roman",
                           .archetype = &temple_archetype,
                           .selection = BuildingSelectionStyle{3.8F, 3.8F}});
}

} // namespace Render::GL::Roman
