#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "terrain_build_internal.h"
#include "terrain_hill_stamp.h"

namespace Game::Map::terrain_build {

namespace {

constexpr int k_hill_ramp_extra_steps = 7;

constexpr float k_hill_ramp_steepness_exponent = 0.92F;

constexpr float k_width_falloff_padding = 4.00F;
// A ramp standing proud of the plain is an embankment: its sides fall away at
// about forty degrees, not as a cliff. Cells of side slope per metre of ramp.
constexpr float k_embankment_run_per_metre = 1.2F;

constexpr float k_entry_bowl_exponent = 1.30F;

constexpr float k_entry_base_width_scale = Game::Map::k_hill_entry_base_width_scale;
constexpr float k_entry_top_width_scale = 0.95F;

constexpr float k_entry_outward_steps_fraction = 0.90F;
constexpr int k_entry_outward_steps_min = 8;
constexpr int k_entry_outward_steps_max = 24;

constexpr float k_entry_mid_dip_strength = 0.0F;

constexpr float k_entry_mid_depth_strength = 0.0F;

constexpr float k_entry_toe_height_fraction = 0.0F;

constexpr float k_entry_lower_ramp_delay = 0.0F;
constexpr float k_entry_mouth_flare_strength =
    Game::Map::k_hill_entry_mouth_flare_strength;
constexpr float k_entry_mouth_soften_strength = 0.045F;
constexpr float k_entry_floor_flatten_strength = 0.075F;
constexpr float k_entry_shoulder_raise_strength = 0.065F;

[[nodiscard]] auto smoothstep(float t) -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto smooth_range(float a, float b, float x) -> float {
  return smoothstep((x - a) / std::max(1e-4F, b - a));
}

[[nodiscard]] auto smootherstep(float t) -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
}

} // namespace

auto HillStamp::locate_ramp_axis(const EntranceCluster& entrance)
    -> std::optional<RampAxis> {
  const float entrance_gx = entrance.grid_x;
  const float entrance_gz = entrance.grid_z;
  int const ex = int(std::round(entrance_gx));
  int const ez = int(std::round(entrance_gz));
  if (!L.in_bounds(ex, ez)) {
    return std::nullopt;
  }

  const int entrance_idx = L.index_at(ex, ez);
  L.hill_entrances[entrance_idx] = true;
  entrance_indices.push_back(entrance_idx);
  if (L.types[entrance_idx] != TerrainType::Mountain) {
    if (L.types[entrance_idx] == TerrainType::Flat) {
      L.types[entrance_idx] = TerrainType::Hill;
    }
    walkable_mask[entrance_idx] = 1;
    entrance_line_mask[entrance_idx] = 1;
    erosion_protected[entrance_idx] = 1;
    L.hill_walkable[entrance_idx] = true;
    L.heights[entrance_idx] = std::max(L.heights[entrance_idx], 0.0F);
  }

  float target_grid_x = grid_center_x;
  float target_grid_z = grid_center_z;
  if (shaped_geometry) {
    const float entrance_dx = float(ex) - grid_center_x;
    const float entrance_dz = float(ez) - grid_center_z;
    const auto target =
        Game::Map::hill_shape_ramp_target(entrance_dx * cos_a + entrance_dz * sin_a,
                                          -entrance_dx * sin_a + entrance_dz * cos_a,
                                          shape_geometry);
    target_grid_x = grid_center_x + target.x * cos_a - target.z * sin_a;
    target_grid_z = grid_center_z + target.x * sin_a + target.z * cos_a;
  }

  float dir_x = target_grid_x - float(ex);
  float dir_z = target_grid_z - float(ez);
  if (std::hypot(dir_x, dir_z) < 0.001F) {
    dir_x = grid_center_x - float(ex);
    dir_z = grid_center_z - float(ez);
  }
  float const length = std::sqrt(dir_x * dir_x + dir_z * dir_z);
  if (length < 0.001F) {
    return std::nullopt;
  }

  dir_x /= length;
  dir_z /= length;
  return RampAxis{ex, ez, dir_x, dir_z, length};
}

