#pragma once

#include <QHash>
#include <QImage>
#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <map>

#include "game/strategic/strategic_graph.h"
#include "game/strategic/strategic_world.h"

namespace MapEditor {

class WorldDocument;

class WorldCanvas : public QWidget {
  Q_OBJECT

public:
  enum class Tool {
    Select,
    AddSite,
    AddRoute,
    Waypoint,
    PathQuery
  };

  enum Layer : unsigned {
    Relief = 1U << 0U,
    Control = 1U << 1U,
    Borders = 1U << 2U,
    Districts = 1U << 3U,
    Routes = 1U << 4U,
    Sites = 1U << 5U,
    Labels = 1U << 6U,
    Armies = 1U << 7U,
    Supply = 1U << 8U,
    Resources = 1U << 9U,
    Issues = 1U << 10U,
    Movements = 1U << 11U,
    Ports = 1U << 12U,
    Chokepoints = 1U << 13U,
    Objectives = 1U << 14U,
    Legend = 1U << 15U,
  };

  static constexpr unsigned k_default_layers =
      Relief | Control | Borders | Routes | Sites | Labels | Armies | Issues |
      Movements | Ports | Chokepoints | Objectives;

  explicit WorldCanvas(WorldDocument* document, QWidget* parent = nullptr);

  [[nodiscard]] auto layers() const -> unsigned { return m_layers; }
  void set_layer(Layer layer, bool enabled);
  void set_tool(Tool tool);
  [[nodiscard]] auto tool() const -> Tool { return m_tool; }
  void set_new_site_kind(Game::Strategic::SiteKind kind) { m_new_site_kind = kind; }
  void set_new_route_kind(Game::Strategic::RouteKind kind) { m_new_route_kind = kind; }
  void set_highlight_path(const Game::Strategic::Path& path);
  void clear_highlight_path();
  void set_reach(const std::map<QString, double>& reach, double max_days);
  void clear_reach();
  void focus_on(const QString& id);
  void fit_all();
  void zoom_by(double factor, QPointF around);
  [[nodiscard]] auto view_center() const -> Game::Strategic::LonLat {
    return lonlat_at(QPointF(width() * 0.5, height() * 0.5));
  }
  void set_drag_edit_enabled(bool enabled) { m_drag_edit = enabled; }

signals:
  void status_message(const QString& message);
  void cursor_moved(const QString& position);
  void layers_changed(unsigned layers);
  void context_requested(const QString& id,
                         const Game::Strategic::LonLat& position,
                         const QPoint& global);
  void delete_requested(const QString& id);
  void tool_changed(Tool tool);
  void path_requested(const QString& from, const QString& to);
  void activated(const QString& id);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

private:
  void load_backdrop();
  void rebuild_shapes();
  [[nodiscard]] auto map_of(const Game::Strategic::LonLat& point) const -> QPointF;
  [[nodiscard]] auto lonlat_of_map(QPointF map) const -> Game::Strategic::LonLat;
  [[nodiscard]] auto screen_of(const Game::Strategic::LonLat& point) const -> QPointF;
  [[nodiscard]] auto lonlat_at(QPointF screen) const -> Game::Strategic::LonLat;
  [[nodiscard]] auto map_rect() const -> QRectF;
  [[nodiscard]] auto hit_site(QPointF screen) const -> QString;
  [[nodiscard]] auto hit_army(QPointF screen) const -> QString;
  [[nodiscard]] auto hit_route(QPointF screen) const -> QString;
  [[nodiscard]] auto hit_region(QPointF screen) const -> QString;
  [[nodiscard]] auto hit_any(QPointF screen) const -> QString;
  [[nodiscard]] auto hit_waypoint(QPointF screen) const -> int;
  [[nodiscard]] auto
  route_screen_path(const Game::Strategic::Route& route) const -> QPainterPath;
  [[nodiscard]] auto site_radius(const Game::Strategic::Site& site) const -> double;
  [[nodiscard]] auto faction_color(const QString& faction) const -> QColor;
  [[nodiscard]] auto army_anchor(const Game::Strategic::ArmyState& army,
                                 int stack) const -> QPointF;
  [[nodiscard]] auto describe(const QString& id) const -> QString;
  [[nodiscard]] auto banner_rect(const Game::Strategic::ArmyState& army,
                                 int stack) const -> QRectF;
  [[nodiscard]] static auto
  banner_text(const Game::Strategic::ArmyState& army) -> QString;
  [[nodiscard]] auto army_banners() const -> QVector<std::pair<QString, QRectF>>;

  void paint_backdrop(QPainter& painter) const;
  void paint_regions(QPainter& painter) const;
  void paint_districts(QPainter& painter) const;
  void paint_routes(QPainter& painter) const;
  void paint_movements(QPainter& painter) const;
  void paint_supply(QPainter& painter) const;
  void paint_path(QPainter& painter) const;
  void paint_sites(QPainter& painter) const;
  void paint_site_glyph(QPainter& painter,
                        const Game::Strategic::Site& site,
                        QPointF at,
                        double radius,
                        const QColor& fill) const;
  void paint_resources(QPainter& painter,
                       const Game::Strategic::Site& site,
                       QPointF at) const;
  void paint_armies(QPainter& painter) const;
  void paint_labels(QPainter& painter) const;
  void paint_issues(QPainter& painter) const;
  void paint_waypoints(QPainter& painter) const;
  void paint_overlay(QPainter& painter) const;
  void paint_legend(QPainter& painter) const;

  WorldDocument* m_document = nullptr;
  QImage m_backdrop;
  QImage m_backdrop_plain;
  QHash<QString, QPainterPath> m_region_paths;
  QHash<QString, QPointF> m_region_labels;
  double m_aspect = 1.0;
  double m_scale = 0.5;
  QPointF m_offset;
  unsigned m_layers = k_default_layers;
  Tool m_tool = Tool::Select;
  Game::Strategic::SiteKind m_new_site_kind = Game::Strategic::SiteKind::Town;
  Game::Strategic::RouteKind m_new_route_kind = Game::Strategic::RouteKind::Road;
  QString m_hover;
  QString m_route_start;
  QString m_drag_site;
  Game::Strategic::LonLat m_drag_position;
  Game::Strategic::LonLat m_drag_origin;
  bool m_dragging_site = false;
  bool m_panning = false;
  bool m_drag_edit = true;
  QPoint m_last_mouse;
  QPointF m_mouse;
  Game::Strategic::Path m_path;
  std::map<QString, double> m_reach;
  double m_reach_days = 0.0;
  bool m_fitted = false;
  bool m_user_view = false;
  bool m_space_held = false;
  int m_drag_waypoint = -1;
  QPointF m_press;
};

} // namespace MapEditor
