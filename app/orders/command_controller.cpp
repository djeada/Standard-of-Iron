#include "app/orders/command_controller.h"

#include <utility>

#include "app/orders/order_cues.h"
#include "scene/camera.h"

namespace App::Controllers {

CommandController::CommandController(Engine::Core::World* world,
                                     Game::Session::SelectionService* selection_system,
                                     Game::Systems::PickingService* picking_service,
                                     QObject* parent)
    : QObject(parent)
    , m_picking_service(picking_service)
    , m_orders(world,
               [this](const App::Core::OrderOutcome& outcome) {
                 emit order_feedback(outcome);
               })
    , m_formation(world,
                  selection_system,
                  [this](const App::Core::OrderOutcome& outcome) {
                    emit order_feedback(outcome);
                  })
    , m_modes(world, selection_system, m_orders)
    , m_movement(world, selection_system, picking_service, m_orders, m_modes)
    , m_workers(world, selection_system, picking_service, m_orders)
    , m_roster(world, selection_system, m_orders)
    , m_patrol(world, selection_system, picking_service, m_orders) {
  connect(&m_formation,
          &ArmyFormationController::formation_mode_changed,
          this,
          &CommandController::formation_mode_changed);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_started,
          this,
          &CommandController::formation_placement_started);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_updated,
          this,
          &CommandController::formation_placement_updated);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_ended,
          this,
          &CommandController::formation_placement_ended);
  connect(&m_formation,
          &ArmyFormationController::formation_deployed,
          this,
          &CommandController::formation_deployed);
  connect(&m_formation,
          &ArmyFormationController::formation_placement_rejected,
          this,
          &CommandController::formation_placement_rejected);
  connect(&m_formation,
          &ArmyFormationController::formation_preview_changed,
          this,
          &CommandController::formation_preview_changed);

  connect(&m_modes,
          &ModeToggleCommands::hold_mode_changed,
          this,
          &CommandController::hold_mode_changed);
  connect(&m_modes,
          &ModeToggleCommands::guard_mode_changed,
          this,
          &CommandController::guard_mode_changed);
  connect(&m_modes,
          &ModeToggleCommands::gate_mode_changed,
          this,
          &CommandController::gate_mode_changed);
  connect(&m_modes,
          &ModeToggleCommands::run_mode_changed,
          this,
          &CommandController::run_mode_changed);
  connect(&m_modes,
          &ModeToggleCommands::formation_mode_changed,
          this,
          &CommandController::formation_mode_changed);
  connect(&m_workers,
          &WorkerCommands::auto_gather_changed,
          this,
          &CommandController::auto_gather_changed);
}

auto CommandController::target_at(qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  void* camera) -> PointerTarget {
  return {.sx = sx,
          .sy = sy,
          .viewport_width = viewport_width,
          .viewport_height = viewport_height,
          .camera = static_cast<Render::GL::Camera*>(camera)};
}

auto CommandController::on_attack_click(qreal sx,
                                        qreal sy,
                                        int viewport_width,
                                        int viewport_height,
                                        void* camera,
                                        int local_owner_id) -> CommandResult {
  return m_movement.on_attack_click(
      target_at(sx, sy, viewport_width, viewport_height, camera), local_owner_id);
}

auto CommandController::on_attack_press(qreal sx,
                                        qreal sy,
                                        int viewport_width,
                                        int viewport_height,
                                        void* camera,
                                        int local_owner_id) -> CommandResult {
  return m_movement.on_attack_press(
      target_at(sx, sy, viewport_width, viewport_height, camera), local_owner_id);
}

auto CommandController::on_move_or_attack_click(qreal sx,
                                                qreal sy,
                                                int viewport_width,
                                                int viewport_height,
                                                void* camera,
                                                int local_owner_id) -> CommandResult {
  return m_movement.on_move_or_attack_click(
      target_at(sx, sy, viewport_width, viewport_height, camera), local_owner_id);
}

auto CommandController::on_minimap_move(const QVector3D& world_target,
                                        int local_owner_id) -> CommandResult {
  return m_movement.on_minimap_move(world_target, local_owner_id);
}

auto CommandController::refuse_unreachable_move(const QVector3D& destination)
    -> App::Core::OrderOutcome {
  return m_movement.refuse_unreachable_move(destination);
}

auto CommandController::on_stop_command() -> CommandResult {
  return m_modes.on_stop_command();
}

auto CommandController::on_hold_command() -> CommandResult {
  return m_modes.on_hold_command();
}

