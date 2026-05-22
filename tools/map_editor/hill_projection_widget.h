#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QWidget>

#include <optional>

#include "hill_projection_model.h"

namespace MapEditor {

class HillProjectionWidget : public QWidget {
  Q_OBJECT

public:
  enum class EditLayer {
    Hill,
    Entrance,
    Nothing,
  };

  explicit HillProjectionWidget(QWidget* parent = nullptr);

  void set_hill_json(const QJsonObject& hill_json);
  void set_edit_layer(EditLayer layer) { m_edit_layer = layer; }
  [[nodiscard]] bool is_hill_active() const { return m_hill_active; }
  [[nodiscard]] auto hill_cells() const -> QVector<QPoint>;
  [[nodiscard]] auto entrance_cells() const -> QVector<QPoint>;

signals:
  void projection_changed();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

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

  [[nodiscard]] auto compute_geometry() const -> GridGeometry;
  [[nodiscard]] auto cell_from_position(const QPoint& position) const
      -> std::optional<QPoint>;

  void apply_line(const QPoint& from, const QPoint& to);
  void set_cell_marked(const QPoint& cell, bool marked);
  void emit_projection_changed();

  static auto encode_cell(const QPoint& cell) -> quint64;
  static auto decode_cell(quint64 encoded) -> QPoint;

  HillProjection::Model m_model;
  QSet<quint64> m_hill_cells;
  QSet<quint64> m_entrance_cells;
  QPoint m_last_drag_cell;
  DragMode m_drag_mode = DragMode::None;
  EditLayer m_edit_layer = EditLayer::Entrance;
  bool m_hill_active = false;
};

} // namespace MapEditor
