#include "unit_layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace Game::Formation {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_rad_to_deg = 180.0F / k_pi;

struct ShapeName {
  UnitLayoutShape shape;
  const char* name;
};

constexpr std::array<ShapeName, 8> k_shape_names = {{
    {UnitLayoutShape::Ranks, "ranks"},
    {UnitLayoutShape::Wedge, "wedge"},
    {UnitLayoutShape::LooseOrder, "loose_order"},
    {UnitLayoutShape::Column, "column"},
    {UnitLayoutShape::Cluster, "cluster"},
    {UnitLayoutShape::Circle, "circle"},
    {UnitLayoutShape::Shell, "shell"},
    {UnitLayoutShape::Arc, "arc"},
}};

struct StateName {
  UnitLayoutState state;
  const char* name;
};

constexpr std::array<StateName, 8> k_state_names = {{
    {UnitLayoutState::Normal, "normal"},
    {UnitLayoutState::Defensive, "defensive"},
    {UnitLayoutState::Attacking, "attacking"},
    {UnitLayoutState::Braced, "braced"},
    {UnitLayoutState::Marching, "marching"},
    {UnitLayoutState::Routing, "routing"},
    {UnitLayoutState::Disrupted, "disrupted"},
    {UnitLayoutState::Working, "working"},
}};

auto hash_u32(std::uint32_t value) -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return value;
}

auto signed_unit(std::uint32_t seed, int index, int channel) -> float {
  std::uint32_t const mixed =
      hash_u32(seed ^ (static_cast<std::uint32_t>(index) * 2654435761U) ^
               (static_cast<std::uint32_t>(channel) * 0x9E3779B9U));
  return (static_cast<float>(mixed & 0xFFFFU) / 32767.5F) - 1.0F;
}

auto centered(int value, int count) -> float {
  return static_cast<float>(value) - (static_cast<float>(count) - 1.0F) * 0.5F;
}

constexpr int k_max_wedge_ranks = 16;

auto wedge_rank_sizes(int count,
                      float growth,
                      std::array<int, k_max_wedge_ranks>& sizes) -> int {
  float const step = std::max(0.5F, growth);
  int ranks = static_cast<int>(
      std::lround(std::sqrt(2.0F * static_cast<float>(std::max(1, count)) / step)));
  ranks = std::clamp(ranks, 1, std::min(count, k_max_wedge_ranks));

  float weight_sum = 0.0F;
  for (int r = 0; r < ranks; ++r) {
    weight_sum += 1.0F + step * static_cast<float>(r);
  }

  int assigned = 0;
  for (int r = 0; r < ranks; ++r) {
    float const weight = 1.0F + step * static_cast<float>(r);
    sizes[static_cast<std::size_t>(r)] =
        std::max(1, static_cast<int>(static_cast<float>(count) * weight / weight_sum));
    assigned += sizes[static_cast<std::size_t>(r)];
  }
  for (int r = ranks - 1; assigned < count; r = (r == 0 ? ranks - 1 : r - 1)) {
    ++sizes[static_cast<std::size_t>(r)];
    ++assigned;
  }
  for (int r = ranks - 1; assigned > count; r = (r == 0 ? ranks - 1 : r - 1)) {
    if (sizes[static_cast<std::size_t>(r)] > 1) {
      --sizes[static_cast<std::size_t>(r)];
      --assigned;
    }
  }
  return ranks;
}

auto wedge_slot_for(int index, int count, float growth) -> RankSlot {
  std::array<int, k_max_wedge_ranks> sizes{};
  int const ranks = wedge_rank_sizes(count, growth, sizes);

  RankSlot slot;
  slot.rows = ranks;
  slot.cols = *std::max_element(sizes.begin(), sizes.begin() + ranks);

  int consumed = 0;
  for (int r = 0; r < ranks; ++r) {
    int const size = sizes[static_cast<std::size_t>(r)];
    if (index < consumed + size) {
      slot.row = ranks - 1 - r;
      slot.col = index - consumed;
      slot.rank_cols = size;
      return slot;
    }
    consumed += size;
  }
  slot.row = 0;
  slot.col = 0;
  slot.rank_cols = sizes[static_cast<std::size_t>(ranks - 1)];
  return slot;
}

