#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantMap>

#include "../models/cursor_mode.h"

struct ViewportState;
class InputCommandHandler;
class ProductionManager;

namespace App::Controllers {
class CommandController;
}

namespace App::ViewModels {

class PlacementHost {
public:
  PlacementHost() = default;
  PlacementHost(const PlacementHost&) = delete;
  PlacementHost(PlacementHost&&) = delete;
  auto operator=(const PlacementHost&) -> PlacementHost& = delete;
  auto operator=(PlacementHost&&) -> PlacementHost& = delete;
  virtual ~PlacementHost() = default;

  virtual void ensure_initialized() = 0;
  [[nodiscard]] virtual auto map_input_to_viewport(qreal sx,
                                                   qreal sy) const -> QPointF = 0;
  [[nodiscard]] virtual auto viewport() const -> const ViewportState& = 0;
  [[nodiscard]] virtual auto local_owner_id() const -> int = 0;
  virtual void set_cursor_mode(CursorMode mode) = 0;

  [[nodiscard]] virtual auto input_handler() const -> InputCommandHandler* = 0;
  [[nodiscard]] virtual auto
  command_controller() const -> App::Controllers::CommandController* = 0;
  [[nodiscard]] virtual auto production_manager() const -> ProductionManager* = 0;
};

class PlacementViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool is_placing_formation READ is_placing_formation NOTIFY
                 placing_formation_changed)
  Q_PROPERTY(bool is_placing_construction READ is_placing_construction NOTIFY
                 placing_construction_changed)
  Q_PROPERTY(QString pending_builder_construction_type READ
                 pending_builder_construction_type NOTIFY placing_construction_changed)
  Q_PROPERTY(QString pending_building_type READ pending_building_type NOTIFY
                 placing_construction_changed)
  Q_PROPERTY(bool construction_preview_active READ construction_preview_active NOTIFY
                 construction_preview_active_changed)
  Q_PROPERTY(bool construction_preview_valid READ construction_preview_valid NOTIFY
                 construction_preview_valid_changed)
  Q_PROPERTY(
      int construction_preview_segment_count READ construction_preview_segment_count
          NOTIFY construction_preview_summary_changed)
  Q_PROPERTY(int construction_preview_valid_segment_count READ
                 construction_preview_valid_segment_count NOTIFY
                     construction_preview_summary_changed)
  Q_PROPERTY(int construction_preview_total_cost READ construction_preview_total_cost
                 NOTIFY construction_preview_summary_changed)

public:
  explicit PlacementViewModel(PlacementHost& host, QObject* parent = nullptr);

  Q_INVOKABLE void on_formation_command();
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;
  Q_INVOKABLE [[nodiscard]] bool is_placing_formation() const;
  Q_INVOKABLE void on_formation_mouse_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_formation_scroll(float delta);
  Q_INVOKABLE void on_formation_confirm();
  Q_INVOKABLE void on_formation_cancel();

  Q_INVOKABLE [[nodiscard]] bool is_placing_construction() const;
  Q_INVOKABLE [[nodiscard]] QString pending_builder_construction_type() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_active() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_valid() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_rotatable() const;
  Q_INVOKABLE [[nodiscard]] int construction_preview_segment_count() const;
  Q_INVOKABLE [[nodiscard]] int construction_preview_valid_segment_count() const;
  Q_INVOKABLE [[nodiscard]] int construction_preview_total_cost() const;
  Q_INVOKABLE void on_construction_mouse_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_construction_pointer_pressed(qreal sx, qreal sy);
  Q_INVOKABLE void on_construction_pointer_released(qreal sx, qreal sy);
  Q_INVOKABLE void on_construction_scroll(float delta);
  Q_INVOKABLE void on_construction_confirm();
  Q_INVOKABLE void on_construction_cancel();

  Q_INVOKABLE void start_builder_construction(const QString& item_type);
  Q_INVOKABLE [[nodiscard]] QVariantMap
  get_construction_info(const QString& item_type) const;

  Q_INVOKABLE void start_building_placement(const QString& building_type);
  Q_INVOKABLE void place_building_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_building_placement();
  Q_INVOKABLE [[nodiscard]] QString pending_building_type() const;

signals:
  void placing_formation_changed();
  void placing_construction_changed();
  void construction_preview_active_changed();
  void construction_preview_valid_changed();
  void construction_preview_summary_changed();

private:
  PlacementHost& m_host;
};

} // namespace App::ViewModels
