#pragma once

#include <QMouseEvent>

#include "terrain_projection_widget.h"

namespace MapEditor {

// Mountains are fully impassable — the game engine skips all entrance processing.
// This widget is display-only: it shows the approximate footprint but accepts no edits.
class MountainProjectionWidget : public TerrainProjectionWidget {
  Q_OBJECT

public:
  explicit MountainProjectionWidget(QWidget* parent = nullptr);

  [[nodiscard]] QVector<QPair<QString, QColor>> layer_definitions() const override {
    return {};
  }
  [[nodiscard]] bool body_cells_user_editable() const override { return false; }
  [[nodiscard]] int entrance_layer_index() const override { return -1; }
  [[nodiscard]] int body_layer_index() const override { return -1; }

protected:
  void mousePressEvent(QMouseEvent* event) override { event->accept(); }
  void mouseMoveEvent(QMouseEvent* event) override { event->accept(); }
  void mouseReleaseEvent(QMouseEvent* event) override { event->accept(); }
};

} // namespace MapEditor