auto HillStamp::measure_plateau_steps(const RampAxis& axis, int steps) const -> int {
  const int ex = axis.ex;
  const int ez = axis.ez;
  const float dir_x = axis.dir_x;
  const float dir_z = axis.dir_z;
  auto cur_x = float(ex);
  auto cur_z = float(ez);
  int plateau_steps = steps;
  {
    auto test_x = cur_x;
    auto test_z = cur_z;
    for (int step = 0; step < steps; ++step) {
      int const ix = int(std::round(test_x));
      int const iz = int(std::round(test_z));
      if (!L.in_bounds(ix, iz)) {
        break;
      }
      const float cell_dx = float(ix) - grid_center_x;
      const float cell_dz = float(iz) - grid_center_z;
      const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
      const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
      const float plateau_norm_dist = crown_distance(cell_rot_x, cell_rot_z);
      if (plateau_norm_dist <= 1.0F) {
        plateau_steps = std::max(1, step);
        break;
      }
      test_x += dir_x;
      test_z += dir_z;
    }
  }
  return plateau_steps;
}

auto HillStamp::measure_ramp(const RampAxis& axis,
                             const EntranceCluster& entrance) const -> RampLayout {
  const int ex = axis.ex;
  const int ez = axis.ez;
  const float dir_x = axis.dir_x;
  const float dir_z = axis.dir_z;
  const int steps = int(axis.length) + 3;
  const int plateau_steps = measure_plateau_steps(axis, steps);

  const int ramp_steps = std::min(steps, plateau_steps + k_hill_ramp_extra_steps);

  int const outward_steps =
      std::clamp(int(std::round(float(ramp_steps) * k_entry_outward_steps_fraction)),
                 k_entry_outward_steps_min,
                 k_entry_outward_steps_max);
  int const total_ramp_steps = outward_steps + ramp_steps;

  float const entry_width = Game::Map::hill_entry_half_width_cells(
      crown, entrance.radius, campaign_landform_scale);

  float const perp_x = -dir_z;
  float const perp_z = dir_x;

  float const ramp_origin_x = float(ex) - dir_x * float(outward_steps);
  float const ramp_origin_z = float(ez) - dir_z * float(outward_steps);
  float const ramp_span = float(std::max(1, total_ramp_steps - 1));

  float const max_taper = std::max(
      1.0F,
      entry_width * std::max(k_entry_base_width_scale, k_entry_top_width_scale) *
          (1.0F + k_entry_mouth_flare_strength));
  float const corridor_reach =
      max_taper +
      std::max(k_width_falloff_padding, hill_height * k_embankment_run_per_metre) +
      1.0F;

  float const ramp_end_x = ramp_origin_x + dir_x * ramp_span;
  float const ramp_end_z = ramp_origin_z + dir_z * ramp_span;
  int const corridor_min_x = std::max(
      0, int(std::floor(std::min(ramp_origin_x, ramp_end_x) - corridor_reach)));
  int const corridor_max_x =
      std::min(L.width - 1,
               int(std::ceil(std::max(ramp_origin_x, ramp_end_x) + corridor_reach)));
  int const corridor_min_z = std::max(
      0, int(std::floor(std::min(ramp_origin_z, ramp_end_z) - corridor_reach)));
  int const corridor_max_z =
      std::min(L.height - 1,
               int(std::ceil(std::max(ramp_origin_z, ramp_end_z) + corridor_reach)));

  return RampLayout{ex,
                    ez,
                    dir_x,
                    dir_z,
                    perp_x,
                    perp_z,
                    outward_steps,
                    total_ramp_steps,
                    entry_width,
                    ramp_origin_x,
                    ramp_origin_z,
                    ramp_span,
                    ramp_end_x,
                    ramp_end_z,
                    corridor_min_x,
                    corridor_max_x,
                    corridor_min_z,
                    corridor_max_z};
}

