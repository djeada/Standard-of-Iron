#include "hill_projection_model.h"

#include <QPointF>
#include <QSet>
#include <queue>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "game/map/terrain_footprint.h"
#include "map_json_keys.h"

namespace MapEditor::HillProjection {

namespace {

constexpr double k_pi = 3.14159265358979323846;
constexpr double k_entrance_rim_tolerance_cells = 2.0;

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

struct EntranceSpec {
  double x = 0.0;
  double z = 0.0;
  double radius = 0.0;
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

auto model_tile_size(const Model& model) -> double {
  return model.context.tile_size > 0.0 ? model.context.tile_size : 1.0;
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

auto cell_set(const QVector<QPoint>& cells) -> QSet<quint64> {
  QSet<quint64> set;
  set.reserve(cells.size());
  for (const QPoint& cell : cells) {
    set.insert(encode_cell(cell));
  }
  return set;
}

auto connected_components(const QVector<QPoint>& cells) -> QVector<QVector<QPoint>> {
  constexpr int k_neighbors[8][2] = {
      {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

  const QSet<quint64> known = cell_set(cells);
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
        if (!known.contains(neighbor_key) || visited.contains(neighbor_key)) {
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

auto mountain_radius_threshold(double local_u,
                               double local_v,
                               double base_radius) -> double {
  const auto normalized = [&](double radius) {
    const double major = Game::Map::mountain_major_radius_cells(float(radius));
    const double minor = Game::Map::mountain_minor_radius_cells(float(radius));
    return (local_u * local_u) / (major * major) +
           (local_v * local_v) / (minor * minor);
  };

  double low = 0.5;
  double high = std::max(base_radius, low);
  while (normalized(high) > 1.0 && high < 512.0) {
    high *= 2.0;
  }
  for (int iter = 0; iter < 28; ++iter) {
    const double mid = (low + high) * 0.5;
    if (normalized(mid) <= 1.0) {
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
  const QSet<quint64> selected_set = cell_set(selected_cells);

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

auto read_entrance_specs(const QJsonArray& entrances,
                         QJsonArray* out_invalid) -> QVector<EntranceSpec> {
  QVector<EntranceSpec> specs;
  specs.reserve(entrances.size());
  for (const QJsonValue entrance_value : entrances) {
    if (!entrance_value.isObject()) {
      out_invalid->append(entrance_value);
      continue;
    }

    const QJsonObject entrance = entrance_value.toObject();
    EntranceSpec spec;
    if (!numeric_value(entrance.value(MapJsonKeys::x), &spec.x) ||
        !numeric_value(entrance.value(MapJsonKeys::z), &spec.z)) {
      out_invalid->append(entrance_value);
      continue;
    }
    if (!numeric_value(entrance.value(MapJsonKeys::radius), &spec.radius)) {
      double width = 0.0;
      if (numeric_value(entrance.value(MapJsonKeys::width), &width)) {
        spec.radius = width * 0.5;
      }
    }
    specs.append(spec);
  }
  return specs;
}

auto projection_half_span(const Model& model,
                          const Game::Map::FootprintCells& footprint,
                          const QVector<EntranceSpec>& entrances) -> int {
  const double spread = 1.0 + static_cast<double>(footprint.organic_spread);
  const double span_u = static_cast<double>(footprint.half_width) * spread;
  const double span_v = static_cast<double>(footprint.half_depth) * spread;
  const RotationAxes axes = rotation_axes(model.runtime_rotation_deg);
  const double half_x =
      std::abs(axes.cos_yaw) * span_u + std::abs(axes.sin_yaw) * span_v;
  const double half_z =
      std::abs(axes.sin_yaw) * span_u + std::abs(axes.cos_yaw) * span_v;

  double required = std::max(half_x, half_z);
  for (const EntranceSpec& entrance : entrances) {
    const double reach = std::max(entrance.radius, 0.0);
    required = std::max(required, std::abs(entrance.x - model.center_x) + reach);
    required = std::max(required, std::abs(entrance.z - model.center_z) + reach);
  }

  const int half_span =
      static_cast<int>(std::ceil(required)) + k_projection_padding_cells;
  const int size = std::clamp(
      (half_span * 2) + 1, k_min_projection_size + 1, k_max_projection_size + 1);
  return (size - 1) / 2;
}

auto append_entrance_cells(const Model& model,
                           const EntranceSpec& entrance,
                           QSet<quint64>* out_cells) -> bool {
  const QPoint center_cell = cell_from_world(model, entrance.x, entrance.z);
  if (entrance.radius > 0.0) {
    const double radius_sq = entrance.radius * entrance.radius;
    const int min_cell_x = std::max(
        0,
        static_cast<int>(std::floor((entrance.x - entrance.radius) - model.origin_x)));
    const int max_cell_x = std::min(
        model.grid_width - 1,
        static_cast<int>(std::ceil((entrance.x + entrance.radius) - model.origin_x)));
    const int min_cell_z = std::max(
        0,
        static_cast<int>(std::floor((entrance.z - entrance.radius) - model.origin_z)));
    const int max_cell_z = std::min(
        model.grid_height - 1,
        static_cast<int>(std::ceil((entrance.z + entrance.radius) - model.origin_z)));

    bool has_visible_cell = false;
    for (int cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z) {
      for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x) {
        const double dx = world_x_from_cell(model, cell_x) - entrance.x;
        const double dz = world_z_from_cell(model, cell_z) - entrance.z;
        if ((dx * dx + dz * dz) > radius_sq) {
          continue;
        }
        const QPoint sampled(cell_x, cell_z);
        if (!in_bounds(model, sampled)) {
          continue;
        }
        out_cells->insert(encode_cell(sampled));
        has_visible_cell = true;
      }
    }
    if (has_visible_cell) {
      return true;
    }
  }

  if (in_bounds(model, center_cell)) {
    out_cells->insert(encode_cell(center_cell));
    return true;
  }
  return false;
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

auto component_centre(const Model& model, const QVector<QPoint>& component) -> QPointF {
  double sum_x = 0.0;
  double sum_z = 0.0;
  for (const QPoint& cell : component) {
    sum_x += world_x_from_cell(model, cell.x());
    sum_z += world_z_from_cell(model, cell.y());
  }
  const auto count = static_cast<double>(std::max<qsizetype>(component.size(), 1));
  return {sum_x / count, sum_z / count};
}

auto centre_reaches_rim(const Model& model,
                        const QPointF& centre,
                        const QVector<QPoint>& rim) -> bool {
  const double tolerance_sq =
      k_entrance_rim_tolerance_cells * k_entrance_rim_tolerance_cells;
  for (const QPoint& cell : rim) {
    const double dx = world_x_from_cell(model, cell.x()) - centre.x();
    const double dz = world_z_from_cell(model, cell.y()) - centre.y();
    if ((dx * dx) + (dz * dz) <= tolerance_sq) {
      return true;
    }
  }
  return false;
}

auto component_order(const QVector<QPoint>& lhs, const QVector<QPoint>& rhs) -> bool {
  if (lhs.size() != rhs.size()) {
    return lhs.size() > rhs.size();
  }
  if (lhs.isEmpty() || rhs.isEmpty()) {
    return !lhs.isEmpty();
  }
  if (lhs.first().y() != rhs.first().y()) {
    return lhs.first().y() < rhs.first().y();
  }
  return lhs.first().x() < rhs.first().x();
}

} // namespace

auto build_model(const QJsonObject& hill_json, const MapContext& context) -> Model {
  Model model;
  model.context = context;
  const auto tile = static_cast<float>(model_tile_size(model));

  model.center_x = hill_json.value(MapJsonKeys::x).toDouble(0.0);
  model.center_z = hill_json.value(MapJsonKeys::z).toDouble(0.0);
  model.rotation_deg = hill_json.value(MapJsonKeys::rotation).toDouble(0.0);

  const double radius =
      std::max(1.0, hill_json.value(MapJsonKeys::radius).toDouble(10.0));
  model.base_radius = radius / static_cast<double>(tile);
  const QString terrain_type =
      hill_json.value(MapJsonKeys::type).toString().trimmed().toLower();
  model.is_mountain = terrain_type == QStringLiteral("mountain");

  const auto width =
      static_cast<float>(hill_json.value(MapJsonKeys::width).toDouble(0.0));
  const auto depth =
      static_cast<float>(hill_json.value(MapJsonKeys::depth).toDouble(0.0));
  const bool campaign_scale = Game::Map::is_campaign_landform_scale(
      context.map_grid_width, context.map_grid_height);

  const Game::Map::FootprintCells footprint =
      model.is_mountain ? Game::Map::mountain_footprint_cells(
                              {.width = width,
                               .depth = depth,
                               .radius = static_cast<float>(radius),
                               .rotation_deg = static_cast<float>(model.rotation_deg),
                               .tile_size = tile})
                        : Game::Map::hill_footprint_cells(
                              {.width = width,
                               .depth = depth,
                               .radius = static_cast<float>(radius),
                               .rotation_deg = static_cast<float>(model.rotation_deg),
                               .tile_size = tile,
                               .grid_center_x = static_cast<float>(model.center_x),
                               .grid_center_z = static_cast<float>(model.center_z),
                               .campaign_scale = campaign_scale});

  model.hill_half_width = footprint.half_width;
  model.hill_half_depth = footprint.half_depth;
  model.runtime_rotation_deg = footprint.rotation_deg;
  model.organic_spread = footprint.organic_spread;

  const QVector<EntranceSpec> entrance_specs = read_entrance_specs(
      hill_json.value(MapJsonKeys::entrances).toArray(), &model.invalid_entrances);

  const int half_span = projection_half_span(model, footprint, entrance_specs);
  model.grid_width = (half_span * 2) + 1;
  model.grid_height = model.grid_width;
  model.origin_x = std::round(model.center_x) - static_cast<double>(half_span);
  model.origin_z = std::round(model.center_z) - static_cast<double>(half_span);

  append_rotated_ellipse_cells(
      &model, model.hill_half_width, model.hill_half_depth, model.runtime_rotation_deg);
  if (model.hill_cells.isEmpty()) {
    const QPoint center_cell = cell_from_world(model, model.center_x, model.center_z);
    if (in_bounds(model, center_cell)) {
      model.hill_cells.append(center_cell);
    }
  }

  QSet<quint64> unique_entrances;
  for (const EntranceSpec& entrance : entrance_specs) {
    if (append_entrance_cells(model, entrance, &unique_entrances)) {
      continue;
    }
    model.invalid_entrances.append(
        QJsonObject{{MapJsonKeys::x, entrance.x}, {MapJsonKeys::z, entrance.z}});
  }

  for (quint64 key : unique_entrances) {
    model.entrance_cells.append(QPoint(static_cast<int>(key & 0xFFFFFFFFULL),
                                       static_cast<int>((key >> 32U) & 0xFFFFFFFFULL)));
  }

  sort_cells(&model.hill_cells);
  sort_cells(&model.entrance_cells);

  return model;
}

auto entrance_cells_from_json(const Model& model,
                              const QJsonArray& entrances) -> QVector<QPoint> {
  QJsonArray ignored_invalid;
  const QVector<EntranceSpec> specs = read_entrance_specs(entrances, &ignored_invalid);

  QSet<quint64> cells;
  for (const EntranceSpec& spec : specs) {
    append_entrance_cells(model, spec, &cells);
  }

  QVector<QPoint> expanded;
  expanded.reserve(cells.size());
  for (quint64 key : cells) {
    expanded.append(QPoint(static_cast<int>(key & 0xFFFFFFFFULL),
                           static_cast<int>((key >> 32U) & 0xFFFFFFFFULL)));
  }
  sort_cells(&expanded);
  return expanded;
}

auto rim_cells(const QVector<QPoint>& body_cells) -> QVector<QPoint> {
  constexpr int k_neighbors[4][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
  const QSet<quint64> body = cell_set(body_cells);

  QVector<QPoint> rim;
  rim.reserve(body_cells.size());
  for (const QPoint& cell : body_cells) {
    for (const auto& delta : k_neighbors) {
      const QPoint neighbor(cell.x() + delta[0], cell.y() + delta[1]);
      if (!body.contains(encode_cell(neighbor))) {
        rim.append(cell);
        break;
      }
    }
  }
  sort_cells(&rim);
  return rim;
}

auto default_entrance_cell(const Model& model,
                           const QVector<QPoint>& body_cells) -> QPoint {
  const QVector<QPoint> rim = rim_cells(unique_in_bounds_cells(model, body_cells));
  if (rim.isEmpty()) {
    return {-1, -1};
  }

  const RotationAxes axes = rotation_axes(model.runtime_rotation_deg);
  const QPointF offset = unproject_world(-model.hill_half_width * 0.98, 0.0, axes);
  const double target_x = model.center_x + offset.x();
  const double target_z = model.center_z + offset.y();

  QPoint best = rim.first();
  double best_distance_sq = std::numeric_limits<double>::max();
  for (const QPoint& cell : rim) {
    const double dx = world_x_from_cell(model, cell.x()) - target_x;
    const double dz = world_z_from_cell(model, cell.y()) - target_z;
    const double distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best = cell;
    }
  }
  return best;
}

auto normalize_entrance_cells(const Model& model,
                              const QVector<QPoint>& body_cells,
                              const QVector<QPoint>& entrance_cells)
    -> QVector<QPoint> {
  const QVector<QPoint> body = unique_in_bounds_cells(model, body_cells);
  const QVector<QPoint> rim = rim_cells(body);

  QVector<QVector<QPoint>> components =
      connected_components(unique_in_bounds_cells(model, entrance_cells));
  components.removeIf([&](const QVector<QPoint>& component) {
    return !centre_reaches_rim(model, component_centre(model, component), rim);
  });

  if (components.size() > k_max_entrances) {
    std::stable_sort(components.begin(), components.end(), component_order);
    components.resize(k_max_entrances);
  }

  QVector<QPoint> normalized;
  for (const QVector<QPoint>& component : components) {
    normalized.append(component);
  }

  if (normalized.isEmpty() && !body.isEmpty()) {
    const QPoint generated = default_entrance_cell(model, body);
    if (in_bounds(model, generated)) {
      normalized.append(generated);
    }
  }

  sort_cells(&normalized);
  return normalized;
}

auto entrance_issues(const Model& model,
                     const QVector<QPoint>& body_cells,
                     const QVector<QPoint>& entrance_cells) -> QStringList {
  QStringList issues;
  if (model.is_mountain) {
    return issues;
  }

  const QVector<QPoint> body = unique_in_bounds_cells(model, body_cells);
  const QVector<QPoint> rim = rim_cells(body);

  int off_rim_ramps = 0;
  int cluster_count = 0;
  for (const QVector<QPoint>& component :
       connected_components(unique_in_bounds_cells(model, entrance_cells))) {
    if (centre_reaches_rim(model, component_centre(model, component), rim)) {
      ++cluster_count;
    } else {
      ++off_rim_ramps;
    }
  }

  if (!model.invalid_entrances.isEmpty()) {
    issues << QStringLiteral("%1 authored entrance(s) sit outside the projection and "
                             "will be dropped.")
                  .arg(model.invalid_entrances.size());
  }
  if (off_rim_ramps > 0) {
    issues << QStringLiteral("%1 entrance(s) centre away from the hill edge and will "
                             "be dropped; a ramp has to start on the slope.")
                  .arg(off_rim_ramps);
  }

  if (cluster_count > k_max_entrances) {
    issues << QStringLiteral("%1 entrances authored; only the %2 largest are kept.")
                  .arg(cluster_count)
                  .arg(k_max_entrances);
  }
  if (cluster_count < k_min_entrances && !body.isEmpty()) {
    issues << QStringLiteral(
        "No entrance on the hill edge; one is generated so units can climb.");
  }

  return issues;
}

auto entrances_from_cells(const Model& model,
                          const QVector<QPoint>& entrance_cells) -> QJsonArray {
  QJsonArray entrances;
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
      double component_radius = 0.0;
      for (const QPoint& cell : component) {
        const double dx = world_x_from_cell(model, cell.x()) - center_x;
        const double dz = world_z_from_cell(model, cell.y()) - center_z;
        component_radius = std::max(component_radius, std::sqrt((dx * dx) + (dz * dz)));
      }
      entrance[MapJsonKeys::radius] = std::max(0.5, component_radius);
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
  const double rotation_deg = model.runtime_rotation_deg;
  const double tile = model_tile_size(model);

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
      if (std::abs(rotation_deg - model.rotation_deg) > 1e-6) {
        updated[MapJsonKeys::rotation] = rotation_deg;
      }

      if (is_mountain) {
        const double radius = fit_mountain_radius(
            model,
            normalized_hill,
            center.x(),
            center.y(),
            rotation_deg,
            base_hill_json.value(MapJsonKeys::radius).toDouble(model.base_radius) /
                tile);
        updated[MapJsonKeys::radius] = radius * tile;
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
              base_hill_json.value(MapJsonKeys::radius).toDouble(model.base_radius) /
                  tile,
              false);
          updated[MapJsonKeys::radius] = radius * tile;
          updated.remove(MapJsonKeys::width);
          updated.remove(MapJsonKeys::depth);
        } else {
          updated[MapJsonKeys::width] = half_width * 2.0 * tile;
          updated[MapJsonKeys::depth] = half_depth * 2.0 * tile;
          updated.remove(MapJsonKeys::radius);
        }
      }
    }
  }

  if (is_mountain) {
    updated.remove(MapJsonKeys::entrances);
    return updated;
  }

  const QJsonArray entrances = entrances_from_cells(
      model, normalize_entrance_cells(model, normalized_hill, entrance_cells));
  if (entrances.isEmpty()) {
    updated.remove(MapJsonKeys::entrances);
  } else {
    updated[MapJsonKeys::entrances] = entrances;
  }

  return updated;
}

} // namespace MapEditor::HillProjection
