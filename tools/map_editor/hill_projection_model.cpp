#include "hill_projection_model.h"

#include <QSet>
#include <queue>

#include <algorithm>
#include <cmath>

#include "map_json_keys.h"

namespace MapEditor::HillProjection {

namespace {

constexpr int k_max_projection_size = 80;
constexpr int k_grid_half_span = k_max_projection_size / 2;

auto encode_cell(const QPoint& cell) -> quint64 {
  return (static_cast<quint64>(static_cast<quint32>(cell.y())) << 32U) |
         static_cast<quint32>(cell.x());
}

auto numeric_value(const QJsonValue& value, double* out_value) -> bool {
  if (!value.isDouble()) {
    return false;
  }
  *out_value = value.toDouble();
  return true;
}

auto world_x_from_cell(const Model& model, int cell_x) -> double {
  return model.origin_x + static_cast<double>(cell_x);
}

auto world_z_from_cell(const Model& model, int cell_z) -> double {
  return model.origin_z + static_cast<double>(cell_z);
}

auto cell_from_world(const Model& model, double world_x, double world_z) -> QPoint {
  return QPoint(static_cast<int>(std::lround(world_x - model.origin_x)),
                static_cast<int>(std::lround(world_z - model.origin_z)));
}

auto in_bounds(const Model& model, const QPoint& cell) -> bool {
  return cell.x() >= 0 && cell.x() < model.grid_width && cell.y() >= 0 &&
         cell.y() < model.grid_height;
}

auto sort_cells(QVector<QPoint>* cells) -> void {
  std::sort(cells->begin(), cells->end(), [](const QPoint& lhs, const QPoint& rhs) {
    if (lhs.y() == rhs.y()) {
      return lhs.x() < rhs.x();
    }
    return lhs.y() < rhs.y();
  });
}

auto unique_in_bounds_cells(const Model& model,
                            const QVector<QPoint>& cells) -> QVector<QPoint> {
  QSet<quint64> seen;
  QVector<QPoint> normalized;
  normalized.reserve(cells.size());
  for (const QPoint& cell : cells) {
    if (!in_bounds(model, cell)) {
      continue;
    }
    const quint64 key = encode_cell(cell);
    if (seen.contains(key)) {
      continue;
    }
    seen.insert(key);
    normalized.append(cell);
  }
  sort_cells(&normalized);
  return normalized;
}

auto connected_components(const QVector<QPoint>& cells) -> QVector<QVector<QPoint>> {
  constexpr int k_neighbors[8][2] = {
      {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

  QSet<quint64> cell_set;
  for (const QPoint& cell : cells) {
    cell_set.insert(encode_cell(cell));
  }

  QSet<quint64> visited;
  QVector<QVector<QPoint>> components;

  for (const QPoint& start : cells) {
    const quint64 start_key = encode_cell(start);
    if (visited.contains(start_key)) {
      continue;
    }

    QVector<QPoint> component;
    std::queue<QPoint> pending;
    pending.push(start);
    visited.insert(start_key);

    while (!pending.empty()) {
      const QPoint current = pending.front();
      pending.pop();
      component.append(current);

      for (const auto& delta : k_neighbors) {
        const QPoint neighbor(current.x() + delta[0], current.y() + delta[1]);
        const quint64 neighbor_key = encode_cell(neighbor);
        if (!cell_set.contains(neighbor_key) || visited.contains(neighbor_key)) {
          continue;
        }
        visited.insert(neighbor_key);
        pending.push(neighbor);
      }
    }

    sort_cells(&component);
    components.append(component);
  }

  return components;
}

} // namespace

auto build_model(const QJsonObject& hill_json) -> Model {
  Model model;
  model.grid_width = k_max_projection_size;
  model.grid_height = k_max_projection_size;

  model.center_x = hill_json.value(MapJsonKeys::x).toDouble(0.0);
  model.center_z = hill_json.value(MapJsonKeys::z).toDouble(0.0);
  model.origin_x = std::floor(model.center_x) - static_cast<double>(k_grid_half_span);
  model.origin_z = std::floor(model.center_z) - static_cast<double>(k_grid_half_span);

  const double radius =
      std::max(1.0, hill_json.value(MapJsonKeys::radius).toDouble(10.0));
  const QString terrain_type =
      hill_json.value(MapJsonKeys::type).toString().trimmed().toLower();
  const bool is_mountain = terrain_type == QStringLiteral("mountain");

  double resolved_width;
  double resolved_depth;
  if (is_mountain) {
    // Match game engine: major_radius = radius*1.8, minor_radius = radius*0.22
    const double major_r = std::max(radius * 1.8, radius + 3.0);
    const double minor_r = std::max(radius * 0.22, 0.8);
    resolved_width = major_r * 2.0;
    resolved_depth = minor_r * 2.0;
  } else {
    const double width = hill_json.value(MapJsonKeys::width).toDouble(0.0);
    const double depth = hill_json.value(MapJsonKeys::depth).toDouble(0.0);
    resolved_width = width > 0.0 ? width : (radius * 2.0);
    resolved_depth = depth > 0.0 ? depth : (radius * 2.0);
  }
  model.hill_half_width = std::max(0.5, resolved_width * 0.5);
  model.hill_half_depth = std::max(0.5, resolved_depth * 0.5);

  const double min_x = model.center_x - model.hill_half_width;
  const double max_x = model.center_x + model.hill_half_width;
  const double min_z = model.center_z - model.hill_half_depth;
  const double max_z = model.center_z + model.hill_half_depth;

  const int hill_min_x = std::clamp(
      static_cast<int>(std::floor(min_x - model.origin_x)), 0, model.grid_width - 1);
  const int hill_max_x = std::clamp(
      static_cast<int>(std::ceil(max_x - model.origin_x)), 0, model.grid_width - 1);
  const int hill_min_z = std::clamp(
      static_cast<int>(std::floor(min_z - model.origin_z)), 0, model.grid_height - 1);
  const int hill_max_z = std::clamp(
      static_cast<int>(std::ceil(max_z - model.origin_z)), 0, model.grid_height - 1);
  for (int z = hill_min_z; z <= hill_max_z; ++z) {
    for (int x = hill_min_x; x <= hill_max_x; ++x) {
      model.hill_cells.append(QPoint(x, z));
    }
  }
  if (model.hill_cells.isEmpty()) {
    const QPoint center_cell = cell_from_world(model, model.center_x, model.center_z);
    if (in_bounds(model, center_cell)) {
      model.hill_cells.append(center_cell);
    }
  }

  const QJsonArray entrances = hill_json.value(MapJsonKeys::entrances).toArray();
  QSet<quint64> unique_entrances;
  for (const QJsonValue& entrance_value : entrances) {
    if (!entrance_value.isObject()) {
      model.preserved_entrances.append(entrance_value);
      continue;
    }

    const QJsonObject entrance = entrance_value.toObject();
    double x = 0.0;
    double z = 0.0;
    double radius = 0.0;
    if (numeric_value(entrance.value(MapJsonKeys::x), &x) &&
        numeric_value(entrance.value(MapJsonKeys::z), &z)) {
      if (!numeric_value(entrance.value(MapJsonKeys::radius), &radius)) {
        double width = 0.0;
        if (numeric_value(entrance.value(MapJsonKeys::width), &width)) {
          radius = width * 0.5;
        }
      }

      const QPoint center_cell = cell_from_world(model, x, z);
      if (radius <= 0.0) {
        if (in_bounds(model, center_cell)) {
          unique_entrances.insert(encode_cell(center_cell));
          continue;
        }
      } else {
        const double radius_sq = radius * radius;
        const int min_cell_x =
            std::max(0, static_cast<int>(std::floor((x - radius) - model.origin_x)));
        const int max_cell_x =
            std::min(model.grid_width - 1,
                     static_cast<int>(std::ceil((x + radius) - model.origin_x)));
        const int min_cell_z =
            std::max(0, static_cast<int>(std::floor((z - radius) - model.origin_z)));
        const int max_cell_z =
            std::min(model.grid_height - 1,
                     static_cast<int>(std::ceil((z + radius) - model.origin_z)));
        bool has_visible_cell = false;
        for (int cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z) {
          for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x) {
            const double world_cell_x = world_x_from_cell(model, cell_x);
            const double world_cell_z = world_z_from_cell(model, cell_z);
            const double dx = world_cell_x - x;
            const double dz = world_cell_z - z;
            const double dist_sq = dx * dx + dz * dz;
            if (dist_sq > radius_sq) {
              continue;
            }
            const QPoint sampled(cell_x, cell_z);
            if (!in_bounds(model, sampled)) {
              continue;
            }
            unique_entrances.insert(encode_cell(sampled));
            has_visible_cell = true;
          }
        }
        if (has_visible_cell) {
          continue;
        }
      }

      if (in_bounds(model, center_cell)) {
        unique_entrances.insert(encode_cell(center_cell));
        continue;
      }
    }

    model.preserved_entrances.append(entrance_value);
  }

  for (quint64 key : unique_entrances) {
    model.entrance_cells.append(QPoint(static_cast<int>(key & 0xFFFFFFFFULL),
                                       static_cast<int>((key >> 32U) & 0xFFFFFFFFULL)));
  }

  sort_cells(&model.hill_cells);
  sort_cells(&model.entrance_cells);

  return model;
}

auto entrances_from_cells(const Model& model,
                          const QVector<QPoint>& entrance_cells) -> QJsonArray {
  QJsonArray entrances = model.preserved_entrances;
  const QVector<QPoint> normalized_cells =
      unique_in_bounds_cells(model, entrance_cells);
  const QVector<QVector<QPoint>> components = connected_components(normalized_cells);
  for (const QVector<QPoint>& component : components) {
    if (component.isEmpty()) {
      continue;
    }

    double sum_x = 0.0;
    double sum_z = 0.0;
    for (const QPoint& cell : component) {
      sum_x += world_x_from_cell(model, cell.x());
      sum_z += world_z_from_cell(model, cell.y());
    }

    const double center_x = sum_x / static_cast<double>(component.size());
    const double center_z = sum_z / static_cast<double>(component.size());

    QJsonObject entrance;
    entrance[MapJsonKeys::x] = center_x;
    entrance[MapJsonKeys::z] = center_z;

    if (component.size() > 1) {
      double max_dist = 0.0;
      for (const QPoint& cell : component) {
        const double cell_x = world_x_from_cell(model, cell.x());
        const double cell_z = world_z_from_cell(model, cell.y());
        const double dx = cell_x - center_x;
        const double dz = cell_z - center_z;
        max_dist = std::max(max_dist, std::sqrt(dx * dx + dz * dz));
      }
      entrance[MapJsonKeys::radius] = std::max(0.5, max_dist);
    }
    entrances.append(entrance);
  }

  return entrances;
}

auto apply_projection_to_hill_json(const QJsonObject& base_hill_json,
                                   const Model& model,
                                   const QVector<QPoint>& hill_cells,
                                   const QVector<QPoint>& entrance_cells)
    -> QJsonObject {
  QJsonObject updated = base_hill_json;

  const QString terrain_type =
      base_hill_json.value(MapJsonKeys::type).toString().trimmed().toLower();
  const bool is_mountain = terrain_type == QStringLiteral("mountain");

  if (!is_mountain) {
    // For hills: update position and size from the painted body cells.
    const QVector<QPoint> normalized_hill = unique_in_bounds_cells(model, hill_cells);
    if (!normalized_hill.isEmpty()) {
      double min_x = world_x_from_cell(model, normalized_hill.first().x());
      double max_x = min_x;
      double min_z = world_z_from_cell(model, normalized_hill.first().y());
      double max_z = min_z;

      for (const QPoint& cell : normalized_hill) {
        const double world_x = world_x_from_cell(model, cell.x());
        const double world_z = world_z_from_cell(model, cell.y());
        min_x = std::min(min_x, world_x);
        max_x = std::max(max_x, world_x);
        min_z = std::min(min_z, world_z);
        max_z = std::max(max_z, world_z);
      }

      updated[MapJsonKeys::x] = (min_x + max_x) * 0.5;
      updated[MapJsonKeys::z] = (min_z + max_z) * 0.5;
      updated[MapJsonKeys::width] = (max_x - min_x) + 1.0;
      updated[MapJsonKeys::depth] = (max_z - min_z) + 1.0;
      updated.remove(MapJsonKeys::radius);
    }
  }
  // For mountains: the game engine derives shape from `radius` (major_radius =
  // radius*1.8, minor_radius = radius*0.22). Never modify radius/width/depth/x/z — only
  // entrances.

  const QJsonArray entrances = entrances_from_cells(model, entrance_cells);
  if (entrances.isEmpty()) {
    updated.remove(MapJsonKeys::entrances);
  } else {
    updated[MapJsonKeys::entrances] = entrances;
  }

  return updated;
}

} // namespace MapEditor::HillProjection
