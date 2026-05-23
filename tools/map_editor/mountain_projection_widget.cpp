#include "mountain_projection_widget.h"

namespace MapEditor {

namespace {

constexpr QColor k_entrance_color(31, 139, 245, 210);

} // namespace

MountainProjectionWidget::MountainProjectionWidget(QWidget* parent)
    : TerrainProjectionWidget(parent) {
  set_active_layer(0); // Entrance is layer 0
}

QVector<QPair<QString, QColor>> MountainProjectionWidget::layer_definitions() const {
  return {
      {QStringLiteral("Entrance"), k_entrance_color},
      {QStringLiteral("Nothing"), QColor()},
  };
}

} // namespace MapEditor