auto style_for_state(UnitLayoutStyle style, UnitLayoutState state) -> UnitLayoutStyle {
  switch (state) {
  case UnitLayoutState::Normal:
    break;
  case UnitLayoutState::Defensive:
    style.lateral_spacing_scale *= 0.86F;
    style.depth_spacing_scale *= 0.90F;
    style.lateral_jitter *= 0.45F;
    style.depth_jitter *= 0.45F;
    style.facing_jitter_degrees *= 0.35F;
    style.front_rank_tightening = std::max(style.front_rank_tightening, 0.12F);
    break;
  case UnitLayoutState::Attacking:
    style.lateral_spacing_scale *= 1.06F;
    style.depth_spacing_scale *= 0.94F;
    style.lateral_jitter *= 1.35F;
    style.depth_jitter *= 1.5F;
    style.rank_arc += 0.18F;
    style.facing_jitter_degrees *= 1.4F;
    break;
  case UnitLayoutState::Braced:
    style.lateral_spacing_scale *= 0.92F;
    style.depth_spacing_scale *= 1.12F;
    style.weapon_clearance = std::max(style.weapon_clearance, 0.08F);
    style.lateral_jitter *= 0.3F;
    style.depth_jitter *= 0.3F;
    style.facing_jitter_degrees *= 0.2F;
    break;
  case UnitLayoutState::Marching:
    style.shape = UnitLayoutShape::Column;
    style.depth_spacing_scale *= 1.05F;
    style.rank_stagger = 0.22F;
    style.lateral_jitter *= 0.6F;
    break;
  case UnitLayoutState::Routing:
    style.shape = UnitLayoutShape::Cluster;
    style.lateral_spacing_scale *= 1.25F;
    style.depth_spacing_scale *= 1.25F;
    style.lateral_jitter = std::max(style.lateral_jitter, 0.45F);
    style.depth_jitter = std::max(style.depth_jitter, 0.45F);
    style.facing_jitter_degrees = std::max(style.facing_jitter_degrees, 35.0F);
    break;
  case UnitLayoutState::Disrupted:
    style.lateral_jitter = std::max(style.lateral_jitter, 0.28F) * 1.4F;
    style.depth_jitter = std::max(style.depth_jitter, 0.28F) * 1.4F;
    style.facing_jitter_degrees = std::max(style.facing_jitter_degrees, 18.0F);
    style.rank_stagger *= 1.6F;
    break;
  case UnitLayoutState::Working:
    style.lateral_jitter = std::max(style.lateral_jitter, 0.14F);
    style.depth_jitter = std::max(style.depth_jitter, 0.14F);
    style.facing_jitter_degrees = std::max(style.facing_jitter_degrees, 8.0F);
    break;
  }
  return style;
}

auto make_style(std::string id, UnitLayoutShape shape) -> UnitLayoutStyle {
  UnitLayoutStyle style;
  style.id = std::move(id);
  style.shape = shape;
  return style;
}

