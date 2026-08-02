#include "temple_renderer.h"

#include <QVector3D>

#include <cstdint>

#include "../../../render_archetype.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"
#include "../../temple_renderer_common.h"

namespace Render::GL::Carthage {
namespace {

struct CarthageTemplePalette {
  QVector3D sandstone{0.82F, 0.70F, 0.52F};
  QVector3D sandstone_light{0.92F, 0.84F, 0.68F};
  QVector3D sandstone_dark{0.55F, 0.44F, 0.31F};
  QVector3D mortar{0.63F, 0.55F, 0.42F};
  QVector3D basalt{0.22F, 0.21F, 0.22F};
  QVector3D basalt_light{0.34F, 0.33F, 0.34F};
  QVector3D cedar{0.34F, 0.21F, 0.10F};
  QVector3D cedar_light{0.52F, 0.35F, 0.17F};
  QVector3D bronze{0.48F, 0.27F, 0.075F};
  QVector3D gold{0.70F, 0.47F, 0.13F};
  QVector3D indigo{0.10F, 0.12F, 0.30F};
  QVector3D oxblood{0.47F, 0.10F, 0.08F};
  QVector3D saffron{0.76F, 0.36F, 0.035F};
  QVector3D ember{0.76F, 0.19F, 0.025F};
  QVector3D soot{0.15F, 0.13F, 0.12F};
};

constexpr std::uint8_t k_temple_team_slot = 1;

void add_votive_pillar(BuildingArchetypeDesc& desc,
                       const CarthageTemplePalette& c,
                       float x,
                       float z,
                       float base_y,
                       float height) {
  desc.add_box(
      QVector3D(x, base_y + 0.030F, z), QVector3D(0.105F, 0.030F, 0.105F), c.basalt);
  desc.add_box(QVector3D(x, base_y + 0.075F, z),
               QVector3D(0.086F, 0.020F, 0.086F),
               c.basalt_light,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  desc.add_cylinder(QVector3D(x, base_y + 0.090F, z),
                    QVector3D(x, base_y + height - 0.10F, z),
                    0.062F,
                    c.sandstone);

  for (int band = 1; band < 4; ++band) {
    float const band_y =
        base_y + 0.090F + (height - 0.19F) * static_cast<float>(band) / 4.0F;
    desc.add_cylinder(QVector3D(x, band_y, z),
                      QVector3D(x, band_y + 0.020F, z),
                      0.070F,
                      c.bronze,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
  }

  desc.add_cylinder(QVector3D(x, base_y + height - 0.11F, z),
                    QVector3D(x, base_y + height - 0.055F, z),
                    0.082F,
                    c.sandstone_light,
                    k_building_state_mask_intact);
  desc.add_box(QVector3D(x, base_y + height - 0.030F, z),
               QVector3D(0.095F, 0.028F, 0.095F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_cone(QVector3D(x, base_y + height, z),
                QVector3D(x, base_y + height + 0.135F, z),
                0.070F,
                c.gold,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
}

void add_incense_brazier(BuildingArchetypeDesc& desc,
                         const CarthageTemplePalette& c,
                         const QVector3D& base) {
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_cylinder(base + QVector3D(side * 0.055F, 0.0F, 0.045F),
                      base + QVector3D(0.0F, 0.120F, 0.0F),
                      0.014F,
                      c.bronze,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
    desc.add_cylinder(base + QVector3D(side * 0.055F, 0.0F, -0.045F),
                      base + QVector3D(0.0F, 0.120F, 0.0F),
                      0.014F,
                      c.bronze,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
  }
  desc.add_cylinder(base + QVector3D(0.0F, 0.118F, 0.0F),
                    base + QVector3D(0.0F, 0.185F, 0.0F),
                    0.098F,
                    c.bronze,
                    k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(0.0F, 0.180F, 0.0F),
                    base + QVector3D(0.0F, 0.196F, 0.0F),
                    0.078F,
                    c.soot,
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);
  desc.add_cone(base + QVector3D(0.0F, 0.194F, 0.0F),
                base + QVector3D(0.0F, 0.300F, 0.0F),
                0.058F,
                c.ember,
                BuildingStateMask::Normal,
                BuildingLODMask::Full);
}

auto build_temple_archetype(BuildingState state) -> RenderArchetype {
  CarthageTemplePalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.74F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.42F;
  }

  BuildingArchetypeDesc desc("carthage_temple");
  desc.set_full_lod_max_distance(84.0F);

  desc.add_box(
      QVector3D(0.0F, 0.050F, 0.0F), QVector3D(1.38F, 0.050F, 1.16F), c.basalt);
  desc.add_box(
      QVector3D(0.0F, 0.155F, 0.0F), QVector3D(1.31F, 0.055F, 1.09F), c.sandstone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.245F, 0.0F), QVector3D(1.25F, 0.035F, 1.03F), c.sandstone);
  desc.add_box(QVector3D(0.0F, 0.290F, 0.0F),
               QVector3D(1.27F, 0.010F, 1.05F),
               c.sandstone_light);

  float const podium_y = 0.300F;

  for (float const joint_x : {-0.84F, -0.24F, 0.36F, 0.96F}) {
    desc.add_box(QVector3D(joint_x, 0.165F, 0.0F),
                 QVector3D(0.008F, 0.052F, 1.095F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(0.0F, 0.212F, 1.095F),
               QVector3D(1.30F, 0.006F, 0.006F),
               c.mortar,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  for (int step = 0; step < 4; ++step) {
    float const step_index = static_cast<float>(step);
    float const top = 0.074F * (step_index + 1.0F);
    desc.add_box(QVector3D(-1.600F + 0.086F * step_index, top * 0.5F, 0.0F),
                 QVector3D(0.045F, top * 0.5F, 0.62F),
                 (step % 2 == 0) ? c.sandstone_dark : c.sandstone,
                 k_building_state_mask_intact);
  }
  for (float const cheek : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-1.47F, 0.15F, cheek * 0.675F),
                 QVector3D(0.19F, 0.15F, 0.055F),
                 c.basalt,
                 k_building_state_mask_intact);
  }

  float const wall_h = 1.12F * height_multiplier;
  float const wall_y = podium_y + wall_h * 0.5F;

  desc.add_box(QVector3D(0.47F, wall_y, 0.0F),
               QVector3D(0.77F, wall_h * 0.5F, 0.92F),
               c.sandstone);

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.58F, wall_y, side * 0.80F),
                 QVector3D(0.30F, wall_h * 0.5F, 0.12F),
                 c.sandstone);
    desc.add_box(QVector3D(-0.86F, wall_y, side * 0.80F),
                 QVector3D(0.026F, wall_h * 0.5F, 0.14F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.58F, podium_y + wall_h + 0.030F, side * 0.80F),
                 QVector3D(0.33F, 0.030F, 0.15F),
                 c.sandstone_light,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.74F, 0.0F),
               QVector3D(0.30F, wall_h * 0.055F, 0.70F),
               c.cedar,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.83F, 0.0F),
               QVector3D(0.32F, wall_h * 0.040F, 0.72F),
               c.sandstone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.90F, 0.0F),
               QVector3D(0.34F, wall_h * 0.030F, 0.74F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.895F, podium_y + wall_h * 0.86F, 0.0F),
               QVector3D(0.014F, wall_h * 0.070F, 0.74F),
               c.basalt,
               k_building_state_mask_intact);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.86F, side * 0.752F),
                 QVector3D(0.34F, wall_h * 0.070F, 0.014F),
                 c.basalt,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  for (int rafter = 0; rafter < 5; ++rafter) {
    float const z = -0.56F + 0.28F * static_cast<float>(rafter);
    desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.70F, z),
                 QVector3D(0.30F, wall_h * 0.022F, 0.030F),
                 c.cedar_light,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  for (float const cz : {-0.34F, 0.34F}) {
    desc.add_box(QVector3D(-0.66F, podium_y + 0.030F, cz),
                 QVector3D(0.10F, 0.030F, 0.10F),
                 c.basalt);
    desc.add_cylinder(QVector3D(-0.66F, podium_y + 0.055F, cz),
                      QVector3D(-0.66F, podium_y + wall_h * 0.60F, cz),
                      0.070F,
                      c.basalt_light);
    desc.add_cylinder(QVector3D(-0.66F, podium_y + wall_h * 0.60F, cz),
                      QVector3D(-0.66F, podium_y + wall_h * 0.64F, cz),
                      0.086F,
                      c.bronze,
                      k_building_state_mask_intact,
                      BuildingLODMask::Full);
    desc.add_box(QVector3D(-0.66F, podium_y + wall_h * 0.67F, cz),
                 QVector3D(0.098F, wall_h * 0.032F, 0.098F),
                 c.basalt,
                 k_building_state_mask_intact);
  }

