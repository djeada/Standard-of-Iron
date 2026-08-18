#include "farm_renderer_common.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "building_decay.h"
#include "game/core/component.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

auto hash01(int seed) -> float {
  return decay_hash(seed * 7 + 3);
}

auto lerp(const QVector3D& a, const QVector3D& b, float t) -> QVector3D {
  return a + (b - a) * std::clamp(t, 0.0F, 1.0F);
}

auto state_index(BuildingState state) -> std::size_t {
  switch (state) {
  case BuildingState::Normal:
    return 0;
  case BuildingState::Damaged:
    return 1;
  case BuildingState::Destroyed:
    return 2;
  }
  return 0;
}

struct StalkLook {
  float height{0.0F};
  float radius{0.0F};
  float lean{0.0F};
  float head_length{0.0F};
  float head_radius{0.0F};
  bool has_head{false};
  bool leaf{false};
};

auto stalk_look(int stage) -> StalkLook {
  switch (stage) {
  case 1:
    return {.height = 0.040F, .radius = 0.010F, .lean = 0.006F, .leaf = false};
  case 2:
    return {.height = 0.105F,
            .radius = 0.011F,
            .lean = 0.014F,
            .head_length = 0.0F,
            .head_radius = 0.0F,
            .has_head = false,
            .leaf = true};
  case 3:
    return {.height = 0.165F,
            .radius = 0.012F,
            .lean = 0.022F,
            .head_length = 0.045F,
            .head_radius = 0.016F,
            .has_head = true,
            .leaf = true};
  case 4:
    return {.height = 0.200F,
            .radius = 0.012F,
            .lean = 0.038F,
            .head_length = 0.058F,
            .head_radius = 0.019F,
            .has_head = true,
            .leaf = false};
  default:
    return {};
  }
}

void add_soil_bed(BuildingArchetypeDesc& desc,
                  const FarmFieldSpec& spec,
                  const FarmFieldPalette& palette,
                  int stage) {
  const float y = spec.ground_y;

  desc.add_box(spec.center + QVector3D(0.0F, y + 0.012F, 0.0F),
               QVector3D(spec.half_x + 0.05F, 0.012F, spec.half_z + 0.05F),
               palette.soil_dark,
               BuildingStateMask::All);
  desc.add_box(spec.center + QVector3D(0.0F, y + 0.028F, 0.0F),
               QVector3D(spec.half_x, 0.006F, spec.half_z),
               palette.soil,
               k_building_state_mask_intact);
  desc.add_box(spec.center + QVector3D(0.0F, y + 0.028F, 0.0F),
               QVector3D(spec.half_x, 0.006F, spec.half_z),
               palette.ash,
               BuildingStateMask::Destroyed);

  const int rows = std::max(1, spec.rows);
  const float row_span = spec.rows_along_x ? spec.half_z : spec.half_x;
  const float row_step = (row_span * 2.0F) / static_cast<float>(rows);
  const float ridge_length = spec.rows_along_x ? spec.half_x : spec.half_z;
  const float ridge_height = stage == 0 ? 0.030F : 0.022F;

  for (int row = 0; row < rows; ++row) {
    const float offset = -row_span + row_step * (static_cast<float>(row) + 0.5F);
    const float wobble = (hash01(spec.seed * 31 + row) - 0.5F) * 0.012F;
    QVector3D const centre =
        spec.rows_along_x
            ? spec.center + QVector3D(wobble, y + 0.034F + ridge_height * 0.5F, offset)
            : spec.center + QVector3D(offset, y + 0.034F + ridge_height * 0.5F, wobble);
    QVector3D const extent =
        spec.rows_along_x
            ? QVector3D(ridge_length * 0.985F, ridge_height * 0.5F, row_step * 0.24F)
            : QVector3D(row_step * 0.24F, ridge_height * 0.5F, ridge_length * 0.985F);
    desc.add_box(centre, extent, palette.soil_light, k_building_state_mask_intact);
    desc.add_box(centre, extent, palette.ash * 1.35F, BuildingStateMask::Destroyed);
  }
}

