#include "hill_projection_model.h"

#include <QPointF>
#include <QSet>
#include <queue>

#include <algorithm>
#include <cmath>
#include <utility>

#include "map_json_keys.h"

namespace MapEditor::HillProjection {

namespace {

constexpr int k_max_projection_size = 80;
constexpr int k_grid_half_span = k_max_projection_size / 2;
constexpr double k_pi = 3.14159265358979323846;
constexpr double k_mountain_major_scale = 1.8;
constexpr double k_mountain_minor_scale = 0.22;

struct RotationAxes {
  double cos_yaw = 1.0;
  double sin_yaw = 0.0;
};

struct OrientedBounds {
  bool valid = false;
  double min_u = 0.0;
  double max_u = 0.0;
  double min_v = 0.0;
  double max_v = 0.0;
};

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

auto rotation_axes(double rotation_deg) -> RotationAxes {
  const double radians = rotation_deg * (k_pi / 180.0);
  return {std::cos(radians), std::sin(radians)};
}

auto project_world(double world_x,
                   double world_z,
                   const RotationAxes& axes) -> std::pair<double, double> {
  return {world_x * axes.cos_yaw + world_z * axes.sin_yaw,
          -world_x * axes.sin_yaw + world_z * axes.cos_yaw};
}

auto unproject_world(double projected_u,
                     double projected_v,
                     const RotationAxes& axes) -> QPointF {
  return {projected_u * axes.cos_yaw - projected_v * axes.sin_yaw,
          projected_u * axes.sin_yaw + projected_v * axes.cos_yaw};
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

auto mountain_major_radius(double radius) -> double {
  return std::max(radius * k_mountain_major_scale, radius + 3.0);
}

auto mountain_minor_radius(double radius) -> double {
  return std::max(radius * k_mountain_minor_scale, 0.8);
}

auto mountain_radius_threshold(double local_u,
                               double local_v,
                               double base_radius) -> double {
  double low = 0.5;
  double high = std::max(base_radius, low);
  while (true) {
    const double major = mountain_major_radius(high);
    const double minor = mountain_minor_radius(high);
    const double norm =
        (local_u * local_u) / (major * major) + (local_v * local_v) / (minor * minor);
    if (norm <= 1.0 || high >= 512.0) {
      break;
    }
    high *= 2.0;
  }
  for (int iter = 0; iter < 28; ++iter) {
    const double mid = (low + high) * 0.5;
    const double major = mountain_major_radius(mid);
    const double minor = mountain_minor_radius(mid);
    const double norm =
        (local_u * local_u) / (major * major) + (local_v * local_v) / (minor * minor);
    if (norm <= 1.0) {
      high = mid;
    } else {
      low = mid;
    }
  }
  return high;
}

auto radius_from_matching_cells(const Model& model,
                                const QVector<QPoint>& selected_cells,
                                double center_x,
                                double center_z,
                                double rotation_deg,
                                double base_radius,
                                bool is_mountain) -> double {
  if (selected_cells.isEmpty()) {
    return std::max(0.5, base_radius);
  }

  const RotationAxes axes = rotation_axes(rotation_deg);
  QSet<quint64> selected_set;
  for (const QPoint& cell : selected_cells) {
    selected_set.insert(encode_cell(cell));
  }

  double lower = 0.5;
  double upper = 512.0;
  for (int cell_z = 0; cell_z < model.grid_height; ++cell_z) {
    const double world_z = world_z_from_cell(model, cell_z);
    for (int cell_x = 0; cell_x < model.grid_width; ++cell_x) {
      const double world_x = world_x_from_cell(model, cell_x);
      const auto [local_u, local_v] =
          project_world(world_x - center_x, world_z - center_z, axes);
      const double threshold =
          is_mountain ? mountain_radius_threshold(local_u, local_v, base_radius)
                      : std::sqrt(local_u * local_u + local_v * local_v);
      if (selected_set.contains(encode_cell(QPoint(cell_x, cell_z)))) {
        lower = std::max(lower, threshold);
      } else {
        upper = std::min(upper, threshold);
      }
    }
  }

  constexpr double k_interval_epsilon = 1e-6;
  if (lower + k_interval_epsilon < upper) {
    if (base_radius >= lower && base_radius < upper) {
      return base_radius;
    }
    return (lower + upper) * 0.5;
  }
  return lower;
}

auto append_rotated_ellipse_cells(Model* model,
                                  double half_width,
                                  double half_depth,
                                  double rotation_deg) -> void {
  if (model == nullptr || half_width <= 0.0 || half_depth <= 0.0) {
    return;
  }

  const RotationAxes axes = rotation_axes(rotation_deg);
  const double inv_width_sq = 1.0 / (half_width * half_width);
  const double inv_depth_sq = 1.0 / (half_depth * half_depth);

  for (int cell_z = 0; cell_z < model->grid_height; ++cell_z) {
    const double world_z = world_z_from_cell(*model, cell_z);
    for (int cell_x = 0; cell_x < model->grid_width; ++cell_x) {
      const double world_x = world_x_from_cell(*model, cell_x);
      const auto [local_u, local_v] =
          project_world(world_x - model->center_x, world_z - model->center_z, axes);
      const double norm =
          (local_u * local_u) * inv_width_sq + (local_v * local_v) * inv_depth_sq;
      if (norm <= 1.0) {
        model->hill_cells.append(QPoint(cell_x, cell_z));
      }
    }
  }
}

auto oriented_bounds_from_cells(const Model& model,
                                const QVector<QPoint>& cells,
                                double rotation_deg) -> OrientedBounds {
  OrientedBounds bounds;
  const RotationAxes axes = rotation_axes(rotation_deg);
  for (const QPoint& cell : cells) {
    const double world_x = world_x_from_cell(model, cell.x());
    const double world_z = world_z_from_cell(model, cell.y());
    const auto [projected_u, projected_v] = project_world(world_x, world_z, axes);
    if (!bounds.valid) {
      bounds.valid = true;
      bounds.min_u = bounds.max_u = projected_u;
      bounds.min_v = bounds.max_v = projected_v;
      continue;
    }
    bounds.min_u = std::min(bounds.min_u, projected_u);
    bounds.max_u = std::max(bounds.max_u, projected_u);
    bounds.min_v = std::min(bounds.min_v, projected_v);
    bounds.max_v = std::max(bounds.max_v, projected_v);
  }
  return bounds;
}

auto fit_mountain_radius(const Model& model,
                         const QVector<QPoint>& mountain_cells,
                         double center_x,
                         double center_z,
                         double rotation_deg,
                         double base_radius) -> double {
  return radius_from_matching_cells(
      model, mountain_cells, center_x, center_z, rotation_deg, base_radius, true);
}

} // namespace

auto build_model(const QJsonObject& hill_json) -> Model {
  Model model;
  model.grid_width = k_max_projection_size;
  model.grid_height = k_max_projection_size;

  model.center_x = hill_json.value(MapJsonKeys::x).toDouble(0.0);
  model.center_z = hill_json.value(MapJsonKeys::z).toDouble(0.0);
  model.rotation_deg = hill_json.value(MapJsonKeys::rotation).toDouble(0.0);
  model.origin_x = std::floor(model.center_x) - static_cast<double>(k_grid_half_span);
  model.origin_z = std::floor(model.center_z) - static_cast<double>(k_grid_half_span);

  const double radius =
      std::max(1.0, hill_json.value(MapJsonKeys::radius).toDouble(10.0));
  model.base_radius = radius;
  const QString terrain_type =
      hill_json.value(MapJsonKeys::type).toString().trimmed().toLower();
  model.is_mountain = terrain_type == QStringLiteral("mountain");

  if (model.is_mountain) {
    model.hill_half_width = mountain_major_radius(radius);
    model.hill_half_depth = mountain_minor_radius(radius);
  } else {
    const double width = hill_json.value(MapJsonKeys::width).toDouble(0.0);
    const double depth = hill_json.value(MapJsonKeys::depth).toDouble(0.0);
    model.hill_half_width = std::max(0.5, width > 0.0 ? width : radius);
    model.hill_half_depth = std::max(0.5, depth > 0.0 ? depth : radius);
  }
  append_rotated_ellipse_cells(
      &model, model.hill_half_width, model.hill_half_depth, model.rotation_deg);
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
  const double rotation_deg = base_hill_json.value(MapJsonKeys::rotation).toDouble(0.0);

  const QVector<QPoint> normalized_hill = unique_in_bounds_cells(model, hill_cells);
  if (!normalized_hill.isEmpty()) {
    const OrientedBounds bounds =
        oriented_bounds_from_cells(model, normalized_hill, rotation_deg);
    if (bounds.valid) {
      const RotationAxes axes = rotation_axes(rotation_deg);
      const double center_u = (bounds.min_u + bounds.max_u) * 0.5;
      const double center_v = (bounds.min_v + bounds.max_v) * 0.5;
      const QPointF center = unproject_world(center_u, center_v, axes);
      updated[MapJsonKeys::x] = center.x();
      updated[MapJsonKeys::z] = center.y();

      if (is_mountain) {
        const double radius = fit_mountain_radius(
            model,
            normalized_hill,
            center.x(),
            center.y(),
            rotation_deg,
            base_hill_json.value(MapJsonKeys::radius).toDouble(model.base_radius));
        updated[MapJsonKeys::radius] = radius;
        updated.remove(MapJsonKeys::width);
        updated.remove(MapJsonKeys::depth);
      } else {
        const double half_width = std::max(0.5, (bounds.max_u - bounds.min_u) * 0.5);
        const double half_depth = std::max(0.5, (bounds.max_v - bounds.min_v) * 0.5);
        if (std::abs(half_width - half_depth) <= 1e-6) {
          const double radius = radius_from_matching_cells(
              model,
              normalized_hill,
              center.x(),
              center.y(),
              rotation_deg,
              base_hill_json.value(MapJsonKeys::radius).toDouble(model.base_radius),
              false);
          updated[MapJsonKeys::radius] = radius;
          updated.remove(MapJsonKeys::width);
          updated.remove(MapJsonKeys::depth);
        } else {
          updated[MapJsonKeys::width] = half_width;
          updated[MapJsonKeys::depth] = half_depth;
          updated.remove(MapJsonKeys::radius);
        }
      }
    }
  }

  if (is_mountain) {
    updated.remove(MapJsonKeys::entrances);
    return updated;
  }

  const QJsonArray entrances = entrances_from_cells(model, entrance_cells);
  if (entrances.isEmpty()) {
    updated.remove(MapJsonKeys::entrances);
  } else {
    updated[MapJsonKeys::entrances] = entrances;
  }

  return updated;
}

} // namespace MapEditor::HillProjection
