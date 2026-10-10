#include "campaign_film_spec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <tuple>

namespace CampaignFilm {

namespace {

auto fail(QString* error, const QString& message) -> bool {
  if (error != nullptr) {
    *error = message;
  }
  return false;
}

auto read_float(const QJsonObject& object, const char* key, float fallback) -> float {
  const QJsonValue value = object.value(QLatin1String(key));
  return value.isDouble() ? static_cast<float>(value.toDouble()) : fallback;
}

auto read_color(const QJsonValue& value, const QVector4D& fallback) -> QVector4D {
  const QJsonArray array = value.toArray();
  if (array.size() < 3) {
    return fallback;
  }
  return {static_cast<float>(array.at(0).toDouble()),
          static_cast<float>(array.at(1).toDouble()),
          static_cast<float>(array.at(2).toDouble()),
          array.size() >= 4 ? static_cast<float>(array.at(3).toDouble()) : 1.0F};
}

auto read_pair(const QJsonValue& value, QVector2D* out) -> bool {
  const QJsonArray array = value.toArray();
  if (array.size() < 2 || !array.at(0).isDouble() || !array.at(1).isDouble()) {
    return false;
  }
  *out = QVector2D(static_cast<float>(array.at(0).toDouble()),
                   static_cast<float>(array.at(1).toDouble()));
  return true;
}

auto shorter_arc(float from, float to) -> float {
  float delta = std::fmod(to - from, 360.0F);
  if (delta > 180.0F) {
    delta -= 360.0F;
  } else if (delta < -180.0F) {
    delta += 360.0F;
  }
  return delta;
}

auto parse_window(const QJsonObject& object, Window fallback) -> Window {
  Window window = fallback;
  window.in = read_float(object, "in", window.in);
  window.out = read_float(object, "out", window.out);
  window.fade = std::max(0.0F, read_float(object, "fade", window.fade));
  return window;
}

auto parse_target(const QJsonValue& value,
                  const LoadContext& context,
                  Target* out,
                  QString* error) -> bool {
  Target target;
  if (value.isString()) {
    const QString text = value.toString();
    if (text == QStringLiteral("head")) {
      target.kind = Target::Kind::Head;
    } else {
      target.kind = Target::Kind::Stop;
      target.stop = text;
    }
  } else if (value.isObject()) {
    const QJsonObject object = value.toObject();
    QVector2D pair;
    if (object.contains(QStringLiteral("stop"))) {
      target.kind = Target::Kind::Stop;
      target.stop = object.value(QStringLiteral("stop")).toString();
    } else if (object.value(QStringLiteral("head")).toBool(false)) {
      target.kind = Target::Kind::Head;
      target.smooth_seconds = read_float(object, "smooth", 0.0F);
    } else if (read_pair(object.value(QStringLiteral("lonlat")), &pair)) {
      target.kind = Target::Kind::Uv;
      target.uv = context.bounds.to_uv(pair.x(), pair.y());
    } else if (read_pair(object.value(QStringLiteral("uv")), &pair)) {
      target.kind = Target::Kind::Uv;
      target.uv = pair;
    } else {
      return fail(error,
                  QStringLiteral("a target needs one of stop, head, lonlat or uv"));
    }
    QVector2D offset;
    if (read_pair(object.value(QStringLiteral("offset")), &offset)) {
      target.offset = offset;
    }
  } else {
    return fail(error, QStringLiteral("a target must be a string or an object"));
  }
  if (target.kind == Target::Kind::Stop) {
    if (context.march == nullptr || context.march->find(target.stop) == nullptr) {
      return fail(error, QStringLiteral("unknown march stop '%1'").arg(target.stop));
    }
  }
  *out = target;
  return true;
}

auto march_progress_window(const Spec& spec,
                           const March& march) -> std::pair<float, float> {
  float from = 0.0F;
  float to = 1.0F;
  if (!spec.route.from.isEmpty()) {
    from = march.progress_of(spec.route.from).value_or(0.0F);
  }
  if (!spec.route.to.isEmpty()) {
    to = march.progress_of(spec.route.to).value_or(1.0F);
  }
  return {from, to};
}

} // namespace

auto parse_ease(const QString& name, Ease fallback) -> Ease {
  const QString normalized = name.trimmed().toLower();
  if (normalized.isEmpty()) {
    return fallback;
  }
  if (normalized == QStringLiteral("linear")) {
    return Ease::Linear;
  }
  if (normalized == QStringLiteral("in") || normalized == QStringLiteral("ease_in")) {
    return Ease::EaseIn;
  }
  if (normalized == QStringLiteral("out") || normalized == QStringLiteral("ease_out")) {
    return Ease::EaseOut;
  }
  return Ease::Smooth;
}

auto ease_value(Ease ease, float t) -> float {
  const float clamped = std::clamp(t, 0.0F, 1.0F);
  switch (ease) {
  case Ease::Linear:
    return clamped;
  case Ease::EaseIn:
    return clamped * clamped * clamped;
  case Ease::EaseOut: {
    const float inverted = 1.0F - clamped;
    return 1.0F - (inverted * inverted * inverted);
  }
  case Ease::Smooth:
    break;
  }
  return clamped * clamped * (3.0F - (2.0F * clamped));
}

auto Window::alpha(float time) const -> float {
  if (time < in || time > out) {
    return 0.0F;
  }
  if (fade <= 0.0F) {
    return 1.0F;
  }
  const float rise = ease_value(Ease::Smooth, (time - in) / fade);
  const float fall =
      std::isinf(out) ? 1.0F : ease_value(Ease::Smooth, (out - time) / fade);
  return std::min(rise, fall);
}

auto March::find(const QString& id) const -> const MarchStop* {
  for (const auto& stop : stops) {
    if (stop.id == id) {
      return &stop;
    }
  }
  return nullptr;
}

auto March::progress_of(const QString& id) const -> std::optional<float> {
  const MarchStop* stop = find(id);
  if (stop == nullptr) {
    return std::nullopt;
  }
  return path.progress_at_raw_index(stop->index);
}

auto parse_march(const QJsonObject& hannibal_path,
                 QString* error) -> std::optional<March> {
  const QJsonObject object = hannibal_path.value(QStringLiteral("march")).toObject();
  if (object.isEmpty()) {
    fail(error,
         QStringLiteral("hannibal_path.json has no \"march\"; regenerate it "
                        "with tools/map_pipeline/hannibal_path.py"));
    return std::nullopt;
  }
  March march;
  for (const auto& value : object.value(QStringLiteral("points")).toArray()) {
    QVector2D pt;
    if (!read_pair(value, &pt)) {
      fail(error, QStringLiteral("march point is not a [u, v] pair"));
      return std::nullopt;
    }
    march.points.push_back(pt);
  }
  for (const auto& value : object.value(QStringLiteral("stops")).toArray()) {
    const QJsonObject stop_object = value.toObject();
    MarchStop stop;
    stop.id = stop_object.value(QStringLiteral("id")).toString();
    stop.name = stop_object.value(QStringLiteral("name")).toString();
    stop.date = stop_object.value(QStringLiteral("date")).toString();
    stop.kind = stop_object.value(QStringLiteral("kind")).toString();
    stop.index =
        static_cast<std::size_t>(stop_object.value(QStringLiteral("index")).toInt(-1));
    if (stop.id.isEmpty() ||
        !read_pair(stop_object.value(QStringLiteral("uv")), &stop.uv) ||
        stop.index >= march.points.size()) {
      fail(error, QStringLiteral("march stop '%1' is malformed").arg(stop.id));
      return std::nullopt;
    }
    march.stops.push_back(stop);
  }
  if (march.points.size() < 2 || march.stops.empty()) {
    fail(error, QStringLiteral("march needs at least two points and one stop"));
    return std::nullopt;
  }
  march.path = CampaignMapFilm::RoutePath(march.points);
  return march;
}

auto Catalog::find(const QString& id) const -> const CatalogRegion* {
  for (const auto& region : regions) {
    if (region.id == id) {
      return &region;
    }
  }
  return nullptr;
}

auto parse_catalog(const QJsonObject& object,
                   const MapBounds& bounds,
                   QString* error) -> std::optional<Catalog> {
  Catalog catalog;
  catalog.bounds = bounds;
  for (const auto& value : object.value(QStringLiteral("regions")).toArray()) {
    const QJsonObject region_object = value.toObject();
    CatalogRegion region;
    region.id = region_object.value(QStringLiteral("id")).toString();
    region.name = region_object.value(QStringLiteral("name")).toString();
    for (const auto& province :
         region_object.value(QStringLiteral("provinces")).toArray()) {
      region.provinces.push_back(province.toString());
    }
    for (const auto& point : region_object.value(QStringLiteral("lonlat")).toArray()) {
      QVector2D pair;
      if (!read_pair(point, &pair)) {
        fail(error,
             QStringLiteral("region '%1' has a malformed lonlat point").arg(region.id));
        return std::nullopt;
      }
      region.polygon.push_back(bounds.to_uv(pair.x(), pair.y()));
    }
    if (region.id.isEmpty() ||
        (region.provinces.isEmpty() && region.polygon.size() < 3)) {
      fail(error,
           QStringLiteral("region '%1' needs provinces or a polygon").arg(region.id));
      return std::nullopt;
    }
    catalog.regions.push_back(region);
  }
  return catalog;
}

auto triangulate(const std::vector<QVector2D>& polygon) -> std::vector<QVector2D> {
  std::vector<QVector2D> out;
  if (polygon.size() < 3) {
    return out;
  }
  std::vector<QVector2D> ring = polygon;
  if ((ring.front() - ring.back()).lengthSquared() < 1e-14F) {
    ring.pop_back();
  }
  double area = 0.0;
  for (std::size_t i = 0; i < ring.size(); ++i) {
    const QVector2D& a = ring[i];
    const QVector2D& b = ring[(i + 1) % ring.size()];
    area += static_cast<double>(a.x()) * b.y() - static_cast<double>(b.x()) * a.y();
  }
  if (area < 0.0) {
    std::reverse(ring.begin(), ring.end());
  }
  auto cross = [](const QVector2D& o, const QVector2D& a, const QVector2D& b) {
    return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
  };
  auto inside = [&](const QVector2D& p,
                    const QVector2D& a,
                    const QVector2D& b,
                    const QVector2D& c) {
    return cross(a, b, p) >= 0.0F && cross(b, c, p) >= 0.0F && cross(c, a, p) >= 0.0F;
  };
  std::vector<std::size_t> indices(ring.size());
  for (std::size_t i = 0; i < ring.size(); ++i) {
    indices[i] = i;
  }
  std::size_t guard = 0;
  while (indices.size() > 3 && guard < ring.size() * ring.size() * 2) {
    ++guard;
    bool clipped = false;
    for (std::size_t i = 0; i < indices.size(); ++i) {
      const std::size_t prev = indices[(i + indices.size() - 1) % indices.size()];
      const std::size_t curr = indices[i];
      const std::size_t next = indices[(i + 1) % indices.size()];
      const QVector2D& a = ring[prev];
      const QVector2D& b = ring[curr];
      const QVector2D& c = ring[next];
      if (cross(a, b, c) <= 0.0F) {
        continue;
      }
      bool contains = false;
      for (const std::size_t other : indices) {
        if (other == prev || other == curr || other == next) {
          continue;
        }
        if (inside(ring[other], a, b, c)) {
          contains = true;
          break;
        }
      }
      if (contains) {
        continue;
      }
      out.push_back(a);
      out.push_back(b);
      out.push_back(c);
      indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
      clipped = true;
      break;
    }
    if (!clipped) {
      break;
    }
  }
  if (indices.size() == 3) {
    out.push_back(ring[indices[0]]);
    out.push_back(ring[indices[1]]);
    out.push_back(ring[indices[2]]);
  }
  return out;
}

auto Spec::frame_count() const -> int {
  return std::max(1, static_cast<int>(std::lround(duration * static_cast<float>(fps))));
}

auto load_json_object(const QString& path,
                      QString* error) -> std::optional<QJsonObject> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    fail(error, QStringLiteral("cannot open %1").arg(path));
    return std::nullopt;
  }
  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    fail(error,
         QStringLiteral("%1: %2 at offset %3")
             .arg(path, parse_error.errorString())
             .arg(parse_error.offset));
    return std::nullopt;
  }
  return document.object();
}

