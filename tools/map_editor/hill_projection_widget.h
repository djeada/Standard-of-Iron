#pragma once

#include "terrain_projection_widget.h"

namespace MapEditor {

class HillProjectionWidget : public TerrainProjectionWidget {
  Q_OBJECT

public:
  enum class EditLayer {
    Hill,
    Entrance,
    Nothing,
  };

  explicit HillProjectionWidget(QWidget* parent = nullptr);

  void set_hill_json(const QJsonObject& json) { set_terrain_json(json); }
  void set_edit_layer(EditLayer layer) { set_active_layer(static_cast<int>(layer)); }
  [[nodiscard]] bool is_hill_active() const { return is_active(); }
  [[nodiscard]] QVector<QPoint> hill_cells() const { return body_cells(); }

  [[nodiscard]] QVector<QPair<QString, QColor>> layer_definitions() const override;
  [[nodiscard]] bool body_cells_user_editable() const override { return true; }
  [[nodiscard]] int entrance_layer_index() const override { return 1; }
  [[nodiscard]] int body_layer_index() const override { return 0; }
};

} // namespace MapEditor
