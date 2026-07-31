#pragma once

#include <QColor>
#include <QJsonObject>
#include <QPair>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

#include <optional>

#include "hill_projection_model.h"

namespace MapEditor {

class TerrainProjectionWidget : public QWidget {
  Q_OBJECT

public:
  explicit TerrainProjectionWidget(QWidget* parent = nullptr);

  void set_map_context(const HillProjection::MapContext& context);
  void set_terrain_json(const QJsonObject& json);
  void set_active_layer(int index);
  void set_brush_size(int cells);
  [[nodiscard]] int brush_size() const { return m_brush_size; }
  void set_entrance_cells(const QVector<QPoint>& cells);

  [[nodiscard]] QVector<QPoint> entrance_cells() const;
  [[nodiscard]] QVector<QPoint> body_cells() const;
  [[nodiscard]] QStringList issues() const;
  [[nodiscard]] bool is_active() const { return m_active; }
  [[nodiscard]] const HillProjection::Model& get_model() const { return m_model; }

  [[nodiscard]] virtual QVector<QPair<QString, QColor>> layer_definitions() const = 0;
  [[nodiscard]] virtual bool body_cells_user_editable() const = 0;
  [[nodiscard]] virtual int entrance_layer_index() const = 0;
  [[nodiscard]] virtual int body_layer_index() const = 0;

signals:
  void projection_changed();
  void entrance_rejected(const QString& reason);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  HillProjection::Model m_model;
  HillProjection::MapContext m_map_context;
  QSet<quint64> m_body_cells;
  QSet<quint64> m_entrance_cells;

  [[nodiscard]] const QSet<quint64>& rim_cells() const;

  static quint64 encode_cell(const QPoint& cell);
  static QPoint decode_cell(quint64 encoded);

private:
  enum class DragMode {
    None,
    Paint,
    Erase,
  };

  struct GridGeometry {
    QRectF rect;
    double cell_size = 0.0;
    bool valid = false;
  };

  [[nodiscard]] GridGeometry compute_geometry() const;
  [[nodiscard]] QRectF cell_rect(const GridGeometry& geometry,
                                 const QPoint& cell) const;
  [[nodiscard]] std::optional<QPoint> cell_from_position(const QPoint& position) const;

  void apply_line(const QPoint& from, const QPoint& to);
  void stamp_brush(const QPoint& cell, bool marked);
  [[nodiscard]] bool reaches_body(const QPoint& cell) const;
  void set_cell_marked(const QPoint& cell, bool marked);
  void finish_stroke();

  QPoint m_last_drag_cell;
  DragMode m_drag_mode = DragMode::None;
  int m_active_layer = 0;
  int m_brush_size = 1;
  bool m_active = false;
  bool m_stroke_changed = false;
  bool m_stroke_rejected = false;
  mutable QSet<quint64> m_rim_cells;
  mutable bool m_rim_dirty = true;
};

} // namespace MapEditor
