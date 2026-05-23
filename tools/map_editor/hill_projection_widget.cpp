#include "hill_projection_widget.h"

namespace MapEditor {

namespace {

constexpr QColor k_hill_color(153, 121, 76, 95);
constexpr QColor k_entrance_color(31, 139, 245, 210);

} // namespace

HillProjectionWidget::HillProjectionWidget(QWidget* parent)
    : TerrainProjectionWidget(parent) {
  set_active_layer(static_cast<int>(EditLayer::Entrance));
}

QVector<QPair<QString, QColor>> HillProjectionWidget::layer_definitions() const {
  return {
      {QStringLiteral("Hill"), k_hill_color},
      {QStringLiteral("Entrance"), k_entrance_color},
      {QStringLiteral("Nothing"), QColor()},
  };
}

} // namespace MapEditor
