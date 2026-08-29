#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "app/orders/order_issuer.h"
#include "game/formation/army_formation_planner.h"
#include "game/formation/army_formation_types.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {
class SelectionSystem;
}

namespace App::Controllers {

struct CommandResult;

class ArmyFormationController : public QObject {
  Q_OBJECT
public:
  ArmyFormationController(Engine::Core::World* world,
                          Game::Systems::SelectionSystem* selection_system,
                          App::Orders::OrderIssuer::FeedbackSink feedback,
                          QObject* parent = nullptr);

  auto on_formation_command() -> CommandResult;
  void reset_transient_state();

  [[nodiscard]] bool is_placing_formation() const { return m_is_placing_formation; }
  [[nodiscard]] bool begin_move_placement_at_position(const QVector3D& position);
  void update_formation_placement(const QVector3D& position);
  void update_formation_rotation(float angle_degrees);
  void confirm_formation_placement();
  void cancel_formation_placement();
  [[nodiscard]] QVector3D get_formation_placement_position() const {
    return m_formation_placement_position;
  }

  [[nodiscard]] float get_formation_facing_degrees() const {
    return m_formation_facing_degrees;
  }

  [[nodiscard]] float get_formation_aim_distance() const {
    return m_formation_aim_distance;
  }

  [[nodiscard]] bool is_right_drag_placement() const {
    return m_is_placing_formation && m_is_right_drag_formation;
  }

  [[nodiscard]] bool is_single_unit_placement() const {
    return m_is_placing_formation && m_formation_units.size() == 1;
  }

  void aim_formation_at(const QVector3D& aim_point);

  Q_INVOKABLE void set_formation_intent(const QString& intent_id);
  Q_INVOKABLE [[nodiscard]] QString formation_intent() const;
  Q_INVOKABLE [[nodiscard]] QStringList formation_intents() const;
  Q_INVOKABLE [[nodiscard]] QString
  formation_intent_display_name(const QString& intent_id) const;
  Q_INVOKABLE [[nodiscard]] QString
  formation_intent_unavailable_reason(const QString& intent_id) const;
  Q_INVOKABLE [[nodiscard]] QString formation_doctrine() const;
  Q_INVOKABLE [[nodiscard]] QString formation_doctrine_display_name() const;
  Q_INVOKABLE [[nodiscard]] QVariantList formation_doctrine_options() const;

  void begin_formation_drag(const QVector3D& start);
  void update_formation_drag(const QVector3D& current);
  void end_formation_drag();
  [[nodiscard]] bool is_dragging_formation() const { return m_formation_drag_active; }
  [[nodiscard]] QVector3D formation_drag_start() const {
    return m_formation_drag_start;
  }
  Q_INVOKABLE void adjust_formation_depth(float wheel_delta);
  Q_INVOKABLE void set_formation_preserve_order(bool preserve);

  Q_INVOKABLE void set_formation_frontage_preset(const QString& preset);
  Q_INVOKABLE void set_formation_depth_preset(const QString& preset);
  Q_INVOKABLE void set_formation_spacing_preset(const QString& preset);
  Q_INVOKABLE void set_formation_flank_preference(const QString& preference);
  Q_INVOKABLE void set_formation_ranged_placement(const QString& placement);
  Q_INVOKABLE void set_formation_reserve_rows(int rows);
  Q_INVOKABLE void set_formation_movement_policy(const QString& policy);
  Q_INVOKABLE void set_formation_mixed_policy(const QString& policy);
  Q_INVOKABLE void set_formation_doctrine_override(const QString& doctrine);
  Q_INVOKABLE [[nodiscard]] QVariantMap formation_options() const;
  Q_INVOKABLE void reset_formation_options();

  Q_INVOKABLE [[nodiscard]] QVariantMap selected_formation_status() const;

  [[nodiscard]] auto
  formation_preview() const -> const Game::Formation::ArmyFormationPlan& {
    return m_formation_preview;
  }
  [[nodiscard]] QString formation_preview_warning() const;
  [[nodiscard]] float formation_frontage() const { return m_formation_frontage; }
  void refresh_formation_preview();

  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;

signals:
  void formation_mode_changed(bool active);
  void formation_placement_started();
  void formation_placement_updated(QVector3D position, float angle);
  void formation_placement_ended();
  void formation_deployed(int unit_count);
  void formation_placement_rejected(const QString& reason);
  void formation_preview_changed();

private:
  [[nodiscard]] auto auto_formation_facing() const -> float;
  [[nodiscard]] auto formation_unit_label() const -> QString;
  void set_formation_facing(float degrees, bool explicit_choice);
  void follow_auto_formation_facing();
  void reset_formation_facing();
  void apply_formation_option_change();
  void invalidate_formation_layout();

  Engine::Core::World* m_world = nullptr;
  Game::Systems::SelectionSystem* m_selection_system = nullptr;
  App::Orders::OrderIssuer m_orders;

  bool m_is_placing_formation = false;
  bool m_is_right_drag_formation = false;
  QVector3D m_formation_placement_position;

  float m_formation_facing_degrees = 0.0F;
  bool m_formation_facing_explicit = false;
  float m_formation_aim_distance = 0.0F;
  std::vector<Engine::Core::EntityID> m_formation_units;

  Game::Formation::ArmyFormationIntent m_formation_intent =
      Game::Formation::ArmyFormationIntent::FactionDefault;
  Game::Formation::ArmyFormationOptions m_formation_options;
  float m_formation_frontage = 0.0F;

  bool m_formation_drag_active = false;
  QVector3D m_formation_drag_start;
  Game::Formation::ArmyFormationPlan m_formation_preview;
  Game::Formation::FormationDoctrineId m_formation_doctrine_override;

  std::vector<Game::Formation::ArmyFormationMember> m_formation_members;
  Game::Formation::ArmyFormationLayout m_formation_layout;
  bool m_formation_layout_valid = false;
  bool m_formation_preview_dirty = true;
  QVector3D m_formation_previewed_anchor;
  float m_formation_previewed_facing = 0.0F;
};

} // namespace App::Controllers
