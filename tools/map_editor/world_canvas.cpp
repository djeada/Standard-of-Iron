#include "world_canvas.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_encounter.h"
#include "world_document.h"

namespace MapEditor {

namespace {

namespace S = Game::Strategic;

constexpr double k_units_per_degree = 100.0;
constexpr double k_backdrop_oversample = 1.5;
constexpr double k_min_scale = 0.12;
constexpr double k_max_scale = 14.0;
constexpr double k_zoom_step = 1.18;

const QColor k_neutral(138, 128, 112);
const QColor k_ink(52, 36, 22);
const QColor k_gold(232, 201, 139);
const QColor k_error(214, 64, 52);
const QColor k_warning(245, 166, 35);

auto resource_color(S::Resource resource) -> QColor {
  switch (resource) {
  case S::Resource::Food:
    return {196, 170, 60};
  case S::Resource::Manpower:
    return {170, 70, 60};
  case S::Resource::Timber:
    return {70, 130, 62};
  case S::Resource::Stone:
    return {150, 150, 150};
  case S::Resource::Iron:
    return {90, 110, 150};
  }
  return Qt::gray;
}

auto supply_color(S::SupplyLevel level) -> QColor {
  switch (level) {
  case S::SupplyLevel::Supplied:
    return {96, 200, 110};
  case S::SupplyLevel::Stretched:
    return {230, 205, 80};
  case S::SupplyLevel::Foraging:
    return {236, 140, 50};
  case S::SupplyLevel::CutOff:
    return {220, 50, 45};
  }
  return Qt::gray;
}

struct RouteStyle {
  QColor color;
  double width = 2.0;
  Qt::PenStyle style = Qt::SolidLine;
  QVector<qreal> dashes;
  QColor inner;
};

auto route_style(S::RouteKind kind) -> RouteStyle {
  switch (kind) {
  case S::RouteKind::MajorRoad:
    return {QColor(78, 50, 26), 3.4, Qt::SolidLine, {}, QColor(214, 178, 112)};
  case S::RouteKind::Road:
    return {QColor(98, 68, 40), 2.4, Qt::SolidLine, {}, QColor()};
  case S::RouteKind::Track:
    return {QColor(110, 82, 52), 1.8, Qt::CustomDashLine, {2.0, 2.0}, QColor()};
  case S::RouteKind::MountainPass:
    return {
        QColor(70, 54, 46), 2.4, Qt::CustomDashLine, {4.0, 1.5, 1.0, 1.5}, QColor()};
  case S::RouteKind::RiverCrossing:
    return {QColor(40, 86, 130), 2.6, Qt::SolidLine, {}, QColor(170, 205, 230)};
  case S::RouteKind::Coastal:
    return {QColor(98, 68, 40), 2.0, Qt::CustomDashLine, {5.0, 2.0}, QColor()};
  case S::RouteKind::SeaLane:
    return {QColor(214, 230, 242, 210), 1.6, Qt::CustomDashLine, {6.0, 4.0}, QColor()};
  }
  return {};
}

auto lerp_color(const QColor& a, const QColor& b, double t) -> QColor {
  t = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(
      static_cast<float>(a.redF() + ((b.redF() - a.redF()) * t)),
      static_cast<float>(a.greenF() + ((b.greenF() - a.greenF()) * t)),
      static_cast<float>(a.blueF() + ((b.blueF() - a.blueF()) * t)),
      static_cast<float>(a.alphaF() + ((b.alphaF() - a.alphaF()) * t)));
}

auto distance_to_segment(QPointF p, QPointF a, QPointF b) -> double {
  const QPointF d = b - a;
  const double len2 = QPointF::dotProduct(d, d);
  const double t =
      len2 > 0.0 ? std::clamp(QPointF::dotProduct(p - a, d) / len2, 0.0, 1.0) : 0.0;
  const QPointF q = a + (d * t) - p;
  return std::hypot(q.x(), q.y());
}

auto thousands(int soldiers) -> QString {
  if (soldiers >= 10000) {
    return QStringLiteral("%1k").arg(std::lround(soldiers / 1000.0));
  }
  if (soldiers >= 1000) {
    return QStringLiteral("%1k").arg(soldiers / 1000.0, 0, 'f', 1);
  }
  return QString::number(soldiers);
}

auto apply_hillshade(QImage& base,
                     const QImage& height,
                     double min_m,
                     double max_m) -> void {
  if (height.isNull()) {
    return;
  }
  const QImage h16 = height.convertToFormat(QImage::Format_Grayscale16);
  const int hw = h16.width();
  const int hh = h16.height();
  QImage shade(hw, hh, QImage::Format_Grayscale8);
  QImage land(hw, hh, QImage::Format_Grayscale8);
  const auto sample = [&](int x, int y) {
    x = std::clamp(x, 0, hw - 1);
    y = std::clamp(y, 0, hh - 1);
    const auto* row = reinterpret_cast<const quint16*>(h16.constScanLine(y));
    return min_m + ((max_m - min_m) * row[x] / 65535.0);
  };
  const double light_x = -0.6;
  const double light_y = -0.55;
  const double light_z = 0.58;
  const double exaggeration = 0.012;
  for (int y = 0; y < hh; ++y) {
    auto* out = shade.scanLine(y);
    auto* mask = land.scanLine(y);
    for (int x = 0; x < hw; ++x) {
      const double z = sample(x, y);
      const double dx = (sample(x + 1, y) - sample(x - 1, y)) * exaggeration;
      const double dy = (sample(x, y + 1) - sample(x, y - 1)) * exaggeration;
      const double len = std::sqrt((dx * dx) + (dy * dy) + 1.0);
      const double lambert = ((-dx * light_x) + (-dy * light_y) + light_z) / len;
      out[x] = static_cast<uchar>(std::clamp(lambert, 0.0, 1.0) * 255.0);
      mask[x] = z > 0.0 ? 255 : 0;
    }
  }
  const QImage shade_big =
      shade.scaled(base.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  const QImage land_big =
      land.scaled(base.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  for (int y = 0; y < base.height(); ++y) {
    auto* px = reinterpret_cast<QRgb*>(base.scanLine(y));
    const uchar* s = shade_big.constScanLine(y);
    const uchar* m = land_big.constScanLine(y);
    for (int x = 0; x < base.width(); ++x) {
      if (m[x] < 8) {
        continue;
      }
      const double factor = 0.70 + (0.42 * (s[x] / 255.0));
      const double weight = m[x] / 255.0;
      const double f = 1.0 + ((factor - 1.0) * weight);
      px[x] = qRgb(std::min(255, static_cast<int>(qRed(px[x]) * f)),
                   std::min(255, static_cast<int>(qGreen(px[x]) * f)),
                   std::min(255, static_cast<int>(qBlue(px[x]) * f)));
    }
  }
}

} // namespace

WorldCanvas::WorldCanvas(WorldDocument* document, QWidget* parent)
    : QWidget(parent)
    , m_document(document) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(480, 320);
  setAttribute(Qt::WA_OpaquePaintEvent);
  connect(m_document, &WorldDocument::loaded, this, [this]() {
    load_backdrop();
    rebuild_shapes();
    m_fitted = false;
    m_user_view = false;
    fit_all();
  });
  connect(m_document, &WorldDocument::changed, this, [this]() { update(); });
  connect(m_document, &WorldDocument::selection_changed, this, [this]() { update(); });
}

void WorldCanvas::load_backdrop() {
  const auto& def = m_document->definition();
  const double mid_lat = (def.projection.lat_min + def.projection.lat_max) * 0.5;
  m_aspect = std::cos(mid_lat * std::numbers::pi / 180.0);
  const QRectF rect = map_rect();
  const QString root = m_document->root();
  if (root.isEmpty()) {
    m_backdrop = QImage();
    return;
  }
  QImage base(S::resolve_asset_path(
      root, QStringLiteral("assets/campaign_map/campaign_base_color.png")));
  if (base.isNull()) {
    m_backdrop = QImage();
    return;
  }
  const QSize size(static_cast<int>(rect.width() * k_backdrop_oversample),
                   static_cast<int>(rect.height() * k_backdrop_oversample));
  m_backdrop_plain = base.convertToFormat(QImage::Format_RGB32)
                         .scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  m_backdrop = m_backdrop_plain;
  const QImage height(S::resolve_asset_path(
      root, QStringLiteral("assets/campaign_map/terrain_height.png")));
  apply_hillshade(m_backdrop, height, -4997.69, 4002.03);
}

void WorldCanvas::rebuild_shapes() {
  m_region_paths.clear();
  m_region_labels.clear();
  const auto* geometry = m_document->geometry();
  if (geometry == nullptr) {
    return;
  }
  const auto& def = m_document->definition();
  for (const auto& [id, shape] : geometry->shapes()) {
    QPainterPath path;
    for (int i = 0; i < shape.border.size(); ++i) {
      const S::LonLat ll =
          def.projection.from_uv(shape.border[i].x(), shape.border[i].y());
      const QPointF p = map_of(ll);
      if (i == 0) {
        path.moveTo(p);
      } else {
        path.lineTo(p);
      }
    }
    path.closeSubpath();
    m_region_paths.insert(id, path);
    m_region_labels.insert(
        id, map_of(def.projection.from_uv(shape.label.x(), shape.label.y())));
  }
}

auto WorldCanvas::map_rect() const -> QRectF {
  const auto& p = m_document->definition().projection;
  return {0.0,
          0.0,
          (p.lon_max - p.lon_min) * m_aspect * k_units_per_degree,
          (p.lat_max - p.lat_min) * k_units_per_degree};
}

auto WorldCanvas::map_of(const S::LonLat& point) const -> QPointF {
  const auto& p = m_document->definition().projection;
  return {(point.lon - p.lon_min) * m_aspect * k_units_per_degree,
          (p.lat_max - point.lat) * k_units_per_degree};
}

auto WorldCanvas::lonlat_of_map(QPointF map) const -> S::LonLat {
  const auto& p = m_document->definition().projection;
  return {p.lon_min + (map.x() / (m_aspect * k_units_per_degree)),
          p.lat_max - (map.y() / k_units_per_degree)};
}

auto WorldCanvas::screen_of(const S::LonLat& point) const -> QPointF {
  return (map_of(point) * m_scale) + m_offset;
}

auto WorldCanvas::lonlat_at(QPointF screen) const -> S::LonLat {
  return lonlat_of_map((screen - m_offset) / m_scale);
}

void WorldCanvas::set_layer(Layer layer, bool enabled) {
  const unsigned next =
      enabled ? (m_layers | layer) : (m_layers & ~static_cast<unsigned>(layer));
  if (next == m_layers) {
    return;
  }
  m_layers = next;
  emit layers_changed(m_layers);
  update();
}

void WorldCanvas::set_tool(Tool tool) {
  m_tool = tool;
  m_route_start.clear();
  setCursor(tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
  update();
}

void WorldCanvas::set_highlight_path(const S::Path& path) {
  m_path = path;
  update();
}

void WorldCanvas::clear_highlight_path() {
  m_path = {};
  update();
}

void WorldCanvas::set_reach(const std::map<QString, double>& reach, double max_days) {
  m_reach = reach;
  m_reach_days = max_days;
  update();
}

void WorldCanvas::clear_reach() {
  m_reach.clear();
  update();
}

void WorldCanvas::fit_all() {
  const QRectF rect = map_rect();
  if (rect.isEmpty() || width() <= 0 || height() <= 0) {
    return;
  }
  m_scale = std::min(width() / rect.width(), height() / rect.height()) * 0.98;
  m_offset = QPointF((width() - (rect.width() * m_scale)) * 0.5,
                     (height() - (rect.height() * m_scale)) * 0.5);
  m_fitted = true;
  update();
}

void WorldCanvas::focus_on(const QString& id) {
  const auto& def = m_document->definition();
  QPointF target;
  double zoom = std::max(m_scale, 1.6);
  if (const auto* site = def.site(id)) {
    target = map_of(site->position);
  } else if (const auto* route = def.route(id)) {
    const auto line = def.route_polyline(*route);
    QRectF box;
    for (const auto& p : line) {
      box = box.isNull() ? QRectF(map_of(p), QSizeF(1, 1))
                         : box.united(QRectF(map_of(p), QSizeF(1, 1)));
    }
    target = box.center();
    zoom = std::clamp(
        std::min(width() / (box.width() + 120.0), height() / (box.height() + 120.0)),
        k_min_scale,
        3.0);
  } else if (m_region_paths.contains(id)) {
    const QRectF box = m_region_paths.value(id).boundingRect();
    target = box.center();
    zoom = std::clamp(
        std::min(width() / (box.width() + 80.0), height() / (box.height() + 80.0)),
        k_min_scale,
        3.0);
  } else if (const auto* army = m_document->state().army(id)) {
    if (const auto* site = def.site(army->location)) {
      target = map_of(site->position);
    }
  } else {
    return;
  }
  m_scale = zoom;
  m_offset = QPointF(width() * 0.5, height() * 0.5) - (target * m_scale);
  m_user_view = true;
  update();
}

auto WorldCanvas::faction_color(const QString& faction) const -> QColor {
  if (const auto* entry = m_document->definition().faction(faction)) {
    const QColor color(entry->color);
    if (color.isValid()) {
      return color;
    }
  }
  return k_neutral;
}

auto WorldCanvas::site_radius(const S::Site& site) const -> double {
  double base = 4.0;
  switch (site.population) {
  case S::PopulationBand::None:
    base = 4.5;
    break;
  case S::PopulationBand::Hamlet:
    base = 4.5;
    break;
  case S::PopulationBand::Small:
    base = 5.5;
    break;
  case S::PopulationBand::Medium:
    base = 6.8;
    break;
  case S::PopulationBand::Large:
    base = 8.2;
    break;
  case S::PopulationBand::Metropolis:
    base = 10.0;
    break;
  }
  return base * std::clamp(std::sqrt(m_scale / 0.6), 0.75, 1.8);
}

auto WorldCanvas::route_screen_path(const S::Route& route) const -> QPainterPath {
  const auto& def = m_document->definition();
  const auto line = def.route_polyline(route);
  QPainterPath path;
  if (line.size() < 2) {
    return path;
  }
  path.moveTo(screen_of(line.first()));
  if (route.kind == S::RouteKind::SeaLane && route.waypoints.isEmpty()) {
    const QPointF a = screen_of(line.first());
    const QPointF b = screen_of(line.last());
    const QPointF mid = (a + b) * 0.5;
    const QPointF d = b - a;
    const QPointF normal(-d.y() * 0.12, d.x() * 0.12);
    path.quadTo(mid + normal, b);
    return path;
  }
  for (int i = 1; i < line.size(); ++i) {
    path.lineTo(screen_of(line[i]));
  }
  return path;
}

auto WorldCanvas::army_anchor(const S::ArmyState& army, int stack) const -> QPointF {
  const auto* site = m_document->definition().site(army.location);
  if (site == nullptr) {
    return {};
  }
  return screen_of(site->position) +
         QPointF(site_radius(*site) + 4.0, -22.0 - (stack * 19.0));
}

auto WorldCanvas::banner_rect(const S::ArmyState& army, int stack) const -> QRectF {
  QFont font = this->font();
  font.setBold(true);
  font.setPointSizeF(8.0);
  const QFontMetricsF metrics(font);
  return {army_anchor(army, stack),
          QSizeF(metrics.horizontalAdvance(banner_text(army)) + 14.0, 17.0)};
}

auto WorldCanvas::banner_text(const S::ArmyState& army) -> QString {
  QString label = thousands(army.soldiers());
  if (army.elephants > 0) {
    label += QStringLiteral(" · %1 eleph.").arg(army.elephants);
  }
  return label;
}

auto WorldCanvas::hit_site(QPointF screen) const -> QString {
  QString best;
  double best_distance = 1e9;
  for (const auto& site : m_document->definition().sites) {
    const double d = QLineF(screen, screen_of(site.position)).length();
    if (d <= site_radius(site) + 5.0 && d < best_distance) {
      best_distance = d;
      best = site.id;
    }
  }
  return best;
}

auto WorldCanvas::army_banners() const -> QVector<std::pair<QString, QRectF>> {
  QVector<std::pair<QString, QRectF>> out;
  QVector<QRectF> taken;
  QHash<QString, int> stacks;
  for (const auto& army : m_document->state().armies) {
    if (m_document->definition().site(army.location) == nullptr) {
      continue;
    }
    int stack = stacks[army.location]++;
    QRectF rect = banner_rect(army, stack);
    for (int guard = 0; guard < 8; ++guard) {
      const bool clear =
          std::none_of(taken.begin(), taken.end(), [&rect](const QRectF& other) {
            return other.adjusted(-2, -2, 2, 2).intersects(rect);
          });
      if (clear) {
        break;
      }
      rect.translate(0, -19.0);
    }
    taken.append(rect);
    out.append({army.id, rect});
  }
  return out;
}

auto WorldCanvas::hit_army(QPointF screen) const -> QString {
  if ((m_layers & Armies) == 0U) {
    return {};
  }
  for (const auto& [id, rect] : army_banners()) {
    if (rect.adjusted(-2, -2, 2, 2).contains(screen)) {
      return id;
    }
  }
  return {};
}

auto WorldCanvas::hit_route(QPointF screen) const -> QString {
  if ((m_layers & Routes) == 0U) {
    return {};
  }
  QString best;
  double best_distance = 6.0;
  for (const auto& route : m_document->definition().routes) {
    const QPainterPath path = route_screen_path(route);
    for (int i = 1; i < path.elementCount(); ++i) {
      const QPointF a(path.elementAt(i - 1).x, path.elementAt(i - 1).y);
      const QPointF b(path.elementAt(i).x, path.elementAt(i).y);
      const double d = distance_to_segment(screen, a, b);
      if (d < best_distance) {
        best_distance = d;
        best = route.id;
      }
    }
  }
  return best;
}

auto WorldCanvas::hit_region(QPointF screen) const -> QString {
  const QPointF map = (screen - m_offset) / m_scale;
  for (auto it = m_region_paths.cbegin(); it != m_region_paths.cend(); ++it) {
    if (it.value().contains(map) &&
        m_document->definition().region(it.key()) != nullptr) {
      return it.key();
    }
  }
  return {};
}

auto WorldCanvas::hit_any(QPointF screen) const -> QString {
  QString id = hit_army(screen);
  if (id.isEmpty()) {
    id = hit_site(screen);
  }
  if (id.isEmpty()) {
    id = hit_route(screen);
  }
  if (id.isEmpty()) {
    id = hit_region(screen);
  }
  return id;
}

auto WorldCanvas::describe(const QString& id) const -> QString {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  if (const auto* site = def.site(id)) {
    QString text = QStringLiteral("<b>%1</b> — %2, %3<br>held by %4")
                       .arg(site->name,
                            S::to_string(site->kind),
                            def.display_name(site->region),
                            def.display_name(S::site_controller(def, state, id)));
    for (const auto* army : state.armies_at(id)) {
      text +=
          QStringLiteral("<br>⚑ %1 (%2)").arg(army->name, thousands(army->soldiers()));
    }
    const auto issues = m_document->report().for_entity(id);
    for (const auto& issue : issues) {
      text += QStringLiteral("<br><span style='color:%1'>%2</span>")
                  .arg(issue.severity == S::Severity::Error ? QStringLiteral("#ff7b6b")
                                                            : QStringLiteral("#f5c060"),
                       issue.message.toHtmlEscaped());
    }
    return text;
  }
  if (const auto* route = def.route(id)) {
    return QStringLiteral("<b>%1</b><br>%2 · %3 km · %4 days · throughput %5%6")
        .arg(def.display_name(id), S::to_string(route->kind))
        .arg(def.route_length_km(*route), 0, 'f', 0)
        .arg(def.route_days(*route), 0, 'f', 1)
        .arg(route->throughput)
        .arg(S::route_status(state, id) == S::RouteStatus::Open
                 ? QString()
                 : QStringLiteral("<br><b>%1</b>")
                       .arg(S::to_string(S::route_status(state, id))));
  }
  if (const auto* army = state.army(id)) {
    const auto* supply = m_document->supply_for(id);
    return QStringLiteral("<b>%1</b><br>%2 foot · %3 horse%4<br>supply: %5")
        .arg(army->name)
        .arg(army->infantry)
        .arg(army->cavalry)
        .arg(army->elephants > 0
                 ? QStringLiteral(" · %1 %2")
                       .arg(army->elephants)
                       .arg(army->elephants == 1 ? QStringLiteral("elephant")
                                                 : QStringLiteral("elephants"))
                 : QString())
        .arg(supply != nullptr ? S::to_string(supply->level) : QStringLiteral("?"));
  }
  if (const auto* region = def.region(id)) {
    return QStringLiteral("<b>%1</b><br>%2 · held by %3%4")
        .arg(region->name,
             region->historical_name,
             def.display_name(S::region_controller(def, state, id)),
             S::region_is_contested(def, state, id) ? QStringLiteral(" · contested")
                                                    : QString());
  }
  return id;
}

void WorldCanvas::paintEvent(QPaintEvent* /*event*/) {
  if (!m_fitted && width() > 0) {
    fit_all();
  }
  QPainter painter(this);
  painter.fillRect(rect(), QColor(23, 36, 52));
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, !m_panning);
  paint_backdrop(painter);
  paint_regions(painter);
  if ((m_layers & Districts) != 0U) {
    paint_districts(painter);
  }
  if ((m_layers & Routes) != 0U) {
    paint_routes(painter);
  }
  if ((m_layers & Supply) != 0U) {
    paint_supply(painter);
  }
  if ((m_layers & Movements) != 0U) {
    paint_movements(painter);
  }
  paint_path(painter);
  if ((m_layers & Sites) != 0U) {
    paint_sites(painter);
  }
  if ((m_layers & Labels) != 0U) {
    paint_labels(painter);
  }
  if ((m_layers & Armies) != 0U) {
    paint_armies(painter);
  }
  if ((m_layers & Issues) != 0U) {
    paint_issues(painter);
  }
  paint_waypoints(painter);
  paint_overlay(painter);
  paint_legend(painter);
}

void WorldCanvas::paint_backdrop(QPainter& painter) const {
  const QImage& image = (m_layers & Relief) != 0U ? m_backdrop : m_backdrop_plain;
  const QRectF target(m_offset, map_rect().size() * m_scale);
  if (image.isNull()) {
    painter.fillRect(target, QColor(232, 218, 190));
    return;
  }
  painter.drawImage(target, image);
}

void WorldCanvas::paint_regions(QPainter& painter) const {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  painter.save();
  painter.translate(m_offset);
  painter.scale(m_scale, m_scale);
  for (auto it = m_region_paths.cbegin(); it != m_region_paths.cend(); ++it) {
    const QString& id = it.key();
    const bool modelled = def.region(id) != nullptr;
    if ((m_layers & Control) != 0U && modelled) {
      const QString owner = S::region_controller(def, state, id);
      const bool neutral = owner == QLatin1String(S::k_neutral_faction);
      QColor fill = faction_color(owner);
      fill.setAlpha(neutral ? 18 : 92);
      painter.fillPath(it.value(), fill);
      if (!neutral) {
        painter.save();
        painter.setClipPath(it.value());
        QColor edge = faction_color(owner);
        edge.setAlpha(200);
        painter.strokePath(
            it.value(),
            QPen(edge, 7.0 / m_scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.restore();
      }
      if (S::region_is_contested(def, state, id)) {
        QBrush hatch(QColor(200, 40, 30, 110), Qt::BDiagPattern);
        hatch.setTransform(
            QTransform::fromScale(1.0 / m_scale * 1.4, 1.0 / m_scale * 1.4));
        painter.fillPath(it.value(), hatch);
      }
    }
    const bool selected = m_document->selection() == id;
    const bool hovered = m_hover == id;
    if ((m_layers & Borders) != 0U || selected || hovered) {
      QPen pen(selected  ? k_gold
               : hovered ? QColor(255, 240, 210, 200)
                         : QColor(70, 46, 26, 150));
      pen.setWidthF((selected ? 2.6 : hovered ? 1.8 : 1.0) / m_scale);
      pen.setStyle(selected || hovered ? Qt::SolidLine : Qt::DashLine);
      painter.strokePath(it.value(), pen);
    }
  }
  painter.restore();
}

void WorldCanvas::paint_districts(QPainter& painter) const {
  painter.save();
  for (const auto& region : m_document->definition().regions) {
    for (const auto& district : region.districts) {
      QPolygonF poly;
      for (const auto& p : district.outline) {
        poly.append(screen_of(p));
      }
      QPen pen(QColor(120, 60, 30, 170));
      pen.setStyle(Qt::DotLine);
      pen.setWidthF(1.4);
      painter.setPen(pen);
      painter.setBrush(QColor(255, 230, 180, 22));
      painter.drawPolygon(poly);
      QFont font = painter.font();
      font.setItalic(true);
      font.setPointSizeF(8.5);
      painter.setFont(font);
      painter.setPen(QColor(90, 45, 20, 200));
      painter.drawText(poly.boundingRect().topLeft() + QPointF(4, 12), district.name);
    }
  }
  painter.restore();
}

void WorldCanvas::paint_routes(QPainter& painter) const {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  const double zoom_width = std::clamp(m_scale / 0.55, 0.6, 2.2);
  const auto bridges = (m_layers & Chokepoints) != 0U
                           ? S::StrategicGraph(def, state)
                                 .bridge_routes(S::TraversalRules::anything(), 2)
                           : QStringList();
  painter.save();
  painter.setBrush(Qt::NoBrush);
  for (const auto& route : def.routes) {
    const QPainterPath path = route_screen_path(route);
    const RouteStyle style = route_style(route.kind);
    const S::RouteStatus status = S::route_status(state, route.id);
    const bool selected = m_document->selection() == route.id;
    const bool hovered = m_hover == route.id || route.touches(m_hover) ||
                         route.touches(m_document->selection());
    if (selected || hovered) {
      QPen halo(selected ? k_gold : QColor(255, 245, 220, 160),
                (style.width + 5.0) * zoom_width);
      halo.setCapStyle(Qt::RoundCap);
      halo.setJoinStyle(Qt::RoundJoin);
      painter.strokePath(path, halo);
    }
    if (status == S::RouteStatus::Contested) {
      painter.strokePath(path,
                         QPen(QColor(236, 140, 50, 150),
                              (style.width + 4.0) * zoom_width,
                              Qt::SolidLine,
                              Qt::RoundCap,
                              Qt::RoundJoin));
    }
    QColor color = style.color;
    if (status == S::RouteStatus::Blocked) {
      color.setAlpha(110);
    }
    QPen pen(color, style.width * zoom_width, style.style, Qt::RoundCap, Qt::RoundJoin);
    if (style.style == Qt::CustomDashLine) {
      pen.setDashPattern(style.dashes);
    }
    painter.strokePath(path, pen);
    if (style.inner.isValid()) {
      painter.strokePath(path,
                         QPen(style.inner,
                              style.width * zoom_width * 0.35,
                              Qt::SolidLine,
                              Qt::RoundCap,
                              Qt::RoundJoin));
    }
    const QPointF mid = path.pointAtPercent(0.5);
    if (status == S::RouteStatus::Blocked) {
      painter.setPen(QPen(k_error, 2.6));
      painter.drawLine(mid + QPointF(-5, -5), mid + QPointF(5, 5));
      painter.drawLine(mid + QPointF(-5, 5), mid + QPointF(5, -5));
    }
    if ((m_layers & Chokepoints) != 0U &&
        (route.chokepoint || bridges.contains(route.id))) {
      QPolygonF diamond{mid + QPointF(0, -4.5),
                        mid + QPointF(4.5, 0),
                        mid + QPointF(0, 4.5),
                        mid + QPointF(-4.5, 0)};
      painter.setPen(QPen(k_ink, 1.0));
      painter.setBrush(route.chokepoint ? k_gold : QColor(236, 140, 50));
      painter.drawPolygon(diamond);
      painter.setBrush(Qt::NoBrush);
    }
  }
  painter.restore();
}

void WorldCanvas::paint_movements(QPainter& painter) const {
  const auto& def = m_document->definition();
  const auto& before = m_document->previous_state();
  const auto& after = m_document->state();
  if (m_document->timeline_index() == 0) {
    return;
  }
  const S::StrategicGraph graph(def, before);
  painter.save();
  for (const auto& army : after.armies) {
    const auto* previous = before.army(army.id);
    const QColor color = faction_color(army.faction);
    if (previous == nullptr) {
      if (const auto* site = def.site(army.location)) {
        const QPointF at = screen_of(site->position);
        painter.setPen(QPen(color.lighter(130), 2.0));
        for (int i = 0; i < 8; ++i) {
          const double a = i * std::numbers::pi / 4.0;
          painter.drawLine(at + QPointF(std::cos(a) * 12, std::sin(a) * 12),
                           at + QPointF(std::cos(a) * 18, std::sin(a) * 18));
        }
      }
      continue;
    }
    if (previous->location == army.location) {
      continue;
    }
    const S::Path path = graph.shortest_path(
        previous->location, army.location, S::TraversalRules::march(army.faction));
    const S::Path route = path.found
                              ? path
                              : graph.shortest_path(previous->location,
                                                    army.location,
                                                    S::TraversalRules::anything());
    QPainterPath trail;
    if (route.found) {
      for (int i = 0; i < route.routes.size(); ++i) {
        const auto* leg = def.route(route.routes[i]);
        auto line = def.route_polyline(*leg);
        if (leg->from != route.sites[i]) {
          std::reverse(line.begin(), line.end());
        }
        for (int j = 0; j < line.size(); ++j) {
          if (i == 0 && j == 0) {
            trail.moveTo(screen_of(line[j]));
          } else {
            trail.lineTo(screen_of(line[j]));
          }
        }
      }
    }
    QPen glow(
        QColor(255, 255, 255, 120), 7.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.strokePath(trail, glow);
    QPen pen(color, 3.6, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setDashPattern({3.0, 1.6});
    painter.strokePath(trail, pen);
    if (trail.elementCount() >= 2) {
      const QPointF tip = trail.pointAtPercent(1.0);
      const double angle = trail.angleAtPercent(0.995) * std::numbers::pi / 180.0;
      const QPointF dir(std::cos(angle), -std::sin(angle));
      const QPointF normal(-dir.y(), dir.x());
      QPolygonF head{tip,
                     tip - (dir * 13.0) + (normal * 6.0),
                     tip - (dir * 13.0) - (normal * 6.0)};
      painter.setPen(QPen(Qt::white, 1.2));
      painter.setBrush(color);
      painter.drawPolygon(head);
    }
  }
  for (const auto& army : before.armies) {
    if (after.army(army.id) == nullptr) {
      if (const auto* site = def.site(army.location)) {
        const QPointF at = screen_of(site->position) + QPointF(0, -16);
        painter.setPen(QPen(QColor(40, 30, 20, 220), 3.0));
        painter.drawLine(at + QPointF(-7, -7), at + QPointF(7, 7));
        painter.drawLine(at + QPointF(-7, 7), at + QPointF(7, -7));
      }
    }
  }
  painter.restore();
}

void WorldCanvas::paint_supply(QPainter& painter) const {
  const auto& def = m_document->definition();
  painter.save();
  for (const auto& report : m_document->supply()) {
    const auto* army = m_document->state().army(report.army);
    if (army == nullptr) {
      continue;
    }
    const QColor color = supply_color(report.level);
    if (report.path.found && report.path.sites.size() > 1) {
      QPainterPath line;
      for (int i = 0; i < report.path.sites.size(); ++i) {
        const auto* site = def.site(report.path.sites[i]);
        if (site == nullptr) {
          continue;
        }
        if (i == 0) {
          line.moveTo(screen_of(site->position));
        } else {
          line.lineTo(screen_of(site->position));
        }
      }
      QPen pen(color, 3.0, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
      pen.setDashPattern({1.0, 2.0});
      painter.strokePath(
          line, QPen(QColor(20, 20, 20, 110), 5.0, Qt::SolidLine, Qt::RoundCap));
      painter.strokePath(line, pen);
      if (const auto* source = def.site(report.source)) {
        painter.setPen(QPen(color, 2.4));
        painter.setBrush(Qt::NoBrush);
        const double r = site_radius(*source) + 6.0;
        painter.drawEllipse(screen_of(source->position), r, r);
      }
    }
    if (const auto* here = def.site(army->location)) {
      painter.setPen(QPen(color, 2.0, Qt::DashLine));
      painter.setBrush(Qt::NoBrush);
      const double r = site_radius(*here) + 10.0;
      painter.drawEllipse(screen_of(here->position), r, r);
    }
  }
  painter.restore();
}

void WorldCanvas::paint_path(QPainter& painter) const {
  if (!m_path.found || m_path.sites.size() < 2) {
    return;
  }
  const auto& def = m_document->definition();
  QPainterPath trail;
  for (int i = 0; i < m_path.routes.size(); ++i) {
    const auto* leg = def.route(m_path.routes[i]);
    if (leg == nullptr) {
      continue;
    }
    auto line = def.route_polyline(*leg);
    if (leg->from != m_path.sites[i]) {
      std::reverse(line.begin(), line.end());
    }
    for (int j = 0; j < line.size(); ++j) {
      if (i == 0 && j == 0) {
        trail.moveTo(screen_of(line[j]));
      } else {
        trail.lineTo(screen_of(line[j]));
      }
    }
  }
  painter.save();
  painter.strokePath(
      trail,
      QPen(QColor(20, 14, 8, 170), 9.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.strokePath(trail,
                     QPen(k_gold, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  QFont font = painter.font();
  font.setBold(true);
  font.setPointSizeF(8.0);
  painter.setFont(font);
  for (int i = 0; i < m_path.sites.size(); ++i) {
    const auto* site = def.site(m_path.sites[i]);
    if (site == nullptr) {
      continue;
    }
    const QPointF at = screen_of(site->position);
    painter.setPen(QPen(k_ink, 1.4));
    painter.setBrush(k_gold);
    painter.drawEllipse(at, 8.5, 8.5);
    painter.drawText(QRectF(at - QPointF(8.5, 8.5), QSizeF(17, 17)),
                     Qt::AlignCenter,
                     QString::number(i + 1));
  }
  painter.restore();
}

void WorldCanvas::paint_site_glyph(QPainter& painter,
                                   const S::Site& site,
                                   QPointF at,
                                   double r,
                                   const QColor& fill) const {
  const double outline = 1.0 + (site.fortification * 0.6);
  painter.setPen(QPen(k_ink, outline));
  painter.setBrush(fill);
  switch (site.kind) {
  case S::SiteKind::City: {
    const QRectF body(at.x() - r, at.y() - (r * 0.7), r * 2.0, r * 1.7);
    QPolygonF crown;
    crown << body.bottomLeft() << body.topLeft()
          << QPointF(body.left(), body.top() - (r * 0.45))
          << QPointF(body.left() + (r * 0.55), body.top() - (r * 0.45))
          << QPointF(body.left() + (r * 0.55), body.top())
          << QPointF(at.x() - (r * 0.22), body.top())
          << QPointF(at.x() - (r * 0.22), body.top() - (r * 0.45))
          << QPointF(at.x() + (r * 0.22), body.top() - (r * 0.45))
          << QPointF(at.x() + (r * 0.22), body.top())
          << QPointF(body.right() - (r * 0.55), body.top())
          << QPointF(body.right() - (r * 0.55), body.top() - (r * 0.45))
          << QPointF(body.right(), body.top() - (r * 0.45)) << body.topRight()
          << body.bottomRight();
    painter.drawPolygon(crown);
    break;
  }
  case S::SiteKind::Town:
    painter.drawEllipse(at, r, r);
    break;
  case S::SiteKind::Colony:
    painter.drawEllipse(at, r, r);
    painter.setBrush(k_ink);
    painter.drawEllipse(at, r * 0.32, r * 0.32);
    break;
  case S::SiteKind::Fort: {
    QPolygonF diamond{at + QPointF(0, -r * 1.15),
                      at + QPointF(r * 1.15, 0),
                      at + QPointF(0, r * 1.15),
                      at + QPointF(-r * 1.15, 0)};
    painter.drawPolygon(diamond);
    break;
  }
  case S::SiteKind::Camp: {
    QPolygonF tent{at + QPointF(0, -r * 1.1),
                   at + QPointF(r * 1.1, r * 0.8),
                   at + QPointF(-r * 1.1, r * 0.8)};
    painter.drawPolygon(tent);
    break;
  }
  case S::SiteKind::Pass: {
    QPolygonF peaks{at + QPointF(-r * 1.4, r * 0.8),
                    at + QPointF(-r * 0.45, -r * 0.9),
                    at + QPointF(0, -r * 0.1),
                    at + QPointF(r * 0.55, -r * 1.2),
                    at + QPointF(r * 1.4, r * 0.8)};
    painter.drawPolygon(peaks);
    painter.setPen(QPen(Qt::white, 1.4));
    painter.drawLine(at + QPointF(r * 0.3, -r * 0.85),
                     at + QPointF(r * 0.55, -r * 1.2));
    painter.drawLine(at + QPointF(r * 0.55, -r * 1.2),
                     at + QPointF(r * 0.8, -r * 0.85));
    break;
  }
  case S::SiteKind::Crossing:
    painter.drawEllipse(at, r, r);
    painter.setPen(QPen(QColor(40, 86, 130), 1.8));
    painter.drawArc(
        QRectF(at - QPointF(r * 0.8, r * 0.3), QSizeF(r * 1.6, r * 1.2)), 0, 180 * 16);
    painter.drawLine(at + QPointF(-r * 0.8, r * 0.35), at + QPointF(r * 0.8, r * 0.35));
    break;
  case S::SiteKind::Field:
    painter.drawEllipse(at, r * 0.95, r * 0.95);
    painter.setPen(QPen(k_ink, 2.0));
    painter.drawLine(at + QPointF(-r * 0.55, -r * 0.55),
                     at + QPointF(r * 0.55, r * 0.55));
    painter.drawLine(at + QPointF(-r * 0.55, r * 0.55),
                     at + QPointF(r * 0.55, -r * 0.55));
    break;
  }
  if ((m_layers & Ports) != 0U && site.has_port()) {
    const QPointF a = at + QPointF(r * 0.9, r * 0.9);
    const double s = site.port == S::PortClass::Major ? 4.5 : 3.4;
    painter.setPen(QPen(QColor(20, 50, 90), 1.6));
    painter.setBrush(QColor(210, 230, 245));
    painter.drawEllipse(a, s, s);
    painter.drawLine(a + QPointF(0, -s * 0.6), a + QPointF(0, s * 0.6));
    painter.drawLine(a + QPointF(-s * 0.55, s * 0.15), a + QPointF(s * 0.55, s * 0.15));
  }
}

void WorldCanvas::paint_resources(QPainter& painter,
                                  const S::Site& site,
                                  QPointF at) const {
  const double bar_w = 3.0;
  const double unit = 2.6;
  const double r = site_radius(site);
  double x = at.x() - ((S::k_all_resources.size() * (bar_w + 1.0)) * 0.5);
  const double base = at.y() + r + 4.0 + (S::k_resource_max * unit);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(20, 14, 8, 120));
  painter.drawRect(QRectF(x - 1.5,
                          base - (S::k_resource_max * unit) - 1.5,
                          (S::k_all_resources.size() * (bar_w + 1.0)) + 2.0,
                          (S::k_resource_max * unit) + 3.0));
  for (const S::Resource resource : S::k_all_resources) {
    const int value = site.resources.get(resource);
    painter.setBrush(resource_color(resource));
    painter.drawRect(QRectF(x, base - (value * unit), bar_w, value * unit));
    x += bar_w + 1.0;
  }
}

void WorldCanvas::paint_sites(QPainter& painter) const {
  const auto& def = m_document->definition();
  const auto& state = m_document->state();
  const auto chokepoints = (m_layers & Chokepoints) != 0U
                               ? S::StrategicGraph(def, state)
                                     .chokepoint_sites(S::TraversalRules::anything())
                               : QStringList();
  QSet<QString> objectives;
  if ((m_layers & Objectives) != 0U) {
    for (const auto& army : state.armies) {
      if (!army.objective.isEmpty()) {
        objectives.insert(army.objective);
      }
    }
  }
  painter.save();
  for (const auto& site : def.sites) {
    const QPointF at = screen_of(site.position);
    if (!rect().adjusted(-40, -40, 40, 40).contains(at.toPoint())) {
      continue;
    }
    const double r = site_radius(site);
    const QString owner = S::site_controller(def, state, site.id);
    QColor fill = faction_color(owner);
    if (owner == QLatin1String(S::k_neutral_faction)) {
      fill = QColor(222, 210, 186);
    }
    if (!m_reach.empty()) {
      const auto it = m_reach.find(site.id);
      if (it != m_reach.end()) {
        const double t = m_reach_days > 0.0 ? it->second / m_reach_days : 0.0;
        const QColor ring = lerp_color(QColor(90, 200, 100), QColor(220, 60, 40), t);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(ring.red(), ring.green(), ring.blue(), 110));
        painter.drawEllipse(at, r + 9.0, r + 9.0);
      } else {
        fill = fill.darker(180);
      }
    }
    if (chokepoints.contains(site.id)) {
      painter.setPen(QPen(QColor(236, 140, 50), 2.0, Qt::DotLine));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(at, r + 5.0, r + 5.0);
    }
    if (objectives.contains(site.id)) {
      painter.setPen(QPen(QColor(200, 40, 30), 1.6));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(at, r + 7.5, r + 7.5);
      painter.drawLine(at + QPointF(-r - 11, 0), at + QPointF(-r - 4, 0));
      painter.drawLine(at + QPointF(r + 4, 0), at + QPointF(r + 11, 0));
    }
    const bool selected = m_document->selection() == site.id;
    if (selected) {
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(QColor(20, 14, 8, 200), 6.0));
      painter.drawEllipse(at, r + 7.0, r + 7.0);
      painter.setPen(QPen(k_gold, 3.0));
      painter.drawEllipse(at, r + 7.0, r + 7.0);
    } else if (m_hover == site.id || m_route_start == site.id) {
      painter.setPen(QPen(QColor(255, 250, 235, 220), 2.0));
      painter.setBrush(QColor(255, 250, 235, 70));
      painter.drawEllipse(at, r + 5.0, r + 5.0);
    }
    paint_site_glyph(painter, site, at, r, fill);
    if (!site.battlefield.empty()) {
      painter.setPen(QPen(k_ink, 1.0));
      painter.setBrush(QColor(196, 40, 40));
      const QPointF flag = at + QPointF(-r - 2.0, -r - 2.0);
      painter.drawLine(flag, flag + QPointF(0, -9));
      painter.drawPolygon(QPolygonF{
          flag + QPointF(0, -9), flag + QPointF(7, -6.5), flag + QPointF(0, -4)});
    }
    if ((m_layers & Resources) != 0U && S::is_settlement(site.kind)) {
      paint_resources(painter, site, at);
    }
    if (!m_reach.empty() && m_reach.count(site.id) != 0U && m_scale > 0.45) {
      painter.setPen(QColor(255, 255, 255));
      QFont font = painter.font();
      font.setPointSizeF(7.5);
      font.setBold(true);
      painter.setFont(font);
      painter.drawText(at + QPointF(-10, r + 22),
                       QStringLiteral("%1d").arg(m_reach.at(site.id), 0, 'f', 1));
    }
  }
  painter.restore();
}

void WorldCanvas::paint_armies(QPainter& painter) const {
  painter.save();
  QFont font = painter.font();
  font.setBold(true);
  font.setPointSizeF(8.0);
  painter.setFont(font);
  for (const auto& entry : army_banners()) {
    const QRectF banner = entry.second;
    const auto& army = *m_document->state().army(entry.first);
    const auto* site = m_document->definition().site(army.location);
    if (site == nullptr) {
      continue;
    }
    const QPointF base = screen_of(site->position);
    const QColor color = faction_color(army.faction);
    const bool selected = m_document->selection() == army.id;
    painter.setPen(QPen(k_ink, 1.6));
    painter.drawLine(base +
                         QPointF(site_radius(*site) * 0.6, -site_radius(*site) * 0.6),
                     banner.bottomLeft());
    if (selected || m_hover == army.id) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(selected ? k_gold : QColor(255, 250, 235, 170));
      painter.drawRoundedRect(banner.adjusted(-3, -3, 3, 3), 4, 4);
    }
    QPolygonF flag{banner.topLeft(),
                   banner.topRight(),
                   QPointF(banner.right() - 5, banner.center().y()),
                   banner.bottomRight(),
                   banner.bottomLeft()};
    painter.setPen(QPen(k_ink, 1.2));
    painter.setBrush(color);
    painter.drawPolygon(flag);
    painter.setPen(Qt::white);
    painter.drawText(banner.adjusted(5, 0, -6, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     banner_text(army));
    if (const auto* supply = m_document->supply_for(army.id)) {
      painter.setPen(QPen(k_ink, 1.0));
      painter.setBrush(supply_color(supply->level));
      painter.drawEllipse(banner.topLeft() + QPointF(0, 0), 4.0, 4.0);
    }
  }
  painter.restore();
}

void WorldCanvas::paint_labels(QPainter& painter) const {
  const auto& def = m_document->definition();
  painter.save();
  QVector<QRectF> taken;
  const QString selected = m_document->selection();
  struct Candidate {
    const S::Site* site;
    int priority;
  };
  QVector<Candidate> candidates;
  for (const auto& site : def.sites) {
    int priority = static_cast<int>(site.population) * 10;
    if (!site.battlefield.empty()) {
      priority += 25;
    }
    if (site.id == selected) {
      priority += 1000;
    } else if (site.id == m_hover) {
      priority += 900;
    }
    const bool major = priority >= 40;
    if (!major && m_scale < 0.85 && site.id != selected && site.id != m_hover) {
      continue;
    }
    candidates.append({&site, priority});
  }
  std::stable_sort(
      candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.priority > b.priority;
      });
  QFont font(QStringLiteral("Serif"));
  for (const auto& candidate : candidates) {
    const S::Site& site = *candidate.site;
    const QPointF at = screen_of(site.position);
    if (!rect().adjusted(-20, -20, 20, 20).contains(at.toPoint())) {
      continue;
    }
    const bool emphasis = candidate.priority >= 900;
    font.setPointSizeF(site.population >= S::PopulationBand::Large ? 10.0 : 8.8);
    font.setBold(site.population >= S::PopulationBand::Large || emphasis);
    font.setItalic(!S::is_settlement(site.kind));
    const QFontMetricsF metrics(font);
    const QSizeF size(metrics.horizontalAdvance(site.name) + 8.0,
                      metrics.height() + 1.0);
    const double r = site_radius(site);
    const std::array<QPointF, 4> offsets = {
        QPointF(r + 3.0, -size.height() * 0.5),
        QPointF(-r - 3.0 - size.width(), -size.height() * 0.5),
        QPointF(-size.width() * 0.5, r + 2.0),
        QPointF(-size.width() * 0.5, -r - 2.0 - size.height())};
    QRectF placed;
    for (const QPointF& offset : offsets) {
      const QRectF box(at + offset, size);
      const bool clear =
          std::none_of(taken.begin(), taken.end(), [&box](const QRectF& other) {
            return other.intersects(box);
          });
      if (clear || emphasis) {
        placed = box;
        break;
      }
    }
    if (placed.isNull()) {
      continue;
    }
    taken.append(placed.adjusted(-1, -1, 1, 1));
    painter.setFont(font);
    painter.setPen(emphasis ? QPen(k_gold, 1.2) : Qt::NoPen);
    painter.setBrush(emphasis ? QColor(40, 28, 16, 235) : QColor(245, 234, 210, 200));
    painter.drawRoundedRect(placed, 3, 3);
    painter.setPen(emphasis ? QColor(255, 240, 205) : k_ink);
    painter.drawText(placed, Qt::AlignCenter, site.name);
  }
  QFont region_font(QStringLiteral("Serif"));
  region_font.setPointSizeF(std::clamp(10.0 * std::sqrt(m_scale / 0.5), 8.0, 20.0));
  region_font.setLetterSpacing(QFont::PercentageSpacing, 135);
  region_font.setCapitalization(QFont::AllUppercase);
  painter.setFont(region_font);
  const QFontMetricsF region_metrics(region_font);
  for (auto it = m_region_labels.cbegin(); it != m_region_labels.cend(); ++it) {
    const auto* region = def.region(it.key());
    if (region == nullptr) {
      continue;
    }
    const QPointF at = (it.value() * m_scale) + m_offset;
    const double w = region_metrics.horizontalAdvance(region->name.toUpper()) + 8.0;
    const QRectF box(at - QPointF(w * 0.5, region_metrics.height() * 0.5),
                     QSizeF(w, region_metrics.height()));
    if (std::any_of(taken.begin(), taken.end(), [&box](const QRectF& other) {
          return other.intersects(box);
        })) {
      continue;
    }
    painter.setPen(QColor(70, 44, 24, 150));
    painter.drawText(box, Qt::AlignCenter, region->name);
  }
  painter.restore();
}

void WorldCanvas::paint_issues(QPainter& painter) const {
  const auto& def = m_document->definition();
  painter.save();
  painter.setBrush(Qt::NoBrush);
  for (const auto& issue : m_document->report().issues) {
    if (issue.severity == S::Severity::Info) {
      continue;
    }
    const QColor color = issue.severity == S::Severity::Error ? k_error : k_warning;
    if (const auto* site = def.site(issue.entity)) {
      const QPointF at = screen_of(site->position);
      const double r = site_radius(*site) + 11.0;
      painter.setPen(QPen(color, 2.6));
      painter.drawEllipse(at, r, r);
      painter.setPen(QPen(Qt::white));
      painter.setBrush(color);
      painter.drawEllipse(at + QPointF(r * 0.7, -r * 0.7), 6.5, 6.5);
      QFont font = painter.font();
      font.setBold(true);
      font.setPointSizeF(7.5);
      painter.setFont(font);
      painter.drawText(
          QRectF(at + QPointF(r * 0.7 - 6.5, -r * 0.7 - 6.5), QSizeF(13, 13)),
          Qt::AlignCenter,
          QStringLiteral("!"));
      painter.setBrush(Qt::NoBrush);
    } else if (const auto* route = def.route(issue.entity)) {
      QPen pen(color, 3.0, Qt::DashLine, Qt::RoundCap);
      painter.strokePath(route_screen_path(*route), pen);
    } else if (m_region_paths.contains(issue.entity)) {
      painter.save();
      painter.translate(m_offset);
      painter.scale(m_scale, m_scale);
      painter.strokePath(m_region_paths.value(issue.entity),
                         QPen(color, 2.4 / m_scale, Qt::DashLine));
      painter.restore();
    }
  }
  painter.restore();
}

void WorldCanvas::paint_waypoints(QPainter& painter) const {
  const auto* route = m_document->definition().route(m_document->selection());
  if (route == nullptr) {
    return;
  }
  painter.save();
  for (int i = 0; i < route->waypoints.size(); ++i) {
    const QPointF at = screen_of(route->waypoints[i]);
    painter.setPen(QPen(k_ink, 1.5));
    painter.setBrush(i == m_drag_waypoint ? k_gold : QColor(255, 250, 235));
    painter.drawRect(QRectF(at - QPointF(4.5, 4.5), QSizeF(9, 9)));
  }
  painter.restore();
}

void WorldCanvas::paint_overlay(QPainter& painter) const {
  if (m_route_start.isEmpty()) {
    return;
  }
  const auto* site = m_document->definition().site(m_route_start);
  if (site == nullptr) {
    return;
  }
  painter.save();
  const QColor color = m_tool == Tool::PathQuery ? k_gold : QColor(255, 255, 255);
  QPen pen(color, 2.4, Qt::DashLine, Qt::RoundCap);
  painter.setPen(pen);
  painter.drawLine(screen_of(site->position), m_mouse);
  painter.restore();
}

void WorldCanvas::paint_legend(QPainter& painter) const {
  const auto& def = m_document->definition();
  painter.save();
  QFont font = painter.font();
  font.setPointSizeF(8.0);
  painter.setFont(font);
  if ((m_layers & Legend) != 0U) {
    const QStringList route_names = {QStringLiteral("Major road"),
                                     QStringLiteral("Road"),
                                     QStringLiteral("Track"),
                                     QStringLiteral("Mountain pass"),
                                     QStringLiteral("River crossing"),
                                     QStringLiteral("Coastal road"),
                                     QStringLiteral("Sea lane")};
    const double row = 15.0;
    const double height = 14.0 + ((def.factions.size() + 1 + route_names.size()) * row);
    QRectF box(10, this->height() - height - 10, 150, height);
    painter.setPen(QPen(QColor(140, 106, 62), 1.0));
    painter.setBrush(QColor(28, 21, 15, 215));
    painter.drawRoundedRect(box, 6, 6);
    double y = box.top() + 12.0;
    const auto swatch = [&](const QColor& color, const QString& text) {
      painter.setPen(QPen(QColor(20, 14, 8), 1.0));
      painter.setBrush(color);
      painter.drawRect(QRectF(box.left() + 10, y - 6, 16, 10));
      painter.setPen(QColor(244, 231, 200));
      painter.drawText(QPointF(box.left() + 34, y + 3), text);
      y += row;
    };
    for (const auto& faction : def.factions) {
      swatch(faction_color(faction.id), faction.name);
    }
    swatch(QColor(222, 210, 186), QStringLiteral("Neutral"));
    for (int i = 0; i < route_names.size(); ++i) {
      const RouteStyle style = route_style(static_cast<S::RouteKind>(i));
      QPen pen(style.color.alpha() < 255 ? QColor(150, 190, 220) : style.color,
               style.width,
               style.style,
               Qt::FlatCap);
      if (style.style == Qt::CustomDashLine) {
        pen.setDashPattern(style.dashes);
      }
      painter.setPen(pen);
      painter.drawLine(QPointF(box.left() + 8, y - 1), QPointF(box.left() + 28, y - 1));
      if (style.inner.isValid()) {
        painter.setPen(QPen(style.inner, style.width * 0.35));
        painter.drawLine(QPointF(box.left() + 8, y - 1),
                         QPointF(box.left() + 28, y - 1));
      }
      painter.setPen(QColor(244, 231, 200));
      painter.drawText(QPointF(box.left() + 34, y + 3), route_names[i]);
      y += row;
    }
  }
  const S::LonLat a = lonlat_at(QPointF(width() - 160, this->height() - 30));
  const S::LonLat b = lonlat_at(QPointF(width() - 60, this->height() - 30));
  const double km_per_100px = S::great_circle_km(a, b);
  const double nice = km_per_100px > 400   ? 200
                      : km_per_100px > 200 ? 100
                      : km_per_100px > 100 ? 50
                                           : 20;
  const double px = 100.0 * nice / std::max(1.0, km_per_100px);
  const QPointF origin(width() - 30 - px, this->height() - 22);
  painter.setPen(QPen(QColor(244, 231, 200), 2.0));
  painter.drawLine(origin, origin + QPointF(px, 0));
  painter.drawLine(origin, origin + QPointF(0, -5));
  painter.drawLine(origin + QPointF(px, 0), origin + QPointF(px, -5));
  painter.drawText(origin + QPointF(0, -8), QStringLiteral("%1 km").arg(nice));
  painter.restore();
}

auto WorldCanvas::hit_waypoint(QPointF screen) const -> int {
  const auto* route = m_document->definition().route(m_document->selection());
  if (route == nullptr) {
    return -1;
  }
  for (int i = 0; i < route->waypoints.size(); ++i) {
    if (QLineF(screen, screen_of(route->waypoints[i])).length() <= 7.0) {
      return i;
    }
  }
  return -1;
}

void WorldCanvas::mousePressEvent(QMouseEvent* event) {
  setFocus(Qt::MouseFocusReason);
  m_last_mouse = event->pos();
  m_mouse = event->position();
  m_press = event->position();
  if (event->button() == Qt::MiddleButton ||
      (event->button() == Qt::LeftButton && m_space_held)) {
    m_panning = true;
    setCursor(Qt::ClosedHandCursor);
    return;
  }
  if (event->button() == Qt::RightButton) {
    const QString id = hit_any(event->position());
    if (!id.isEmpty() && m_document->definition().region(id) == nullptr) {
      m_document->select(id);
    }
    emit context_requested(
        id, lonlat_at(event->position()), event->globalPosition().toPoint());
    return;
  }
  if (event->button() != Qt::LeftButton) {
    return;
  }
  const QPointF pos = event->position();
  switch (m_tool) {
  case Tool::Select: {
    const int waypoint = hit_waypoint(pos);
    if (waypoint >= 0) {
      m_drag_waypoint = waypoint;
      return;
    }
    const QString id = hit_any(pos);
    m_document->select(id);
    if (m_drag_edit && m_document->definition().site(id) != nullptr &&
        hit_army(pos).isEmpty()) {
      m_drag_site = id;
      m_drag_origin = m_document->definition().site(id)->position;
      m_drag_position = m_drag_origin;
    } else if (id.isEmpty() || m_document->definition().region(id) != nullptr) {
      m_panning = true;
      setCursor(Qt::ClosedHandCursor);
    }
    break;
  }
  case Tool::AddSite: {
    const QString existing = hit_site(pos);
    if (!existing.isEmpty()) {
      m_document->select(existing);
      emit status_message(QStringLiteral("%1 is already here — selected it instead.")
                              .arg(m_document->definition().display_name(existing)));
      break;
    }
    m_document->add_site(lonlat_at(pos), m_new_site_kind);
    if ((event->modifiers() & Qt::ShiftModifier) == 0U) {
      set_tool(Tool::Select);
      emit tool_changed(Tool::Select);
    }
    emit status_message(
        QStringLiteral(
            "Added %1 — drag it into place, then connect it with Add Route (R).")
            .arg(m_document->definition().display_name(m_document->selection())));
    break;
  }
  case Tool::AddRoute:
  case Tool::PathQuery: {
    const QString site = hit_site(pos);
    if (site.isEmpty()) {
      emit status_message(QStringLiteral("Click on a site."));
      break;
    }
    if (m_route_start.isEmpty()) {
      m_route_start = site;
      emit status_message(
          QStringLiteral("From %1 — click the destination. Esc cancels.")
              .arg(m_document->definition().display_name(site)));
    } else if (site != m_route_start) {
      if (m_tool == Tool::AddRoute) {
        const QString id = m_document->add_route(m_route_start, site, m_new_route_kind);
        emit status_message(
            QStringLiteral("Added %1. Keep clicking sites to chain roads.")
                .arg(m_document->definition().display_name(id)));
        m_route_start = site;
      } else {
        emit path_requested(m_route_start, site);
        m_route_start.clear();
      }
    }
    update();
    break;
  }
  case Tool::Waypoint: {
    const QString route = m_document->selection();
    if (m_document->definition().route(route) == nullptr) {
      const QString hit = hit_route(pos);
      if (!hit.isEmpty()) {
        m_document->select(hit);
        emit status_message(
            QStringLiteral("Route selected — click the map to bend it."));
      } else {
        emit status_message(
            QStringLiteral("Click a route to select it, then click to add bends."));
      }
      break;
    }
    m_document->add_waypoint(route, lonlat_at(pos));
    break;
  }
  }
}

void WorldCanvas::mouseMoveEvent(QMouseEvent* event) {
  m_mouse = event->position();
  if (m_panning) {
    m_user_view = true;
    m_offset += event->pos() - m_last_mouse;
    m_last_mouse = event->pos();
    update();
    return;
  }
  if (m_drag_waypoint >= 0 && (event->buttons() & Qt::LeftButton) != 0U) {
    auto& def = const_cast<S::WorldDefinition&>(m_document->definition());
    if (S::Route* route = def.route_mut(m_document->selection());
        route != nullptr && m_drag_waypoint < route->waypoints.size()) {
      if (!m_dragging_site) {
        m_drag_origin = route->waypoints[m_drag_waypoint];
      }
      m_dragging_site = true;
      m_drag_position = lonlat_at(event->position());
      route->waypoints[m_drag_waypoint] = m_drag_position;
    }
    update();
    return;
  }
  if (!m_drag_site.isEmpty() && (event->buttons() & Qt::LeftButton) != 0U) {
    if (!m_dragging_site && QLineF(m_press, event->position()).length() < 3.0) {
      return;
    }
    m_dragging_site = true;
    m_drag_position = lonlat_at(event->position());
    auto& def = const_cast<S::WorldDefinition&>(m_document->definition());
    if (S::Site* site = def.site_mut(m_drag_site)) {
      site->position = m_drag_position;
    }
    update();
    return;
  }
  const QString hover = hit_any(event->position());
  if (hover != m_hover) {
    m_hover = hover;
    if (!hover.isEmpty()) {
      QToolTip::showText(event->globalPosition().toPoint(), describe(hover), this);
    } else {
      QToolTip::hideText();
    }
    update();
  } else if (!m_route_start.isEmpty()) {
    update();
  }
  setCursor(hit_waypoint(event->position()) >= 0 ? Qt::SizeAllCursor
            : m_tool == Tool::Select
                ? (hover.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor)
                : Qt::CrossCursor);
  const S::LonLat ll = lonlat_at(event->position());
  emit cursor_moved(QStringLiteral("%1° %2  %3° %4")
                        .arg(std::abs(ll.lat), 0, 'f', 2)
                        .arg(ll.lat >= 0 ? QStringLiteral("N") : QStringLiteral("S"))
                        .arg(std::abs(ll.lon), 0, 'f', 2)
                        .arg(ll.lon >= 0 ? QStringLiteral("E") : QStringLiteral("W")));
}

void WorldCanvas::mouseReleaseEvent(QMouseEvent* /*event*/) {
  if (m_panning) {
    m_panning = false;
    setCursor(m_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
    return;
  }
  auto& def = const_cast<S::WorldDefinition&>(m_document->definition());
  if (m_drag_waypoint >= 0) {
    const int index = m_drag_waypoint;
    m_drag_waypoint = -1;
    if (m_dragging_site) {
      m_dragging_site = false;
      const QString route = m_document->selection();
      if (S::Route* entry = def.route_mut(route);
          entry != nullptr && index < entry->waypoints.size()) {
        entry->waypoints[index] = m_drag_origin;
      }
      m_document->move_waypoint(route, index, m_drag_position);
    }
    return;
  }
  if (m_drag_site.isEmpty()) {
    return;
  }
  const QString id = m_drag_site;
  m_drag_site.clear();
  if (!m_dragging_site) {
    return;
  }
  m_dragging_site = false;
  if (S::Site* site = def.site_mut(id)) {
    site->position = m_drag_origin;
  }
  m_document->move_site(id, m_drag_position);
  emit status_message(
      QStringLiteral("Moved %1. Ctrl+Z undoes.").arg(def.display_name(id)));
}

void WorldCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  const int waypoint = hit_waypoint(event->position());
  if (waypoint >= 0) {
    m_document->remove_waypoint(m_document->selection(), waypoint);
    return;
  }
  const QString id = hit_any(event->position());
  if (!id.isEmpty()) {
    emit activated(id);
  }
}

void WorldCanvas::wheelEvent(QWheelEvent* event) {
  zoom_by(std::pow(k_zoom_step, event->angleDelta().y() / 120.0), event->position());
}

void WorldCanvas::zoom_by(double factor, QPointF around) {
  m_user_view = true;
  const double next = std::clamp(m_scale * factor, k_min_scale, k_max_scale);
  const QPointF map = (around - m_offset) / m_scale;
  m_scale = next;
  m_offset = around - (map * m_scale);
  update();
}

void WorldCanvas::resizeEvent(QResizeEvent* /*event*/) {
  if (!m_user_view) {
    fit_all();
  }
}

void WorldCanvas::leaveEvent(QEvent* /*event*/) {
  m_hover.clear();
  update();
}

void WorldCanvas::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
    m_space_held = false;
    setCursor(m_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    return;
  }
  QWidget::keyReleaseEvent(event);
}

void WorldCanvas::keyPressEvent(QKeyEvent* event) {
  const double step = 80.0;
  switch (event->key()) {
  case Qt::Key_Escape:
    if (!m_route_start.isEmpty()) {
      m_route_start.clear();
      emit status_message(QStringLiteral("Cancelled."));
    } else if (m_path.found || !m_reach.empty()) {
      clear_highlight_path();
      clear_reach();
    } else {
      m_document->select({});
    }
    update();
    return;
  case Qt::Key_Space:
    if (!event->isAutoRepeat()) {
      m_space_held = true;
      setCursor(Qt::OpenHandCursor);
    }
    return;
  case Qt::Key_Left:
    m_offset.rx() += step;
    break;
  case Qt::Key_Right:
    m_offset.rx() -= step;
    break;
  case Qt::Key_Up:
    m_offset.ry() += step;
    break;
  case Qt::Key_Down:
    m_offset.ry() -= step;
    break;
  case Qt::Key_Plus:
  case Qt::Key_Equal:
    zoom_by(k_zoom_step * k_zoom_step, rect().center());
    return;
  case Qt::Key_Minus:
    zoom_by(1.0 / (k_zoom_step * k_zoom_step), rect().center());
    return;
  case Qt::Key_F:
    if (m_document->selection().isEmpty()) {
      fit_all();
    } else {
      focus_on(m_document->selection());
    }
    return;
  case Qt::Key_Delete:
  case Qt::Key_Backspace:
    emit delete_requested(m_document->selection());
    return;
  default:
    QWidget::keyPressEvent(event);
    return;
  }
  m_user_view = true;
  update();
}

} // namespace MapEditor
