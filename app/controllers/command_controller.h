#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../../game/formation/army_formation_planner.h"
#include "../../game/formation/army_formation_types.h"

namespace Engine::Core {
class World;
class Entity;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {
class SelectionSystem;
class PickingService;
} // namespace Game::Systems

namespace App::Controllers {

struct CommandResult {
  bool input_consumed = false;
  bool reset_cursor_to_normal = false;
};

class CommandController : public QObject {
  Q_OBJECT
public:
  CommandController(Engine::Core::World* world,
                    Game::Systems::SelectionSystem* selection_system,
                    Game::Systems::PickingService* picking_service,
                    QObject* parent = nullptr);

  auto on_attack_click(qreal sx,
                       qreal sy,
                       int viewport_width,
                       int viewport_height,
                       void* camera) -> CommandResult;
  auto on_stop_command() -> CommandResult;
  auto on_hold_command() -> CommandResult;
  auto on_gate_command() -> CommandResult;
  auto on_guard_command() -> CommandResult;
  auto on_formation_command() -> CommandResult;
  auto on_run_command() -> CommandResult;
  void enable_run_mode_for_selected();
  void disable_run_mode_for_selected();
  auto on_guard_click(qreal sx,
                      qreal sy,
                      int viewport_width,
                      int viewport_height,
                      void* camera) -> CommandResult;
  auto on_civilian_delivery_click(qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  void* camera,
                                  int local_owner_id) -> CommandResult;
  auto on_builder_repair_click(qreal sx,
                               qreal sy,
                               int viewport_width,
                               int viewport_height,
                               void* camera,
                               int local_owner_id) -> CommandResult;
  auto on_patrol_click(qreal sx,
                       qreal sy,
                       int viewport_width,
                       int viewport_height,
                       void* camera) -> CommandResult;
  auto set_rally_at_screen(qreal sx,
                           qreal sy,
                           int viewport_width,
                           int viewport_height,
                           void* camera,
                           int local_owner_id) -> CommandResult;
  void recruit_near_selected(const QString& unit_type, int local_owner_id);

  [[nodiscard]] bool has_patrol_first_waypoint() const {
    return m_has_patrol_first_waypoint;
  }
  [[nodiscard]] QVector3D get_patrol_first_waypoint() const {
    return m_patrol_first_waypoint;
  }
  void clear_patrol_first_waypoint() { m_has_patrol_first_waypoint = false; }
  void reset_transient_state();

  [[nodiscard]] bool is_placing_formation() const { return m_is_placing_formation; }
  void begin_move_placement_at_position(const QVector3D& position);
  void update_formation_placement(const QVector3D& position);
  void update_formation_rotation(float angle_degrees);
  void confirm_formation_placement();
  void cancel_formation_placement();
  [[nodiscard]] QVector3D get_formation_placement_position() const {
    return m_formation_placement_position;
  }
  [[nodiscard]] float get_formation_placement_angle() const {
    return m_formation_placement_angle;
  }

  Q_INVOKABLE void set_formation_intent(const QString& intent_id);
  Q_INVOKABLE [[nodiscard]] QString formation_intent() const;
  Q_INVOKABLE [[nodiscard]] QStringList available_formation_intents() const;
  Q_INVOKABLE [[nodiscard]] QString
  formation_intent_display_name(const QString& intent_id) const;
  Q_INVOKABLE [[nodiscard]] QString
  formation_intent_unavailable_reason(const QString& intent_id) const;
  Q_INVOKABLE [[nodiscard]] QString formation_doctrine() const;
  Q_INVOKABLE [[nodiscard]] QString formation_doctrine_display_name() const;

  void begin_formation_drag(const QVector3D& start);
  void update_formation_drag(const QVector3D& current);
  void end_formation_drag();
  [[nodiscard]] bool is_dragging_formation() const { return m_formation_drag_active; }
  [[nodiscard]] QVector3D formation_drag_start() const {
    return m_formation_drag_start;
  }
  Q_INVOKABLE void adjust_formation_depth(float wheel_delta);
  Q_INVOKABLE void mirror_formation_flank();
  Q_INVOKABLE void set_formation_preserve_order(bool preserve);
  Q_INVOKABLE void set_formation_tight_spacing(bool tight);

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

  Q_INVOKABLE [[nodiscard]] bool any_selected_in_hold_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_guard_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_run_mode() const;

signals:
  void attack_target_selected();
  void troop_limit_reached();
  void insufficient_manpower();
  void insufficient_resources(const QString& message);
  void hold_mode_changed(bool active);
  void gate_mode_changed(const QString& mode);
  void guard_mode_changed(bool active);
  void formation_mode_changed(bool active);
  void run_mode_changed(bool active);
  void formation_placement_started();
  void formation_placement_updated(QVector3D position, float angle);
  void formation_placement_ended();
  void formation_placement_rejected(const QString& reason);
  void formation_preview_changed();

private:
  Engine::Core::World* m_world;
  Game::Systems::SelectionSystem* m_selection_system;
  Game::Systems::PickingService* m_picking_service;

  bool m_has_patrol_first_waypoint = false;
  QVector3D m_patrol_first_waypoint;

  bool m_is_placing_formation = false;
  bool m_is_right_drag_formation = false;
  QVector3D m_formation_placement_position;
  float m_formation_placement_angle = 0.0F;
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

  void apply_formation_option_change();
  void invalidate_formation_layout();

  static void reset_movement(Engine::Core::Entity* entity);
};

} // namespace App::Controllers
