#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

#include "terrain.h"
#include "terrain_build_internal.h"
#include "terrain_footprint.h"
#include "terrain_landform.h"

namespace Game::Map::terrain_build {

struct EntranceCluster {
  float grid_x{};
  float grid_z{};
  float radius{};
  int samples{};
};

struct RampAxis {
  int ex;
  int ez;
  float dir_x;
  float dir_z;
  float length;
};

struct RampLayout {
  int ex;
  int ez;
  float dir_x;
  float dir_z;
  float perp_x;
  float perp_z;
  int outward_steps;
  int total_ramp_steps;
  float entry_width;
  float ramp_origin_x;
  float ramp_origin_z;
  float ramp_span;
  float ramp_end_x;
  float ramp_end_z;
  int corridor_min_x;
  int corridor_max_x;
  int corridor_min_z;
  int corridor_max_z;
};

struct RampCell {
  float across;
  float edge_t;
  float tapered_width;
  float center_ramp_height;
  float s;
  float mouth;
  float lower_ramp;
  float ramp_progress;
  bool builds_apron;
};

class HillStamp {
public:
  HillStamp(BuildContext& ctx,
            const TerrainFeature& feature,
            const FeatureFrame& frame);
  HillStamp(const HillStamp&) = delete;
  auto operator=(const HillStamp&) -> HillStamp& = delete;

  void run();

private:
  void place_footprint();
  void build_shape();
  void measure_extents();
  void configure_landform();

  [[nodiscard]] auto local_to_world(float local_x, float local_z) const -> QVector3D;
  [[nodiscard]] auto slope_distance(float local_x, float local_z) const -> float;
  [[nodiscard]] auto crown_distance(float local_x, float local_z) const -> float;
  [[nodiscard]] auto entrance_outside_hill(float local_x,
                                           float local_z,
                                           float dir_local_x,
                                           float dir_local_z,
                                           float start_reach) const -> QVector3D;

  [[nodiscard]] auto resolve_entrances() const -> std::vector<QVector3D>;
  void raise_body();
  [[nodiscard]] auto cluster_entrances(const std::vector<QVector3D>& hill_entrances)
      const -> std::vector<EntranceCluster>;
  void carve_entrance(const EntranceCluster& entrance);
  [[nodiscard]] auto
  locate_ramp_axis(const EntranceCluster& entrance) -> std::optional<RampAxis>;
  [[nodiscard]] auto measure_plateau_steps(const RampAxis& axis,
                                           int steps) const -> int;
  [[nodiscard]] auto measure_ramp(const RampAxis& axis,
                                  const EntranceCluster& entrance) const -> RampLayout;
  void sculpt_ramp(const RampLayout& ramp);
  [[nodiscard]] auto sample_ramp_cell(const RampLayout& ramp,
                                      int ix,
                                      int iz) const -> std::optional<RampCell>;
  void apply_ramp_cell(const RampCell& cell, int ix, int iz);
  void trim_walkable_to_crown();
  void trim_walkable_edges();
  void flood_walkable_from_entrances();

  GridLayers& L;
  const std::vector<float>& base_heights;
  std::vector<float>& erosion_strength;
  std::vector<std::uint8_t>& erosion_protected;
  const TerrainFeature& feature;
  const float grid_center_x;
  const float grid_center_z;
  const float grid_half_width;
  const float grid_half_height;

  bool campaign_landform_scale = false;
  bool shaped_geometry = false;
  float cos_a = 1.0F;
  float sin_a = 0.0F;
  float hill_height = 0.0F;
  float crown_thickness = 0.0F;
  float slope_width = 0.0F;
  float slope_depth = 0.0F;
  int min_x = 0;
  int max_x = 0;
  int min_z = 0;
  int max_z = 0;
  FootprintCells footprint;
  HillCrownProfile crown_profile;
  HillShapeGeometry shape_geometry;
  HillCrownCells crown;
  Landform::HillConfig hill_config;
  std::vector<std::uint8_t> walkable_mask;
  std::vector<std::uint8_t> entrance_line_mask;
  std::vector<int> entrance_indices;
  std::vector<float> ground_before_hill;
};

} // namespace Game::Map::terrain_build
