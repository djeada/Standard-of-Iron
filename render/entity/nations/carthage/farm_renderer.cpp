#include "farm_renderer.h"

#include <QVector3D>

#include <cmath>
#include <string>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/farm_renderer_common.h"
#include "render/entity/registry.h"
#include "render/render_archetype.h"

namespace Render::GL::Carthage {
namespace {

struct CarthageFarmPalette {
  QVector3D sandstone{0.78F, 0.68F, 0.51F};
  QVector3D sandstone_shade{0.63F, 0.53F, 0.39F};
  QVector3D sandstone_dark{0.37F, 0.31F, 0.23F};
  QVector3D mudbrick{0.63F, 0.43F, 0.28F};
  QVector3D mudbrick_dark{0.44F, 0.28F, 0.18F};
  QVector3D lime_wash{0.85F, 0.81F, 0.72F};
  QVector3D basalt{0.24F, 0.23F, 0.22F};
  QVector3D palm_wood{0.42F, 0.29F, 0.17F};
  QVector3D palm_dark{0.24F, 0.16F, 0.09F};
  QVector3D thatch{0.70F, 0.56F, 0.30F};
  QVector3D thatch_dark{0.50F, 0.39F, 0.21F};
  QVector3D indigo{0.18F, 0.21F, 0.36F};
  QVector3D oxblood{0.39F, 0.11F, 0.09F};
  QVector3D clay{0.63F, 0.40F, 0.24F};
  QVector3D clay_band{0.32F, 0.15F, 0.10F};
  QVector3D sack{0.72F, 0.62F, 0.44F};
  QVector3D straw{0.78F, 0.64F, 0.32F};
  QVector3D water{0.14F, 0.29F, 0.32F};
};

void add_mudbrick_boundary(BuildingArchetypeDesc& desc, const CarthageFarmPalette& c) {
  constexpr float k_wall_h = 0.075F;
  auto add_run = [&](const QVector3D& start, const QVector3D& end, int seed) {
    QVector3D const mid = (start + end) * 0.5F;
    QVector3D const span = end - start;
    QVector3D const half(span.x() != 0.0F ? std::abs(span.x()) * 0.5F : 0.032F,
                         k_wall_h * 0.5F,
                         span.z() != 0.0F ? std::abs(span.z()) * 0.5F : 0.032F);
    desc.add_box(mid + QVector3D(0.0F, k_wall_h * 0.5F, 0.0F),
                 half,
                 c.mudbrick,
                 BuildingStateMask::All);
    desc.add_box(mid + QVector3D(0.0F, k_wall_h + 0.008F, 0.0F),
                 QVector3D(half.x() + 0.004F, 0.008F, half.z() + 0.004F),
                 c.lime_wash,
                 k_building_state_mask_intact);

    int const posts =
        static_cast<int>(std::max(std::abs(span.x()), std::abs(span.z())) / 0.31F);
    for (int i = 0; i <= posts; ++i) {
      float const t =
          posts > 0 ? static_cast<float>(i) / static_cast<float>(posts) : 0.5F;
      QVector3D const post = start + span * t;
      float const rise = 0.11F + (decay_hash(seed + i) - 0.5F) * 0.02F;
      desc.add_rotated_box(
          post + QVector3D(0.0F, rise * 0.5F, 0.0F),
          QVector3D(0.045F, rise * 0.5F, 0.045F),
          QVector3D(0.0F, (decay_hash(seed + i * 5) - 0.5F) * 4.0F, 0.0F),
          c.mudbrick_dark * (0.94F + decay_hash(seed + i * 7) * 0.10F),
          BuildingStateMask::All);
    }
  };

  add_run(QVector3D(-0.94F, 0.0F, -0.94F), QVector3D(0.94F, 0.0F, -0.94F), 13);
  add_run(QVector3D(-0.94F, 0.0F, -0.94F), QVector3D(-0.94F, 0.0F, 0.94F), 43);
  add_run(QVector3D(0.94F, 0.0F, -0.94F), QVector3D(0.94F, 0.0F, 0.18F), 73);
  add_run(QVector3D(0.94F, 0.0F, 0.44F), QVector3D(0.94F, 0.0F, 0.94F), 93);
  add_run(QVector3D(-0.94F, 0.0F, 0.94F), QVector3D(0.94F, 0.0F, 0.94F), 123);
}

void add_storehouse(BuildingArchetypeDesc& desc, const CarthageFarmPalette& c) {
  QVector3D const centre(0.50F, 0.0F, 0.68F);
  constexpr float k_half_x = 0.30F;
  constexpr float k_half_z = 0.20F;
  constexpr float k_wall_h = 0.32F;

  desc.add_box(centre + QVector3D(0.0F, 0.03F, 0.0F),
               QVector3D(k_half_x + 0.05F, 0.03F, k_half_z + 0.05F),
               c.sandstone_dark,
               BuildingStateMask::All);
  desc.add_box(centre + QVector3D(0.0F, 0.075F, 0.0F),
               QVector3D(k_half_x + 0.02F, 0.018F, k_half_z + 0.02F),
               c.sandstone,
               BuildingStateMask::All);

  desc.add_box(centre + QVector3D(0.0F, 0.09F + k_wall_h * 0.5F, 0.0F),
               QVector3D(k_half_x, k_wall_h * 0.5F, k_half_z),
               c.lime_wash,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.09F + 0.06F, 0.0F),
               QVector3D(k_half_x + 0.004F, 0.06F, k_half_z + 0.004F),
               c.mudbrick,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.09F + k_wall_h - 0.05F, 0.0F),
               QVector3D(k_half_x + 0.004F, 0.022F, k_half_z + 0.004F),
               c.indigo,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.09F + k_wall_h - 0.095F, 0.0F),
               QVector3D(k_half_x + 0.003F, 0.012F, k_half_z + 0.003F),
               c.oxblood,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.09F + 0.08F, 0.0F),
               QVector3D(k_half_x * 0.9F, 0.08F, k_half_z * 0.9F),
               c.mudbrick_dark,
               BuildingStateMask::Destroyed);

  desc.add_box(centre + QVector3D(0.0F, 0.09F + 0.11F, -k_half_z - 0.004F),
               QVector3D(0.07F, 0.11F, 0.006F),
               c.palm_dark,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.09F + 0.225F, -k_half_z - 0.006F),
               QVector3D(0.09F, 0.012F, 0.008F),
               c.palm_wood,
               k_building_state_mask_intact);

  float const roof_y = 0.09F + k_wall_h;
  desc.add_box(centre + QVector3D(0.0F, roof_y + 0.012F, 0.0F),
               QVector3D(k_half_x + 0.03F, 0.012F, k_half_z + 0.03F),
               c.palm_wood,
               k_building_state_mask_intact);
  for (int i = 0; i < 6; ++i) {
    float const x = centre.x() - k_half_x + 0.03F + static_cast<float>(i) * 0.108F;
    desc.add_cylinder(QVector3D(x, roof_y + 0.024F, centre.z() - k_half_z - 0.05F),
                      QVector3D(x, roof_y + 0.024F, centre.z() + k_half_z + 0.05F),
                      0.012F,
                      c.palm_dark,
                      k_building_state_mask_intact);
  }
  desc.add_box(centre + QVector3D(0.0F, roof_y + 0.045F, 0.0F),
               QVector3D(k_half_x + 0.03F, 0.014F, k_half_z + 0.03F),
               c.thatch,
               k_building_state_mask_intact);
  for (float const sx : {-1.0F, 1.0F}) {
    desc.add_box(centre + QVector3D(sx * (k_half_x + 0.015F), roof_y + 0.075F, 0.0F),
                 QVector3D(0.015F, 0.03F, k_half_z + 0.03F),
                 c.mudbrick,
                 k_building_state_mask_intact);
  }
  desc.add_box(centre + QVector3D(0.0F, roof_y + 0.075F, k_half_z + 0.015F),
               QVector3D(k_half_x + 0.03F, 0.03F, 0.015F),
               c.mudbrick,
               k_building_state_mask_intact);

  desc.add_cylinder(centre + QVector3D(-k_half_x - 0.10F, 0.09F, 0.06F),
                    centre + QVector3D(-k_half_x + 0.03F, roof_y + 0.09F, 0.06F),
                    0.012F,
                    c.palm_wood,
                    k_building_state_mask_intact);
  desc.add_cylinder(centre + QVector3D(-k_half_x - 0.10F, 0.09F, -0.02F),
                    centre + QVector3D(-k_half_x + 0.03F, roof_y + 0.09F, -0.02F),
                    0.012F,
                    c.palm_wood,
                    k_building_state_mask_intact);
  for (int i = 1; i < 6; ++i) {
    float const t = static_cast<float>(i) / 6.0F;
    QVector3D const rung =
        centre + QVector3D(-k_half_x - 0.10F + 0.13F * t, 0.09F + (roof_y)*t, 0.02F);
    desc.add_cylinder(rung + QVector3D(0.0F, 0.0F, -0.04F),
                      rung + QVector3D(0.0F, 0.0F, 0.04F),
                      0.008F,
                      c.palm_dark,
                      k_building_state_mask_intact);
  }

  for (int i = 0; i < 4; ++i) {
    float const x = centre.x() - k_half_x + 0.05F + static_cast<float>(i) * 0.115F;
    QVector3D const base(x, 0.09F, centre.z() + k_half_z + 0.11F);
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.025F, 0.0F),
                      0.03F,
                      c.clay_band,
                      k_building_state_mask_intact);
    desc.add_cylinder(base + QVector3D(0.0F, 0.025F, 0.0F),
                      base + QVector3D(0.0F, 0.13F, 0.0F),
                      0.058F,
                      c.clay,
                      k_building_state_mask_intact);
    desc.add_cone(base + QVector3D(0.0F, 0.12F, 0.0F),
                  base + QVector3D(0.0F, 0.19F, 0.0F),
                  0.059F,
                  c.clay,
                  k_building_state_mask_intact);
    desc.add_cylinder(base + QVector3D(0.0F, 0.15F, 0.0F),
                      base + QVector3D(0.0F, 0.165F, 0.0F),
                      0.045F,
                      (i % 2 == 0) ? c.indigo : c.oxblood,
                      k_building_state_mask_intact);
  }

  add_ruin_dressing(desc,
                    RuinDressing{.center = centre,
                                 .extent = QVector3D(k_half_x, 0.0F, k_half_z),
                                 .stone = c.sandstone,
                                 .stone_dark = c.sandstone_dark,
                                 .timber = c.palm_dark,
                                 .ground_y = 0.09F,
                                 .scale = 0.7F,
                                 .seed = 223,
                                 .collapsed_roof = false});
}