auto CommandController::on_gate_command() -> CommandResult {
  return m_modes.on_gate_command();
}

auto CommandController::on_roll_stones_command() -> CommandResult {
  return m_modes.on_roll_stones_command();
}

auto CommandController::on_guard_command() -> CommandResult {
  return m_modes.on_guard_command();
}

auto CommandController::on_run_command() -> CommandResult {
  return m_modes.on_run_command();
}

auto CommandController::on_guard_click(qreal sx,
                                       qreal sy,
                                       int viewport_width,
                                       int viewport_height,
                                       void* camera) -> CommandResult {
  if (m_picking_service == nullptr) {
    CommandResult refused;
    refused.reset_cursor_to_normal = true;
    return refused;
  }
  return m_modes.on_guard_click(
      target_at(sx, sy, viewport_width, viewport_height, camera));
}

void CommandController::enable_run_mode_for_selected() {
  m_modes.enable_run_mode_for_selected();
}

void CommandController::disable_run_mode_for_selected() {
  m_modes.disable_run_mode_for_selected();
}

auto CommandController::any_selected_in_hold_mode() const -> bool {
  return m_modes.any_selected_in_hold_mode();
}

auto CommandController::any_selected_in_guard_mode() const -> bool {
  return m_modes.any_selected_in_guard_mode();
}

auto CommandController::any_selected_in_run_mode() const -> bool {
  return m_modes.any_selected_in_run_mode();
}

auto CommandController::on_auto_gather_command(const QString& priority_product_type)
    -> CommandResult {
  return m_workers.on_auto_gather_command(priority_product_type);
}

auto CommandController::set_auto_gather(
    bool active, const QString& priority_product_type) -> CommandResult {
  return m_workers.set_auto_gather(active, priority_product_type);
}

auto CommandController::start_food_harvest(Engine::Core::EntityID target,
                                           const QString& product_type,
                                           int local_owner_id) -> CommandResult {
  return m_workers.start_food_harvest(target, product_type, local_owner_id);
}

auto CommandController::on_civilian_delivery_click(qreal sx,
                                                   qreal sy,
                                                   int viewport_width,
                                                   int viewport_height,
                                                   void* camera,
                                                   int local_owner_id)
    -> CommandResult {
  return m_workers.on_civilian_delivery_click(
      target_at(sx, sy, viewport_width, viewport_height, camera), local_owner_id);
}

auto CommandController::on_builder_repair_click(qreal sx,
                                                qreal sy,
                                                int viewport_width,
                                                int viewport_height,
                                                void* camera,
                                                int local_owner_id) -> CommandResult {
  return m_workers.on_builder_repair_click(
      target_at(sx, sy, viewport_width, viewport_height, camera), local_owner_id);
}

auto CommandController::on_builder_dismantle_click(qreal sx,
                                                   qreal sy,
                                                   int viewport_width,
                                                   int viewport_height,
                                                   void* camera,
                                                   int local_owner_id)
    -> CommandResult {
  return m_workers.on_builder_dismantle_click(
      target_at(sx, sy, viewport_width, viewport_height, camera), local_owner_id);
}

auto CommandController::divide_selected_squads() -> CommandResult {
  return m_roster.divide_selected_squads();
}

auto CommandController::merge_selected_squads() -> CommandResult {
  return m_roster.merge_selected_squads();
}

void CommandController::recruit_near_selected(const QString& unit_type,
                                              int local_owner_id) {
  m_roster.recruit_near_selected(unit_type, local_owner_id);
}

auto CommandController::on_patrol_click(qreal sx,
                                        qreal sy,
                                        int viewport_width,
                                        int viewport_height,
                                        void* camera) -> CommandResult {
  return m_patrol.on_patrol_click(
      target_at(sx, sy, viewport_width, viewport_height, camera));
}

void CommandController::reset_transient_state() {
  m_patrol.reset();
  m_formation.reset_transient_state();
}

auto CommandController::selection_mounts(
    Engine::Core::World& world, const std::vector<Engine::Core::EntityID>& units)
    -> Game::Audio::Cue::SelectionMounts {
  return App::Orders::selection_mounts(world, units);
}

auto CommandController::charge_cue(const Game::Audio::Cue::SelectionMounts& mounts)
    -> const char* {
  return App::Orders::charge_cue(mounts);
}

auto CommandController::move_order_cue(const Game::Audio::Cue::SelectionMounts& mounts)
    -> const char* {
  return App::Orders::move_order_cue(mounts);
}

} // namespace App::Controllers
