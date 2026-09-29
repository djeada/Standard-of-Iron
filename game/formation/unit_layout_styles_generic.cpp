#include <utility>

#include "unit_layout_style_tables.h"

namespace Game::Formation::style_tables {

auto make_style(std::string id, UnitLayoutShape shape) -> UnitLayoutStyle {
  UnitLayoutStyle style;
  style.id = std::move(id);
  style.shape = shape;
  return style;
}

void register_generic_infantry_styles(std::vector<UnitLayoutStyle>& out) {
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
}

void register_generic_mounted_styles(std::vector<UnitLayoutStyle>& out) {
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
}

void register_generic_support_styles(std::vector<UnitLayoutStyle>& out) {
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
}

void register_generic_sepulcher_styles(std::vector<UnitLayoutStyle>& out) {
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

void register_default_styles(std::vector<UnitLayoutStyle>& out) {
  register_generic_infantry_styles(out);
  register_generic_mounted_styles(out);
  register_generic_support_styles(out);
  register_generic_sepulcher_styles(out);
  register_rome_styles(out);
  register_carthage_styles(out);
  register_sepulcher_styles(out);
}

} // namespace Game::Formation::style_tables