void add_threshing_floor(BuildingArchetypeDesc& desc, const CarthageFarmPalette& c) {
  QVector3D const centre(-0.52F, 0.0F, 0.70F);
  desc.add_cylinder(centre,
                    centre + QVector3D(0.0F, 0.035F, 0.0F),
                    0.24F,
                    c.sandstone_shade,
                    BuildingStateMask::All);
  desc.add_cylinder(centre + QVector3D(0.0F, 0.035F, 0.0F),
                    centre + QVector3D(0.0F, 0.045F, 0.0F),
                    0.215F,
                    c.sandstone,
                    k_building_state_mask_intact);
  for (int i = 0; i < 10; ++i) {
    float const angle = static_cast<float>(i) * 0.6283F;
    QVector3D const stone =
        centre + QVector3D(std::cos(angle) * 0.235F, 0.05F, std::sin(angle) * 0.235F);
    desc.add_box(
        stone, QVector3D(0.03F, 0.02F, 0.03F), c.basalt, BuildingStateMask::All);
  }
  desc.add_cylinder(centre + QVector3D(0.05F, 0.045F, 0.0F),
                    centre + QVector3D(0.05F, 0.10F, 0.0F),
                    0.13F,
                    c.straw,
                    k_building_state_mask_intact);
  desc.add_cone(centre + QVector3D(0.05F, 0.09F, 0.0F),
                centre + QVector3D(0.05F, 0.19F, 0.0F),
                0.13F,
                c.straw * 0.95F,
                k_building_state_mask_intact);
  desc.add_cylinder(centre + QVector3D(-0.16F, 0.045F, 0.10F),
                    centre + QVector3D(0.02F, 0.10F, 0.14F),
                    0.012F,
                    c.palm_wood,
                    k_building_state_mask_intact);
}

