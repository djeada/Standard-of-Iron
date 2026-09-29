#include "app/orders/army_formation_controller.h"

#include <QCoreApplication>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "app/orders/command_result.h"
#include "app/orders/formation_readout.h"
#include "app/orders/local_command.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/formation/army_formation_service.h"
#include "game/formation/formation_doctrine.h"
#include "game/game_config.h"
#include "game/session/selection_service.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"

namespace App::Controllers {

namespace {

auto troop_units(Engine::Core::World& world,
                 const std::vector<Engine::Core::EntityID>& selected)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> troops;
  for (auto id : selected) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit != nullptr && Game::Units::is_troop_spawn(unit->spawn_type)) {
      troops.push_back(id);
    }
  }
  return troops;
}

auto centroid_of(Engine::Core::World& world,
                 const std::vector<Engine::Core::EntityID>& units)
    -> std::optional<QVector3D> {
  QVector3D sum;
  int counted = 0;
  for (auto id : units) {
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
    if (transform == nullptr) {
      continue;
    }
    sum +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    counted++;
  }
  if (counted == 0) {
    return std::nullopt;
  }
  return sum / static_cast<float>(counted);
}

} // namespace

ArmyFormationController::ArmyFormationController(
    Engine::Core::World* world,
    Game::Session::SelectionService* selection_system,
    App::Orders::OrderIssuer::FeedbackSink feedback,
    QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_selection_system(selection_system)
    , m_orders(world, std::move(feedback)) {
}

void ArmyFormationController::end_formation_placement(FormationTeardown teardown) {
  const bool was_right_drag = m_placement.right_drag();

  if (m_world != nullptr && !m_placement.units().empty()) {
    if (teardown == FormationTeardown::Cancel) {
      App::Orders::submit_local_command(
          m_world, Game::Command::ReleaseFormation{.units = m_placement.units()});
    } else {
      App::Orders::submit_local_command(
          m_world,
          Game::Command::SetFormationMode{.units = m_placement.units(),
                                          .active = false});
    }
  }

  m_placement.end();
  m_preview.clear();

  emit formation_preview_changed();
  emit formation_placement_ended();
  if (teardown == FormationTeardown::Reset || !was_right_drag) {
    emit formation_mode_changed(false);
  }
}

void ArmyFormationController::reset_transient_state() {
  end_formation_placement(FormationTeardown::Reset);
}

void ArmyFormationController::announce_placement_update() {
  emit formation_placement_updated(m_placement.position(),
                                   m_placement.facing_degrees());
}

void ArmyFormationController::start_placement(std::vector<Engine::Core::EntityID> units,
                                              const QVector3D& position,
                                              bool right_drag) {
  m_placement.begin(std::move(units), position, right_drag);
  reset_formation_facing();
  m_preview.clear();
  refresh_formation_preview();

  emit formation_placement_started();
  announce_placement_update();
}

auto ArmyFormationController::on_formation_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  if (m_placement.placing()) {
    cancel_formation_placement();
    result.input_consumed = true;
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  auto troops = troop_units(*m_world, selected);
  if (troops.empty()) {
    return result;
  }

  App::Orders::submit_local_command(
      m_world,
      Game::Command::SetFormationMode{.units = {selected.begin(), selected.end()},
                                      .active = true});

  if (const auto center = centroid_of(*m_world, troops); center.has_value()) {
    start_placement(std::move(troops), *center, false);
  } else {
    m_placement.set_units(std::move(troops));
  }

  result.input_consumed = true;
  return result;
}

bool ArmyFormationController::any_selected_in_formation_mode() const {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  for (auto id : m_selection_system->get_selected_units()) {
    const auto* formation_mode =
        m_world->try_get<Engine::Core::FormationModeComponent>(id);
    if ((formation_mode != nullptr) && formation_mode->active) {
      return true;
    }
  }
  return false;
}

auto ArmyFormationController::auto_formation_facing() const -> float {
  if ((m_world == nullptr) || m_placement.units().empty()) {
    return 0.0F;
  }
  return Game::Formation::ArmyFormationService::auto_facing(
      *m_world, m_placement.units(), m_placement.position());
}

void ArmyFormationController::follow_auto_formation_facing() {
  m_placement.follow_auto_facing(auto_formation_facing());
}

void ArmyFormationController::reset_formation_facing() {
  m_placement.clear_facing_choice();
  follow_auto_formation_facing();
}

void ArmyFormationController::update_formation_placement(const QVector3D& position) {
  if (!m_placement.placing()) {
    return;
  }
  m_placement.move_to(position);
  follow_auto_formation_facing();
  refresh_formation_preview();
  announce_placement_update();
}

void ArmyFormationController::update_formation_rotation(float angle_degrees) {
  if (!m_placement.placing()) {
    return;
  }
  m_placement.set_facing(angle_degrees, true);
  refresh_formation_preview();
  announce_placement_update();
}

void ArmyFormationController::aim_formation_at(const QVector3D& aim_point) {
  if (!m_placement.placing() || !m_placement.aim_at(aim_point)) {
    return;
  }
  refresh_formation_preview();
  announce_placement_update();
}

auto ArmyFormationController::begin_move_placement_at_position(
    const QVector3D& position) -> bool {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return false;
  }

  auto troops = troop_units(*m_world, selected);
  if (troops.empty()) {
    return false;
  }

  start_placement(std::move(troops), position, true);
  return true;
}