auto parse_spec(const QJsonObject& object,
                const LoadContext& context,
                QString* error) -> std::optional<Spec> {
  Spec spec;
  spec.id = object.value(QStringLiteral("id")).toString();
  if (spec.id.isEmpty()) {
    fail(error, QStringLiteral("spec needs an id"));
    return std::nullopt;
  }
  spec.title = object.value(QStringLiteral("title")).toString(spec.id);
  spec.width = object.value(QStringLiteral("width")).toInt(spec.width);
  spec.height = object.value(QStringLiteral("height")).toInt(spec.height);
  spec.fps = object.value(QStringLiteral("fps")).toInt(spec.fps);
  spec.supersample =
      object.value(QStringLiteral("supersample")).toInt(spec.supersample);
  spec.duration = read_float(object, "duration", spec.duration);
  spec.reference_height = read_float(object, "reference_height", spec.reference_height);
  spec.terrain_height_scale =
      read_float(object, "terrain_height_scale", spec.terrain_height_scale);
  spec.province_fills = object.value(QStringLiteral("province_fills")).toBool(false);
  spec.province_fill_alpha =
      read_float(object, "province_fill_alpha", spec.province_fill_alpha);
  spec.show_symbols = object.value(QStringLiteral("symbols")).toBool(false);
  spec.draped_lines = object.value(QStringLiteral("draped_lines")).toBool(true);
  spec.coast_width =
      std::max(0.0F, read_float(object, "coast_width", spec.coast_width));
  spec.river_width =
      std::max(0.0F, read_float(object, "river_width", spec.river_width));
  spec.show_game_route = object.value(QStringLiteral("game_route")).toBool(false);
  spec.show_borders = object.value(QStringLiteral("borders")).toBool(false);
  spec.forbid_world_edge =
      object.value(QStringLiteral("forbid_world_edge")).toBool(false);
  spec.drape_radius =
      std::max(0.0F, read_float(object, "drape_radius", spec.drape_radius));
  spec.burn_text = object.value(QStringLiteral("burn_text")).toBool(true);
  spec.interp = object.value(QStringLiteral("interp")).toString().toLower() ==
                        QStringLiteral("keys")
                    ? Interp::Keys
                    : Interp::Spline;
  spec.ends = object.value(QStringLiteral("ends")).toString().toLower() ==
                      QStringLiteral("moving")
                  ? Ends::Moving
                  : Ends::Ease;

  if (spec.width < 16 || spec.height < 16 || spec.fps < 1 || spec.supersample < 1 ||
      spec.supersample > 4 || !(spec.duration > 0.0F)) {
    fail(error, QStringLiteral("width/height/fps/supersample/duration out of range"));
    return std::nullopt;
  }

  const QJsonArray camera = object.value(QStringLiteral("camera")).toArray();
  if (camera.isEmpty()) {
    fail(error, QStringLiteral("spec needs at least one camera key"));
    return std::nullopt;
  }
  CameraKey previous;
  previous.look.kind = Target::Kind::Uv;
  for (int i = 0; i < camera.size(); ++i) {
    const QJsonObject key_object = camera.at(i).toObject();
    CameraKey key = previous;
    key.time = read_float(key_object, "time", previous.time);
    if (key_object.contains(QStringLiteral("look"))) {
      QString target_error;
      if (!parse_target(key_object.value(QStringLiteral("look")),
                        context,
                        &key.look,
                        &target_error)) {
        fail(error, QStringLiteral("camera key %1: %2").arg(i).arg(target_error));
        return std::nullopt;
      }
    } else if (i == 0) {
      fail(error, QStringLiteral("the first camera key needs a look target"));
      return std::nullopt;
    }
    key.distance = read_float(key_object, "distance", previous.distance);
    key.yaw = read_float(key_object, "yaw", previous.yaw);
    key.pitch = read_float(key_object, "pitch", previous.pitch);
    key.fov = read_float(key_object, "fov", previous.fov);
    key.ease =
        parse_ease(key_object.value(QStringLiteral("ease")).toString(), Ease::Smooth);
    if (i > 0 && key.time < previous.time) {
      fail(error, QStringLiteral("camera keys must be in time order (key %1)").arg(i));
      return std::nullopt;
    }
    if (!(key.distance > 0.0F) || key.pitch < 1.0F || key.pitch > 90.0F) {
      fail(
          error,
          QStringLiteral("camera key %1: distance must be > 0 and pitch 1..90").arg(i));
      return std::nullopt;
    }
    spec.camera.push_back(key);
    previous = key;
  }

  const QJsonObject route = object.value(QStringLiteral("route")).toObject();
  if (!route.isEmpty()) {
    if (context.march == nullptr) {
      fail(error, QStringLiteral("route needs the march data"));
      return std::nullopt;
    }
    spec.route.enabled = true;
    spec.route.from = route.value(QStringLiteral("from")).toString();
    spec.route.to = route.value(QStringLiteral("to")).toString();
    for (const QString& stop : {spec.route.from, spec.route.to}) {
      if (!stop.isEmpty() && context.march->find(stop) == nullptr) {
        fail(error, QStringLiteral("route: unknown march stop '%1'").arg(stop));
        return std::nullopt;
      }
    }
    const auto from = spec.route.from.isEmpty()
                          ? std::optional<float>(0.0F)
                          : context.march->progress_of(spec.route.from);
    const auto to = spec.route.to.isEmpty() ? std::optional<float>(1.0F)
                                            : context.march->progress_of(spec.route.to);
    if (!(from.value_or(0.0F) < to.value_or(1.0F))) {
      fail(error, QStringLiteral("route: 'from' must come before 'to' on the march"));
      return std::nullopt;
    }
    auto& style = spec.route.style;
    style.width_px = read_float(route, "width", style.width_px);
    style.head = route.value(QStringLiteral("head")).toBool(true);
    style.head_radius_px = read_float(route, "head_radius", style.head_radius_px);
    style.ghost_alpha = read_float(route, "ghost", style.ghost_alpha);
    style.shadow_alpha = read_float(route, "shadow", style.shadow_alpha);
    style.casing =
        read_color(route.value(QStringLiteral("casing_color")), style.casing);
    style.gold = read_color(route.value(QStringLiteral("gold_color")), style.gold);
    style.core = read_color(route.value(QStringLiteral("core_color")), style.core);
    float last_time = -std::numeric_limits<float>::infinity();
    const QJsonArray keys = route.value(QStringLiteral("keys")).toArray();
    for (int i = 0; i < keys.size(); ++i) {
      const QJsonObject key_object = keys.at(i).toObject();
      RouteKey key;
      key.time = read_float(key_object, "time", 0.0F);
      key.ease =
          parse_ease(key_object.value(QStringLiteral("ease")).toString(), Ease::Linear);
      if (key_object.contains(QStringLiteral("at"))) {
        key.at = key_object.value(QStringLiteral("at")).toString();
        const auto stop_progress = context.march->progress_of(key.at);
        if (!stop_progress) {
          fail(error,
               QStringLiteral("route key %1: unknown stop '%2'").arg(i).arg(key.at));
          return std::nullopt;
        }
        if (*stop_progress < *from - 1e-5F || *stop_progress > *to + 1e-5F) {
          fail(error,
               QStringLiteral("route key %1: stop '%2' lies outside from..to")
                   .arg(i)
                   .arg(key.at));
          return std::nullopt;
        }
      } else if (key_object.value(QStringLiteral("progress")).isDouble()) {
        key.progress = std::clamp(read_float(key_object, "progress", 0.0F), 0.0F, 1.0F);
      } else {
        fail(error, QStringLiteral("route key %1 needs 'at' or 'progress'").arg(i));
        return std::nullopt;
      }
      if (key.time < last_time) {
        fail(error, QStringLiteral("route keys must be in time order (key %1)").arg(i));
        return std::nullopt;
      }
      last_time = key.time;
      spec.route.keys.push_back(key);
    }
    if (spec.route.keys.empty()) {
      fail(error, QStringLiteral("route needs at least one key"));
      return std::nullopt;
    }
  }

  for (const auto& value : object.value(QStringLiteral("regions")).toArray()) {
    const QJsonObject region_object = value.toObject();
    RegionTrack region;
    region.id = region_object.value(QStringLiteral("id")).toString();
    const CatalogRegion* catalog_region =
        context.catalog != nullptr ? context.catalog->find(region.id) : nullptr;
    if (catalog_region != nullptr) {
      region.name = catalog_region->name;
      region.provinces = catalog_region->provinces;
      region.triangles = triangulate(catalog_region->polygon);
    }
    for (const auto& province :
         region_object.value(QStringLiteral("provinces")).toArray()) {
      region.provinces.push_back(province.toString());
    }
    std::vector<QVector2D> polygon;
    for (const auto& point : region_object.value(QStringLiteral("lonlat")).toArray()) {
      QVector2D pair;
      if (read_pair(point, &pair)) {
        polygon.push_back(context.bounds.to_uv(pair.x(), pair.y()));
      }
    }
    if (!polygon.empty()) {
      region.triangles = triangulate(polygon);
    }
    if (region.provinces.isEmpty() && region.triangles.empty()) {
      fail(error,
           QStringLiteral("region '%1' is not in the catalog and defines no "
                          "provinces or lonlat polygon")
               .arg(region.id));
      return std::nullopt;
    }
    region.name = region_object.value(QStringLiteral("name")).toString(region.name);
    region.fill = read_color(region_object.value(QStringLiteral("fill")), region.fill);
    region.rim = read_color(region_object.value(QStringLiteral("rim")), region.rim);
    region.rim_px = read_float(region_object, "rim_width", region.rim_px);
    region.dim_outside = read_float(region_object, "dim_outside", region.dim_outside);
    region.window = parse_window(region_object, region.window);
    for (const auto& key_value :
         region_object.value(QStringLiteral("keys")).toArray()) {
      const QJsonObject key_object = key_value.toObject();
      ScalarKey key;
      key.time = read_float(key_object, "time", 0.0F);
      key.value = read_float(key_object, "amount", 1.0F);
      key.ease =
          parse_ease(key_object.value(QStringLiteral("ease")).toString(), Ease::Smooth);
      region.keys.push_back(key);
    }
    spec.regions.push_back(region);
  }

  for (const auto& value : object.value(QStringLiteral("markers")).toArray()) {
    const QJsonObject marker_object = value.toObject();
    MarkerTrack marker;
    marker.site = marker_object.value(QStringLiteral("site")).toString();
    const MarchStop* stop =
        context.march != nullptr ? context.march->find(marker.site) : nullptr;
    QVector2D lonlat;
    if (stop != nullptr) {
      marker.name = stop->name;
      marker.date = stop->date;
      marker.uv = stop->uv;
      marker.kind = stop->kind == QStringLiteral("battle") ? QStringLiteral("battle")
                                                           : QStringLiteral("place");
    } else if (read_pair(marker_object.value(QStringLiteral("lonlat")), &lonlat)) {
      marker.uv = context.bounds.to_uv(lonlat.x(), lonlat.y());
    } else {
      fail(error,
           QStringLiteral("marker '%1' is not a march stop and has no lonlat")
               .arg(marker.site));
      return std::nullopt;
    }
    marker.name = marker_object.value(QStringLiteral("name")).toString(marker.name);
    marker.date = marker_object.value(QStringLiteral("date")).toString(marker.date);
    marker.kind = marker_object.value(QStringLiteral("kind")).toString(marker.kind);
    marker.show_name = marker_object.value(QStringLiteral("label")).toBool(true);
    marker.show_date = marker_object.value(QStringLiteral("show_date")).toBool(true);
    const QJsonValue appear = marker_object.value(QStringLiteral("appear"));
    marker.window = parse_window(marker_object, marker.window);
    if (appear.isString() && appear.toString() == QStringLiteral("arrival")) {
      if (stop == nullptr || !spec.route.enabled) {
        fail(error,
             QStringLiteral("marker '%1' appears on arrival but is not a stop "
                            "on a route")
                 .arg(marker.site));
        return std::nullopt;
      }
      marker.on_arrival = true;
    } else if (appear.isDouble()) {
      marker.window.in = static_cast<float>(appear.toDouble());
    }
    marker.hold = read_float(marker_object, "hold", marker.hold);
    marker.label_hold = read_float(marker_object, "label_hold", marker.label_hold);
    marker.label_side = marker_object.value(QStringLiteral("label_side")).toString();
    if (!marker.label_side.isEmpty() && marker.label_side != QStringLiteral("left") &&
        marker.label_side != QStringLiteral("right")) {
      fail(error,
           QStringLiteral("marker '%1': label_side is left or right").arg(marker.site));
      return std::nullopt;
    }
    spec.markers.push_back(marker);
  }

  for (const auto& value : object.value(QStringLiteral("labels")).toArray()) {
    const QJsonObject label_object = value.toObject();
    LabelTrack label;
    label.text = label_object.value(QStringLiteral("text")).toString();
    QString target_error;
    if (label.text.isEmpty() || !parse_target(label_object.value(QStringLiteral("at")),
                                              context,
                                              &label.at,
                                              &target_error)) {
      fail(error,
           QStringLiteral("label '%1': %2")
               .arg(label.text,
                    target_error.isEmpty() ? QStringLiteral("needs text and at")
                                           : target_error));
      return std::nullopt;
    }
    label.style = label_object.value(QStringLiteral("style")).toString(label.style);
    label.size_px = read_float(label_object, "size", 0.0F);
    QVector2D offset;
    if (read_pair(label_object.value(QStringLiteral("offset")), &offset)) {
      label.offset_px = QPointF(offset.x(), offset.y());
    }
    label.window = parse_window(label_object, label.window);
    spec.labels.push_back(label);
  }

  for (const auto& value : object.value(QStringLiteral("stamps")).toArray()) {
    const QJsonObject stamp_object = value.toObject();
    StampTrack stamp;
    stamp.title = stamp_object.value(QStringLiteral("title")).toString();
    stamp.subtitle = stamp_object.value(QStringLiteral("subtitle")).toString();
    stamp.window = parse_window(stamp_object, stamp.window);
    spec.stamps.push_back(stamp);
  }

  for (const auto& value : object.value(QStringLiteral("armies")).toArray()) {
    const QJsonObject army_object = value.toObject();
    ArmyTrack army;
    army.id = army_object.value(QStringLiteral("id")).toString();
    army.label = army_object.value(QStringLiteral("label")).toString(army.id);
    army.side = army_object.value(QStringLiteral("side")).toString();
    army.source = army_object.value(QStringLiteral("source")).toString();
    army.linear = army_object.value(QStringLiteral("interp")).toString() ==
                  QStringLiteral("linear");
    army.window = parse_window(army_object, army.window);
    QString target_error;
    const QJsonValue anchor = army_object.value(QStringLiteral("anchor"));
    if (army.id.isEmpty() ||
        !parse_target(anchor.isUndefined() ? QJsonValue(QStringLiteral("head"))
                                           : anchor,
                      context,
                      &army.anchor,
                      &target_error)) {
      fail(error,
           QStringLiteral("army '%1': %2")
               .arg(army.id,
                    target_error.isEmpty() ? QStringLiteral("needs an id")
                                           : target_error));
      return std::nullopt;
    }
    for (const auto& key_value : army_object.value(QStringLiteral("keys")).toArray()) {
      const QJsonObject key_object = key_value.toObject();
      ArmyKey key;
      key.time = read_float(key_object, "time", 0.0F);
      key.at = key_object.value(QStringLiteral("at")).toString();
      if (!key.at.isEmpty() &&
          (context.march == nullptr || context.march->find(key.at) == nullptr)) {
        fail(error,
             QStringLiteral("army '%1': unknown stop '%2'").arg(army.id, key.at));
        return std::nullopt;
      }
      for (auto it = key_object.begin(); it != key_object.end(); ++it) {
        if (it.key() == QStringLiteral("time") || it.key() == QStringLiteral("at")) {
          continue;
        }
        if (it.value().isDouble()) {
          key.values.insert(it.key(), it.value().toDouble());
        }
      }
      army.keys.push_back(key);
    }
    if (army.keys.empty()) {
      fail(error, QStringLiteral("army '%1' needs at least one key").arg(army.id));
      return std::nullopt;
    }
    spec.armies.push_back(army);
  }

  return spec;
}

