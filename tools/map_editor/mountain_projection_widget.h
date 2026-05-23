#pragma once

#include "terrain_projection_widget.h"

namespace MapEditor {

class MountainProjectionWidget : public TerrainProjectionWidget {
  Q_OBJECT

public:
  explicit MountainProjectionWidget(QWidget* parent = nullptr);

  [[nodiscard]] QVector<QPair<QString, QColor>> layer_definitions() const override;
  [[nodiscard]] bool body_cells_user_editable() const override { return false; }
  [[nodiscard]] int entrance_layer_index() const override { return 0; }
  [[nodiscard]] int body_layer_index() const override { return -1; }
};

} // namespace MapEditor