void add_well(BuildingArchetypeDesc& desc, const CarthageFarmPalette& c) {
  QVector3D const centre(0.62F, 0.0F, -0.62F);
  desc.add_cylinder(centre,
                    centre + QVector3D(0.0F, 0.13F, 0.0F),
                    0.11F,
                    c.sandstone_shade,
                    BuildingStateMask::All);
  constexpr int k_coping_stones = 12;
  for (int stone = 0; stone < k_coping_stones; ++stone) {
    float const angle = static_cast<float>(stone) * 6.2831853F /
                        static_cast<float>(k_coping_stones);
    QVector3D const stone_center =
        centre + QVector3D(std::cos(angle) * 0.095F,
                           0.14F + (decay_hash(601 + stone) - 0.5F) * 0.006F,
                           std::sin(angle) * 0.095F);
    desc.add_rotated_box(stone_center,
                         QVector3D(0.034F, 0.014F, 0.025F),
                         QVector3D(0.0F, 90.0F - angle * 57.29578F, 0.0F),
                         stone % 3 == 0 ? c.sandstone_shade : c.sandstone,
                         k_building_state_mask_intact);
  }
  desc.add_cylinder(centre + QVector3D(0.0F, 0.11F, 0.0F),
                    centre + QVector3D(0.0F, 0.135F, 0.0F),
                    0.075F,
                    c.water,
                    BuildingStateMask::All);
  for (float const sx : {-1.0F, 1.0F}) {
    desc.add_cylinder(centre + QVector3D(sx * 0.10F, 0.10F, 0.0F),
                      centre + QVector3D(sx * 0.10F, 0.40F, 0.0F),
                      0.014F,
                      c.palm_wood,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(centre + QVector3D(-0.11F, 0.36F, 0.0F),
                    centre + QVector3D(0.11F, 0.36F, 0.0F),
                    0.02F,
                    c.palm_dark,
                    k_building_state_mask_intact);
  desc.add_cylinder(centre + QVector3D(0.0F, 0.36F, 0.0F),
                    centre + QVector3D(0.0F, 0.20F, 0.0F),
                    0.004F,
                    c.basalt,
                    k_building_state_mask_intact);
  desc.add_cylinder(centre + QVector3D(0.0F, 0.16F, 0.0F),
                    centre + QVector3D(0.0F, 0.21F, 0.0F),
                    0.03F,
                    c.clay,
                    k_building_state_mask_intact);
}

void add_grain_sacks(BuildingArchetypeDesc& desc, const CarthageFarmPalette& c) {
  QVector3D const base(0.06F, 0.0F, 0.72F);
  int seed = 501;
  for (int i = 0; i < 5; ++i) {
    ++seed;
    float const x = base.x() + (static_cast<float>(i % 3) - 1.0F) * 0.10F;
    float const z = base.z() + (i >= 3 ? 0.06F : -0.03F);
    float const y = i >= 3 ? 0.10F : 0.05F;
    QVector3D const centre(x, y, z);
    desc.add_box(centre,
                 QVector3D(0.048F, 0.05F, 0.038F),
                 c.sack * (0.94F + decay_hash(seed) * 0.1F),
                 k_building_state_mask_intact);
    desc.add_cylinder(centre + QVector3D(0.0F, 0.05F, 0.0F),
                      centre + QVector3D(0.0F, 0.07F, 0.0F),
                      0.02F,
                      c.palm_dark,
                      k_building_state_mask_intact);
  }
}

auto build_farm_archetype(BuildingState state, int stage) -> RenderArchetype {
  CarthageFarmPalette const c;
  BuildingArchetypeDesc desc("carthage_farm_stage_" + std::to_string(stage));

  FarmFieldPalette field_palette;
  field_palette.soil = QVector3D(0.46F, 0.33F, 0.19F);
  field_palette.soil_dark = QVector3D(0.32F, 0.22F, 0.13F);
  field_palette.soil_light = QVector3D(0.58F, 0.44F, 0.26F);
  add_farm_field(desc,
                 FarmFieldSpec{.center = QVector3D(-0.08F, 0.0F, -0.22F),
                               .half_x = 0.80F,
                               .half_z = 0.62F,
                               .ground_y = 0.0F,
                               .rows = 20,
                               .stalks_per_row = 34,
                               .seed = 19,
                               .rows_along_x = false},
                 field_palette,
                 stage);

  add_mudbrick_boundary(desc, c);
  add_storehouse(desc, c);
  add_threshing_floor(desc, c);
  add_well(desc, c);
  add_grain_sacks(desc, c);
  add_farm_scarecrow(
      desc, QVector3D(-0.62F, 0.03F, -0.62F), c.palm_dark, c.indigo, c.straw);

  add_scorch_patch(desc,
                   ScorchPatch{.center = QVector3D(-0.08F, 0.0F, -0.2F),
                               .radius = 0.55F,
                               .ground_y = 0.05F,
                               .count = 6,
                               .seed = 431});
  add_charred_beams(desc,
                    CharredBeams{.center = QVector3D(0.50F, 0.0F, 0.68F),
                                 .extent = QVector3D(0.30F, 0.0F, 0.20F),
                                 .timber = c.palm_dark * 0.5F,
                                 .ground_y = 0.10F,
                                 .length = 0.5F,
                                 .radius = 0.02F,
                                 .count = 4,
                                 .seed = 457});

  return build_building_archetype(desc, state);
}

auto farm_archetype(BuildingState state, int stage) -> const RenderArchetype& {
  static const auto k_table = build_farm_archetype_table(build_farm_archetype);
  return farm_archetype_from_table(k_table, state, stage);
}

} // namespace

void register_farm_renderer(EntityRendererRegistry& registry) {
  register_farm_renderer_variant(
      registry,
      FarmRendererConfig{.nation_slug = "carthage",
                         .archetype = &farm_archetype,
                         .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 0.9F},
                         .selection = BuildingSelectionStyle{2.2F, 2.2F}});
}

} // namespace Render::GL::Carthage