auto hermite_channel(const std::vector<float>& times,
                     const std::vector<float>& values,
                     float time,
                     Ends ends) -> float {
  const std::size_t count = std::min(times.size(), values.size());
  if (count == 0) {
    return 0.0F;
  }
  if (count == 1 || time <= times.front()) {
    return values.front();
  }
  if (time >= times[count - 1]) {
    return values[count - 1];
  }
  auto tangent = [&](std::size_t index) -> float {
    if (index == 0) {
      return ends == Ends::Ease
                 ? 0.0F
                 : (values[1] - values[0]) / std::max(1e-4F, times[1] - times[0]);
    }
    if (index == count - 1) {
      return ends == Ends::Ease ? 0.0F
                                : (values[index] - values[index - 1]) /
                                      std::max(1e-4F, times[index] - times[index - 1]);
    }
    return (values[index + 1] - values[index - 1]) /
           std::max(1e-4F, times[index + 1] - times[index - 1]);
  };
  std::size_t segment = 1;
  while (segment < count - 1 && time > times[segment]) {
    ++segment;
  }
  const float span = std::max(1e-4F, times[segment] - times[segment - 1]);
  const float u = std::clamp((time - times[segment - 1]) / span, 0.0F, 1.0F);
  const float u2 = u * u;
  const float u3 = u2 * u;
  const float h00 = 2.0F * u3 - 3.0F * u2 + 1.0F;
  const float h10 = u3 - 2.0F * u2 + u;
  const float h01 = -2.0F * u3 + 3.0F * u2;
  const float h11 = u3 - u2;
  return values[segment - 1] * h00 + tangent(segment - 1) * h10 * span +
         values[segment] * h01 + tangent(segment) * h11 * span;
}

