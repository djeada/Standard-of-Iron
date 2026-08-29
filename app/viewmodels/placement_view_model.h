#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "app/core/frame_snapshot.h"

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class PlacementViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool is_placing_formation READ is_placing_formation NOTIFY
                 placing_formation_changed)
  Q_PROPERTY(QVariantMap formation_options READ formation_options NOTIFY
                 formation_options_changed)
  Q_PROPERTY(QStringList formation_intents READ formation_intents NOTIFY
                 formation_options_changed)
  Q_PROPERTY(QVariantMap selected_formation_status READ selected_formation_status NOTIFY
                 formation_options_changed)
  Q_PROPERTY(bool is_placing_construction READ is_placing_construction NOTIFY
                 placing_construction_changed)
  Q_PROPERTY(QString pending_builder_construction_type READ
                 pending_builder_construction_type NOTIFY placing_construction_changed)
  Q_PROPERTY(QString pending_building_type READ pending_building_type NOTIFY
                 placing_construction_changed)
  Q_PROPERTY(bool construction_preview_active READ construction_preview_active NOTIFY
                 construction_preview_active_changed)
  Q_PROPERTY(QString construction_preview_reason READ construction_preview_reason NOTIFY
                 construction_preview_reason_changed)
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
  PlacementViewModel(const App::Core::ClientContext& context,
                     App::Core::ClientHost& host,
                     QObject* parent = nullptr);

  Q_INVOKABLE void on_formation_command();

  void publish_frame();

  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;
  Q_INVOKABLE [[nodiscard]] bool is_placing_formation() const;
  Q_INVOKABLE void on_formation_mouse_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_formation_scroll(float delta);
  Q_INVOKABLE void on_formation_confirm();
  Q_INVOKABLE void on_formation_cancel();
  Q_INVOKABLE void on_formation_drag_begin(qreal sx, qreal sy);
  Q_INVOKABLE void on_formation_drag_update(qreal sx, qreal sy);
  Q_INVOKABLE void on_formation_drag_end();
  Q_INVOKABLE [[nodiscard]] bool is_dragging_formation() const;

  Q_INVOKABLE void set_formation_intent(const QString& intent_id);
  Q_INVOKABLE [[nodiscard]] QString formation_intent() const;
  Q_INVOKABLE [[nodiscard]] QStringList formation_intents() const;
  Q_INVOKABLE [[nodiscard]] QString
  formation_intent_display_name(const QString& intent_id) const;
  Q_INVOKABLE [[nodiscard]] QString
  formation_intent_unavailable_reason(const QString& intent_id) const;
  Q_INVOKABLE [[nodiscard]] QVariantList formation_doctrine_options() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap formation_options() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selected_formation_status() const;
  Q_INVOKABLE void reset_formation_options();
  Q_INVOKABLE void set_formation_frontage_preset(const QString& preset);
  Q_INVOKABLE void set_formation_depth_preset(const QString& preset);
  Q_INVOKABLE void set_formation_spacing_preset(const QString& preset);
  Q_INVOKABLE void set_formation_flank_preference(const QString& preference);
  Q_INVOKABLE void set_formation_ranged_placement(const QString& placement);
  Q_INVOKABLE void set_formation_reserve_rows(int rows);
  Q_INVOKABLE void set_formation_movement_policy(const QString& policy);
  Q_INVOKABLE void set_formation_mixed_policy(const QString& policy);
  Q_INVOKABLE void set_formation_doctrine_override(const QString& doctrine);
  Q_INVOKABLE void set_formation_preserve_order(bool preserve);
  Q_INVOKABLE void adjust_formation_depth(float wheel_delta);

  Q_INVOKABLE [[nodiscard]] bool is_placing_construction() const;
  Q_INVOKABLE [[nodiscard]] QString pending_builder_construction_type() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_active() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_valid() const;
  Q_INVOKABLE [[nodiscard]] QString construction_preview_reason() const;
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
  void formation_options_changed();
  void formation_deployed(int unit_count);
  void placing_construction_changed();
  void construction_preview_active_changed();
  void construction_preview_valid_changed();
  void construction_preview_reason_changed();
  void construction_preview_summary_changed();

private:
  App::Core::Published<App::Core::PlacementReadout> m_readout;

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
};

} // namespace App::ViewModels
