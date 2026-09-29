#include <utility>

#include "unit_layout_style_tables.h"

namespace Game::Formation::style_tables {

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

} // namespace Game::Formation::style_tables