auto keyed_channel(const std::vector<float>& times,
                   const std::vector<float>& values,
                   const std::vector<Ease>& eases,
                   float time) -> float {
  const std::size_t count = std::min(times.size(), values.size());
  if (count == 0) {
    return 0.0F;
  }
  if (count == 1 || time <= times.front()) {
    return values.front();
  }
  if (time >= times[count - 1]) {
    return values[count - 1];
  }
  for (std::size_t i = 1; i < count; ++i) {
    if (time > times[i]) {
      continue;
    }
    const float span = times[i] - times[i - 1];
    const float raw = span > 0.0F ? (time - times[i - 1]) / span : 1.0F;
    const Ease ease = i < eases.size() ? eases[i] : Ease::Smooth;
    return std::lerp(values[i - 1], values[i], ease_value(ease, raw));
  }
  return values[count - 1];
}

Timeline::Timeline(const Spec& spec, const March& march)
    : m_spec(spec)
    , m_march(march) {
  if (spec.route.enabled) {
    std::tie(m_window_from, m_window_to) = march_progress_window(spec, march);
    for (const auto& key : spec.route.keys) {
      m_key_progress.push_back(key_progress(key));
    }
  }
  for (const auto& marker : spec.markers) {
    if (marker.on_arrival && !arrival_time(marker.site)) {
      m_warnings.push_back(
          QStringLiteral("marker '%1': the route never reaches it").arg(marker.site));
    }
  }
  for (const auto& army : spec.armies) {
    for (const auto& key : army.keys) {
      if (!key.at.isEmpty() && !arrival_time(key.at)) {
        m_warnings.push_back(QStringLiteral("army '%1': the route never reaches '%2'")
                                 .arg(army.id, key.at));
      }
    }
  }
}

