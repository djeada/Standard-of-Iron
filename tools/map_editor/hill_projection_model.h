#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPoint>
#include <QVector>

namespace MapEditor::HillProjection {

struct Model {
  int grid_width = 80;
  int grid_height = 80;
  double origin_x = 0.0;
  double origin_z = 0.0;
  double center_x = 0.0;
  double center_z = 0.0;
  double rotation_deg = 0.0;
  double base_radius = 10.0;
  double hill_half_width = 10.0;
  double hill_half_depth = 10.0;
  bool is_mountain = false;
  QVector<QPoint> hill_cells;
  QVector<QPoint> entrance_cells;
  QJsonArray preserved_entrances;
};

auto build_model(const QJsonObject& hill_json) -> Model;
auto entrances_from_cells(const Model& model,
                          const QVector<QPoint>& entrance_cells) -> QJsonArray;
auto apply_projection_to_hill_json(const QJsonObject& base_hill_json,
                                   const Model& model,
                                   const QVector<QPoint>& hill_cells,
                                   const QVector<QPoint>& entrance_cells)
    -> QJsonObject;

} // namespace MapEditor::HillProjection