void add_stubble(BuildingArchetypeDesc& desc,
                 const FarmFieldSpec& spec,
                 const FarmFieldPalette& palette) {
  const int rows = std::max(1, spec.rows);
  const int per_row = std::max(1, spec.stalks_per_row / 2);
  const float row_span = spec.rows_along_x ? spec.half_z : spec.half_x;
  const float run_span = spec.rows_along_x ? spec.half_x : spec.half_z;
  const float row_step = (row_span * 2.0F) / static_cast<float>(rows);
  const float run_step = (run_span * 2.0F) / static_cast<float>(per_row);
  int seed = spec.seed * 101;
  for (int row = 0; row < rows; ++row) {
    const float offset = -row_span + row_step * (static_cast<float>(row) + 0.5F);
    for (int i = 0; i < per_row; ++i) {
      ++seed;
      const float run = -run_span + run_step * (static_cast<float>(i) + 0.5F) +
                        (hash01(seed) - 0.5F) * run_step * 0.6F;
      QVector3D const base =
          spec.rows_along_x
              ? spec.center + QVector3D(run, spec.ground_y + 0.05F, offset)
              : spec.center + QVector3D(offset, spec.ground_y + 0.05F, run);
      const float height = 0.03F + hash01(seed + 5) * 0.025F;
      desc.add_cylinder(base,
                        base + QVector3D(0.0F, height, 0.0F),
                        0.011F,
                        palette.stubble,
                        k_building_state_mask_intact);
      desc.add_cylinder(base,
                        base + QVector3D(0.0F, height * 0.7F, 0.0F),
                        0.011F,
                        palette.ash,
                        BuildingStateMask::Destroyed);
    }
  }
}

void add_crop(BuildingArchetypeDesc& desc,
              const FarmFieldSpec& spec,
              const FarmFieldPalette& palette,
              int stage) {
  const StalkLook look = stalk_look(stage);
  if (look.height <= 0.0F) {
    return;
  }
  const int rows = std::max(1, spec.rows);
  const int per_row = std::max(1, spec.stalks_per_row);
  const float row_span = spec.rows_along_x ? spec.half_z : spec.half_x;
  const float run_span = spec.rows_along_x ? spec.half_x : spec.half_z;
  const float row_step = (row_span * 2.0F) / static_cast<float>(rows);
  const float run_step = (run_span * 2.0F) / static_cast<float>(per_row);

  const float ripe_t = stage >= 4 ? 1.0F : (stage == 3 ? 0.45F : 0.0F);
  const QVector3D stalk_base_colour =
      stage <= 2 ? (stage == 1 ? palette.sprout : palette.leaf)
                 : lerp(palette.stalk_green, palette.stalk_gold, ripe_t);
  const QVector3D stalk_shade_colour =
      stage <= 2 ? palette.leaf_dark
                 : lerp(palette.leaf_dark, palette.stalk_gold_dark, ripe_t);
  const QVector3D head_colour = lerp(palette.head_green, palette.head_gold, ripe_t);
  const QVector3D head_light =
      lerp(palette.head_green, palette.head_gold_light, ripe_t);

  int seed = spec.seed * 977 + stage * 13;
  for (int row = 0; row < rows; ++row) {
    const float offset = -row_span + row_step * (static_cast<float>(row) + 0.5F);
    for (int i = 0; i < per_row; ++i) {
      ++seed;
      const float jitter_run = (hash01(seed) - 0.5F) * run_step * 0.55F;
      const float jitter_row = (hash01(seed + 17) - 0.5F) * row_step * 0.28F;
      const float run =
          -run_span + run_step * (static_cast<float>(i) + 0.5F) + jitter_run;
      const float scale = 0.82F + hash01(seed + 29) * 0.36F;
      const float lean_angle = hash01(seed + 41) * 6.2831853F;
      const float lean = look.lean * (0.4F + hash01(seed + 53));

      QVector3D const base =
          spec.rows_along_x
              ? spec.center +
                    QVector3D(run, spec.ground_y + 0.045F, offset + jitter_row)
              : spec.center +
                    QVector3D(offset + jitter_row, spec.ground_y + 0.045F, run);
      QVector3D const tip = base + QVector3D(std::cos(lean_angle) * lean,
                                             look.height * scale,
                                             std::sin(lean_angle) * lean);
      const bool shaded = hash01(seed + 61) < 0.35F;
      QVector3D const stalk_colour = shaded ? stalk_shade_colour : stalk_base_colour;

      if (stage == 1) {
        desc.add_cone(
            base, tip, look.radius * 1.9F, stalk_colour, k_building_state_mask_intact);
        continue;
      }

      desc.add_cylinder(
          base, tip, look.radius, stalk_colour, k_building_state_mask_intact);

      if (look.leaf) {
        const float leaf_h = look.height * scale * (0.35F + hash01(seed + 71) * 0.2F);
        QVector3D const leaf_root = base + QVector3D(0.0F, leaf_h, 0.0F);
        const float leaf_angle = hash01(seed + 83) * 6.2831853F;
        QVector3D const leaf_tip = leaf_root + QVector3D(std::cos(leaf_angle) * 0.035F,
                                                         0.022F,
                                                         std::sin(leaf_angle) * 0.035F);
        desc.add_cone(leaf_tip,
                      leaf_root,
                      0.008F,
                      palette.leaf_dark,
                      k_building_state_mask_intact);
      }

      if (look.has_head) {
        QVector3D const lean_dir(std::cos(lean_angle), 0.0F, std::sin(lean_angle));
        const float droop = ripe_t * 0.35F;
        QVector3D const head_top =
            tip + QVector3D(lean_dir.x() * look.head_length * droop,
                            look.head_length * (1.0F - droop * 0.5F),
                            lean_dir.z() * look.head_length * droop);
        const bool light = hash01(seed + 97) < 0.4F;
        desc.add_cylinder(tip,
                          head_top,
                          look.head_radius * scale,
                          light ? head_light : head_colour,
                          k_building_state_mask_intact);
        if (stage >= 4) {
          desc.add_cone(head_top,
                        head_top + QVector3D(lean_dir.x() * 0.018F,
                                             0.026F,
                                             lean_dir.z() * 0.018F),
                        look.head_radius * 0.55F,
                        head_light,
                        k_building_state_mask_intact);
        }
      }
    }
  }
}

} // namespace

