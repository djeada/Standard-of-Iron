#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantMap>

#include "app/core/frame_snapshot.h"

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class CommanderViewModel;
class PlacementViewModel;

class OrdersViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(
      QVariantMap context_intent READ context_intent NOTIFY context_intent_changed)

public:
  OrdersViewModel(const App::Core::ClientContext& context,
                  App::Core::ClientHost& host,
                  PlacementViewModel& placement,
                  CommanderViewModel& commander,
                  QObject* parent = nullptr);

  Q_INVOKABLE void on_click_select(qreal sx, qreal sy, bool additive = false);
  Q_INVOKABLE void
  on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive = false);
  Q_INVOKABLE void select_all_troops();
  Q_INVOKABLE void select_unit_by_id(qulonglong unit_id);
  Q_INVOKABLE void select_by_type(const QString& unit_type);
  Q_INVOKABLE void set_hover_at_screen(qreal sx, qreal sy);

  Q_INVOKABLE void on_map_clicked(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_double_click(qreal sx, qreal sy);
  Q_INVOKABLE [[nodiscard]] bool on_right_press(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_release(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_drag_orient(qreal sx, qreal sy);

  Q_INVOKABLE void attack_at(qreal sx, qreal sy);
  Q_INVOKABLE void guard_at(qreal sx, qreal sy);
  Q_INVOKABLE void patrol_at(qreal sx, qreal sy);
  Q_INVOKABLE void deliver_civilians_at(qreal sx, qreal sy);
  Q_INVOKABLE void stop();
  Q_INVOKABLE void hold();
  Q_INVOKABLE void gate();
  Q_INVOKABLE void guard();
  Q_INVOKABLE void run();
  Q_INVOKABLE void heal();
  Q_INVOKABLE void build();

  Q_INVOKABLE [[nodiscard]] QVariantMap action_states() const;
  Q_INVOKABLE [[nodiscard]] QString command_mode() const;
  Q_INVOKABLE [[nodiscard]] QString toggle_state(const QString& mode) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap mode_availability() const;

  // Called by the engine once per frame while it holds the frame lock.
  void publish_frame();

  [[nodiscard]] auto context_intent() const -> QVariantMap { return m_context_intent; }
  void refresh_context_intent(qreal sx, qreal sy);
  void clear_context_intent();

signals:
  void context_intent_changed();

public:
  void reset_gesture() { m_right_mouse.reset(); }

private:
  [[nodiscard]] auto action_enabled(const QString& action_id) const -> bool;

  App::Core::Published<App::Core::OrdersReadout> m_readout;

  struct RightMouseGesture {
    QPointF press_position;
    bool active = false;
    bool dragged = false;
    bool suppress_release_click = false;
    bool double_click_handled = false;
    bool placement_was_active_on_press = false;
    bool started_formation_placement = false;

    void reset() { *this = RightMouseGesture{}; }
  };

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  PlacementViewModel& m_placement;
  CommanderViewModel& m_commander;
  RightMouseGesture m_right_mouse;
  QVariantMap m_context_intent;
};

} // namespace App::ViewModels