auto Timeline::key_progress(const RouteKey& key) const -> float {
  if (!key.at.isEmpty()) {
    return m_march.progress_of(key.at).value_or(m_window_from);
  }
  return m_window_from + (m_window_to - m_window_from) * key.progress;
}

auto Timeline::route_progress(float time) const -> float {
  if (!m_spec.route.enabled || m_key_progress.empty()) {
    return m_window_from;
  }
  std::vector<float> times;
  std::vector<Ease> eases;
  times.reserve(m_spec.route.keys.size());
  for (const auto& key : m_spec.route.keys) {
    times.push_back(key.time);
    eases.push_back(key.ease);
  }
  return keyed_channel(times, m_key_progress, eases, time);
}

auto Timeline::arrival_time(float progress) const -> std::optional<float> {
  if (!m_spec.route.enabled) {
    return std::nullopt;
  }
  constexpr float k_step = 1.0F / 480.0F;
  constexpr float k_tolerance = 2e-7F;
  if (route_progress(0.0F) >= progress - k_tolerance) {
    return 0.0F;
  }
  float previous_time = 0.0F;
  float previous_value = route_progress(0.0F);
  const float end = m_spec.duration;
  for (float t = k_step; t <= end + k_step * 0.5F; t += k_step) {
    const float value = route_progress(t);
    if (value >= progress - k_tolerance) {
      const float span = value - previous_value;
      const float blend =
          span > 1e-9F ? std::clamp((progress - previous_value) / span, 0.0F, 1.0F)
                       : 1.0F;
      return previous_time + (t - previous_time) * blend;
    }
    previous_time = t;
    previous_value = value;
  }
  return std::nullopt;
}