void add_farm_field(BuildingArchetypeDesc& desc,
                    const FarmFieldSpec& spec,
                    const FarmFieldPalette& palette,
                    int stage) {
  const int clamped_stage = std::clamp(stage, 0, k_farm_render_stage_count - 1);
  add_soil_bed(desc, spec, palette, clamped_stage);
  if (clamped_stage == 0) {
    add_stubble(desc, spec, palette);
    return;
  }
  add_crop(desc, spec, palette, clamped_stage);
}

void add_farm_scarecrow(BuildingArchetypeDesc& desc,
                        const QVector3D& base,
                        const QVector3D& timber,
                        const QVector3D& cloth,
                        const QVector3D& straw) {
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.52F, 0.0F),
                    0.018F,
                    timber,
                    k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(-0.16F, 0.40F, 0.0F),
                    base + QVector3D(0.16F, 0.40F, 0.0F),
                    0.014F,
                    timber,
                    k_building_state_mask_intact);
  desc.add_box(base + QVector3D(0.0F, 0.36F, 0.0F),
               QVector3D(0.075F, 0.085F, 0.045F),
               cloth,
               k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(-0.16F, 0.40F, 0.0F),
                    base + QVector3D(-0.19F, 0.32F, 0.0F),
                    0.02F,
                    straw,
                    k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(0.16F, 0.40F, 0.0F),
                    base + QVector3D(0.19F, 0.32F, 0.0F),
                    0.02F,
                    straw,
                    k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(0.0F, 0.45F, 0.0F),
                    base + QVector3D(0.0F, 0.53F, 0.0F),
                    0.045F,
                    straw,
                    k_building_state_mask_intact);
  desc.add_cone(base + QVector3D(0.0F, 0.52F, 0.0F),
                base + QVector3D(0.0F, 0.60F, 0.0F),
                0.075F,
                straw * 0.85F,
                k_building_state_mask_intact);
  desc.add_cylinder(base,
                    base + QVector3D(0.06F, 0.18F, 0.02F),
                    0.018F,
                    timber * 0.6F,
                    BuildingStateMask::Destroyed);
}

auto farm_archetype_from_table(
    const std::array<std::array<RenderArchetype, 3>, k_farm_render_stage_count>& table,
    BuildingState state,
    int stage) -> const RenderArchetype& {
  const int clamped_stage = std::clamp(stage, 0, k_farm_render_stage_count - 1);
  return table[static_cast<std::size_t>(clamped_stage)][state_index(state)];
}

void register_farm_renderer_variant(EntityRendererRegistry& registry,
                                    const FarmRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "farm",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        int stage = 0;
        if (const auto* farm =
                ctx.entity->get_component<Engine::Core::FarmComponent>()) {
          stage = farm->growth_stage();
        }
        const BuildingState state = resolve_building_state(ctx);
        submit_building_instance(out, ctx, config.archetype(state, stage));
        draw_building_health_bar(out, ctx, config.health_bar);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
