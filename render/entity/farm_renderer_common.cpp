#include "farm_renderer_common.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "building_decay.h"
#include "game/core/component.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

constexpr float k_tau = 6.2831853F;

auto hash01(int seed) -> float {
  return decay_hash(seed * 7 + 3);
}

auto wave(float u, float v, float freq, float phase) -> float {
  return std::sin(u * freq + phase) * std::cos(v * freq * 0.83F - phase * 0.61F);
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
  float awn_length{0.0F};
  float awn_chance{0.0F};
  float droop{0.0F};
  float leaf_length{0.0F};
  float leaf_chance{0.0F};
  int tillers{1};
  bool has_head{false};
  bool leaf{false};
};

auto stalk_look(int stage) -> StalkLook {
  switch (stage) {
  case 1:
    return {.height = 0.050F, .radius = 0.0042F, .lean = 0.010F};
  case 2:
    return {.height = 0.128F,
            .radius = 0.0036F,
            .lean = 0.022F,
            .leaf_length = 0.030F,
            .leaf_chance = 0.90F,
            .tillers = 3,
            .leaf = true};
  case 3:
    return {.height = 0.198F,
            .radius = 0.0032F,
            .lean = 0.034F,
            .head_length = 0.031F,
            .head_radius = 0.0047F,
            .awn_length = 0.018F,
            .awn_chance = 0.32F,
            .droop = 0.16F,
            .leaf_length = 0.034F,
            .leaf_chance = 0.72F,
            .tillers = 3,
            .has_head = true,
            .leaf = true};
  case 4:
    return {.height = 0.228F,
            .radius = 0.0027F,
            .lean = 0.058F,
            .head_length = 0.036F,
            .head_radius = 0.0053F,
            .awn_length = 0.030F,
            .awn_chance = 0.42F,
            .droop = 0.62F,
            .leaf_length = 0.030F,
            .leaf_chance = 0.36F,
            .tillers = 3,
            .has_head = true,
            .leaf = true};
  default:
    return {};
  }
}