void HillStamp::carve_entrance(const EntranceCluster& entrance) {
  const std::optional<RampAxis> axis = locate_ramp_axis(entrance);
  if (!axis.has_value()) {
    return;
  }
  const RampLayout ramp = measure_ramp(*axis, entrance);
  L.entrance_centerlines.push_back(
      {{(ramp.ramp_origin_x - grid_half_width) * L.tile_size,
        0.0F,
        (ramp.ramp_origin_z - grid_half_height) * L.tile_size},
       {(ramp.ramp_end_x - grid_half_width) * L.tile_size,
        0.0F,
        (ramp.ramp_end_z - grid_half_height) * L.tile_size}});
  sculpt_ramp(ramp);
}

void HillStamp::sculpt_ramp(const RampLayout& ramp) {
  for (int iz = ramp.corridor_min_z; iz <= ramp.corridor_max_z; ++iz) {
    for (int ix = ramp.corridor_min_x; ix <= ramp.corridor_max_x; ++ix) {
      const std::optional<RampCell> cell = sample_ramp_cell(ramp, ix, iz);
      if (cell.has_value()) {
        apply_ramp_cell(*cell, ix, iz);
      }
    }
  }
}

auto HillStamp::sample_ramp_cell(const RampLayout& ramp,
                                 int ix,
                                 int iz) const -> std::optional<RampCell> {
  const float dir_x = ramp.dir_x;
  const float dir_z = ramp.dir_z;
  const float perp_x = ramp.perp_x;
  const float perp_z = ramp.perp_z;
  const int outward_steps = ramp.outward_steps;
  const int total_ramp_steps = ramp.total_ramp_steps;
  const float entry_width = ramp.entry_width;
  const float ramp_origin_x = ramp.ramp_origin_x;
  const float ramp_origin_z = ramp.ramp_origin_z;
  const float ramp_span = ramp.ramp_span;

  float const rel_x = float(ix) - ramp_origin_x;
  float const rel_z = float(iz) - ramp_origin_z;

  float const along = rel_x * dir_x + rel_z * dir_z;
  if (along < -0.5F || along > ramp_span + 0.5F) {
    return std::nullopt;
  }
  float const across = rel_x * perp_x + rel_z * perp_z;

  float const ramp_step = std::clamp(along, 0.0F, ramp_span);
  bool const is_outward = ramp_step < float(outward_steps);

  float const center_x = ramp_origin_x + dir_x * ramp_step;
  float const center_z = ramp_origin_z + dir_z * ramp_step;
  const float cell_dx = center_x - grid_center_x;
  const float cell_dz = center_z - grid_center_z;
  const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
  const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
  const float cell_norm_dist = slope_distance(cell_rot_x, cell_rot_z);

  bool const builds_apron = is_outward || cell_norm_dist > 1.0F;

  float const ramp_progress =
      (total_ramp_steps > 1) ? std::clamp(ramp_step / ramp_span, 0.0F, 1.0F) : 1.0F;

  float const delayed_progress =
      std::clamp((ramp_progress - k_entry_lower_ramp_delay) /
                     std::max(1e-4F, 1.0F - k_entry_lower_ramp_delay),
                 0.0F,
                 1.0F);
  float const s = smootherstep(delayed_progress);
  float const mid = 4.0F * ramp_progress * (1.0F - ramp_progress);
  float const lower_ramp = 1.0F - smooth_range(0.58F, 0.95F, s);
  float const mouth = 1.0F - smooth_range(0.18F, 0.58F, s);

  float const height_base = std::pow(s, k_hill_ramp_steepness_exponent);
  float const height_frac =
      std::clamp(height_base * (1.0F - k_entry_mid_dip_strength * mid), 0.0F, 1.0F);

  float const toe_frac = k_entry_toe_height_fraction * (1.0F - s) * (1.0F - s);
  float center_ramp_height = hill_height * std::max(height_frac, toe_frac);
  center_ramp_height *= std::clamp(1.0F - k_entry_mid_depth_strength * mid, 0.0F, 1.0F);

  float const width_scale =
      (1.0F - s) * k_entry_base_width_scale + s * k_entry_top_width_scale;
  float tapered_width = std::max(
      1.0F, entry_width * width_scale * (1.0F + k_entry_mouth_flare_strength * mouth));

  if (is_outward && outward_steps > 0) {
    float const outward_t = ramp_step / float(std::max(1, outward_steps));

    float const outward_width_mul = 0.82F + 0.18F * smootherstep(outward_t);
    tapered_width = std::max(1.0F, tapered_width * outward_width_mul);
  }

  float const side_run = std::max(k_width_falloff_padding,
                                  center_ramp_height * k_embankment_run_per_metre);
  float const edge_t =
      smooth_range(tapered_width * 0.16F, tapered_width + side_run, std::abs(across));

  return RampCell{across,
                  edge_t,
                  tapered_width,
                  center_ramp_height,
                  s,
                  mouth,
                  lower_ramp,
                  ramp_progress,
                  builds_apron};
}

