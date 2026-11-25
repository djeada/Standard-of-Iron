#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/carthage/barracks_authoring.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/frame/render_archetype.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Carthage {

using namespace barracks_detail;

namespace {

constexpr auto k_mask_intact = k_building_state_mask_intact;
constexpr auto k_mask_normal = BuildingStateMask::Normal;

constexpr float k_pi = 3.14159265F;

void add_palm(BuildingArchetypeDesc& desc,
              const CarthagePalette& c,
              const QVector3D& base,
              float height,
              float lean_x,
              int seed) {
  constexpr int k_segments = 4;
  QVector3D from = base;
  for (int i = 0; i < k_segments; ++i) {
    const float t0 = static_cast<float>(i) / k_segments;
    const float t1 = static_cast<float>(i + 1) / k_segments;
    const QVector3D to(base.x() + lean_x * t1 * t1,
                       base.y() + height * t1,
                       base.z() + 0.04F * std::sin(static_cast<float>(seed + i)));
    desc.add_cylinder(from,
                      to,
                      0.062F - 0.012F * t0,
                      weathered(c.palm_trunk, seed + i, 0.03F),
                      k_mask_intact);
    from = to;
  }
  const QVector3D crown = from;
  desc.add_cylinder(crown - QVector3D(0.0F, 0.02F, 0.0F),
                    crown + QVector3D(0.0F, 0.06F, 0.0F),
                    0.07F,
                    c.frond_dark,
                    k_mask_intact);
  for (int i = 0; i < 9; ++i) {
    const float yaw = static_cast<float>(i) * (2.0F * k_pi / 9.0F) +
                      0.3F * static_cast<float>(seed % 3);
    const float droop = (i % 2 == 0) ? 0.62F : 0.36F;
    const float len = (i % 2 == 0) ? 0.62F : 0.52F;
    const QVector3D dir(std::sin(yaw) * std::cos(droop),
                        -std::sin(droop),
                        std::cos(yaw) * std::cos(droop));
    const QVector3D start = crown + QVector3D(0.0F, 0.05F, 0.0F) + dir * 0.04F;
    desc.add_rotated_box(start + dir * (len * 0.5F),
                         QVector3D(0.048F, 0.008F, len * 0.5F),
                         QVector3D(droop * 180.0F / k_pi, yaw * 180.0F / k_pi, 0.0F),
                         (i % 2 == 0) ? c.frond : c.frond_dark,
                         k_mask_intact);
  }
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(crown + QVector3D(side * 0.09F, -0.06F, 0.0F),
                 QVector3D(0.035F, 0.05F, 0.035F),
                 c.date,
                 k_mask_normal);
  }
}

void add_palms(BuildingArchetypeDesc& desc, const CarthagePalette& c) {
  add_palm(desc, c, QVector3D(1.74F, 0.0F, 1.44F), 2.05F, -0.10F, 3);
  add_palm(desc, c, QVector3D(-1.80F, 0.0F, -0.42F), 2.30F, 0.08F, 7);
}

void add_fir(BuildingArchetypeDesc& desc,
             const CarthagePalette& c,
             const QVector3D& base,
             float height,
             int seed) {
  const QVector3D needle{0.13F, 0.24F, 0.17F};
  const QVector3D needle_dark{0.09F, 0.18F, 0.13F};
  const QVector3D snow{0.86F, 0.89F, 0.94F};
  desc.add_box(base + QVector3D(0.0F, 0.018F, 0.0F),
               QVector3D(0.24F, 0.018F, 0.24F),
               snow,
               k_mask_intact);
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, height * 0.30F, 0.0F),
                    0.055F,
                    weathered(c.wood_dark, seed, 0.03F),
                    k_mask_intact);
  constexpr int k_tiers = 3;
  for (int tier = 0; tier < k_tiers; ++tier) {
    const float t = static_cast<float>(tier) / k_tiers;
    const float tier_base = height * (0.18F + 0.25F * t);
    const float tier_top = tier_base + height * (0.46F - 0.08F * t);
    const float radius = 0.46F - 0.12F * static_cast<float>(tier);
    desc.add_cone(base + QVector3D(0.0F, tier_base, 0.0F),
                  base + QVector3D(0.0F, tier_top, 0.0F),
                  radius,
                  (tier % 2 == 0) ? needle : needle_dark,
                  k_mask_intact);
    const float cap_base = tier_base + (tier_top - tier_base) * 0.42F;
    desc.add_cone(base + QVector3D(0.0F, cap_base, 0.0F),
                  base + QVector3D(0.0F, tier_top + 0.015F, 0.0F),
                  radius * 0.48F,
                  snow,
                  k_mask_intact);
  }
}

void add_firs(BuildingArchetypeDesc& desc, const CarthagePalette& c) {
  add_fir(desc, c, QVector3D(1.74F, 0.0F, 1.44F), 1.85F, 3);
  add_fir(desc, c, QVector3D(-1.80F, 0.0F, -0.42F), 2.10F, 7);
}

auto build_barracks_greenery_desc_impl(bool cold_climate) -> BuildingArchetypeDesc {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc(cold_climate ? "carthage_barracks_firs"
                                          : "carthage_barracks_palms");
  if (cold_climate) {
    add_firs(desc, c);
  } else {
    add_palms(desc, c);
  }
  for (const QVector3D& base :
       {QVector3D(1.74F, 0.0F, 1.44F), QVector3D(-1.80F, 0.0F, -0.42F)}) {
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.16F, 0.0F),
                      0.06F,
                      c.stone_dark,
                      BuildingStateMask::Destroyed);
  }
  return desc;
}

} // namespace

auto build_barracks_palms_desc(BuildingState) -> BuildingArchetypeDesc {
  return build_barracks_greenery_desc_impl(false);
}

auto build_barracks_firs_desc(BuildingState) -> BuildingArchetypeDesc {
  return build_barracks_greenery_desc_impl(true);
}

} // namespace Render::GL::Carthage
