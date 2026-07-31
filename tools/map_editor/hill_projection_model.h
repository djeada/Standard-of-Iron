#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPoint>
#include <QStringList>
#include <QVector>

namespace MapEditor::HillProjection {

inline constexpr int k_min_entrances = 1;
inline constexpr int k_max_entrances = 3;
inline constexpr int k_min_projection_size = 24;
inline constexpr int k_max_projection_size = 240;
inline constexpr int k_projection_padding_cells = 4;

struct MapContext {
  double tile_size = 1.0;
  int map_grid_width = 0;
  int map_grid_height = 0;
};

struct Model {
  int grid_width = 80;
  int grid_height = 80;
  double origin_x = 0.0;
  double origin_z = 0.0;
  double center_x = 0.0;
  double center_z = 0.0;
  double rotation_deg = 0.0;
  double runtime_rotation_deg = 0.0;
  double base_radius = 10.0;
  double hill_half_width = 10.0;
  double hill_half_depth = 10.0;
  double organic_spread = 0.0;
  bool is_mountain = false;
  MapContext context;
  QVector<QPoint> hill_cells;
  QVector<QPoint> entrance_cells;
  QJsonArray invalid_entrances;
};

auto build_model(const QJsonObject& hill_json, const MapContext& context = {}) -> Model;

auto entrance_cells_from_json(const Model& model,
                              const QJsonArray& entrances) -> QVector<QPoint>;

auto rim_cells(const QVector<QPoint>& body_cells) -> QVector<QPoint>;

auto default_entrance_cell(const Model& model,
                           const QVector<QPoint>& body_cells) -> QPoint;

auto normalize_entrance_cells(const Model& model,
                              const QVector<QPoint>& body_cells,
                              const QVector<QPoint>& entrance_cells) -> QVector<QPoint>;

auto entrance_issues(const Model& model,
                     const QVector<QPoint>& body_cells,
                     const QVector<QPoint>& entrance_cells) -> QStringList;

auto entrances_from_cells(const Model& model,
                          const QVector<QPoint>& entrance_cells) -> QJsonArray;

auto apply_projection_to_hill_json(const QJsonObject& base_hill_json,
                                   const Model& model,
                                   const QVector<QPoint>& hill_cells,
                                   const QVector<QPoint>& entrance_cells)
    -> QJsonObject;

} // namespace MapEditor::HillProjection
