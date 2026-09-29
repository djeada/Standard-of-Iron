#pragma once

#include <QObject>
#include <QtGlobal>

#include "app/orders/order_feedback.h"

namespace Render::GL {
class Camera;
}

namespace App::Controllers {

struct CommandResult {
  bool input_consumed = false;
  bool reset_cursor_to_normal = false;

  App::Core::OrderOutcome order;
};

struct PointerTarget {
  qreal sx = 0.0;
  qreal sy = 0.0;
  int viewport_width = 0;
  int viewport_height = 0;
  Render::GL::Camera* camera = nullptr;
};

} // namespace App::Controllers