auto Timeline::arrival_time(const QString& stop) const -> std::optional<float> {
  const auto progress = m_march.progress_of(stop);
  if (!progress) {
    return std::nullopt;
  }
  return arrival_time(*progress);
}

auto Timeline::resolve(const Target& target, float time) const -> QVector2D {
  QVector2D base = target.uv;
  switch (target.kind) {
  case Target::Kind::Uv:
    break;
  case Target::Kind::Stop: {
    const MarchStop* stop = m_march.find(target.stop);
    base = stop != nullptr ? stop->uv : base;
    break;
  }
  case Target::Kind::Head: {
    if (target.smooth_seconds > 0.0F) {
      constexpr int k_taps = 9;
      QVector2D sum;
      float weight_sum = 0.0F;
      for (int i = 0; i < k_taps; ++i) {
        const float offset =
            (static_cast<float>(i) / static_cast<float>(k_taps - 1) - 0.5F) * 2.0F *
            target.smooth_seconds;
        const float weight = 1.0F - std::abs(offset) / (target.smooth_seconds * 1.5F);
        const float sample_time = std::clamp(time + offset, 0.0F, m_spec.duration);
        sum += m_march.path.point_at(route_progress(sample_time)) * weight;
        weight_sum += weight;
      }
      base = sum / std::max(1e-6F, weight_sum);
    } else {
      base = m_march.path.point_at(route_progress(time));
    }
    break;
  }
  }
  return base + target.offset;
}

