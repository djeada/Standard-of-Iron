#include "film_action_dispatch.h"

#include <QLatin1String>
#include <QQuickWindow>
#include <QStringList>
#include <QVariantMap>
#include <Qt>

#include "app/core/benchmark_action_fixture.h"
#include "app/core/game_engine.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/orders_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "app/viewmodels/production_view_model.h"

namespace App::Core {

namespace {

auto number_list(const QString& argument) -> QList<float> {
  QList<float> values;
  for (const QString& part : argument.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    bool ok = false;
    const float value = part.trimmed().toFloat(&ok);
    if (ok) {
      values.push_back(value);
    }
  }
  return values;
}

auto commander_key(const QString& name) -> int {
  const QString key = name.trimmed().toLower();
  if (key == QLatin1String("w")) {
    return Qt::Key_W;
  }
  if (key == QLatin1String("a")) {
    return Qt::Key_A;
  }
  if (key == QLatin1String("s")) {
    return Qt::Key_S;
  }
  if (key == QLatin1String("d")) {
    return Qt::Key_D;
  }
  if (key == QLatin1String("shift")) {
    return Qt::Key_Shift;
  }
  if (key == QLatin1String("space")) {
    return Qt::Key_Space;
  }
  if (key == QLatin1String("alt")) {
    return Qt::Key_Alt;
  }
  return 0;
}

} // namespace

void apply_benchmark_action(GameEngine* engine,
                            QQuickWindow* window,
                            const BenchmarkAction& action) {
  if (engine == nullptr || window == nullptr) {
    return;
  }
  auto* orders =
      qobject_cast<App::ViewModels::OrdersViewModel*>(engine->orders_view_model());
  auto* production = qobject_cast<App::ViewModels::ProductionViewModel*>(
      engine->production_view_model());
  auto* camera =
      qobject_cast<App::ViewModels::CameraViewModel*>(engine->camera_view_model());
  auto* commander = qobject_cast<App::ViewModels::CommanderViewModel*>(
      engine->commander_view_model());
  auto* placement = qobject_cast<App::ViewModels::PlacementViewModel*>(
      engine->placement_view_model());
  if (orders == nullptr) {
    return;
  }

  const qreal width = window->width();
  const qreal height = window->height();
  qreal sx = action.x * width;
  qreal sy = action.y * height;
  QString name = action.action;
  QList<float> numbers = number_list(action.argument);

  if (name.endsWith(QLatin1String("_world"))) {
    if (camera == nullptr || numbers.size() < 2) {
      return;
    }
    const QVariantMap projected = camera->project_world(numbers[0], 0.0F, numbers[1]);
    if (!projected.value(QStringLiteral("valid")).toBool()) {
      return;
    }
    sx = projected.value(QStringLiteral("x")).toReal();
    sy = projected.value(QStringLiteral("y")).toReal();
    name.chop(6);
    numbers = numbers.mid(2);
  }

  if (name == QLatin1String("select_all")) {
    orders->select_all_troops();
  } else if (name == QLatin1String("select_at")) {
    orders->on_click_select(sx, sy, false);
  } else if (name == QLatin1String("select_id")) {
    if (!numbers.isEmpty()) {
      orders->select_unit_by_id(static_cast<qulonglong>(numbers[0]));
    }
  } else if (name == QLatin1String("select_by_type")) {
    orders->select_by_type(action.argument);
  } else if (name == QLatin1String("deselect")) {
    orders->on_map_clicked(sx, sy);
  } else if (name == QLatin1String("move_to")) {
    orders->on_right_click(sx, sy);
  } else if (name == QLatin1String("attack_at")) {
    orders->attack_at(sx, sy);
  } else if (name == QLatin1String("guard_at")) {
    orders->guard_at(sx, sy);
  } else if (name == QLatin1String("patrol_at")) {
    orders->patrol_at(sx, sy);
  } else if (name == QLatin1String("hover_at")) {
    orders->set_hover_at_screen(sx, sy);
  } else if (name == QLatin1String("stop")) {
    orders->stop();
  } else if (name == QLatin1String("hold")) {
    orders->hold();
  } else if (name == QLatin1String("run")) {
    orders->run();
  } else if (name == QLatin1String("guard")) {
    orders->guard();
  } else if (name == QLatin1String("build_panel")) {
    orders->build();
  } else if (production != nullptr && name == QLatin1String("production_panel")) {
    (void)production->selected_state();
  } else if (production != nullptr && name == QLatin1String("recruit")) {
    production->recruit_near_selected(action.argument);
  } else if (production != nullptr && name == QLatin1String("set_rally")) {
    production->set_rally_at_screen(sx, sy);
  } else if (camera != nullptr && name == QLatin1String("camera_look_at")) {
    if (numbers.size() >= 2) {
      camera->look_at_world(numbers[0], numbers[1]);
    }
  } else if (camera != nullptr && name == QLatin1String("camera_move")) {
    if (numbers.size() >= 2) {
      camera->move(numbers[0], numbers[1]);
    }
  } else if (camera != nullptr && name == QLatin1String("camera_zoom")) {
    if (!numbers.isEmpty()) {
      camera->zoom(numbers[0]);
    }
  } else if (camera != nullptr && name == QLatin1String("camera_orbit")) {
    if (numbers.size() >= 2) {
      camera->orbit(numbers[0], numbers[1]);
    }
  } else if (camera != nullptr && name == QLatin1String("camera_follow")) {
    camera->follow_selection(action.argument.trimmed().toLower() !=
                             QLatin1String("off"));
  } else if (name == QLatin1String("game_speed")) {
    if (!numbers.isEmpty()) {
      engine->set_game_speed(numbers[0]);
    }
  } else if (name == QLatin1String("pause")) {
    engine->set_paused(true);
  } else if (name == QLatin1String("resume")) {
    engine->set_paused(false);
  } else if (placement != nullptr && name == QLatin1String("formation_begin")) {
    placement->on_formation_command();
    placement->on_formation_drag_begin(sx, sy);
  } else if (placement != nullptr && name == QLatin1String("formation_drag")) {
    placement->on_formation_drag_update(sx, sy);
  } else if (placement != nullptr && name == QLatin1String("formation_end")) {
    placement->on_formation_drag_end();
  } else if (placement != nullptr && name == QLatin1String("formation_intent")) {
    placement->set_formation_intent(action.argument);
  } else if (placement != nullptr && name == QLatin1String("build_start")) {

    placement->start_builder_construction(
        action.argument.section(QLatin1Char(','), 0, 0).trimmed());
    placement->on_construction_mouse_move(sx, sy);
  } else if (placement != nullptr && name == QLatin1String("build_hover")) {
    placement->on_construction_mouse_move(sx, sy);
  } else if (placement != nullptr && name == QLatin1String("build_place")) {
    placement->on_construction_mouse_move(sx, sy);
    placement->on_construction_pointer_pressed(sx, sy);
    placement->on_construction_pointer_released(sx, sy);
  } else if (placement != nullptr && name == QLatin1String("build_cancel")) {
    placement->on_construction_cancel();
  } else if (commander != nullptr && name == QLatin1String("commander_enter")) {
    commander->enter_mode();
  } else if (commander != nullptr && name == QLatin1String("commander_exit")) {
    commander->exit_mode();
  } else if (commander != nullptr && name == QLatin1String("commander_aura")) {
    commander->trigger_aura();
  } else if (commander != nullptr && name == QLatin1String("commander_rally_at")) {
    commander->start_flag_rally();
    commander->confirm_flag_rally(sx, sy);
  } else if (commander != nullptr && name == QLatin1String("commander_key_down")) {
    if (const int key = commander_key(action.argument); key != 0) {
      commander->key_down(key);
    }
  } else if (commander != nullptr && name == QLatin1String("commander_key_up")) {
    if (const int key = commander_key(action.argument); key != 0) {
      commander->key_up(key);
    }
  } else if (commander != nullptr && name == QLatin1String("commander_attack")) {
    commander->primary_action_down();
    commander->primary_action_up();
  } else if (commander != nullptr && name == QLatin1String("commander_heavy")) {
    commander->heavy_action();
  } else if (commander != nullptr && name == QLatin1String("commander_special")) {
    commander->special_action();
  } else if (commander != nullptr && name == QLatin1String("commander_vanguard")) {
    commander->vanguard_rush();
  } else if (commander != nullptr && name == QLatin1String("commander_dodge")) {
    commander->dodge();
  } else if (commander != nullptr && name == QLatin1String("commander_look")) {
    if (numbers.size() >= 2) {
      commander->mouse_move(numbers[0], numbers[1]);
    }
  } else if (commander != nullptr && name == QLatin1String("commander_lock_on")) {
    commander->cycle_lock_on();
  }
}

} // namespace App::Core
