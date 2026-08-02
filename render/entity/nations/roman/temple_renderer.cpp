#include "temple_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>

#include "../../../render_archetype.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"
#include "../../temple_renderer_common.h"

namespace Render::GL::Roman {
namespace {

struct RomanTemplePalette {
  QVector3D marble{0.88F, 0.86F, 0.80F};
  QVector3D marble_shade{0.74F, 0.71F, 0.64F};
  QVector3D marble_dark{0.55F, 0.52F, 0.46F};
  QVector3D limestone{0.78F, 0.74F, 0.65F};
  QVector3D limestone_dark{0.46F, 0.43F, 0.37F};
  QVector3D mortar{0.52F, 0.49F, 0.43F};
  QVector3D terracotta{0.63F, 0.25F, 0.105F};
  QVector3D terracotta_dark{0.43F, 0.15F, 0.08F};
  QVector3D cloth_red{0.53F, 0.075F, 0.052F};
  QVector3D gold{0.72F, 0.53F, 0.20F};
  QVector3D bronze{0.48F, 0.30F, 0.11F};
  QVector3D blue_accent{0.15F, 0.31F, 0.47F};
  QVector3D soot{0.16F, 0.14F, 0.12F};
  QVector3D flame{0.88F, 0.47F, 0.12F};
  QVector3D cedar{0.40F, 0.25F, 0.13F};
};

constexpr std::uint8_t k_temple_team_slot = 1;

void add_fluted_column(BuildingArchetypeDesc& desc,
                       const RomanTemplePalette& c,
                       float x,
                       float z,
                       float base_y,
                       float height) {
  constexpr float shaft_radius = 0.058F;

  desc.add_box(QVector3D(x, base_y + 0.018F, z),
               QVector3D(shaft_radius * 1.55F, 0.018F, shaft_radius * 1.55F),
               c.marble_shade);
  desc.add_cylinder(QVector3D(x, base_y + 0.036F, z),
                    QVector3D(x, base_y + 0.070F, z),
                    shaft_radius * 1.22F,
                    c.marble_shade,
                    BuildingStateMask::All,
                    BuildingLODMask::Full);

  desc.add_cylinder(QVector3D(x, base_y + 0.056F, z),
                    QVector3D(x, base_y + height - 0.06F, z),
                    shaft_radius,
                    c.marble);

  for (float const flute : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(x + flute * shaft_radius * 0.92F, base_y + height * 0.5F, z),
                 QVector3D(0.006F, height * 0.42F, shaft_radius * 0.55F),
                 c.marble_shade,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(x, base_y + height * 0.5F, z + flute * shaft_radius * 0.92F),
                 QVector3D(shaft_radius * 0.55F, height * 0.42F, 0.006F),
                 c.marble_shade,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  desc.add_cylinder(QVector3D(x, base_y + height - 0.07F, z),
                    QVector3D(x, base_y + height - 0.035F, z),
                    shaft_radius * 1.16F,
                    c.marble_shade,
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);
  desc.add_box(QVector3D(x, base_y + height - 0.012F, z),
               QVector3D(shaft_radius * 1.62F, 0.026F, shaft_radius * 1.62F),
               c.marble,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(
        QVector3D(x + side * shaft_radius * 1.30F, base_y + height - 0.046F, z),
        QVector3D(x + side * shaft_radius * 1.30F, base_y + height - 0.020F, z),
        0.020F,
        c.marble_shade,
        k_building_state_mask_intact,
        BuildingLODMask::Full);
  }
}

void add_votive_altar(BuildingArchetypeDesc& desc,
                      const RomanTemplePalette& c,
                      const QVector3D& base) {
  desc.add_box(base + QVector3D(0.0F, 0.030F, 0.0F),
               QVector3D(0.155F, 0.030F, 0.135F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.115F, 0.0F),
               QVector3D(0.115F, 0.058F, 0.098F),
               c.marble_shade,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.190F, 0.0F),
               QVector3D(0.150F, 0.022F, 0.130F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.220F, 0.0F),
               QVector3D(0.085F, 0.012F, 0.070F),
               c.soot,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_cone(base + QVector3D(0.0F, 0.228F, 0.0F),
                base + QVector3D(0.0F, 0.330F, 0.0F),
                0.052F,
                c.flame,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
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
  desc.set_full_lod_max_distance(84.0F);

  desc.add_box(
      QVector3D(0.0F, 0.050F, 0.0F), QVector3D(1.38F, 0.050F, 1.10F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.155F, 0.0F), QVector3D(1.31F, 0.055F, 1.03F), c.limestone);
  desc.add_box(
      QVector3D(0.0F, 0.248F, 0.0F), QVector3D(1.25F, 0.038F, 0.97F), c.marble_shade);
  desc.add_box(
      QVector3D(0.0F, 0.296F, 0.0F), QVector3D(1.27F, 0.010F, 0.99F), c.marble);

  float const podium_y = 0.306F;

  for (int course = 0; course < 4; ++course) {
    float const course_y = 0.110F + 0.048F * static_cast<float>(course);
    desc.add_box(QVector3D(0.0F, course_y, 1.035F),
                 QVector3D(1.29F, 0.006F, 0.006F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (float const joint_x : {-0.90F, -0.30F, 0.30F, 0.90F}) {
    desc.add_box(QVector3D(joint_x, 0.170F, 0.0F),
                 QVector3D(0.007F, 0.052F, 1.035F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (int step = 0; step < 4; ++step) {
    float const step_index = static_cast<float>(step);
    float const top = 0.076F * (step_index + 1.0F);
    desc.add_box(QVector3D(-1.605F + 0.088F * step_index, top * 0.5F, 0.0F),
                 QVector3D(0.046F, top * 0.5F, 0.76F),
                 (step % 2 == 0) ? c.marble_shade : c.limestone,
                 k_building_state_mask_intact);
  }
  for (float const cheek : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-1.47F, 0.15F, cheek * 0.815F),
                 QVector3D(0.20F, 0.15F, 0.055F),
                 c.limestone,
                 k_building_state_mask_intact);
  }

  float const column_height = 1.06F * height_multiplier;

  for (float const cz : {-0.76F, -0.26F, 0.26F, 0.76F}) {
    add_fluted_column(desc, c, -1.10F, cz, podium_y, column_height);
    add_fluted_column(desc, c, -0.56F, cz, podium_y, column_height);
  }
  for (float const cx : {-0.02F, 0.52F, 1.06F}) {
    for (float const cz : {-0.76F, 0.76F}) {
      add_fluted_column(desc, c, cx, cz, podium_y, column_height);
    }
  }

  float const cella_h = 1.00F * height_multiplier;
  desc.add_box(QVector3D(0.52F, podium_y + cella_h * 0.5F, 0.0F),
               QVector3D(0.70F, cella_h * 0.5F, 0.60F),
               c.marble_shade);
  desc.add_box(QVector3D(0.52F, podium_y + cella_h + 0.030F, 0.0F),
               QVector3D(0.73F, 0.030F, 0.63F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.52F, podium_y + 0.030F, 0.0F),
               QVector3D(0.73F, 0.030F, 0.63F),
               c.marble,
               k_building_state_mask_intact);

  for (int course = 1; course < 6; ++course) {
    float const course_y = podium_y + cella_h * static_cast<float>(course) / 6.0F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(0.52F, course_y, side * 0.605F),
                   QVector3D(0.68F, 0.007F, 0.006F),
                   c.mortar,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
    desc.add_box(QVector3D(1.225F, course_y, 0.0F),
                 QVector3D(0.006F, 0.007F, 0.58F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (float const px : {-0.06F, 0.32F, 0.70F, 1.08F}) {
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(px, podium_y + cella_h * 0.5F, side * 0.615F),
                   QVector3D(0.042F, cella_h * 0.47F, 0.018F),
                   c.marble,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(px, podium_y + cella_h * 0.965F, side * 0.622F),
                   QVector3D(0.058F, cella_h * 0.030F, 0.024F),
                   c.marble,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }
  for (float const pz : {-0.40F, 0.0F, 0.40F}) {
    desc.add_box(QVector3D(1.235F, podium_y + cella_h * 0.5F, pz),
                 QVector3D(0.018F, cella_h * 0.47F, 0.042F),
                 c.marble,
                 k_building_state_mask_intact);
  }

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(0.52F, podium_y + cella_h * 0.90F, side * 0.612F),
                 QVector3D(0.66F, cella_h * 0.045F, 0.014F),
                 c.cloth_red,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.52F, podium_y + cella_h * 0.20F, side * 0.612F),
                 QVector3D(0.66F, cella_h * 0.022F, 0.014F),
                 c.blue_accent,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(1.232F, podium_y + cella_h * 0.90F, 0.0F),
               QVector3D(0.014F, cella_h * 0.045F, 0.56F),
               c.cloth_red,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(1.232F, podium_y + cella_h * 0.20F, 0.0F),
               QVector3D(0.014F, cella_h * 0.022F, 0.56F),
               c.blue_accent,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(-0.185F, podium_y + cella_h * 0.34F, 0.0F),
               QVector3D(0.032F, cella_h * 0.34F, 0.30F),
               c.soot,
               k_building_state_mask_intact);
  for (float const leaf : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.200F, podium_y + cella_h * 0.32F, leaf * 0.135F),
                 QVector3D(0.020F, cella_h * 0.32F, 0.118F),
                 c.bronze,
                 k_building_state_mask_intact);
    for (int panel = 0; panel < 3; ++panel) {
      float const panel_y =
          podium_y + cella_h * (0.10F + 0.21F * static_cast<float>(panel));
      desc.add_box(QVector3D(-0.222F, panel_y, leaf * 0.135F),
                   QVector3D(0.008F, 0.052F, 0.086F),
                   c.gold,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }
  desc.add_box(QVector3D(-0.185F, podium_y + cella_h * 0.70F, 0.0F),
               QVector3D(0.048F, 0.036F, 0.38F),
               c.marble,
               k_building_state_mask_intact);

  float const entablature_y = podium_y + column_height + 0.070F;
  desc.add_box(QVector3D(0.02F, entablature_y, 0.0F),
               QVector3D(1.26F, 0.062F, 0.88F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.02F, entablature_y + 0.082F, 0.0F),
               QVector3D(1.24F, 0.024F, 0.86F),
               c.cloth_red,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.02F, entablature_y + 0.122F, 0.0F),
               QVector3D(1.30F, 0.030F, 0.92F),
               c.marble,
               k_building_state_mask_intact);

  for (int triglyph = 0; triglyph < 11; ++triglyph) {
    float const z = -0.80F + static_cast<float>(triglyph) * 0.16F;
    desc.add_box(QVector3D(-1.245F, entablature_y + 0.082F, z),
                 QVector3D(0.012F, 0.024F, 0.032F),
                 c.gold,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (int triglyph = 0; triglyph < 13; ++triglyph) {
    float const x = -1.18F + static_cast<float>(triglyph) * 0.20F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(x, entablature_y + 0.082F, side * 0.845F),
                   QVector3D(0.034F, 0.024F, 0.012F),
                   c.gold,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }

  float const pediment_y = entablature_y + 0.152F;
  float const pediment_rise = 0.46F;
  float const pediment_half_z = 0.90F;

  auto add_tympanum = [&](float face_x, const QVector3D& field) {
    struct Band {
      float base;
      float top;
    };
    constexpr std::array<Band, 4> k_bands{Band{0.0F, 0.115F},
                                          Band{0.115F, 0.225F},
                                          Band{0.225F, 0.325F},
                                          Band{0.325F, 0.410F}};
    for (const auto& band : k_bands) {
      float const half_z = pediment_half_z * (1.0F - band.top / pediment_rise) * 0.94F;
      desc.add_box(QVector3D(face_x, pediment_y + (band.base + band.top) * 0.5F, 0.0F),
                   QVector3D(0.030F, (band.top - band.base) * 0.5F, half_z),
                   c.marble,
                   k_building_state_mask_intact);
      desc.add_box(
          QVector3D(face_x - 0.018F, pediment_y + (band.base + band.top) * 0.5F, 0.0F),
          QVector3D(0.014F, (band.top - band.base) * 0.5F - 0.014F, half_z - 0.030F),
          field,
          k_building_state_mask_intact,
          BuildingLODMask::Full);
    }
  };

  add_tympanum(-1.20F, c.cloth_red);
  add_tympanum(1.24F, c.blue_accent);

  add_gable_roof_x(
      [&](const QVector3D& center,
          const QVector3D& scale,
          const QVector3D& euler,
          const QVector3D& color) {
        desc.add_rotated_box(center, scale, euler, color, k_building_state_mask_intact);
      },
      0.02F,
      0.0F,
      pediment_y,
      1.30F,
      pediment_half_z,
      pediment_rise,
      0.038F,
      c.terracotta);

  for (int rib = 0; rib < 11; ++rib) {
    float const z = -0.82F + static_cast<float>(rib) * 0.164F;
    float const lift = pediment_rise * (1.0F - std::fabs(z) / pediment_half_z);
    desc.add_box(QVector3D(0.02F, pediment_y + lift + 0.042F, z),
                 QVector3D(1.28F, 0.013F, 0.024F),
                 c.terracotta_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  desc.add_box(QVector3D(0.02F, pediment_y + pediment_rise + 0.026F, 0.0F),
               QVector3D(1.30F, 0.026F, 0.052F),
               c.terracotta_dark,
               k_building_state_mask_intact);

  for (float const side : {-1.0F, 1.0F}) {
    for (int antefix = 0; antefix < 9; ++antefix) {
      float const x = -1.20F + static_cast<float>(antefix) * 0.30F;
      desc.add_box(QVector3D(x, pediment_y + 0.052F, side * 0.905F),
                   QVector3D(0.036F, 0.042F, 0.014F),
                   c.terracotta_dark,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
      desc.add_box(QVector3D(x, pediment_y + 0.084F, side * 0.905F),
                   QVector3D(0.020F, 0.020F, 0.012F),
                   c.gold,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);
    }
  }

  for (float const corner : {-1.0F, 1.0F}) {
    for (float const face : {-1.20F, 1.24F}) {
      desc.add_box(QVector3D(face, pediment_y + 0.022F, corner * 0.86F),
                   QVector3D(0.062F, 0.050F, 0.062F),
                   c.marble,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
      desc.add_cone(QVector3D(face, pediment_y + 0.062F, corner * 0.86F),
                    QVector3D(face, pediment_y + 0.205F, corner * 0.86F),
                    0.056F,
                    c.gold,
                    BuildingStateMask::Normal,
                    BuildingLODMask::Full);
    }
  }

  add_votive_altar(desc, c, QVector3D(-1.44F, 0.0F, 0.92F));
  add_votive_altar(desc, c, QVector3D(-1.44F, 0.0F, -0.92F));

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(
        QVector3D(-1.33F, podium_y, side * 0.60F),
        QVector3D(-1.33F, podium_y + 0.72F * height_multiplier, side * 0.60F),
        0.024F,
        c.bronze,
        k_building_state_mask_intact,
        BuildingLODMask::Full);
    desc.add_palette_box(
        QVector3D(-1.33F, podium_y + 0.52F * height_multiplier, side * 0.60F),
        QVector3D(0.012F, 0.17F, 0.12F),
        k_temple_team_slot,
        BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(QVector3D(-1.33F, podium_y + 0.75F * height_multiplier, side * 0.60F),
                 QVector3D(0.032F, 0.032F, 0.032F),
                 c.gold,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }

  desc.add_palette_box(QVector3D(0.52F, podium_y + cella_h * 0.74F, 0.615F),
                       QVector3D(0.36F, 0.15F, 0.014F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_palette_box(QVector3D(0.52F, podium_y + cella_h * 0.74F, -0.615F),
                       QVector3D(0.36F, 0.15F, 0.014F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  add_roman_aquila_relief(desc,
                          QVector3D(-1.236F, pediment_y + 0.150F, 0.0F),
                          BuildingFacadePlane::ZY,
                          0.34F,
                          c.gold,
                          c.cloth_red);
  add_roman_roof_standard(desc,
                          QVector3D(0.02F, pediment_y + pediment_rise + 0.046F, 0.0F),
                          0.68F,
                          c.gold,
                          c.cloth_red);

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
                           .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 1.5F},
                           .selection = BuildingSelectionStyle{1.9F, 1.9F}});
}

} // namespace Render::GL::Roman