auto Timeline::camera(float time) const -> CampaignMapFilm::CameraPose {
  const auto& keys = m_spec.camera;
  CampaignMapFilm::CameraPose pose;
  if (keys.empty()) {
    return pose;
  }
  std::vector<float> times;
  std::vector<float> us;
  std::vector<float> vs;
  std::vector<float> log_distances;
  std::vector<float> yaws;
  std::vector<float> pitches;
  std::vector<float> fovs;
  std::vector<Ease> eases;
  for (const auto& key : keys) {
    const QVector2D look = resolve(key.look, time);
    times.push_back(key.time);
    us.push_back(look.x());
    vs.push_back(look.y());
    log_distances.push_back(std::log(std::max(1e-4F, key.distance)));
    yaws.push_back(yaws.empty() ? key.yaw
                                : yaws.back() + shorter_arc(yaws.back(), key.yaw));
    pitches.push_back(key.pitch);
    fovs.push_back(key.fov);
    eases.push_back(key.ease);
  }
  auto channel = [&](const std::vector<float>& values) {
    return m_spec.interp == Interp::Spline
               ? hermite_channel(times, values, time, m_spec.ends)
               : keyed_channel(times, values, eases, time);
  };
  pose.target = QVector2D(channel(us), channel(vs));
  pose.distance = std::exp(channel(log_distances));
  pose.yaw = channel(yaws);
  pose.pitch = std::clamp(channel(pitches), 1.0F, 90.0F);
  pose.fov = std::clamp(channel(fovs), 5.0F, 120.0F);
  return pose;
}

auto Timeline::marker_appear_time(const MarkerTrack& marker) const -> float {
  if (marker.on_arrival) {
    return arrival_time(marker.site).value_or(std::numeric_limits<float>::infinity());
  }
  return marker.window.in;
}