void register_generic_styles(std::vector<UnitLayoutStyle>& out) {
  {
    auto style = make_style("close_order_infantry", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.08F;
    style.depth_spacing_scale = 1.06F;
    style.rank_stagger = 0.14F;
    style.front_rank_tightening = 0.08F;
    style.rear_rank_loosening = 0.06F;
    style.lateral_jitter = 0.06F;
    style.depth_jitter = 0.05F;
    style.rear_jitter_gain = 0.8F;
    style.facing_jitter_degrees = 3.0F;
    style.rank_arc = 0.05F;
    out.push_back(style);
  }
  {
    auto style = make_style("shield_wall", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.88F;
    style.depth_spacing_scale = 1.00F;
    style.front_rank_tightening = 0.18F;
    style.lateral_jitter = 0.02F;
    style.depth_jitter = 0.02F;
    style.facing_jitter_degrees = 1.0F;
    style.min_separation_scale = 0.70F;
    out.push_back(style);
  }
  {
    auto style = make_style("spear_ranks", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.02F;
    style.depth_spacing_scale = 1.44F;
    style.weapon_clearance = 0.04F;
    style.rank_stagger = 0.30F;
    style.lateral_jitter = 0.05F;
    style.depth_jitter = 0.04F;
    style.facing_jitter_degrees = 2.5F;
    out.push_back(style);
  }
  {
    auto style = make_style("spear_brace", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.94F;
    style.depth_spacing_scale = 1.72F;
    style.weapon_clearance = 0.06F;
    style.rank_stagger = 0.34F;
    style.front_rank_tightening = 0.14F;
    style.lateral_jitter = 0.02F;
    style.depth_jitter = 0.02F;
    style.facing_jitter_degrees = 1.0F;
    out.push_back(style);
  }
  {
    auto style = make_style("loose_order_ranged", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 1.28F;
    style.depth_spacing_scale = 1.18F;
    style.rank_stagger = 0.42F;
    style.rear_rank_loosening = 0.14F;
    style.lateral_jitter = 0.20F;
    style.depth_jitter = 0.16F;
    style.rear_jitter_gain = 0.6F;
    style.facing_jitter_degrees = 9.0F;
    style.rank_arc = 0.10F;
    style.min_separation_scale = 0.45F;
    out.push_back(style);
  }
  {
    auto style = make_style("marching_column", UnitLayoutShape::Column);
    style.column_files = 3.0F;
    style.lateral_spacing_scale = 1.05F;
    style.depth_spacing_scale = 1.10F;
    style.rank_stagger = 0.20F;
    style.lateral_jitter = 0.06F;
    style.depth_jitter = 0.07F;
    style.facing_jitter_degrees = 3.5F;
    out.push_back(style);
  }
  {
    auto style = make_style("cavalry_wedge", UnitLayoutShape::Wedge);
    style.lateral_spacing_scale = 0.78F;
    style.depth_spacing_scale = 1.16F;
    style.wedge_slope = 0.55F;
    style.rank_stagger = 0.45F;
    style.rear_depth_bias = 0.16F;
    style.lateral_jitter = 0.07F;
    style.depth_jitter = 0.06F;
    style.facing_jitter_degrees = 4.5F;
    style.min_separation_scale = 0.60F;
    out.push_back(style);
  }
  {
    auto style = make_style("cavalry_line", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.82F;
    style.depth_spacing_scale = 1.22F;
    style.rank_stagger = 0.48F;
    style.lateral_jitter = 0.06F;
    style.depth_jitter = 0.05F;
    style.facing_jitter_degrees = 3.5F;
    out.push_back(style);
  }
  {
    auto style = make_style("cavalry_loose", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 0.98F;
    style.depth_spacing_scale = 1.30F;
    style.rank_stagger = 0.52F;
    style.lateral_jitter = 0.22F;
    style.depth_jitter = 0.18F;
    style.facing_jitter_degrees = 11.0F;
    style.min_separation_scale = 0.50F;
    out.push_back(style);
  }
  {
    auto style = make_style("cavalry_column", UnitLayoutShape::Column);
    style.column_files = 2.0F;
    style.lateral_spacing_scale = 0.85F;
    style.depth_spacing_scale = 1.25F;
    style.rank_stagger = 0.30F;
    style.lateral_jitter = 0.07F;
    style.depth_jitter = 0.07F;
    out.push_back(style);
  }
  {
    auto style = make_style("beast_spread", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.85F;
    style.depth_spacing_scale = 1.70F;
    style.rank_stagger = 0.35F;
    style.lateral_jitter = 0.10F;
    style.depth_jitter = 0.10F;
    style.facing_jitter_degrees = 6.0F;
    style.min_separation_scale = 0.80F;
    out.push_back(style);
  }
  {
    auto style = make_style("siege_crew", UnitLayoutShape::Circle);
    style.radius_scale = 2.10F;
    style.lateral_jitter = 0.12F;
    style.depth_jitter = 0.12F;
    style.facing_jitter_degrees = 12.0F;
    out.push_back(style);
  }
  {
    auto style = make_style("support_cluster", UnitLayoutShape::Cluster);
    style.lateral_spacing_scale = 1.15F;
    style.depth_spacing_scale = 1.15F;
    style.cluster_pull = 0.22F;
    style.cluster_size = 3.0F;
    style.lateral_jitter = 0.22F;
    style.depth_jitter = 0.22F;
    style.facing_jitter_degrees = 22.0F;
    style.min_separation_scale = 0.50F;
    out.push_back(style);
  }
  {
    auto style = make_style("work_party", UnitLayoutShape::Circle);
    style.radius_scale = 2.80F;
    style.lateral_jitter = 0.16F;
    style.depth_jitter = 0.16F;
    style.facing_jitter_degrees = 8.0F;
    out.push_back(style);
  }
  {

    auto style = make_style("worker_gang", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 1.10F;
    style.depth_spacing_scale = 1.12F;
    style.rank_stagger = 0.42F;
    style.rear_rank_loosening = 0.12F;
    style.lateral_jitter = 0.26F;
    style.depth_jitter = 0.24F;
    style.rear_jitter_gain = 0.20F;
    style.facing_jitter_degrees = 16.0F;
    style.min_separation_scale = 0.50F;
    out.push_back(style);
  }
  {

    auto style = make_style("worker_file", UnitLayoutShape::Column);
    style.column_files = 3.0F;
    style.lateral_spacing_scale = 0.95F;
    style.depth_spacing_scale = 1.15F;
    style.rank_stagger = 0.34F;
    style.lateral_jitter = 0.16F;
    style.depth_jitter = 0.14F;
    style.facing_jitter_degrees = 7.0F;
    style.min_separation_scale = 0.48F;
    out.push_back(style);
  }
  {
    auto style = make_style("civilian_crowd", UnitLayoutShape::Cluster);
    style.lateral_spacing_scale = 1.20F;
    style.depth_spacing_scale = 1.20F;
    style.cluster_pull = 0.34F;
    style.cluster_size = 3.0F;
    style.lateral_jitter = 0.34F;
    style.depth_jitter = 0.34F;
    style.facing_jitter_degrees = 45.0F;
    style.min_separation_scale = 0.42F;
    out.push_back(style);
  }
  {
    auto style = make_style("command_retinue", UnitLayoutShape::Arc);
    style.lateral_spacing_scale = 1.20F;
    style.depth_spacing_scale = 1.10F;
    style.rank_arc = -0.35F;
    style.lateral_jitter = 0.08F;
    style.depth_jitter = 0.08F;
    style.facing_jitter_degrees = 6.0F;
    out.push_back(style);
  }
  {
    auto style = make_style("procession", UnitLayoutShape::Column);
    style.column_files = 2.0F;
    style.lateral_spacing_scale = 0.90F;
    style.depth_spacing_scale = 1.30F;
    style.lateral_jitter = 0.01F;
    style.depth_jitter = 0.01F;
    style.facing_jitter_degrees = 0.5F;
    out.push_back(style);
  }
  {
    auto style = make_style("burial_guard_block", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.80F;
    style.depth_spacing_scale = 0.84F;
    style.front_rank_tightening = 0.16F;
    style.lateral_jitter = 0.03F;
    style.depth_jitter = 0.03F;
    style.facing_jitter_degrees = 1.5F;
    style.min_separation_scale = 0.72F;
    out.push_back(style);
  }
  {
    auto style = make_style("sepulcher_ranged_line", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.96F;
    style.depth_spacing_scale = 1.05F;
    style.rank_stagger = 0.18F;
    style.lateral_jitter = 0.05F;
    style.depth_jitter = 0.04F;
    style.facing_jitter_degrees = 2.0F;
    out.push_back(style);
  }
  {
    auto style = make_style("dense_advance", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.76F;
    style.depth_spacing_scale = 0.88F;
    style.front_rank_tightening = 0.20F;
    style.lateral_jitter = 0.02F;
    style.depth_jitter = 0.02F;
    style.facing_jitter_degrees = 1.0F;
    style.min_separation_scale = 0.74F;
    out.push_back(style);
  }
}

void register_rome_styles(std::vector<UnitLayoutStyle>& out) {
  {
    auto style = make_style("rome.close_order_infantry", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.00F;
    style.depth_spacing_scale = 1.20F;
    style.rank_stagger = 0.50F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.0F;
    style.front_rank_tightening = 0.06F;
    style.rear_rank_loosening = 0.0F;
    style.lateral_jitter = 0.022F;
    style.depth_jitter = 0.018F;
    style.rear_jitter_gain = 0.4F;
    style.facing_jitter_degrees = 0.9F;
    style.min_separation_scale = 0.66F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.shield_wall", UnitLayoutShape::Shell);
    style.lateral_spacing_scale = 0.34F;
    style.depth_spacing_scale = 0.36F;
    style.front_rank_tightening = 0.20F;
    style.lateral_jitter = 0.004F;
    style.depth_jitter = 0.004F;
    style.facing_jitter_degrees = 0.3F;
    style.min_separation_scale = 0.30F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.spear_ranks", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.00F;
    style.depth_spacing_scale = 1.32F;
    style.weapon_clearance = 0.04F;
    style.rank_stagger = 0.50F;
    style.front_rank_tightening = 0.08F;
    style.lateral_jitter = 0.018F;
    style.depth_jitter = 0.014F;
    style.facing_jitter_degrees = 0.7F;
    style.min_separation_scale = 0.66F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.spear_brace", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.92F;
    style.depth_spacing_scale = 1.46F;
    style.weapon_clearance = 0.06F;
    style.rank_stagger = 0.0F;
    style.front_rank_tightening = 0.16F;
    style.lateral_jitter = 0.008F;
    style.depth_jitter = 0.008F;
    style.facing_jitter_degrees = 0.4F;
    style.min_separation_scale = 0.74F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.loose_order_ranged", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 1.34F;
    style.depth_spacing_scale = 1.44F;
    style.rank_stagger = 0.50F;
    style.rank_arc = 0.0F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.32F;
    style.group_depth_stagger = 0.12F;
    style.lateral_jitter = 0.07F;
    style.depth_jitter = 0.06F;
    style.rear_jitter_gain = 0.4F;
    style.facing_jitter_degrees = 4.0F;
    style.min_separation_scale = 0.60F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.marching_column", UnitLayoutShape::Column);
    style.column_files = 4.0F;
    style.lateral_spacing_scale = 0.90F;
    style.depth_spacing_scale = 1.30F;
    style.rank_stagger = 0.0F;
    style.lateral_jitter = 0.014F;
    style.depth_jitter = 0.018F;
    style.facing_jitter_degrees = 0.7F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.cavalry_wedge", UnitLayoutShape::Wedge);
    style.lateral_spacing_scale = 1.28F;
    style.depth_spacing_scale = 1.45F;
    style.wedge_growth = 2.0F;
    style.wedge_slope = 0.22F;
    style.rank_stagger = 0.0F;
    style.rear_depth_bias = 0.0F;
    style.lateral_jitter = 0.030F;
    style.depth_jitter = 0.026F;
    style.facing_jitter_degrees = 1.5F;
    style.min_separation_scale = 0.62F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.cavalry_line", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.24F;
    style.depth_spacing_scale = 1.46F;
    style.rank_stagger = 0.50F;
    style.front_rank_tightening = 0.06F;
    style.lateral_jitter = 0.03F;
    style.depth_jitter = 0.025F;
    style.facing_jitter_degrees = 1.4F;
    style.min_separation_scale = 0.62F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.cavalry_loose", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 1.40F;
    style.depth_spacing_scale = 1.50F;
    style.rank_stagger = 0.50F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.30F;
    style.lateral_jitter = 0.10F;
    style.depth_jitter = 0.09F;
    style.facing_jitter_degrees = 5.0F;
    style.min_separation_scale = 0.55F;
    out.push_back(style);
  }
  {
    auto style = make_style("rome.beast_spread", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.70F;
    style.depth_spacing_scale = 1.90F;
    style.rank_stagger = 0.0F;
    style.lateral_jitter = 0.04F;
    style.depth_jitter = 0.04F;
    style.facing_jitter_degrees = 2.0F;
    style.min_separation_scale = 0.82F;
    out.push_back(style);
  }
}

void register_carthage_styles(std::vector<UnitLayoutStyle>& out) {
  {
    auto style = make_style("carthage.close_order_infantry", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.04F;
    style.depth_spacing_scale = 1.16F;
    style.rank_stagger = 0.26F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.45F;
    style.rear_rank_loosening = 0.14F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.32F;
    style.group_depth_stagger = 0.18F;
    style.lateral_jitter = 0.09F;
    style.depth_jitter = 0.08F;
    style.rear_jitter_gain = 0.7F;
    style.facing_jitter_degrees = 6.5F;
    style.min_separation_scale = 0.55F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.shield_wall", UnitLayoutShape::Arc);
    style.lateral_spacing_scale = 0.96F;
    style.depth_spacing_scale = 0.88F;
    style.rank_stagger = 0.20F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.40F;
    style.front_rank_tightening = 0.10F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.20F;
    style.group_depth_stagger = 0.10F;
    style.lateral_jitter = 0.045F;
    style.depth_jitter = 0.04F;
    style.facing_jitter_degrees = 3.0F;
    style.min_separation_scale = 0.62F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.spear_ranks", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.02F;
    style.depth_spacing_scale = 1.24F;
    style.weapon_clearance = 0.03F;
    style.rank_stagger = 0.30F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.38F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.40F;
    style.group_depth_stagger = 0.22F;
    style.lateral_jitter = 0.08F;
    style.depth_jitter = 0.07F;
    style.facing_jitter_degrees = 6.0F;
    style.min_separation_scale = 0.56F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.spear_brace", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.98F;
    style.depth_spacing_scale = 1.16F;
    style.weapon_clearance = 0.05F;
    style.rank_stagger = 0.18F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.22F;
    style.file_grouping = 0.0F;
    style.group_gap = 0.0F;
    style.lateral_jitter = 0.05F;
    style.depth_jitter = 0.045F;
    style.facing_jitter_degrees = 3.0F;
    style.min_separation_scale = 0.60F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.loose_order_ranged", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 1.30F;
    style.depth_spacing_scale = 1.22F;
    style.rank_stagger = 0.34F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.26F;
    style.rear_rank_loosening = 0.16F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.58F;
    style.group_depth_stagger = 0.28F;
    style.lateral_jitter = 0.15F;
    style.depth_jitter = 0.13F;
    style.rear_jitter_gain = 0.8F;
    style.facing_jitter_degrees = 13.0F;
    style.min_separation_scale = 0.56F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.marching_column", UnitLayoutShape::Column);
    style.column_files = 3.0F;
    style.lateral_spacing_scale = 1.12F;
    style.depth_spacing_scale = 1.08F;
    style.rank_stagger = 0.30F;
    style.lateral_jitter = 0.11F;
    style.depth_jitter = 0.10F;
    style.facing_jitter_degrees = 6.0F;
    style.min_separation_scale = 0.54F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.cavalry_wedge", UnitLayoutShape::Wedge);
    style.lateral_spacing_scale = 1.32F;
    style.depth_spacing_scale = 1.16F;
    style.wedge_growth = 2.0F;
    style.wedge_slope = 0.10F;
    style.rank_stagger = 0.0F;
    style.rank_echelon = 0.0F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.34F;
    style.group_depth_stagger = 0.30F;
    style.lateral_jitter = 0.12F;
    style.depth_jitter = 0.11F;
    style.facing_jitter_degrees = 11.0F;
    style.min_separation_scale = 0.54F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.cavalry_line", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 1.34F;
    style.depth_spacing_scale = 1.30F;
    style.rank_stagger = 0.26F;
    style.rank_arc = 0.30F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.36F;
    style.group_depth_stagger = 0.28F;
    style.lateral_jitter = 0.12F;
    style.depth_jitter = 0.10F;
    style.facing_jitter_degrees = 8.0F;
    style.min_separation_scale = 0.52F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.cavalry_loose", UnitLayoutShape::LooseOrder);
    style.lateral_spacing_scale = 1.52F;
    style.depth_spacing_scale = 1.34F;
    style.rank_stagger = 0.36F;
    style.file_grouping = 2.0F;
    style.group_gap = 0.60F;
    style.group_depth_stagger = 0.50F;
    style.lateral_jitter = 0.22F;
    style.depth_jitter = 0.20F;
    style.facing_jitter_degrees = 16.0F;
    style.min_separation_scale = 0.48F;
    out.push_back(style);
  }
  {
    auto style = make_style("carthage.beast_spread", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 2.10F;
    style.depth_spacing_scale = 1.60F;
    style.rank_stagger = 0.40F;
    style.rank_echelon = 0.0F;
    style.rank_arc = 0.30F;
    style.lateral_jitter = 0.10F;
    style.depth_jitter = 0.09F;
    style.facing_jitter_degrees = 6.0F;
    style.min_separation_scale = 0.78F;
    out.push_back(style);
  }
}

void register_sepulcher_styles(std::vector<UnitLayoutStyle>& out) {
  {
    auto style =
        make_style("iron_sepulcher.close_order_infantry", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.80F;
    style.depth_spacing_scale = 0.74F;
    style.front_rank_tightening = 0.14F;
    style.lateral_jitter = 0.025F;
    style.depth_jitter = 0.020F;
    style.facing_jitter_degrees = 1.2F;
    style.min_separation_scale = 0.74F;
    out.push_back(style);
  }
  {
    auto style =
        make_style("iron_sepulcher.burial_guard_block", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.74F;
    style.depth_spacing_scale = 0.70F;
    style.front_rank_tightening = 0.18F;
    style.rear_depth_bias = -0.08F;
    style.lateral_jitter = 0.015F;
    style.depth_jitter = 0.012F;
    style.facing_jitter_degrees = 0.8F;
    style.min_separation_scale = 0.78F;
    out.push_back(style);
  }
  {
    auto style = make_style("iron_sepulcher.shield_wall", UnitLayoutShape::Shell);
    style.lateral_spacing_scale = 0.70F;
    style.depth_spacing_scale = 0.74F;
    style.front_rank_tightening = 0.12F;
    style.lateral_jitter = 0.006F;
    style.depth_jitter = 0.006F;
    style.facing_jitter_degrees = 0.4F;
    style.min_separation_scale = 0.84F;
    out.push_back(style);
  }
  {
    auto style =
        make_style("iron_sepulcher.sepulcher_ranged_line", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.92F;
    style.depth_spacing_scale = 1.02F;
    style.rank_stagger = 0.16F;
    style.lateral_jitter = 0.03F;
    style.depth_jitter = 0.025F;
    style.facing_jitter_degrees = 1.4F;
    out.push_back(style);
  }
  {
    auto style =
        make_style("iron_sepulcher.loose_order_ranged", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.98F;
    style.depth_spacing_scale = 1.06F;
    style.rank_stagger = 0.18F;
    style.lateral_jitter = 0.04F;
    style.depth_jitter = 0.03F;
    style.facing_jitter_degrees = 2.0F;
    out.push_back(style);
  }
  {
    auto style = make_style("iron_sepulcher.procession", UnitLayoutShape::Column);
    style.column_files = 2.0F;
    style.lateral_spacing_scale = 0.86F;
    style.depth_spacing_scale = 1.34F;
    style.lateral_jitter = 0.006F;
    style.depth_jitter = 0.006F;
    style.facing_jitter_degrees = 0.3F;
    out.push_back(style);
  }
  {
    auto style = make_style("iron_sepulcher.dense_advance", UnitLayoutShape::Ranks);
    style.lateral_spacing_scale = 0.72F;
    style.depth_spacing_scale = 0.72F;
    style.front_rank_tightening = 0.22F;
    style.lateral_jitter = 0.012F;
    style.depth_jitter = 0.010F;
    style.facing_jitter_degrees = 0.6F;
    style.min_separation_scale = 0.80F;
    out.push_back(style);
  }
  {
    auto style = make_style("iron_sepulcher.command_retinue", UnitLayoutShape::Arc);
    style.lateral_spacing_scale = 1.05F;
    style.depth_spacing_scale = 1.05F;
    style.rank_arc = -0.45F;
    style.lateral_jitter = 0.02F;
    style.depth_jitter = 0.02F;
    style.facing_jitter_degrees = 1.0F;
    out.push_back(style);
  }
  {
    auto style = make_style("iron_sepulcher.marching_column", UnitLayoutShape::Column);
    style.column_files = 3.0F;
    style.lateral_spacing_scale = 0.82F;
    style.depth_spacing_scale = 1.10F;
    style.lateral_jitter = 0.010F;
    style.depth_jitter = 0.010F;
    style.facing_jitter_degrees = 0.6F;
    out.push_back(style);
  }
}

} // namespace

auto layout_shape_to_string(UnitLayoutShape shape) -> const char* {
  for (const auto& entry : k_shape_names) {
    if (entry.shape == shape) {
      return entry.name;
    }
  }
  return "ranks";
}

auto try_parse_layout_shape(const QString& value) -> std::optional<UnitLayoutShape> {
  const QString lowered = value.trimmed().toLower();
  for (const auto& entry : k_shape_names) {
    if (lowered == QLatin1String(entry.name)) {
      return entry.shape;
    }
  }
  return std::nullopt;
}

auto layout_state_to_string(UnitLayoutState state) -> const char* {
  for (const auto& entry : k_state_names) {
    if (entry.state == state) {
      return entry.name;
    }
  }
  return "normal";
}

auto try_parse_layout_state(const QString& value) -> std::optional<UnitLayoutState> {
  const QString lowered = value.trimmed().toLower();
  for (const auto& entry : k_state_names) {
    if (lowered == QLatin1String(entry.name)) {
      return entry.state;
    }
  }
  return std::nullopt;
}

auto apply_state_modifier(const UnitLayoutStyle& style,
                          UnitLayoutState state) -> UnitLayoutStyle {
  return style_for_state(style, state);
}

UnitLayoutLibrary::UnitLayoutLibrary() {
  reset_to_defaults();
}

auto UnitLayoutLibrary::instance() -> UnitLayoutLibrary& {
  static UnitLayoutLibrary library;
  return library;
}

void UnitLayoutLibrary::reset_to_defaults() {
  m_styles.clear();
  m_by_name.clear();

  std::vector<UnitLayoutStyle> defaults;
  defaults.reserve(64);
  register_generic_styles(defaults);
  register_rome_styles(defaults);
  register_carthage_styles(defaults);
  register_sepulcher_styles(defaults);

  for (auto& style : defaults) {
    register_style(std::move(style));
  }

  m_fallback = make_style("close_order_infantry", UnitLayoutShape::Ranks);
  if (auto const id = find("close_order_infantry"); id != k_invalid_layout) {
    m_fallback = m_styles[id];
  }
}

void UnitLayoutLibrary::clear() {
  m_styles.clear();
  m_by_name.clear();
}

auto UnitLayoutLibrary::register_style(UnitLayoutStyle style) -> UnitLayoutId {
  auto existing = m_by_name.find(style.id);
  if (existing != m_by_name.end()) {
    m_styles[existing->second] = std::move(style);
    return existing->second;
  }
  auto const id = static_cast<UnitLayoutId>(m_styles.size());
  m_by_name.emplace(style.id, id);
  m_styles.push_back(std::move(style));
  return id;
}

auto UnitLayoutLibrary::find(std::string_view name) const -> UnitLayoutId {
  auto it = m_by_name.find(std::string(name));
  return it == m_by_name.end() ? k_invalid_layout : it->second;
}

auto UnitLayoutLibrary::resolve(std::string_view doctrine,
                                std::string_view generic_name) const -> UnitLayoutId {
  if (generic_name.empty()) {
    return find("close_order_infantry");
  }
  if (!doctrine.empty()) {
    std::string qualified;
    qualified.reserve(doctrine.size() + generic_name.size() + 1U);
    qualified.append(doctrine);
    qualified.push_back('.');
    qualified.append(generic_name);
    if (auto const id = find(qualified); id != k_invalid_layout) {
      return id;
    }
  }
  if (auto const id = find(generic_name); id != k_invalid_layout) {
    return id;
  }
  return find("close_order_infantry");
}

auto UnitLayoutLibrary::style(UnitLayoutId id) const -> const UnitLayoutStyle& {
  if (id == k_invalid_layout || static_cast<std::size_t>(id) >= m_styles.size()) {
    return m_fallback;
  }
  return m_styles[id];
}

auto UnitLayoutLibrary::name(UnitLayoutId id) const -> const std::string& {
  return style(id).id;
}

auto UnitLayoutLibrary::names() const -> std::vector<std::string> {
  std::vector<std::string> out;
  out.reserve(m_styles.size());
  for (const auto& entry : m_styles) {
    out.push_back(entry.id);
  }
  std::sort(out.begin(), out.end());
  return out;
}

auto rank_slot_for(int index, int count, int max_per_row) -> RankSlot {
  RankSlot slot;
  int const total = std::max(1, count);
  int const files = std::clamp(max_per_row, 1, total);
  slot.rows = (total + files - 1) / files;
  int const base = total / slot.rows;
  int const wide_ranks = total % slot.rows;
  slot.cols = base + (wide_ranks > 0 ? 1 : 0);

  int const clamped = std::clamp(index, 0, total - 1);
  int const wide_span = wide_ranks * (base + 1);
  int rank = 0;
  if (clamped < wide_span) {
    rank = clamped / (base + 1);
    slot.col = clamped % (base + 1);
    slot.rank_cols = base + 1;
  } else {
    int const rest = clamped - wide_span;
    rank = wide_ranks + rest / std::max(1, base);
    slot.col = rest % std::max(1, base);
    slot.rank_cols = std::max(1, base);
  }
  slot.row = slot.rows - 1 - std::min(rank, slot.rows - 1);
  return slot;
}

auto UnitLayoutSystem::instance() -> UnitLayoutSystem& {
  static UnitLayoutSystem system;
  return system;
}

auto UnitLayoutSystem::rows_for(int count, int max_per_row) -> int {
  int const cols = std::max(1, max_per_row);
  return std::max(1, (std::max(1, count) + cols - 1) / cols);
}

namespace {

[[nodiscard]] auto shortest_yaw_delta(float from, float to) -> float {
  float delta = std::fmod(to - from + 540.0F, 360.0F) - 180.0F;
  return delta;
}

} // namespace

auto UnitLayoutSystem::offset(const UnitLayoutQuery& query) const -> SoldierOffset {
  if (query.blend_from == k_invalid_layout || query.blend_from == query.layout ||
      query.blend_ratio >= 0.999F) {
    return raw_offset(query);
  }

  float const t = std::clamp(query.blend_ratio, 0.0F, 1.0F);
  UnitLayoutQuery from_query = query;
  from_query.layout = query.blend_from;
  from_query.blend_from = k_invalid_layout;
  from_query.blend_ratio = 1.0F;
  auto const from = raw_offset(from_query);

  UnitLayoutQuery to_query = query;
  to_query.blend_from = k_invalid_layout;
  to_query.blend_ratio = 1.0F;
  auto const to = raw_offset(to_query);

  float const smooth = t * t * (3.0F - 2.0F * t);
  return {from.offset_x + (to.offset_x - from.offset_x) * smooth,
          from.offset_z + (to.offset_z - from.offset_z) * smooth,
          from.yaw_offset +
              shortest_yaw_delta(from.yaw_offset, to.yaw_offset) * smooth};
}

auto UnitLayoutSystem::raw_offset(const UnitLayoutQuery& query) const -> SoldierOffset {
  const auto& style = UnitLayoutLibrary::instance().style(query.layout);

  int const total =
      std::max(1,
               query.count > 0 ? query.count
                               : std::max(1, query.rows) * std::max(1, query.cols));
  float const formed = std::clamp(query.formed_ratio, 0.0F, 1.0F);
  float const loosen = 1.0F + (1.0F - formed) * 0.35F;
  float const jitter_gain = 1.0F + (1.0F - formed) * 2.0F;

  RankSlot grid;
  switch (style.shape) {
  case UnitLayoutShape::Column:
    grid = rank_slot_for(
        query.index, total, static_cast<int>(std::lround(style.column_files)));
    break;
  case UnitLayoutShape::Wedge:
    grid = wedge_slot_for(
        std::clamp(query.index, 0, total - 1), total, style.wedge_growth);
    break;
  default:
    grid = rank_slot_for(query.index, total, std::max(1, query.cols));
    break;
  }

  if (total <= 1) {
    return {0.0F, 0.0F, 0.0F};
  }

  if (style.shape == UnitLayoutShape::Circle) {
    float const angle =
        (static_cast<float>(query.index) / static_cast<float>(total)) * 2.0F * k_pi;
    float const radius = query.spacing * style.radius_scale * loosen;
    float const jitter = query.spacing * style.lateral_jitter * jitter_gain;
    float const world_x =
        std::cos(angle) * radius + signed_unit(query.seed, query.index, 0) * jitter;
    float const world_z =
        std::sin(angle) * radius + signed_unit(query.seed, query.index, 1) * jitter;
    float const yaw = std::atan2(-world_x, -world_z) * k_rad_to_deg;
    return {world_x, world_z, yaw};
  }

  float const lateral_step = style.lateral_step(query.spacing) * loosen;
  float const depth_step = style.depth_step(query.spacing) * loosen;

  int const rank_from_front = std::max(0, grid.rows - 1 - grid.row);
  float const rear_t = grid.rows > 1 ? static_cast<float>(rank_from_front) /
                                           static_cast<float>(grid.rows - 1)
                                     : 0.0F;
  float const half_span = std::max(0.5F, (static_cast<float>(grid.cols) - 1.0F) * 0.5F);
  float const file_position = centered(grid.col, grid.rank_cols);
  float const file_u = std::clamp(file_position / half_span, -1.0F, 1.0F);

  float lateral_scale = 1.0F - style.front_rank_tightening * (1.0F - rear_t) +
                        style.rear_rank_loosening * rear_t;
  lateral_scale = std::max(0.25F, lateral_scale);

  float lateral = file_position * lateral_step * lateral_scale;
  float depth = centered(grid.row, grid.rows) * depth_step;

  int const grouping = static_cast<int>(std::lround(style.file_grouping));
  int const groups = grouping > 1 ? (grid.rank_cols + grouping - 1) / grouping : 0;
  if (groups >= 3) {
    float const group_u = centered(grid.col / grouping, groups);
    lateral += group_u * lateral_step * style.group_gap;
    depth += signed_unit(query.seed, grid.col / grouping + rank_from_front * 31, 6) *
             depth_step * style.group_depth_stagger;
  }

  if (grid.rows > 1) {
    float const stagger_parity = (rank_from_front % 2) == 0 ? -0.5F : 0.5F;
    lateral += lateral_step * style.rank_stagger * stagger_parity;
  }
  lateral += lateral_step * style.rank_echelon * static_cast<float>(rank_from_front);
  depth -= depth_step * style.rear_depth_bias * static_cast<float>(rank_from_front);
  depth += depth_step * style.rank_arc * (1.0F - (file_u * file_u));

  switch (style.shape) {
  case UnitLayoutShape::Wedge: {
    depth += depth_step * style.wedge_slope * (1.0F - std::abs(file_u));
    break;
  }
  case UnitLayoutShape::Shell: {
    bool const outer = grid.row == 0 || grid.row == grid.rows - 1 || grid.col == 0 ||
                       grid.col == grid.rank_cols - 1;
    if (outer) {
      lateral *= 1.06F;
      depth *= 1.04F;
    }
    break;
  }
  case UnitLayoutShape::Arc: {
    depth += depth_step * style.rank_arc * 1.5F * (file_u * file_u);
    break;
  }
  case UnitLayoutShape::Cluster: {
    int const cluster_size = std::max(1, static_cast<int>(style.cluster_size));
    int const cluster_id = query.index / cluster_size;
    float const phase = static_cast<float>(cluster_id) * 2.399963F +
                        static_cast<float>(query.seed & 0xFFU) * 0.0246F;
    lateral += std::cos(phase) * lateral_step * style.cluster_pull * 3.0F;
    depth += std::sin(phase) * depth_step * style.cluster_pull * 3.0F;
    break;
  }
  case UnitLayoutShape::Ranks:
  case UnitLayoutShape::LooseOrder:
  case UnitLayoutShape::Column:
  case UnitLayoutShape::Circle:
    break;
  }

  float const rear_jitter = 1.0F + style.rear_jitter_gain * rear_t;
  float const lateral_bound =
      lateral_step * (1.0F - std::clamp(style.min_separation_scale, 0.0F, 0.95F)) *
      0.5F;
  float const depth_bound =
      depth_step * (1.0F - std::clamp(style.min_separation_scale, 0.0F, 0.95F)) * 0.5F;

  float lateral_jitter = signed_unit(query.seed, query.index, 3) * lateral_step *
                         style.lateral_jitter * rear_jitter * jitter_gain;
  float depth_jitter = signed_unit(query.seed, query.index, 4) * depth_step *
                       style.depth_jitter * rear_jitter * jitter_gain;
  lateral += std::clamp(lateral_jitter, -lateral_bound, lateral_bound);
  depth += std::clamp(depth_jitter, -depth_bound, depth_bound);

  float const yaw = signed_unit(query.seed, query.index, 5) *
                    style.facing_jitter_degrees * (1.0F + (1.0F - formed));

  return {lateral, depth, yaw};
}

auto UnitLayoutSystem::compute(UnitLayoutId layout,
                               int count,
                               int max_per_row,
                               float spacing,
                               std::uint32_t seed,
                               float formed_ratio,
                               UnitLayoutId blend_from,
                               float blend_ratio) const -> std::vector<SoldierOffset> {
  std::vector<SoldierOffset> out;
  int const safe_count = std::max(0, count);
  if (safe_count == 0) {
    return out;
  }
  int const cols = std::clamp(max_per_row, 1, safe_count);
  int const rows = rows_for(safe_count, cols);

  out.reserve(static_cast<std::size_t>(safe_count));
  for (int index = 0; index < safe_count; ++index) {
    auto const slot = rank_slot_for(index, safe_count, cols);
    UnitLayoutQuery query;
    query.layout = layout;
    query.index = index;
    query.row = slot.row;
    query.col = slot.col;
    query.rows = rows;
    query.cols = cols;
    query.count = safe_count;
    query.spacing = spacing;
    query.seed = seed;
    query.formed_ratio = formed_ratio;
    query.blend_from = blend_from;
    query.blend_ratio = blend_ratio;
    out.push_back(offset(query));
  }
  return out;
}

} // namespace Game::Formation
