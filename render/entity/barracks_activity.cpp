#include "render/entity/barracks_activity.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "animation/clip_manifest.h"
#include "game/core/component.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/home_activity.h"
#include "render/frame/local_lighting.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"

namespace Render::GL {

namespace {

const QVector3D k_forge_offset{-0.34F, 0.0F, 0.02F};
const QVector3D k_anvil_offset{0.16F, 0.0F, -0.10F};

const QVector3D k_barrel_offset{0.52F, 0.0F, 0.12F};
constexpr float k_forge_top = 0.36F;

} // namespace

void add_barracks_grounds(BuildingArchetypeDesc& desc,
                          const BarracksGrounds& g,
                          const BarracksGroundsStyle& s) {
  const auto intact = k_building_state_mask_intact;
  const auto normal = BuildingStateMask::Normal;

  desc.add_box(QVector3D(g.parade_centre.x(), -0.10F, g.parade_centre.z()),
               QVector3D(g.parade_half_x, 0.16F, g.parade_half_z),
               s.earth);
  desc.add_box(QVector3D(g.parade_centre.x(), 0.065F, g.parade_centre.z()),
               QVector3D(g.parade_half_x - 0.06F, 0.012F, g.parade_half_z - 0.06F),
               s.sand);
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    const float x0 = g.parade_centre.x() - g.parade_half_x;
    const float x1 = g.parade_centre.x() + g.parade_half_x;
    const float z0 = g.parade_centre.z() - g.parade_half_z;
    const float z1 = g.parade_centre.z() + g.parade_half_z;
    desc.add_cylinder(QVector3D(x0, 0.09F, z0),
                      QVector3D(x1, 0.09F, z0),
                      0.045F,
                      s.timber_dark,
                      intact);
    desc.add_cylinder(QVector3D(x0, 0.09F, z1),
                      QVector3D(x1, 0.09F, z1),
                      0.045F,
                      s.timber_dark,
                      intact);
    desc.add_cylinder(QVector3D(x0, 0.09F, z0),
                      QVector3D(x0, 0.09F, z1),
                      0.045F,
                      s.timber_dark,
                      intact);
  }
  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    for (const float sx : {-1.0F, 1.0F}) {
      for (const float sz : {-1.0F, 1.0F}) {
        desc.add_box(QVector3D(g.parade_centre.x() + sx * g.parade_half_x,
                               0.05F,
                               g.parade_centre.z() + sz * g.parade_half_z),
                     QVector3D(0.07F, 0.05F, 0.07F),
                     s.stone,
                     intact);
      }
    }
  }
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    const float palus_x = g.parade_centre.x() - g.parade_half_x + 0.16F;
    for (const float pz : {-0.55F, 0.55F}) {
      const QVector3D foot(palus_x, 0.0F, g.parade_centre.z() + pz);
      desc.add_cylinder(
          foot, foot + QVector3D(0.0F, 0.78F, 0.0F), 0.05F, s.timber, intact);
      desc.add_box(foot + QVector3D(0.0F, 0.56F, 0.0F),
                   QVector3D(0.20F, 0.022F, 0.022F),
                   s.timber_dark,
                   normal);
    }
    const QVector3D dummy(palus_x, 0.0F, g.parade_centre.z() - 0.55F);
    desc.add_box(dummy + QVector3D(0.0F, 0.50F, 0.0F),
                 QVector3D(0.11F, 0.15F, 0.07F),
                 s.straw,
                 normal);
    desc.add_cylinder(dummy + QVector3D(0.0F, 0.78F, 0.0F),
                      dummy + QVector3D(0.0F, 0.92F, 0.0F),
                      0.075F,
                      s.iron * 1.6F,
                      normal);
    desc.add_cone(dummy + QVector3D(0.0F, 0.92F, 0.0F),
                  dummy + QVector3D(0.0F, 1.04F, 0.03F),
                  0.035F,
                  s.team_cloth,
                  normal);
  }

  const QVector3D c = g.smithy_centre;
  desc.add_box(
      QVector3D(c.x(), -0.10F, c.z()), QVector3D(0.86F, 0.13F, 0.56F), s.earth_dark);
  {
    BuildingPartMaterial const stone(desc, k_building_material_stone);
    const QVector3D forge = c + k_forge_offset;
    desc.add_box(forge + QVector3D(0.0F, 0.15F, 0.0F),
                 QVector3D(0.20F, 0.15F, 0.18F),
                 s.stone,
                 intact);
    desc.add_box(forge + QVector3D(0.0F, 0.325F, 0.0F),
                 QVector3D(0.22F, 0.025F, 0.20F),
                 s.stone_dark,
                 intact);
    desc.add_box(forge + QVector3D(0.0F, k_forge_top - 0.005F, 0.0F),
                 QVector3D(0.14F, 0.012F, 0.12F),
                 QVector3D(0.16F, 0.07F, 0.04F),
                 intact);
    desc.add_box(forge + QVector3D(-0.17F, 0.62F, -0.02F),
                 QVector3D(0.05F, 0.28F, 0.16F),
                 s.stone_dark,
                 normal);
    desc.add_box(forge + QVector3D(-0.04F, 0.92F, -0.02F),
                 QVector3D(0.18F, 0.03F, 0.18F),
                 s.stone_dark,
                 normal);
  }
  {
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    const QVector3D anvil = c + k_anvil_offset;
    desc.add_cylinder(
        anvil, anvil + QVector3D(0.0F, 0.26F, 0.0F), 0.10F, s.timber_dark, intact);
    const QVector3D barrel = c + k_barrel_offset;
    desc.add_cylinder(
        barrel, barrel + QVector3D(0.0F, 0.30F, 0.0F), 0.11F, s.timber, intact);
    desc.add_cylinder(barrel + QVector3D(0.0F, 0.285F, 0.0F),
                      barrel + QVector3D(0.0F, 0.30F, 0.0F),
                      0.095F,
                      QVector3D(0.20F, 0.33F, 0.40F),
                      normal);
    for (const float px : {-0.80F, -0.10F}) {
      for (const float pz : {-0.44F, 0.44F}) {
        const float top = pz > 0.0F ? 1.22F : 1.02F;
        desc.add_box(c + QVector3D(px, top * 0.5F, pz),
                     QVector3D(0.035F, top * 0.5F, 0.035F),
                     s.timber_dark,
                     intact);
      }
    }
    const QVector3D rack = c + QVector3D(0.62F, 0.0F, -0.30F);
    for (const float dz : {-0.22F, 0.22F}) {
      desc.add_box(rack + QVector3D(0.0F, 0.34F, dz),
                   QVector3D(0.022F, 0.34F, 0.022F),
                   s.timber,
                   intact);
    }
    desc.add_box(rack + QVector3D(0.0F, 0.64F, 0.0F),
                 QVector3D(0.022F, 0.018F, 0.25F),
                 s.timber,
                 intact);
    for (const float dz : {-0.14F, -0.05F, 0.04F, 0.13F}) {
      desc.add_rotated_box(rack + QVector3D(0.03F, 0.40F, dz),
                           QVector3D(0.006F, 0.26F, 0.016F),
                           QVector3D(0.0F, 0.0F, -9.0F),
                           s.iron * 2.4F,
                           normal);
    }
  }
  {
    BuildingPartMaterial const metal(desc, k_building_material_metal);
    const QVector3D anvil = c + k_anvil_offset;
    desc.add_box(anvil + QVector3D(0.0F, 0.31F, 0.0F),
                 QVector3D(0.10F, 0.05F, 0.055F),
                 s.iron,
                 intact);
    desc.add_cone(anvil + QVector3D(0.10F, 0.32F, 0.0F),
                  anvil + QVector3D(0.22F, 0.34F, 0.0F),
                  0.04F,
                  s.iron,
                  intact);
  }
  desc.add_rotated_box(c + QVector3D(-0.45F, 1.16F, 0.0F),
                       QVector3D(0.42F, 0.022F, 0.56F),
                       QVector3D(12.5F, 0.0F, 0.0F),
                       s.roof,
                       normal);
  for (const float rx : {-0.74F, -0.45F, -0.16F}) {
    desc.add_rotated_box(c + QVector3D(rx, 1.185F, 0.0F),
                         QVector3D(0.035F, 0.016F, 0.57F),
                         QVector3D(12.5F, 0.0F, 0.0F),
                         s.roof_dark,
                         normal);
  }
}

} // namespace Render::GL