auto ArmyFormationController::current_request() const
    -> Game::Formation::ArmyFormationRequest {
  Game::Formation::ArmyFormationRequest request;
  request.members = m_placement.units();
  request.anchor = m_placement.position();
  request.facing = m_placement.facing_degrees();
  request.frontage = m_placement.frontage();
  request.intent = m_options.intent();
  request.doctrine = m_options.doctrine_override();
  request.options = m_options.options();
  request.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
  return request;
}

void ArmyFormationController::reject_deployment(QVector3D anchor,
                                                App::Core::OrderRefusal refusal,
                                                App::Core::OrderKind kind) {
  end_formation_placement(FormationTeardown::Cancel);
  (void)m_orders.reject_at(kind, std::move(refusal), anchor);
}

auto ArmyFormationController::deployment_blocked(
    const Game::Command::DeployFormation& deploy) -> bool {
  const auto request = current_request();
  auto const preview =
      Game::Formation::ArmyFormationService::preview(*m_world, request);
  if (!preview.valid) {
    const QString reason = QString::fromStdString(preview.rejection_reason);
    emit formation_placement_rejected(reason);
    reject_deployment(
        deploy.anchor,
        {.failure = App::Core::OrderFailure::CommandUnavailable, .text = reason},
        App::Core::OrderKind::Formation);
    return true;
  }
  if (!deploy.units.empty() &&
      preview.blocked_count >= static_cast<int>(deploy.units.size())) {
    reject_deployment(
        deploy.anchor, App::Core::unreachable_reason(), App::Core::OrderKind::Move);
    return true;
  }
  return false;
}

void ArmyFormationController::confirm_formation_placement() {
  if (!m_placement.placing() || m_placement.units().empty()) {
    cancel_formation_placement();
    return;
  }

  if (!anchor_is_reachable(m_placement.position())) {
    reject_deployment(m_placement.position(),
                      App::Core::unreachable_reason(),
                      App::Core::OrderKind::Move);
    return;
  }

  const auto request = current_request();
  Game::Command::DeployFormation deploy;
  deploy.units = request.members;
  deploy.anchor = request.anchor;
  deploy.facing = request.facing;
  deploy.frontage = request.frontage;
  deploy.intent = request.intent;
  deploy.doctrine = request.doctrine;
  deploy.options = request.options;
  deploy.spacing = request.spacing;

  if (deployment_blocked(deploy)) {
    return;
  }

  const QVector3D anchor = deploy.anchor;
  (void)m_orders.issue(App::Core::OrderKind::Formation, std::move(deploy), 0, &anchor);

  const int deployed_count = static_cast<int>(m_placement.units().size());
  const bool right_drag = m_placement.right_drag();
  m_placement.finish_deployment();
  m_preview.clear();
  emit formation_preview_changed();
  emit formation_placement_ended();
  emit formation_deployed(deployed_count);
  if (!right_drag) {
    emit formation_mode_changed(true);
  }
  m_placement.end_right_drag();
}

auto ArmyFormationController::anchor_is_reachable(const QVector3D& anchor) -> bool {
  if (Game::Systems::NavGrid::is_world_position_walkable(anchor)) {
    return true;
  }
  if (Game::Systems::NavGrid::get_pathfinder() == nullptr) {
    return false;
  }
  const QVector3D snapped = Game::Systems::NavGrid::snap_to_walkable_ground(anchor);
  return Game::Systems::NavGrid::is_world_position_walkable(snapped);
}

void ArmyFormationController::cancel_formation_placement() {
  if (!m_placement.placing()) {
    return;
  }
  end_formation_placement(FormationTeardown::Cancel);
}

