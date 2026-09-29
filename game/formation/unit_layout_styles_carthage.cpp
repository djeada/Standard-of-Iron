#include <utility>

#include "unit_layout_style_tables.h"

namespace Game::Formation::style_tables {

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

} // namespace Game::Formation::style_tables
