#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include "app/core/published.h"

namespace App::Core {

struct CameraProjection {
  QMatrix4x4 view_projection;
  int viewport_width = 0;
  int viewport_height = 0;
  float distance = 0.0F;

  [[nodiscard]] auto project(const QVector3D& world, QPointF& out_screen) const -> bool;
};

struct SelectionReadout {
  QSet<QString> selected_types;
  QVariantMap barracks;
  QVariantMap home;
  QVariantMap temple;
  QVariantMap builder;
  QVariantMap marketplace;
  QVariantMap farm;
};

struct PlacementReadout {
  bool placing_formation = false;
  bool dragging_formation = false;
  bool any_selected_in_formation_mode = false;
  QString formation_intent;
  QStringList formation_intents;
  QVariantList formation_doctrine_options;
  QVariantMap formation_options;
  QVariantMap selected_formation_status;

  bool placing_construction = false;
  bool construction_preview_active = false;
  bool construction_preview_valid = false;
  bool construction_preview_rotatable = false;
  int construction_preview_segment_count = 0;
  int construction_preview_valid_segment_count = 0;
  int construction_preview_total_cost = 0;
  QString pending_builder_construction_type;
  QString pending_building_type;
};

struct OrdersReadout {
  QString command_mode = QStringLiteral("normal");
  bool has_commandable_selection = false;
};

} // namespace App::Core