void ArmyFormationController::apply_formation_option_change() {
  m_preview.invalidate_layout();
  refresh_formation_preview();
  announce_placement_update();
}

void ArmyFormationController::set_formation_intent(const QString& intent_id) {
  if (m_options.set_intent(intent_id)) {
    apply_formation_option_change();
  }
}

auto ArmyFormationController::formation_intent() const -> QString {
  return QString::fromLatin1(Game::Formation::intent_to_string(m_options.intent()));
}

auto ArmyFormationController::formation_intents() const -> QStringList {
  QStringList out;
  if (m_placement.units().size() < 2) {
    return out;
  }
  for (auto intent : Game::Formation::all_intents()) {
    out.append(QString::fromLatin1(Game::Formation::intent_to_string(intent)));
  }
  return out;
}

auto ArmyFormationController::formation_intent_display_name(
    const QString& intent_id) const -> QString {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  return Game::Formation::intent_display_name(
      parsed.value_or(Game::Formation::ArmyFormationIntent::FactionDefault));
}

auto ArmyFormationController::formation_intent_unavailable_reason(
    const QString& intent_id) const -> QString {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  if (!parsed || m_world == nullptr || m_placement.units().empty()) {
    return QCoreApplication::translate("Formation", "No units selected.");
  }
  return QString::fromStdString(Game::Formation::ArmyFormationService::availability(
      *m_world, m_placement.units(), *parsed, m_options.doctrine_override()));
}

auto ArmyFormationController::formation_doctrine() const -> QString {
  if (!m_options.doctrine_override().empty()) {
    return QString::fromStdString(m_options.doctrine_override());
  }
  if (m_world == nullptr || m_placement.units().empty()) {
    return QString::fromStdString(Game::Formation::k_neutral_doctrine);
  }
  return QString::fromStdString(
      Game::Formation::ArmyFormationService::doctrine_for_selection(
          *m_world, m_placement.units(), m_options.options().mixed_policy));
}

auto ArmyFormationController::formation_doctrine_display_name() const -> QString {
  return Game::Util::tr_asset(Game::Util::k_formations_context,
                              Game::Formation::DoctrineRegistry::instance()
                                  .get_or_neutral(formation_doctrine().toStdString())
                                  .display_name);
}

auto ArmyFormationController::formation_doctrine_options() const -> QVariantList {
  QVariantList out;
  QVariantMap automatic;
  automatic["id"] = QString();
  automatic["name"] = tr("Automatic");
  out.append(automatic);

  const auto& registry = Game::Formation::DoctrineRegistry::instance();
  auto ids = registry.ids();
  std::sort(ids.begin(), ids.end());
  for (const auto& id : ids) {
    const auto* doctrine = registry.find(id);
    if (doctrine == nullptr) {
      continue;
    }
    QVariantMap entry;
    entry["id"] = QString::fromStdString(id);
    entry["name"] =
        Game::Util::tr_asset(Game::Util::k_formations_context, doctrine->display_name);
    out.append(entry);
  }
  return out;
}

void ArmyFormationController::begin_formation_drag(const QVector3D& start) {
  if (!m_placement.placing() || m_placement.units().empty()) {
    return;
  }
  m_placement.begin_drag(start);
  follow_auto_formation_facing();
  m_preview.invalidate_layout();
  refresh_formation_preview();
  announce_placement_update();
}

void ArmyFormationController::update_formation_drag(const QVector3D& current) {
  if (!m_placement.dragging()) {
    return;
  }

  const FormationDragStep step = m_placement.drag_to(current);
  if (step.follow_auto_facing) {
    follow_auto_formation_facing();
  }
  if (step.frontage_changed) {
    m_preview.mark_dirty();
  }
  refresh_formation_preview();
  announce_placement_update();
}

void ArmyFormationController::end_formation_drag() {
  m_placement.end_drag();
}