void add_soil_bed(BuildingArchetypeDesc& desc,
                  const FarmFieldSpec& spec,
                  const FarmFieldPalette& palette,
                  int stage) {
  const float y = spec.ground_y;
  const float litter_t = stage >= 4 ? 0.62F : (stage == 3 ? 0.34F : 0.0F);
  const QVector3D litter = palette.stubble * 0.46F;
  const QVector3D bed = lerp(palette.soil, litter, litter_t);
  const QVector3D bed_dark = lerp(palette.soil_dark, litter * 0.74F, litter_t);
  const QVector3D bed_light = lerp(palette.soil_light, litter * 1.12F, litter_t);

  desc.add_box(spec.center + QVector3D(0.0F, y + 0.012F, 0.0F),
               QVector3D(spec.half_x + 0.05F, 0.012F, spec.half_z + 0.05F),
               palette.soil_dark,
               BuildingStateMask::All);
  desc.add_box(spec.center + QVector3D(0.0F, y + 0.028F, 0.0F),
               QVector3D(spec.half_x, 0.006F, spec.half_z),
               bed,
               k_building_state_mask_intact);
  desc.add_box(spec.center + QVector3D(0.0F, y + 0.028F, 0.0F),
               QVector3D(spec.half_x, 0.006F, spec.half_z),
               palette.ash,
               BuildingStateMask::Destroyed);

  for (int patch = 0; patch < 24; ++patch) {
    const int patch_seed = spec.seed * 613 + patch * 37;
    const float px = (hash01(patch_seed) - 0.5F) * 1.82F * spec.half_x;
    const float pz = (hash01(patch_seed + 5) - 0.5F) * 1.82F * spec.half_z;
    const float rx = 0.018F + hash01(patch_seed + 11) * 0.045F;
    const float rz = 0.014F + hash01(patch_seed + 17) * 0.035F;
    const float tone = hash01(patch_seed + 23);
    desc.add_rotated_box(spec.center + QVector3D(px, y + 0.034F, pz),
                         QVector3D(rx, 0.004F + hash01(patch_seed + 29) * 0.004F, rz),
                         QVector3D(0.0F, hash01(patch_seed + 31) * 180.0F, 0.0F),
                         tone < 0.62F ? bed_dark : bed_light,
                         k_building_state_mask_intact);
  }

  const bool intact_ridges = stage < 4;
  const int rows = std::max(1, spec.rows);
  const float row_span = spec.rows_along_x ? spec.half_z : spec.half_x;
  const float row_step = (row_span * 2.0F) / static_cast<float>(rows);
  const float ridge_length = spec.rows_along_x ? spec.half_x : spec.half_z;
  const float ridge_radius = stage == 0 ? 0.016F : 0.011F;
  const QVector3D ridge_colour = stage >= 3 ? bed_dark : bed_light;
  constexpr int k_ridge_segments = 5;

  for (int row = 0; row < rows; ++row) {
    const float offset = -row_span + row_step * (static_cast<float>(row) + 0.5F);
    for (int segment = 0; segment < k_ridge_segments; ++segment) {
      const int ridge_seed = spec.seed * 31 + row * 11 + segment;
      const float a = -ridge_length + 2.0F * ridge_length *
                                          static_cast<float>(segment) /
                                          static_cast<float>(k_ridge_segments);
      const float b = -ridge_length + 2.0F * ridge_length *
                                          static_cast<float>(segment + 1) /
                                          static_cast<float>(k_ridge_segments);
      const float wobble_a = (hash01(ridge_seed) - 0.5F) * row_step * 0.14F;
      const float wobble_b = (hash01(ridge_seed + 1) - 0.5F) * row_step * 0.14F;
      QVector3D const start =
          spec.rows_along_x ? spec.center + QVector3D(a, y + 0.034F, offset + wobble_a)
                            : spec.center + QVector3D(offset + wobble_a, y + 0.034F, a);
      QVector3D const end =
          spec.rows_along_x ? spec.center + QVector3D(b, y + 0.034F, offset + wobble_b)
                            : spec.center + QVector3D(offset + wobble_b, y + 0.034F, b);
      const float radius = ridge_radius * (0.82F + hash01(ridge_seed + 7) * 0.30F);
      if (intact_ridges) {
        desc.add_cylinder(
            start, end, radius, ridge_colour, k_building_state_mask_intact);
      }
      desc.add_cylinder(
          start, end, radius, palette.ash * 1.35F, BuildingStateMask::Destroyed);
    }
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
      if (hash01(seed + 19) < 0.14F) {
        continue;
      }
      const float run = -run_span + run_step * (static_cast<float>(i) + 0.5F) +
                        (hash01(seed) - 0.5F) * run_step * 0.6F;
      const float row_jitter = (hash01(seed + 3) - 0.5F) * row_step * 0.30F;
      QVector3D const base =
          spec.rows_along_x
              ? spec.center +
                    QVector3D(run, spec.ground_y + 0.045F, offset + row_jitter)
              : spec.center +
                    QVector3D(offset + row_jitter, spec.ground_y + 0.045F, run);
      const float height = 0.03F + hash01(seed + 5) * 0.025F;
      const float angle = hash01(seed + 11) * 6.2831853F;
      QVector3D const cut_tip =
          base + QVector3D(std::cos(angle) * 0.008F, height, std::sin(angle) * 0.008F);
      desc.add_cylinder(base,
                        cut_tip,
                        0.006F,
                        palette.stubble * (0.90F + hash01(seed + 13) * 0.16F),
                        k_building_state_mask_intact);
      desc.add_cylinder(base,
                        base + (cut_tip - base) * 0.7F,
                        0.006F,
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
  const QVector3D culm_colour =
      stage <= 2 ? (stage == 1 ? palette.sprout : palette.leaf)
                 : lerp(palette.stalk_green, palette.stalk_gold, ripe_t);
  const QVector3D culm_shade =
      stage <= 2 ? palette.leaf_dark
                 : lerp(palette.leaf_dark, palette.stalk_gold_dark, ripe_t);
  const QVector3D ear_mid_colour = lerp(palette.head_green, palette.head_gold, ripe_t);
  const QVector3D ear_lit_colour =
      lerp(palette.head_green, palette.head_gold_light, ripe_t);
  const QVector3D ear_shade_colour =
      lerp(palette.leaf_dark, palette.head_gold_dark, ripe_t);
  const QVector3D awn_colour = lerp(palette.head_green, palette.awn, ripe_t);
  const QVector3D leaf_colour =
      lerp(palette.leaf_dark, palette.stalk_gold_dark, ripe_t);

  int seed = spec.seed * 977 + stage * 13;
  const float prevailing = hash01(spec.seed * 173 + stage * 29) * k_tau;

  for (int row = 0; row < rows; ++row) {
    const float offset = -row_span + row_step * (static_cast<float>(row) + 0.5F);
    const float row_vigor = 0.95F + hash01(spec.seed * 211 + row * 17) * 0.10F;
    for (int i = 0; i < per_row; ++i) {
      ++seed;
      if (hash01(seed + 109) < (stage == 1 ? 0.12F : 0.05F)) {
        continue;
      }

      const float run = -run_span + run_step * (static_cast<float>(i) + 0.5F) +
                        (hash01(seed) - 0.5F) * run_step * 0.66F;
      const float across = offset + (hash01(seed + 17) - 0.5F) * row_step * 0.36F;

      const float gust = wave(run, across, 5.3F, prevailing) * 0.64F +
                         wave(run, across, 12.1F, prevailing * 1.7F) * 0.26F;
      const float patch = wave(run, across, 3.1F, prevailing * 0.5F);

      const float edge_run =
          std::min(static_cast<float>(i), static_cast<float>(per_row - i - 1));
      const float edge_row =
          std::min(static_cast<float>(row), static_cast<float>(rows - row - 1));
      const float edge_scale = std::clamp(0.72F + edge_run * 0.10F, 0.72F, 1.0F) *
                               std::clamp(0.78F + edge_row * 0.13F, 0.78F, 1.0F);
      const float clump_scale = (0.80F + hash01(seed + 29) * 0.34F) *
                                (1.0F + patch * 0.09F) * row_vigor * edge_scale;

      QVector3D const clump_base =
          spec.rows_along_x
              ? spec.center + QVector3D(run, spec.ground_y + 0.040F, across)
              : spec.center + QVector3D(across, spec.ground_y + 0.040F, run);

      int tillers = 1;
      if (look.tillers > 1 && hash01(seed + 131) < 0.55F) {
        ++tillers;
      }
      if (look.tillers > 2 && hash01(seed + 137) < 0.20F) {
        ++tillers;
      }

      for (int tiller = 0; tiller < tillers; ++tiller) {
        const int tiller_seed = seed * 7 + tiller * 89;
        const bool primary = tiller == 0;
        const float spread = primary ? 0.0F : 0.005F + hash01(tiller_seed + 3) * 0.008F;
        const float spread_angle = hash01(tiller_seed + 5) * k_tau;
        QVector3D const base = clump_base + QVector3D(std::cos(spread_angle) * spread,
                                                      0.0F,
                                                      std::sin(spread_angle) * spread);
        const float scale =
            clump_scale * (primary ? 1.0F : 0.76F + hash01(tiller_seed + 7) * 0.22F);
        const float height = look.height * scale;
        const float lean_angle =
            prevailing + gust * 1.20F + (hash01(tiller_seed + 11) - 0.5F) * 0.80F;
        const float lean = look.lean * (0.45F + hash01(tiller_seed + 13) * 0.95F) *
                           (1.0F + gust * 0.45F);
        QVector3D const lean_dir(std::cos(lean_angle), 0.0F, std::sin(lean_angle));

        const float tone = 0.93F + hash01(tiller_seed + 19) * 0.19F;
        const float shade_mix = hash01(tiller_seed + 17);
        QVector3D const culm = lerp(culm_shade,
                                    culm_colour,
                                    std::clamp(0.34F + shade_mix * 1.05F, 0.0F, 1.0F)) *
                               tone;

        QVector3D const top =
            base + QVector3D(lean_dir.x() * lean, height, lean_dir.z() * lean);

        if (stage == 1) {
          desc.add_cone(base,
                        top,
                        look.radius * (1.4F + hash01(tiller_seed + 23) * 0.9F),
                        culm,
                        k_building_state_mask_intact);
          continue;
        }

        const float knee_t = 0.56F + hash01(tiller_seed + 29) * 0.14F;
        QVector3D const knee = base + QVector3D(lean_dir.x() * lean * knee_t * 0.28F,
                                                height * knee_t,
                                                lean_dir.z() * lean * knee_t * 0.28F);
        if (primary) {
          desc.add_cylinder(base,
                            knee,
                            look.radius * 1.22F,
                            culm * 0.95F,
                            k_building_state_mask_intact);
          desc.add_cylinder(
              knee, top, look.radius * 0.86F, culm, k_building_state_mask_intact);
        } else {
          desc.add_cylinder(base,
                            top,
                            look.radius * 0.92F,
                            culm * 0.95F,
                            k_building_state_mask_intact);
        }

        const bool wants_leaf = look.leaf && (primary || !look.has_head);
        if (wants_leaf && hash01(tiller_seed + 37) < look.leaf_chance) {
          const float leaf_h = height * (0.42F + hash01(tiller_seed + 41) * 0.26F);
          QVector3D const leaf_root =
              base +
              QVector3D(lean_dir.x() * lean * 0.4F, leaf_h, lean_dir.z() * lean * 0.4F);
          const float leaf_angle = lean_angle + 1.9F + hash01(tiller_seed + 43) * 2.6F;
          const float leaf_length =
              look.leaf_length * (0.7F + hash01(tiller_seed + 47) * 0.7F);
          QVector3D const leaf_tip =
              leaf_root +
              QVector3D(std::cos(leaf_angle) * leaf_length,
                        -leaf_length * (0.35F + hash01(tiller_seed + 53) * 0.5F),
                        std::sin(leaf_angle) * leaf_length);
          desc.add_cone(leaf_root,
                        leaf_tip,
                        look.radius * 2.1F,
                        leaf_colour * tone,
                        k_building_state_mask_intact);
        }

        if (!look.has_head) {
          continue;
        }

        const float droop = look.droop * (0.45F + hash01(tiller_seed + 59) * 1.0F);
        const float head_length =
            look.head_length * (0.82F + hash01(tiller_seed + 61) * 0.34F);
        QVector3D const nod(
            lean_dir.x() * droop, 1.0F - droop * 0.62F, lean_dir.z() * droop);
        QVector3D const head_dir = nod.normalized();
        QVector3D const ear_tip = top + head_dir * head_length;

        const float ear_mix = hash01(tiller_seed + 67);
        QVector3D ear = ear_mid_colour;
        if (ear_mix > 0.52F) {
          ear = lerp(ear_mid_colour, ear_lit_colour, (ear_mix - 0.52F) * 2.0F);
        } else if (ear_mix < 0.24F) {
          ear = lerp(ear_mid_colour, ear_shade_colour, (0.24F - ear_mix) * 2.2F);
        }
        ear = ear * (0.92F + hash01(tiller_seed + 71) * 0.16F);
        const float ear_radius =
            look.head_radius * scale * (0.86F + hash01(tiller_seed + 73) * 0.28F);

        desc.add_cone(top,
                      ear_tip,
                      ear_radius * (primary ? 1.0F : 0.88F),
                      ear,
                      k_building_state_mask_intact);

        if (look.awn_length > 0.0F && primary &&
            hash01(tiller_seed + 79) < look.awn_chance) {
          const float awn_length =
              look.awn_length * (0.6F + hash01(tiller_seed + 83) * 0.8F);
          desc.add_cone(ear_tip,
                        ear_tip + head_dir * awn_length,
                        ear_radius * 0.34F,
                        awn_colour * tone,
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
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
