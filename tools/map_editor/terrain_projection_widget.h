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

  void set_terrain_json(const QJsonObject& json);
  void set_active_layer(int index);

  [[nodiscard]] QVector<QPoint> entrance_cells() const;
  [[nodiscard]] QVector<QPoint> body_cells() const;
  [[nodiscard]] bool is_active() const { return m_active; }
  [[nodiscard]] const HillProjection::Model& get_model() const { return m_model; }

  [[nodiscard]] virtual QVector<QPair<QString, QColor>> layer_definitions() const = 0;
  [[nodiscard]] virtual bool body_cells_user_editable() const = 0;
  [[nodiscard]] virtual int entrance_layer_index() const = 0;
  [[nodiscard]] virtual int body_layer_index() const = 0;

signals:
  void projection_changed();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  HillProjection::Model m_model;
  QSet<quint64> m_body_cells;
  QSet<quint64> m_entrance_cells;

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
  [[nodiscard]] std::optional<QPoint> cell_from_position(const QPoint& position) const;

  void apply_line(const QPoint& from, const QPoint& to);
  void set_cell_marked(const QPoint& cell, bool marked);

  QPoint m_last_drag_cell;
  DragMode m_drag_mode = DragMode::None;
  int m_active_layer = 0;
  bool m_active = false;
};

} // namespace MapEditor