void ArmyFormationController::adjust_formation_depth(float wheel_delta) {
  if (!m_placement.placing()) {
    return;
  }
  m_options.adjust_depth(wheel_delta);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_preserve_order(bool preserve) {
  m_options.set_preserve_order(preserve);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_frontage_preset(const QString& preset) {
  m_options.set_frontage_preset(preset);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_depth_preset(const QString& preset) {
  m_options.set_depth_preset(preset);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_spacing_preset(const QString& preset) {
  m_options.set_spacing_preset(preset);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_flank_preference(
    const QString& preference) {
  if (m_options.set_flank_preference(preference)) {
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_ranged_placement(const QString& placement) {
  if (m_options.set_ranged_placement(placement)) {
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_reserve_rows(int rows) {
  m_options.set_reserve_rows(rows);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_movement_policy(const QString& policy) {
  if (m_options.set_movement_policy(policy)) {
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_mixed_policy(const QString& policy) {
  if (m_options.set_mixed_policy(policy)) {
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_doctrine_override(const QString& doctrine) {
  m_options.set_doctrine_override(doctrine);
  apply_formation_option_change();
}

void ArmyFormationController::reset_formation_options() {
  m_options.reset();
  m_placement.clear_frontage();
  apply_formation_option_change();
}

auto ArmyFormationController::formation_options() const -> QVariantMap {
  const auto& plan = m_preview.plan();
  const auto& options = m_options.options();
  const auto indices = m_options.preset_indices();
  const bool from_doctrine = m_options.movement_from_doctrine();
  const auto effective_movement =
      from_doctrine ? plan.movement_policy : options.movement_policy;

  FormationOptionsReadout readout;
  readout.intent = formation_intent();
  readout.doctrine = formation_doctrine();
  readout.doctrine_display_name = formation_doctrine_display_name();
  readout.doctrine_locked = options.doctrine_locked;
  readout.frontage_scale = options.frontage_scale;
  readout.depth_scale = options.depth_scale;
  readout.spacing_scale = options.spacing_scale;
  readout.reserve_rows = options.reserve_rows;
  readout.preserve_member_order = options.preserve_member_order;
  readout.flank = QString::fromLatin1(
      Game::Formation::flank_preference_to_string(options.flank_preference));
  readout.ranged = QString::fromLatin1(
      Game::Formation::ranged_placement_to_string(options.ranged_placement));
  readout.movement = QString::fromLatin1(
      Game::Formation::movement_policy_to_string(effective_movement));
  readout.movement_from_doctrine = from_doctrine;
  readout.effective_movement_index = static_cast<int>(effective_movement);
  readout.mixed = QString::fromLatin1(
      Game::Formation::mixed_policy_to_string(options.mixed_policy));
  readout.frontage = m_placement.frontage();
  readout.blocked_slots = plan.blocked_count;
  readout.adjusted_slots = plan.adjusted_count;
  readout.warning = formation_preview_warning();
  readout.frontage_index = indices.frontage;
  readout.depth_index = indices.depth;
  readout.spacing_index = indices.spacing;
  readout.flank_index = indices.flank;
  readout.ranged_index = indices.ranged;
  readout.reserve_index = indices.reserve;
  readout.movement_index = m_options.movement_index(effective_movement);
  readout.mixed_index = indices.mixed;
  readout.preserve_index = indices.preserve;
  readout.intent_display_name =
      Game::Formation::intent_display_name(m_options.intent());
  readout.unit_count = static_cast<int>(m_placement.units().size());
  readout.single_unit = m_placement.units().size() == 1;
  readout.unit_label = single_unit_label(m_world, m_placement.units());
  readout.gesture =
      m_placement.right_drag() ? QStringLiteral("right_drag") : QStringLiteral("click");
  readout.facing_degrees = m_placement.facing_degrees();
  readout.facing_explicit = m_placement.facing_explicit();
  readout.aim_distance = m_placement.aim_distance();
  readout.placed_count = plan.placed_count();
  readout.slot_count = static_cast<int>(plan.slot_list.size());
  readout.ranks = plan.rank_count();
  readout.files = plan.file_count();
  readout.plan_frontage = plan.frontage;
  readout.plan_depth = plan.depth;
  readout.plan_valid = plan.valid;
  return to_variant_map(readout);
}

auto ArmyFormationController::selected_formation_status() const -> QVariantMap {
  if (m_world == nullptr || m_selection_system == nullptr) {
    return to_variant_map(SelectedFormationStatus{});
  }
  return to_variant_map(
      read_selected_formation(*m_world, m_selection_system->get_selected_units()));
}

auto ArmyFormationController::formation_preview_warning() const -> QString {
  const auto& plan = m_preview.plan();
  if (!plan.rejection_reason.empty()) {
    return QString::fromStdString(plan.rejection_reason);
  }
  if (plan.blocked_count > 0) {
    return tr("%1 of %2 positions do not fit on this ground.")
        .arg(plan.blocked_count)
        .arg(static_cast<int>(plan.slot_list.size()));
  }
  return {};
}

void ArmyFormationController::refresh_formation_preview() {
  const auto outcome =
      m_preview.refresh(m_world, current_request(), m_placement.placing());
  if (outcome == FormationPreviewCache::Refresh::Changed) {
    emit formation_preview_changed();
  }
}

} // namespace App::Controllers
