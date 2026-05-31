#include "mountain_projection_widget.h"

namespace MapEditor {

MountainProjectionWidget::MountainProjectionWidget(QWidget* parent)
    : TerrainProjectionWidget(parent) {
  set_active_layer(body_layer_index());
}

} // namespace MapEditor