auto Timeline::army_key_time(const ArmyKey& key) const -> std::optional<float> {
  if (key.at.isEmpty()) {
    return key.time;
  }
  const auto arrival = arrival_time(key.at);
  if (!arrival) {
    return std::nullopt;
  }
  return *arrival + key.time;
}

auto Timeline::evaluate(float time) const -> FrameEval {
  FrameEval eval;
  eval.time = time;
  eval.camera = camera(time);
  eval.route_from = m_window_from;
  eval.route_head = route_progress(time);
  eval.route_end = m_window_to;
  eval.head_uv = m_march.path.point_at(eval.route_head);

  for (const auto& region : m_spec.regions) {
    float amount = region.window.alpha(time);
    if (!region.keys.empty()) {
      std::vector<float> times;
      std::vector<float> values;
      std::vector<Ease> eases;
      for (const auto& key : region.keys) {
        times.push_back(key.time);
        values.push_back(key.value);
        eases.push_back(key.ease);
      }
      amount *= std::clamp(keyed_channel(times, values, eases, time), 0.0F, 1.0F);
    }
    eval.region_amounts.push_back(amount);
  }

  for (std::size_t i = 0; i < m_spec.markers.size(); ++i) {
    const auto& marker = m_spec.markers[i];
    Window window = marker.window;
    window.in = marker_appear_time(marker);
    if (!std::isinf(marker.hold)) {
      window.out = window.in + marker.hold;
    }
    MarkerValue value;
    value.index = i;
    value.alpha = window.alpha(time);
    Window text_window = window;
    if (!std::isinf(marker.label_hold)) {
      text_window.out = std::min(window.out, window.in + marker.label_hold);
    }
    value.text_alpha = text_window.alpha(time);
    value.pop =
        0.7F +
        0.3F * ease_value(Ease::EaseOut,
                          window.fade > 0.0F ? (time - window.in) / window.fade : 1.0F);
    eval.markers.push_back(value);
  }

  for (std::size_t i = 0; i < m_spec.labels.size(); ++i) {
    const auto& label = m_spec.labels[i];
    eval.labels.push_back({i, resolve(label.at, time), label.window.alpha(time)});
  }

  for (std::size_t i = 0; i < m_spec.stamps.size(); ++i) {
    eval.stamps.push_back({i, m_spec.stamps[i].window.alpha(time)});
  }

  for (const auto& army : m_spec.armies) {
    ArmyValue value;
    value.id = army.id;
    value.uv = resolve(army.anchor, time);
    value.alpha = army.window.alpha(time);
    std::vector<std::pair<float, const ArmyKey*>> timed;
    for (const auto& key : army.keys) {
      const auto at = army_key_time(key);
      if (at) {
        timed.emplace_back(*at, &key);
      }
    }
    std::stable_sort(timed.begin(), timed.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
    });
    if (!timed.empty()) {
      std::size_t current = 0;
      while (current + 1 < timed.size() && timed[current + 1].first <= time) {
        ++current;
      }
      value.values = timed[current].second->values;
      if (army.linear && current + 1 < timed.size() && time >= timed[current].first) {
        const float span = timed[current + 1].first - timed[current].first;
        const float blend = span > 0.0F ? (time - timed[current].first) / span : 1.0F;
        const auto& next = timed[current + 1].second->values;
        for (auto it = value.values.begin(); it != value.values.end(); ++it) {
          if (next.contains(it.key())) {
            it.value() = std::round(std::lerp(
                it.value(), next.value(it.key()), static_cast<double>(blend)));
          }
        }
      }
    }
    eval.armies.push_back(value);
  }
  return eval;
}

auto Timeline::frame_state(const FrameEval& eval) const -> CampaignMapFilm::FrameState {
  CampaignMapFilm::FrameState state;
  state.active = true;
  state.camera = eval.camera;
  state.reference_height = m_spec.reference_height;
  state.terrain_height_scale = m_spec.terrain_height_scale;
  state.province_fills = m_spec.province_fills;
  state.province_fill_alpha = m_spec.province_fill_alpha;
  state.show_game_route = m_spec.show_game_route;
  state.show_symbols = m_spec.show_symbols;
  state.show_borders = m_spec.show_borders;
  state.draped_lines = m_spec.draped_lines;
  state.coast_width_px = m_spec.coast_width;
  state.river_width_px = m_spec.river_width;
  state.drape_radius = m_spec.drape_radius;
  state.route_visible = m_spec.route.enabled;
  state.route_from = eval.route_from;
  state.route_to = eval.route_head;
  state.route_window_end = eval.route_end;
  state.route_style = m_spec.route.style;
  state.time = eval.time;
  for (std::size_t i = 0; i < m_spec.regions.size(); ++i) {
    const auto& region = m_spec.regions[i];
    CampaignMapFilm::RegionHighlight highlight;
    for (const auto& province : region.provinces) {
      highlight.provinces.push_back(province);
    }
    highlight.triangles = region.triangles;
    highlight.fill = region.fill;
    highlight.rim = region.rim;
    highlight.rim_px = region.rim_px;
    highlight.dim_outside = region.dim_outside;
    highlight.amount = i < eval.region_amounts.size() ? eval.region_amounts[i] : 0.0F;
    state.highlights.push_back(highlight);
  }
  return state;
}

} // namespace CampaignFilm
