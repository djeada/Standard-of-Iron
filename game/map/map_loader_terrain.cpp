#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "json_keys.h"
#include "map/map_definition.h"
#include "map/terrain.h"
#include "map_loader_internal.h"
#include "river_geometry.h"

namespace Game::Map::loader_detail {

using namespace JsonKeys;

namespace {

void append_forest_terrain(const std::vector<Forest>& forests,
                           const GridDefinition& grid,
                           CoordSystem coord_sys,
                           std::vector<TerrainFeature>& out_terrain) {
  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  const float tile =
      coord_sys == CoordSystem::Grid ? std::max(min_tile_size, grid.tile_size) : 1.0F;

  out_terrain.reserve(out_terrain.size() + forests.size());
  for (const auto& forest : forests) {
    TerrainFeature feature;
    feature.type = TerrainType::Forest;
    if (coord_sys == CoordSystem::Grid) {
      feature.center_x =
          (forest.x - (grid.width * grid_center_offset - grid_center_offset)) * tile;
      feature.center_z =
          (forest.z - (grid.height * grid_center_offset - grid_center_offset)) * tile;
      feature.radius = forest.radius * tile;
    } else {
      feature.center_x = forest.x;
      feature.center_z = forest.z;
      feature.radius = forest.radius;
    }
    feature.height = 0.0F;
    feature.outline_seed = forest.outline_seed;
    out_terrain.push_back(feature);
  }
}

constexpr float k_authored_center_offset = 0.5F;
constexpr float k_authored_min_tile = 0.0001F;

void read_terrain_scalars(const QJsonObject& terrain_obj,
                          TerrainFeature& feature,
                          const GridDefinition& grid,
                          CoordSystem coord_sys) {
  constexpr double default_terrain_radius = 5.0;
  constexpr double default_terrain_height = 2.0;

  const QString type_str = terrain_obj.value("type").toString("flat");
  if (!try_parse_terrain_type(type_str, feature.type)) {
    qWarning() << "MapLoader: unknown terrain type" << type_str
               << "- defaulting to flat";
    feature.type = TerrainType::Flat;
  }

  const float coord_x = float(terrain_obj.value("x").toDouble(0.0));
  const float coord_z = float(terrain_obj.value("z").toDouble(0.0));

  if (coord_sys == CoordSystem::Grid) {
    const float tile = std::max(k_authored_min_tile, grid.tile_size);
    feature.center_x =
        (coord_x - (grid.width * k_authored_center_offset - k_authored_center_offset)) *
        tile;
    feature.center_z = (coord_z - (grid.height * k_authored_center_offset -
                                   k_authored_center_offset)) *
                       tile;
  } else {
    feature.center_x = coord_x;
    feature.center_z = coord_z;
  }

  feature.radius = float(terrain_obj.value("radius").toDouble(default_terrain_radius));
  feature.width = float(terrain_obj.value("width").toDouble(0.0));
  feature.depth = float(terrain_obj.value("depth").toDouble(0.0));

  if (feature.type == TerrainType::Hill && feature.width == 0.0F &&
      feature.depth == 0.0F) {
    feature.width = feature.radius * 2.0F;
    feature.depth = feature.radius * 2.0F;
  }

  const double default_feature_height = feature.type == TerrainType::Flat ||
                                                feature.type == TerrainType::Forest ||
                                                is_water_terrain(feature.type)
                                            ? 0.0
                                            : default_terrain_height;
  feature.height = float(terrain_obj.value("height").toDouble(default_feature_height));
  feature.rotation_deg = float(terrain_obj.value("rotation").toDouble(0.0));
}

void read_hill_mask_cells(const QJsonObject& terrain_obj,
                          TerrainFeature& feature,
                          const GridDefinition& grid,
                          CoordSystem coord_sys) {
  const QJsonArray cell_arr = terrain_obj.value("cells").toArray();
  const float tile = std::max(k_authored_min_tile, grid.tile_size);
  const auto append_cell = [&](double cell_x, double cell_z) {
    if (coord_sys == CoordSystem::Grid) {
      feature.mask_cells.emplace_back(
          float((cell_x -
                 (grid.width * k_authored_center_offset - k_authored_center_offset)) *
                tile),
          0.0F,
          float((cell_z -
                 (grid.height * k_authored_center_offset - k_authored_center_offset)) *
                tile));
    } else {
      feature.mask_cells.emplace_back(float(cell_x), 0.0F, float(cell_z));
    }
  };
  for (const auto cell_val : cell_arr) {
    const QJsonArray entry = cell_val.toArray();
    if (entry.size() == 2) {
      append_cell(entry.at(0).toDouble(0.0), entry.at(1).toDouble(0.0));
    } else if (entry.size() == 3) {
      const double row = entry.at(0).toDouble(0.0);
      const int from = int(std::lround(entry.at(1).toDouble(0.0)));
      const int to = int(std::lround(entry.at(2).toDouble(0.0)));
      for (int column = std::min(from, to); column <= std::max(from, to); ++column) {
        append_cell(double(column), row);
      }
    }
  }
}

void read_hill_shape_points(const QJsonObject& terrain_obj,
                            TerrainFeature& feature,
                            const GridDefinition& grid,
                            CoordSystem coord_sys) {
  const QJsonArray point_arr = terrain_obj.value("points").toArray();
  feature.shape_points.reserve(point_arr.size());
  for (const auto point_val : point_arr) {
    const QJsonObject point_obj = point_val.toObject();
    const float point_x = float(point_obj.value("x").toDouble(0.0));
    const float point_z = float(point_obj.value("z").toDouble(0.0));
    if (coord_sys == CoordSystem::Grid) {
      const float tile = std::max(k_authored_min_tile, grid.tile_size);
      feature.shape_points.emplace_back(
          (point_x -
           (grid.width * k_authored_center_offset - k_authored_center_offset)) *
              tile,
          0.0F,
          (point_z -
           (grid.height * k_authored_center_offset - k_authored_center_offset)) *
              tile);
    } else {
      feature.shape_points.emplace_back(point_x, 0.0F, point_z);
    }
  }
}

void read_hill_fields(const QJsonObject& terrain_obj,
                      TerrainFeature& feature,
                      const GridDefinition& grid,
                      CoordSystem coord_sys) {
  feature.crown = float(terrain_obj.value("crown").toDouble(0.0));
  feature.exact_height = terrain_obj.value("exact_height").toBool(false);
  const QString shape_str = terrain_obj.value("shape").toString();
  if (!shape_str.isEmpty() &&
      !parse_hill_shape(shape_str.toStdString(), feature.shape)) {
    qWarning() << "MapLoader: unknown hill shape" << shape_str
               << "- defaulting to blob";
    feature.shape = HillShape::Blob;
  }
  feature.thickness = float(terrain_obj.value("thickness").toDouble(0.0));
  feature.taper = float(terrain_obj.value("taper").toDouble(0.0));
  feature.has_sweep = terrain_obj.contains("arc");
  feature.sweep_degrees = float(terrain_obj.value("arc").toDouble(0.0));
  feature.has_sweep_start = terrain_obj.contains("arc_start");
  feature.sweep_start_degrees = float(terrain_obj.value("arc_start").toDouble(0.0));

  if (terrain_obj.value("cells").isArray()) {
    read_hill_mask_cells(terrain_obj, feature, grid, coord_sys);
  }
  if (terrain_obj.value("points").isArray()) {
    read_hill_shape_points(terrain_obj, feature, grid, coord_sys);
  }
}

void append_grid_entrance_samples(TerrainFeature& feature,
                                  const GridDefinition& grid,
                                  float entrance_x,
                                  float entrance_z,
                                  float entrance_radius_authored,
                                  float tile) {
  const auto radius_sq =
      static_cast<double>(entrance_radius_authored * entrance_radius_authored);
  const int min_grid_x =
      static_cast<int>(std::floor(entrance_x - entrance_radius_authored));
  const int max_grid_x =
      static_cast<int>(std::ceil(entrance_x + entrance_radius_authored));
  const int min_grid_z =
      static_cast<int>(std::floor(entrance_z - entrance_radius_authored));
  const int max_grid_z =
      static_cast<int>(std::ceil(entrance_z + entrance_radius_authored));
  for (int grid_x = min_grid_x; grid_x <= max_grid_x; ++grid_x) {
    for (int grid_z = min_grid_z; grid_z <= max_grid_z; ++grid_z) {
      const double dx = static_cast<double>(grid_x) - entrance_x;
      const double dz = static_cast<double>(grid_z) - entrance_z;
      if ((dx * dx + dz * dz) > radius_sq) {
        continue;
      }
      const float sampled_world_x =
          (float(grid_x) -
           (grid.width * k_authored_center_offset - k_authored_center_offset)) *
          tile;
      const float sampled_world_z =
          (float(grid_z) -
           (grid.height * k_authored_center_offset - k_authored_center_offset)) *
          tile;
      feature.entrances.emplace_back(sampled_world_x, 0.0F, sampled_world_z);
    }
  }
}

void append_world_entrance_samples(TerrainFeature& feature,
                                   float world_x,
                                   float world_z,
                                   float world_radius,
                                   float tile) {
  const float sample_step = std::max(0.5F, tile * 0.5F);
  const int radius_steps = std::max(
      1, int(std::ceil(world_radius / std::max(k_authored_min_tile, sample_step))));
  const float radius_sq = world_radius * world_radius;
  for (int dx = -radius_steps; dx <= radius_steps; ++dx) {
    for (int dz = -radius_steps; dz <= radius_steps; ++dz) {
      const float offset_x = float(dx) * sample_step;
      const float offset_z = float(dz) * sample_step;
      if ((offset_x * offset_x + offset_z * offset_z) <= radius_sq) {
        feature.entrances.emplace_back(world_x + offset_x, 0.0F, world_z + offset_z);
      }
    }
  }
}

void read_entrances(const QJsonObject& terrain_obj,
                    TerrainFeature& feature,
                    const GridDefinition& grid,
                    CoordSystem coord_sys) {
  auto entrance_arr = terrain_obj.value("entrances").toArray();
  for (const auto entrance_val : entrance_arr) {
    auto entrance_obj = entrance_val.toObject();
    const float entrance_x = float(entrance_obj.value("x").toDouble(0.0));
    const float entrance_z = float(entrance_obj.value("z").toDouble(0.0));
    float const entrance_radius_authored = float(entrance_obj.value("radius").toDouble(
        entrance_obj.value("width").toDouble(0.0) * 0.5));
    float world_x = entrance_x;
    float world_z = entrance_z;
    const float tile = std::max(k_authored_min_tile, grid.tile_size);
    if (coord_sys == CoordSystem::Grid) {
      world_x = (entrance_x -
                 (grid.width * k_authored_center_offset - k_authored_center_offset)) *
                tile;
      world_z = (entrance_z -
                 (grid.height * k_authored_center_offset - k_authored_center_offset)) *
                tile;
    }

    if (entrance_radius_authored <= 0.0F) {
      feature.entrances.emplace_back(world_x, 0.0F, world_z);
      continue;
    }

    if (coord_sys == CoordSystem::Grid) {
      append_grid_entrance_samples(
          feature, grid, entrance_x, entrance_z, entrance_radius_authored, tile);
      continue;
    }

    append_world_entrance_samples(
        feature, world_x, world_z, entrance_radius_authored, tile);
  }
}

void read_terrain(const QJsonArray& arr,
                  std::vector<TerrainFeature>& out,
                  const GridDefinition& grid,
                  CoordSystem coord_sys) {
  out.clear();
  out.reserve(arr.size());

  for (const auto terrain_val : arr) {
    auto terrain_obj = terrain_val.toObject();
    TerrainFeature feature;
    read_terrain_scalars(terrain_obj, feature, grid, coord_sys);

    if (feature.type == TerrainType::Flat) {
      feature.taper = float(terrain_obj.value("taper").toDouble(0.0));
      feature.raise_only = terrain_obj.value("raise").toBool(false);
      feature.fields = terrain_obj.value("fields").toBool(false);
    }
    if (feature.type == TerrainType::Hill) {
      read_hill_fields(terrain_obj, feature, grid, coord_sys);
    }
    if (terrain_obj.contains("entrances") && terrain_obj.value("entrances").isArray()) {
      read_entrances(terrain_obj, feature, grid, coord_sys);
    }

    out.push_back(feature);
  }
}

void read_rivers(const QJsonArray& arr,
                 std::vector<RiverSegment>& out,
                 const GridDefinition& grid,
                 CoordSystem coord_sys) {
  out.clear();

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr float default_river_width = 2.0F;
  constexpr float duplicate_point_epsilon_sq = 0.0001F;

  auto point_to_world = [&](const QJsonValue& value) -> std::optional<QVector3D> {
    if (!value.isArray()) {
      return std::nullopt;
    }
    const auto point = value.toArray();
    if (point.size() < 2) {
      return std::nullopt;
    }
    const float x = float(point[0].toDouble(0.0));
    const float z = float(point[1].toDouble(0.0));
    if (coord_sys != CoordSystem::Grid) {
      return QVector3D(x, 0.0F, z);
    }
    const float tile = std::max(min_tile_size, grid.tile_size);
    return QVector3D(
        (x - (grid.width * grid_center_offset - grid_center_offset)) * tile,
        0.0F,
        (z - (grid.height * grid_center_offset - grid_center_offset)) * tile);
  };

  for (const auto river_val : arr) {
    const auto river_obj = river_val.toObject();
    const float width = float(river_obj.value("width").toDouble(default_river_width));
    const bool authored_height = river_obj.contains("height");
    const float height = float(river_obj.value("height").toDouble(0.0));

    std::vector<QVector3D> points;
    auto append_point = [&points](const std::optional<QVector3D>& point) {
      if (!point.has_value()) {
        return;
      }
      if (!points.empty() &&
          (points.back() - *point).lengthSquared() < duplicate_point_epsilon_sq) {
        return;
      }
      points.push_back(*point);
    };

    if (river_obj.value("shape").toString().compare(QStringLiteral("ring"),
                                                    Qt::CaseInsensitive) == 0) {
      const float radius = float(river_obj.value("radius").toDouble(0.0));
      const RingRiver ring{
          .center_x = float(river_obj.value(X).toDouble(0.0)),
          .center_z = float(river_obj.value(Z).toDouble(0.0)),
          .radius_x = radius,
          .radius_z = float(river_obj.value("radius_z").toDouble(radius)),
          .segments = river_obj.value("segments").toInt(k_ring_river_default_segments),
      };
      if (ring.radius_x <= 0.0F || ring.radius_z <= 0.0F) {
        qWarning() << "MapLoader: ring river needs a positive radius - skipping";
        continue;
      }
      for (const auto& [px, pz] : ring_river_points(ring)) {
        append_point(point_to_world(QJsonArray{px, pz}));
      }
    } else {
      append_point(point_to_world(river_obj.value("start")));
      if (river_obj.value(ROAD_WAYPOINTS).isArray()) {
        for (const auto waypoint : river_obj.value(ROAD_WAYPOINTS).toArray()) {
          append_point(point_to_world(waypoint));
        }
      }
      append_point(point_to_world(river_obj.value("end")));
    }

    if (points.size() < 2U) {
      qWarning() << "MapLoader: river needs at least two distinct points - skipping";
      continue;
    }

    out.reserve(out.size() + points.size() - 1U);
    for (std::size_t index = 1; index < points.size(); ++index) {
      RiverSegment segment{points[index - 1U], points[index], width};
      if (authored_height) {
        segment.start.setY(height);
        segment.end.setY(height);
        segment.elevation_mode = WaterElevationMode::Authored;
      }
      out.push_back(segment);
    }
  }
}

void read_lakes(const QJsonArray& arr,
                std::vector<Lake>& out,
                const GridDefinition& grid,
                CoordSystem coord_sys) {
  out.clear();
  out.reserve(arr.size());

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr float default_lake_size = 8.0F;

  for (const auto lake_value : arr) {
    const auto lake_obj = lake_value.toObject();
    Lake lake;
    const float authored_x = float(lake_obj.value("x").toDouble(0.0));
    const float authored_z = float(lake_obj.value("z").toDouble(0.0));
    const float tile = std::max(min_tile_size, grid.tile_size);
    if (coord_sys == CoordSystem::Grid) {
      lake.center = {
          (authored_x - (grid.width * grid_center_offset - grid_center_offset)) * tile,
          0.0F,
          (authored_z - (grid.height * grid_center_offset - grid_center_offset)) *
              tile};
    } else {
      lake.center = {authored_x, 0.0F, authored_z};
    }

    const float radius = float(lake_obj.value("radius").toDouble(0.0));
    lake.width = std::max(min_tile_size,
                          float(lake_obj.value("width").toDouble(
                              radius > 0.0F ? radius * 2.0F : default_lake_size)));
    lake.depth = std::max(min_tile_size,
                          float(lake_obj.value("depth").toDouble(
                              radius > 0.0F ? radius * 2.0F : default_lake_size)));
    lake.rotation_deg = float(lake_obj.value("rotation").toDouble(0.0));
    if (lake_obj.contains("height")) {
      lake.center.setY(float(lake_obj.value("height").toDouble(0.0)));
      lake.elevation_mode = WaterElevationMode::Authored;
    }
    out.push_back(lake);
  }
}

void trim_rivers_at_lake_boundaries(std::vector<RiverSegment>& rivers,
                                    const std::vector<Lake>& lakes) {
  for (RiverSegment& river : rivers) {
    for (const Lake& lake : lakes) {
      const bool start_wet = point_in_lake(lake, river.start.x(), river.start.z());
      const bool end_wet = point_in_lake(lake, river.end.x(), river.end.z());
      if (start_wet == end_wet) {
        continue;
      }

      QVector3D& wet_endpoint = start_wet ? river.start : river.end;
      const QVector3D& dry_endpoint = start_wet ? river.end : river.start;
      if (const auto shoreline =
              lake_boundary_intersection(lake, dry_endpoint, wet_endpoint)) {
        wet_endpoint = *shoreline;
      }
    }
  }

  std::erase_if(rivers, [&lakes](const RiverSegment& river) {
    return std::any_of(lakes.begin(), lakes.end(), [&river](const Lake& lake) {
      return point_in_lake(lake, river.start.x(), river.start.z()) &&
             point_in_lake(lake, river.end.x(), river.end.z());
    });
  });
}

void read_roads(const QJsonArray& arr,
                std::vector<RoadSegment>& out,
                const GridDefinition& grid,
                CoordSystem coord_sys) {
  out.clear();

  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr float default_road_width = 3.0F;
  constexpr float duplicate_point_epsilon_sq = 0.0001F;

  auto point_to_world = [&](const QJsonValue& value) -> std::optional<QVector3D> {
    if (!value.isArray()) {
      return std::nullopt;
    }
    const auto point = value.toArray();
    if (point.size() < 2) {
      return std::nullopt;
    }

    const float x = float(point[0].toDouble(0.0));
    const float z = float(point[1].toDouble(0.0));
    if (coord_sys != CoordSystem::Grid) {
      return QVector3D(x, 0.0F, z);
    }

    const float tile = std::max(min_tile_size, grid.tile_size);
    return QVector3D(
        (x - (grid.width * grid_center_offset - grid_center_offset)) * tile,
        0.0F,
        (z - (grid.height * grid_center_offset - grid_center_offset)) * tile);
  };

  for (const auto road_val : arr) {
    const auto road_obj = road_val.toObject();
    const float width = float(road_obj.value("width").toDouble(default_road_width));
    const QString style = road_obj.value("style").toString("default");

    std::vector<QVector3D> points;
    auto append_point = [&points](const std::optional<QVector3D>& point) {
      if (!point.has_value()) {
        return;
      }
      if (!points.empty() &&
          (points.back() - *point).lengthSquared() < duplicate_point_epsilon_sq) {
        return;
      }
      points.push_back(*point);
    };

    append_point(point_to_world(road_obj.value("start")));
    if (road_obj.value(ROAD_WAYPOINTS).isArray()) {
      for (const auto waypoint : road_obj.value(ROAD_WAYPOINTS).toArray()) {
        append_point(point_to_world(waypoint));
      }
    }
    append_point(point_to_world(road_obj.value("end")));

    if (points.size() < 2U) {
      qWarning() << "MapLoader: road needs at least two distinct points - skipping";
      continue;
    }

    out.reserve(out.size() + points.size() - 1U);
    for (std::size_t i = 1; i < points.size(); ++i) {
      out.push_back({points[i - 1U], points[i], width, style});
    }
  }
}

void read_bridges(const QJsonArray& arr,
                  std::vector<Bridge>& out,
                  const GridDefinition& grid,
                  CoordSystem coord_sys) {
  out.clear();
  out.reserve(arr.size());

  constexpr float bridge_y_offset = 0.2F;
  constexpr float grid_center_offset = 0.5F;
  constexpr float min_tile_size = 0.0001F;
  constexpr double default_bridge_width = k_min_bridge_width;
  constexpr double default_bridge_height = 0.5;

  for (const auto bridge_val : arr) {
    auto bridge_obj = bridge_val.toObject();
    Bridge bridge;

    if (bridge_obj.contains("start") && bridge_obj.value("start").isArray()) {
      auto start_arr = bridge_obj.value("start").toArray();
      if (start_arr.size() >= 2) {
        const float start_x = float(start_arr[0].toDouble(0.0));
        const float start_z = float(start_arr[1].toDouble(0.0));

        if (coord_sys == CoordSystem::Grid) {
          const float tile = std::max(min_tile_size, grid.tile_size);
          bridge.start.setX(
              (start_x - (grid.width * grid_center_offset - grid_center_offset)) *
              tile);
          bridge.start.setY(bridge_y_offset);
          bridge.start.setZ(
              (start_z - (grid.height * grid_center_offset - grid_center_offset)) *
              tile);
        } else {
          bridge.start = QVector3D(start_x, bridge_y_offset, start_z);
        }
      }
    }

    if (bridge_obj.contains("end") && bridge_obj.value("end").isArray()) {
      auto end_arr = bridge_obj.value("end").toArray();
      if (end_arr.size() >= 2) {
        const float end_x = float(end_arr[0].toDouble(0.0));
        const float end_z = float(end_arr[1].toDouble(0.0));

        if (coord_sys == CoordSystem::Grid) {
          const float tile = std::max(min_tile_size, grid.tile_size);
          bridge.end.setX(
              (end_x - (grid.width * grid_center_offset - grid_center_offset)) * tile);
          bridge.end.setY(bridge_y_offset);
          bridge.end.setZ(
              (end_z - (grid.height * grid_center_offset - grid_center_offset)) * tile);
        } else {
          bridge.end = QVector3D(end_x, bridge_y_offset, end_z);
        }
      }
    }

    if (bridge_obj.contains("width")) {
      bridge.width =
          std::max(k_min_bridge_width,
                   float(bridge_obj.value("width").toDouble(default_bridge_width)));
    }

    if (bridge_obj.contains("height")) {
      bridge.height = float(bridge_obj.value("height").toDouble(default_bridge_height));
    }

    out.push_back(bridge);
  }
}

void lift_lake_features(MapDefinition& out_map) {
  for (const auto& feature : out_map.terrain) {
    if (feature.type != TerrainType::Lake) {
      continue;
    }
    out_map.lakes.push_back(
        {QVector3D(feature.center_x, 0.0F, feature.center_z),
         feature.width > 0.0F ? feature.width : feature.radius * 2.0F,
         feature.depth > 0.0F ? feature.depth : feature.radius * 2.0F,
         feature.rotation_deg});
  }
  std::erase_if(out_map.terrain, [](const TerrainFeature& feature) {
    return feature.type == TerrainType::Lake;
  });
}

} // namespace

void read_map_terrain(const QJsonObject& root, MapDefinition& out_map) {
  out_map.lakes.clear();
  if (root.contains(TERRAIN) && root.value(TERRAIN).isArray()) {
    read_terrain(root.value(TERRAIN).toArray(),
                 out_map.terrain,
                 out_map.grid,
                 out_map.coordSystem);
    lift_lake_features(out_map);
  }

  append_forest_terrain(
      out_map.forests, out_map.grid, out_map.coordSystem, out_map.terrain);

  if (root.contains(RIVERS) && root.value(RIVERS).isArray()) {
    read_rivers(root.value(RIVERS).toArray(),
                out_map.rivers,
                out_map.grid,
                out_map.coordSystem);
  }

  if (root.contains(LAKES) && root.value(LAKES).isArray()) {
    std::vector<Lake> authored_lakes;
    read_lakes(
        root.value(LAKES).toArray(), authored_lakes, out_map.grid, out_map.coordSystem);
    out_map.lakes.insert(
        out_map.lakes.end(), authored_lakes.begin(), authored_lakes.end());
  }

  trim_rivers_at_lake_boundaries(out_map.rivers, out_map.lakes);

  if (root.contains(ROADS) && root.value(ROADS).isArray()) {
    read_roads(
        root.value(ROADS).toArray(), out_map.roads, out_map.grid, out_map.coordSystem);
  }

  if (root.contains(BRIDGES) && root.value(BRIDGES).isArray()) {
    read_bridges(root.value(BRIDGES).toArray(),
                 out_map.bridges,
                 out_map.grid,
                 out_map.coordSystem);
    for (Bridge& bridge : out_map.bridges) {
      fit_bridge_span_to_riverbanks(bridge, out_map.rivers);
    }
  }
}

} // namespace Game::Map::loader_detail