void HillStamp::apply_ramp_cell(const RampCell& cell, int ix, int iz) {
  const float across = cell.across;
  const float edge_t = cell.edge_t;
  const float tapered_width = cell.tapered_width;
  const float center_ramp_height = cell.center_ramp_height;
  const float s = cell.s;
  const float mouth = cell.mouth;
  const float lower_ramp = cell.lower_ramp;
  const float ramp_progress = cell.ramp_progress;
  const bool builds_apron = cell.builds_apron;

  int const ramp_idx = L.index_at(ix, iz);
  if (L.types[ramp_idx] != TerrainType::Mountain) {
    float const width_factor = 1.0F - edge_t;

    bool const navigable =
        std::abs(across) <=
        std::max(Game::Map::k_hill_entry_min_half_width_cells, tapered_width);

    if (navigable && L.types[ramp_idx] == TerrainType::Flat) {
      L.types[ramp_idx] = TerrainType::Hill;
    }

    if (navigable) {
      walkable_mask[ramp_idx] = 1;
      entrance_line_mask[ramp_idx] = 1;
      erosion_protected[ramp_idx] = 1;
      L.hill_entrances[ramp_idx] = true;
    }

    float const existing_height = L.heights[ramp_idx];

    float const bowl = std::pow(edge_t, k_entry_bowl_exponent);
    float target_height =
        (1.0F - bowl) * (base_heights[ramp_idx] + center_ramp_height) +
        bowl * existing_height;
    float const floor_core = smooth_range(0.22F, 0.82F, width_factor);
    float const shoulder_band = smooth_range(0.16F, 0.46F, width_factor) *
                                (1.0F - smooth_range(0.60F, 0.90F, width_factor));
    float const mouth_soften =
        hill_height * k_entry_mouth_soften_strength * floor_core * mouth * (1.0F - s);

    float const center_channel = std::pow(std::clamp(width_factor, 0.0F, 1.0F), 4.0F);
    float const floor_flatten = hill_height * k_entry_floor_flatten_strength *
                                center_channel * (0.35F + 0.65F * lower_ramp);
    float const apron_shoulder_blend =
        builds_apron ? smooth_range(0.48F, 0.88F, ramp_progress) : 1.0F;
    float const shoulder_raise = hill_height * k_entry_shoulder_raise_strength *
                                 shoulder_band * (0.25F + 0.75F * lower_ramp) *
                                 apron_shoulder_blend;
    target_height =
        std::max(0.0F, target_height - mouth_soften - floor_flatten + shoulder_raise);

    L.heights[ramp_idx] = std::max(target_height, ground_before_hill[ramp_idx]);
  }
}

} // namespace Game::Map::terrain_build
