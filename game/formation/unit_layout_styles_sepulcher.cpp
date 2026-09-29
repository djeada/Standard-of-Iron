#include <utility>

#include "unit_layout_style_tables.h"

namespace Game::Formation::style_tables {

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

} // namespace Game::Formation::style_tables