  for (int register_index = 0; register_index < 3; ++register_index) {
    float const band_y =
        podium_y + wall_h * (0.22F + 0.26F * static_cast<float>(register_index));
    QVector3D const band_color = (register_index == 1) ? c.oxblood : c.indigo;
    desc.add_box(QVector3D(0.47F, band_y, 0.925F),
                 QVector3D(0.74F, wall_h * 0.030F, 0.010F),
                 band_color,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.47F, band_y, -0.925F),
                 QVector3D(0.74F, wall_h * 0.030F, 0.010F),
                 band_color,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(1.245F, band_y, 0.0F),
                 QVector3D(0.010F, wall_h * 0.030F, 0.90F),
                 band_color,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }

  for (int course = 1; course < 7; ++course) {
    float const course_y = podium_y + wall_h * static_cast<float>(course) / 7.0F;
    desc.add_box(QVector3D(0.47F, course_y, 0.926F),
                 QVector3D(0.75F, 0.007F, 0.006F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(1.246F, course_y, 0.0F),
                 QVector3D(0.006F, 0.007F, 0.90F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  desc.add_box(QVector3D(-0.28F, podium_y + wall_h * 0.34F, 0.0F),
               QVector3D(0.034F, wall_h * 0.34F, 0.32F),
               c.soot,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.30F, podium_y + wall_h * 0.31F, 0.0F),
               QVector3D(0.028F, wall_h * 0.31F, 0.27F),
               c.cedar,
               k_building_state_mask_intact);
  for (int stud = 0; stud < 4; ++stud) {
    float const stud_y = podium_y + wall_h * (0.10F + 0.14F * static_cast<float>(stud));
    desc.add_box(QVector3D(-0.332F, stud_y, 0.0F),
                 QVector3D(0.008F, 0.016F, 0.25F),
                 c.bronze,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(-0.28F, podium_y + wall_h * 0.70F, 0.0F),
               QVector3D(0.052F, 0.040F, 0.40F),
               c.basalt,
               k_building_state_mask_intact);

  float const cornice_y = podium_y + wall_h;
  desc.add_box(QVector3D(0.47F, cornice_y + 0.026F, 0.0F),
               QVector3D(0.81F, 0.026F, 0.96F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.47F, cornice_y + 0.066F, 0.0F),
               QVector3D(0.87F, 0.024F, 1.02F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.47F, cornice_y + 0.098F, 0.0F),
               QVector3D(0.84F, 0.014F, 0.99F),
               c.basalt,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.47F, cornice_y + 0.124F, 0.0F),
               QVector3D(0.82F, 0.016F, 0.97F),
               c.sandstone,
               k_building_state_mask_intact);

  add_merlon_strip_z(
      [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
        desc.add_box(center, size, color, k_building_state_mask_intact);
      },
      1.245F,
      cornice_y + 0.190F,
      -0.84F,
      0.28F,
      7,
      QVector3D(0.046F, 0.052F, 0.072F),
      c.sandstone_dark);
  for (float const side : {-1.0F, 1.0F}) {
    add_merlon_strip_x(
        [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
          desc.add_box(center, size, color, k_building_state_mask_intact);
        },
        cornice_y + 0.190F,
        side * 0.925F,
        -0.24F,
        0.29F,
        6,
        QVector3D(0.072F, 0.052F, 0.046F),
        c.sandstone_dark);
  }

  desc.add_box(QVector3D(0.62F, cornice_y + 0.230F, 0.0F),
               QVector3D(0.44F, 0.108F, 0.48F),
               c.sandstone_dark,
               k_building_state_mask_intact);
  for (int band = 1; band < 3; ++band) {
    float const band_y = cornice_y + 0.122F + 0.072F * static_cast<float>(band);
    desc.add_box(QVector3D(0.62F, band_y, 0.485F),
                 QVector3D(0.42F, 0.007F, 0.006F),
                 c.mortar,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(0.62F, cornice_y + 0.354F, 0.0F),
               QVector3D(0.48F, 0.022F, 0.52F),
               c.sandstone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.62F, cornice_y + 0.318F, 0.0F),
               QVector3D(0.45F, 0.012F, 0.49F),
               c.indigo,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  float const pillar_height = 1.52F * height_multiplier;
  add_votive_pillar(desc, c, -1.16F, 0.62F, podium_y, pillar_height);
  add_votive_pillar(desc, c, -1.16F, -0.62F, podium_y, pillar_height);

  desc.add_box(QVector3D(-1.02F, podium_y + 0.024F, 0.0F),
               QVector3D(0.30F, 0.024F, 0.56F),
               c.sandstone_light,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(-1.02F, podium_y + 0.034F, 0.0F),
               QVector3D(0.19F, 0.010F, 0.32F),
               c.indigo,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(-1.02F, podium_y + 0.040F, 0.0F),
               QVector3D(0.09F, 0.010F, 0.15F),
               c.saffron,
               BuildingStateMask::Normal,
               BuildingLODMask::Full);

  add_incense_brazier(desc, c, QVector3D(-1.42F, 0.30F, 0.94F));
  add_incense_brazier(desc, c, QVector3D(-1.42F, 0.30F, -0.94F));

  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(1.18F, podium_y + wall_h * 0.52F, side * 0.60F),
                 QVector3D(0.078F, wall_h * 0.44F, 0.078F),
                 c.sandstone_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(1.18F, podium_y + wall_h * 0.99F, side * 0.60F),
                 QVector3D(0.094F, wall_h * 0.030F, 0.094F),
                 c.basalt,
                 k_building_state_mask_intact);
  }

  desc.add_palette_box(QVector3D(-0.332F, podium_y + wall_h * 0.76F, 0.0F),
                       QVector3D(0.014F, 0.042F, 0.30F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);
  desc.add_palette_box(QVector3D(1.256F, podium_y + wall_h * 0.58F, 0.0F),
                       QVector3D(0.014F, 0.24F, 0.40F),
                       k_temple_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);
  for (float const side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.62F, side * 0.930F),
                 QVector3D(0.16F, 0.020F, 0.010F),
                 c.bronze,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_palette_box(QVector3D(-0.58F, podium_y + wall_h * 0.44F, side * 0.936F),
                         QVector3D(0.13F, 0.17F, 0.010F),
                         k_temple_team_slot,
                         BuildingStateMask::Normal | BuildingStateMask::Damaged);
    desc.add_box(QVector3D(-0.58F, podium_y + wall_h * 0.26F, side * 0.938F),
                 QVector3D(0.13F, 0.014F, 0.010F),
                 c.gold,
                 BuildingStateMask::Normal,
                 BuildingLODMask::Full);
  }

  add_punic_tanit_relief(desc,
                         QVector3D(-0.352F, podium_y + wall_h * 0.46F, 0.0F),
                         BuildingFacadePlane::ZY,
                         0.40F,
                         c.gold,
                         c.basalt);
  add_punic_horned_crown(desc,
                         QVector3D(0.62F, cornice_y + 0.376F, 0.0F),
                         0.82F,
                         c.basalt_light,
                         c.bronze,
                         c.saffron);

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
      TempleRendererConfig{.nation_slug = "carthage",
                           .archetype = &temple_archetype,
                           .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 1.5F},
                           .selection = BuildingSelectionStyle{1.9F, 1.9F}});
}

} // namespace Render::GL::Carthage
