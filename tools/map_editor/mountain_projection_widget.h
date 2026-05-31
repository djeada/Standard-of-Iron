#pragma once

#include "terrain_projection_widget.h"

namespace MapEditor {

class MountainProjectionWidget : public TerrainProjectionWidget {
  Q_OBJECT

public:
  enum class EditLayer {
    Mountain,
    Nothing,
  };

  explicit MountainProjectionWidget(QWidget* parent = nullptr);

  [[nodiscard]] QVector<QPair<QString, QColor>> layer_definitions() const override {
    return {qMakePair(QStringLiteral("Mountain"), QColor(120, 120, 120)),
            qMakePair(QStringLiteral("Nothing"), QColor(0, 0, 0))};
  }
  [[nodiscard]] bool body_cells_user_editable() const override { return true; }
  [[nodiscard]] int entrance_layer_index() const override { return -1; }
  [[nodiscard]] int body_layer_index() const override { return 0; }
};

} // namespace MapEditor
