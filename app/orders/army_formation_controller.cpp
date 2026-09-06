#include "app/orders/army_formation_controller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPointF>
#include <qglobal.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "app/orders/command_controller.h"
#include "app/orders/movement_utils.h"
#include "app/orders/order_issuer.h"
#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "game/audio/audio_cues.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component_gameplay.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/formation/army_formation_service.h"
#include "game/formation/formation_doctrine.h"
#include "game/game_config.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "game/systems/combat_rules.h"
#include "game/systems/command_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"
#include "scene/camera.h"

namespace App::Controllers {
namespace {

void submit(Engine::Core::World* world, Game::Command::Payload payload) {
  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        App::Orders::local_owner(world),
                        std::move(payload));
}

auto scale_for_preset(const QString& preset,
                      float low,
                      float mid,
                      float high) -> float {
  const QString lowered = preset.trimmed().toLower();
  if (lowered == QStringLiteral("narrow") || lowered == QStringLiteral("shallow") ||
      lowered == QStringLiteral("tight")) {
    return low;
  }
  if (lowered == QStringLiteral("wide") || lowered == QStringLiteral("deep") ||
      lowered == QStringLiteral("loose")) {
    return high;
  }
  return mid;
}

auto preset_index_for_scale(float scale, float low, float mid, float high) -> int {
  float const to_low = std::abs(scale - low);
  float const to_mid = std::abs(scale - mid);
  float const to_high = std::abs(scale - high);
  if (to_low <= to_mid && to_low <= to_high) {
    return 0;
  }
  return to_mid <= to_high ? 1 : 2;
}

} // namespace

ArmyFormationController::ArmyFormationController(
    Engine::Core::World* world,
    Game::Systems::SelectionSystem* selection_system,
    App::Orders::OrderIssuer::FeedbackSink feedback,
    QObject* parent)
    : QObject(parent)
    , m_world(world)
    , m_selection_system(selection_system)
    , m_orders(world, std::move(feedback)) {
}

void ArmyFormationController::end_formation_placement(FormationTeardown teardown) {
  const bool was_right_drag = m_is_right_drag_formation;

  if (m_world != nullptr && !m_formation_units.empty()) {
    if (teardown == FormationTeardown::Cancel) {
      submit(m_world, Game::Command::ReleaseFormation{.units = m_formation_units});
    } else {
      submit(
          m_world,
          Game::Command::SetFormationMode{.units = m_formation_units, .active = false});
    }
  }

  m_is_placing_formation = false;
  m_formation_drag_active = false;
  m_is_right_drag_formation = false;
  m_formation_placement_position = QVector3D();
  m_formation_facing_degrees = 0.0F;
  m_formation_facing_explicit = false;
  m_formation_aim_distance = 0.0F;
  m_formation_frontage = 0.0F;
  m_formation_units.clear();
  m_formation_members.clear();
  m_formation_preview = Game::Formation::ArmyFormationPlan{};
  invalidate_formation_layout();

  emit formation_preview_changed();
  emit formation_placement_ended();
  if (teardown == FormationTeardown::Reset || !was_right_drag) {
    emit formation_mode_changed(false);
  }
}

void ArmyFormationController::reset_transient_state() {
  end_formation_placement(FormationTeardown::Reset);
}

auto ArmyFormationController::on_formation_command() -> CommandResult {
  CommandResult result;
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return result;
  }

  const auto& selected = m_selection_system->get_selected_units();
  if (selected.empty()) {
    return result;
  }

  int eligible_count = 0;
  int formation_active_count = 0;

  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (!Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }

    eligible_count++;

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      formation_active_count++;
    }
  }

  if (eligible_count < 1) {
    return result;
  }

  const bool should_enable_formation = (formation_active_count < eligible_count);

  submit(m_world,
         Game::Command::SetFormationMode{.units = {selected.begin(), selected.end()},
                                         .active = should_enable_formation});

  if (should_enable_formation) {
    QVector3D center(0.0F, 0.0F, 0.0F);
    int valid_count = 0;

    m_formation_units.clear();

    for (auto id : selected) {
      auto* entity = m_world->get_entity(id);
      if (entity == nullptr) {
        continue;
      }

      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
        continue;
      }

      m_formation_units.push_back(id);

      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        center.setX(center.x() + transform->position.x);
        center.setY(center.y() + transform->position.y);
        center.setZ(center.z() + transform->position.z);
        valid_count++;
      }
    }

    if (valid_count > 0) {
      center.setX(center.x() / static_cast<float>(valid_count));
      center.setY(center.y() / static_cast<float>(valid_count));
      center.setZ(center.z() / static_cast<float>(valid_count));

      m_is_placing_formation = true;
      m_is_right_drag_formation = false;
      m_formation_placement_position = center;
      reset_formation_facing();
      m_formation_aim_distance = 0.0F;
      m_formation_frontage = 0.0F;
      m_formation_drag_active = false;
      m_formation_members.clear();
      invalidate_formation_layout();
      refresh_formation_preview();

      emit formation_placement_started();
      emit formation_placement_updated(m_formation_placement_position,
                                       m_formation_facing_degrees);
    }
  }

  result.input_consumed = true;
  result.reset_cursor_to_normal = false;
  return result;
}

bool ArmyFormationController::any_selected_in_formation_mode() const {
  if ((m_selection_system == nullptr) || (m_world == nullptr)) {
    return false;
  }

  const auto& selected = m_selection_system->get_selected_units();
  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode != nullptr) && formation_mode->active) {
      return true;
    }
  }

  return false;
}

auto ArmyFormationController::auto_formation_facing() const -> float {
  if ((m_world == nullptr) || m_formation_units.empty()) {
    return 0.0F;
  }
  return Game::Formation::ArmyFormationService::auto_facing(
      *m_world, m_formation_units, m_formation_placement_position);
}

void ArmyFormationController::set_formation_facing(float degrees,
                                                   bool explicit_choice) {
  float normalized = std::fmod(degrees + 180.0F, 360.0F);
  if (normalized < 0.0F) {
    normalized += 360.0F;
  }
  m_formation_facing_degrees = normalized - 180.0F;
  if (explicit_choice) {
    m_formation_facing_explicit = true;
  }
}

void ArmyFormationController::follow_auto_formation_facing() {
  if (m_formation_facing_explicit) {
    return;
  }
  set_formation_facing(auto_formation_facing(), false);
}

void ArmyFormationController::reset_formation_facing() {
  m_formation_facing_explicit = false;
  follow_auto_formation_facing();
}

void ArmyFormationController::update_formation_placement(const QVector3D& position) {
  if (!m_is_placing_formation) {
    return;
  }
  m_formation_placement_position = position;
  m_formation_aim_distance = 0.0F;
  follow_auto_formation_facing();
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void ArmyFormationController::update_formation_rotation(float angle_degrees) {
  if (!m_is_placing_formation) {
    return;
  }
  set_formation_facing(angle_degrees, true);
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void ArmyFormationController::aim_formation_at(const QVector3D& aim_point) {
  if (!m_is_placing_formation) {
    return;
  }
  QVector3D delta = aim_point - m_formation_placement_position;
  delta.setY(0.0F);
  constexpr float k_min_aim_distance = 0.1F;
  float const distance = delta.length();
  if (distance < k_min_aim_distance) {
    return;
  }
  m_formation_aim_distance = distance;
  set_formation_facing(
      std::atan2(delta.x(), delta.z()) * 180.0F / std::numbers::pi_v<float>, true);
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
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

  std::vector<Engine::Core::EntityID> troops;
  for (auto id : selected) {
    auto* entity = m_world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }
    troops.push_back(id);
  }

  if (troops.empty()) {
    return false;
  }

  m_formation_units = std::move(troops);
  m_is_placing_formation = true;
  m_is_right_drag_formation = true;
  m_formation_placement_position = position;
  m_formation_frontage = 0.0F;
  m_formation_aim_distance = 0.0F;
  reset_formation_facing();
  m_formation_members.clear();
  invalidate_formation_layout();
  refresh_formation_preview();

  emit formation_placement_started();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
  return true;
}

void ArmyFormationController::confirm_formation_placement() {
  if (!m_is_placing_formation || m_formation_units.empty()) {
    cancel_formation_placement();
    return;
  }

  Game::Command::DeployFormation deploy;
  deploy.units = m_formation_units;
  deploy.anchor = m_formation_placement_position;
  deploy.facing = m_formation_facing_degrees;
  deploy.frontage = m_formation_frontage;
  deploy.intent = m_formation_intent;
  deploy.doctrine = m_formation_doctrine_override;
  deploy.options = m_formation_options;
  deploy.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;

  {

    Game::Formation::ArmyFormationRequest request;
    request.members = deploy.units;
    request.anchor = deploy.anchor;
    request.facing = deploy.facing;
    request.frontage = deploy.frontage;
    request.intent = deploy.intent;
    request.doctrine = deploy.doctrine;
    request.options = deploy.options;
    request.spacing = deploy.spacing;
    auto const preview =
        Game::Formation::ArmyFormationService::preview(*m_world, request);
    if (!preview.valid) {
      emit formation_placement_rejected(
          QString::fromStdString(preview.rejection_reason));
    }
  }

  {
    const QVector3D anchor = deploy.anchor;
    (void)m_orders.issue(
        App::Core::OrderKind::Formation, std::move(deploy), 0, &anchor);
  }

  const int deployed_count = static_cast<int>(m_formation_units.size());

  m_is_placing_formation = false;
  m_formation_drag_active = false;
  m_formation_facing_explicit = false;
  m_formation_aim_distance = 0.0F;
  m_formation_units.clear();
  m_formation_members.clear();
  invalidate_formation_layout();
  m_formation_preview = Game::Formation::ArmyFormationPlan{};
  emit formation_preview_changed();
  emit formation_placement_ended();
  emit formation_deployed(deployed_count);
  if (!m_is_right_drag_formation) {
    emit formation_mode_changed(true);
  }
  m_is_right_drag_formation = false;
}

void ArmyFormationController::cancel_formation_placement() {
  if (!m_is_placing_formation) {
    return;
  }
  end_formation_placement(FormationTeardown::Cancel);
}

void ArmyFormationController::set_formation_intent(const QString& intent_id) {
  auto parsed = Game::Formation::try_parse_intent(intent_id);
  if (!parsed) {
    return;
  }
  if (m_formation_intent == *parsed) {
    return;
  }
  m_formation_intent = *parsed;
  apply_formation_option_change();
}

auto ArmyFormationController::formation_intent() const -> QString {
  return QString::fromLatin1(Game::Formation::intent_to_string(m_formation_intent));
}

auto ArmyFormationController::formation_intents() const -> QStringList {

  QStringList out;

  if (m_formation_units.size() < 2) {
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
  if (!parsed || m_world == nullptr || m_formation_units.empty()) {
    return QCoreApplication::translate("Formation", "No units selected.");
  }
  return QString::fromStdString(Game::Formation::ArmyFormationService::availability(
      *m_world, m_formation_units, *parsed, m_formation_doctrine_override));
}

auto ArmyFormationController::formation_doctrine() const -> QString {
  if (!m_formation_doctrine_override.empty()) {
    return QString::fromStdString(m_formation_doctrine_override);
  }
  if (m_world == nullptr || m_formation_units.empty()) {
    return QString::fromStdString(Game::Formation::k_neutral_doctrine);
  }
  return QString::fromStdString(
      Game::Formation::ArmyFormationService::doctrine_for_selection(
          *m_world, m_formation_units, m_formation_options.mixed_policy));
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
  if (!m_is_placing_formation || m_formation_units.empty()) {
    return;
  }
  m_formation_drag_active = true;
  m_formation_drag_start = start;
  m_formation_placement_position = start;
  m_formation_frontage = 0.0F;
  m_formation_aim_distance = 0.0F;
  follow_auto_formation_facing();
  invalidate_formation_layout();
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void ArmyFormationController::update_formation_drag(const QVector3D& current) {
  if (!m_formation_drag_active) {
    return;
  }

  QVector3D along = current - m_formation_drag_start;
  along.setY(0.0F);
  float const length = along.length();

  if (m_formation_units.size() == 1) {

    m_formation_placement_position = m_formation_drag_start;
    if (length > 0.5F) {
      aim_formation_at(current);
      return;
    }
    m_formation_aim_distance = 0.0F;
    follow_auto_formation_facing();
    refresh_formation_preview();
    emit formation_placement_updated(m_formation_placement_position,
                                     m_formation_facing_degrees);
    return;
  }

  m_formation_placement_position = m_formation_drag_start + (along * 0.5F);

  if (length > 0.5F) {
    m_formation_frontage = length;
    QVector3D const facing_dir(-along.z(), 0.0F, along.x());
    set_formation_facing(std::atan2(facing_dir.x(), facing_dir.z()) * 180.0F /
                             std::numbers::pi_v<float>,
                         true);

    m_formation_preview_dirty = true;
  } else {
    follow_auto_formation_facing();
  }

  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

void ArmyFormationController::end_formation_drag() {
  m_formation_drag_active = false;
}

void ArmyFormationController::adjust_formation_depth(float wheel_delta) {
  if (!m_is_placing_formation) {
    return;
  }
  float const step = wheel_delta > 0.0F ? 1.15F : (1.0F / 1.15F);
  m_formation_options.depth_scale =
      std::clamp(m_formation_options.depth_scale * step, 0.4F, 3.0F);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_preserve_order(bool preserve) {
  m_formation_options.preserve_member_order = preserve;
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_frontage_preset(const QString& preset) {
  m_formation_options.frontage_scale = scale_for_preset(preset, 0.7F, 1.0F, 1.45F);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_depth_preset(const QString& preset) {
  m_formation_options.depth_scale = scale_for_preset(preset, 0.7F, 1.0F, 1.45F);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_spacing_preset(const QString& preset) {
  m_formation_options.spacing_scale = scale_for_preset(preset, 0.75F, 1.0F, 1.35F);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_flank_preference(
    const QString& preference) {
  if (auto parsed = Game::Formation::try_parse_flank_preference(preference)) {
    m_formation_options.flank_preference = *parsed;
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_ranged_placement(const QString& placement) {
  if (auto parsed = Game::Formation::try_parse_ranged_placement(placement)) {
    m_formation_options.ranged_placement = *parsed;
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_reserve_rows(int rows) {
  m_formation_options.reserve_rows = std::clamp(rows, -1, 2);
  apply_formation_option_change();
}

void ArmyFormationController::set_formation_movement_policy(const QString& policy) {
  if (auto parsed = Game::Formation::try_parse_movement_policy(policy)) {
    m_formation_options.movement_policy = *parsed;
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_mixed_policy(const QString& policy) {
  if (auto parsed = Game::Formation::try_parse_mixed_policy(policy)) {
    m_formation_options.mixed_policy = *parsed;
    apply_formation_option_change();
  }
}

void ArmyFormationController::set_formation_doctrine_override(const QString& doctrine) {
  const QString trimmed = doctrine.trimmed().toLower();
  if (trimmed.isEmpty() || trimmed == QStringLiteral("automatic")) {
    m_formation_doctrine_override.clear();
    m_formation_options.doctrine_locked = false;
  } else {
    m_formation_doctrine_override = trimmed.toStdString();
    m_formation_options.doctrine_locked = true;
  }
  apply_formation_option_change();
}

auto ArmyFormationController::formation_options() const -> QVariantMap {
  QVariantMap map;
  map["intent"] = formation_intent();
  map["doctrine"] = formation_doctrine();
  map["doctrine_display_name"] = formation_doctrine_display_name();
  map["doctrine_locked"] = m_formation_options.doctrine_locked;
  map["frontage_scale"] = m_formation_options.frontage_scale;
  map["depth_scale"] = m_formation_options.depth_scale;
  map["spacing_scale"] = m_formation_options.spacing_scale;
  map["reserve_rows"] = m_formation_options.reserve_rows;
  map["preserve_member_order"] = m_formation_options.preserve_member_order;
  map["flank"] = QString::fromLatin1(Game::Formation::flank_preference_to_string(
      m_formation_options.flank_preference));
  map["ranged"] = QString::fromLatin1(Game::Formation::ranged_placement_to_string(
      m_formation_options.ranged_placement));
  map["movement"] = QString::fromLatin1(
      Game::Formation::movement_policy_to_string(m_formation_options.movement_policy));
  map["mixed"] = QString::fromLatin1(
      Game::Formation::mixed_policy_to_string(m_formation_options.mixed_policy));
  map["frontage"] = m_formation_frontage;
  map["blocked_slots"] = m_formation_preview.blocked_count;
  map["adjusted_slots"] = m_formation_preview.adjusted_count;
  map["warning"] = formation_preview_warning();

  map["frontage_index"] =
      preset_index_for_scale(m_formation_options.frontage_scale, 0.7F, 1.0F, 1.45F);
  map["depth_index"] =
      preset_index_for_scale(m_formation_options.depth_scale, 0.7F, 1.0F, 1.45F);
  map["spacing_index"] =
      preset_index_for_scale(m_formation_options.spacing_scale, 0.75F, 1.0F, 1.35F);
  map["flank_index"] = static_cast<int>(m_formation_options.flank_preference);
  map["ranged_index"] = static_cast<int>(m_formation_options.ranged_placement);
  map["reserve_index"] = std::clamp(m_formation_options.reserve_rows, -1, 2) + 1;
  map["movement_index"] = static_cast<int>(m_formation_options.movement_policy);
  map["mixed_index"] = static_cast<int>(m_formation_options.mixed_policy);

  map["preserve_index"] = m_formation_options.preserve_member_order ? 1 : 0;
  map["intent_display_name"] = Game::Formation::intent_display_name(m_formation_intent);
  map["unit_count"] = static_cast<int>(m_formation_units.size());
  map["single_unit"] = m_formation_units.size() == 1;
  map["unit_label"] = formation_unit_label();
  map["gesture"] = m_is_right_drag_formation ? QStringLiteral("right_drag")
                                             : QStringLiteral("click");
  map["facing_degrees"] = m_formation_facing_degrees;
  map["facing_explicit"] = m_formation_facing_explicit;
  map["aim_distance"] = m_formation_aim_distance;
  map["placed_count"] = m_formation_preview.placed_count();
  map["slot_count"] = static_cast<int>(m_formation_preview.slot_list.size());
  map["ranks"] = m_formation_preview.rank_count();
  map["files"] = m_formation_preview.file_count();
  map["plan_frontage"] = m_formation_preview.frontage;
  map["plan_depth"] = m_formation_preview.depth;
  map["plan_valid"] = m_formation_preview.valid;
  return map;
}

auto ArmyFormationController::formation_unit_label() const -> QString {
  if (m_world == nullptr || m_formation_units.size() != 1) {
    return {};
  }
  auto* entity = m_world->get_entity(m_formation_units.front());
  const auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  if (unit == nullptr) {
    return {};
  }
  const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
  if (!troop_type.has_value()) {
    return QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));
  }
  const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      unit->nation_id, *troop_type);
  return Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
}

auto ArmyFormationController::selected_formation_status() const -> QVariantMap {
  QVariantMap map;
  map["active"] = false;
  if (m_world == nullptr || m_selection_system == nullptr) {
    return map;
  }

  const auto& selected = m_selection_system->get_selected_units();
  auto& registry = Game::Formation::ArmyFormationRegistry::for_world(*m_world);

  Game::Formation::FormationGroupID group = Game::Formation::k_invalid_group;
  int in_group = 0;
  int group_count = 0;
  for (auto const id : selected) {
    auto const owner = registry.group_of(id);
    if (owner == Game::Formation::k_invalid_group) {
      continue;
    }
    if (group == Game::Formation::k_invalid_group) {
      group = owner;
    }
    if (owner == group) {
      ++in_group;
    } else {
      ++group_count;
    }
  }

  const auto* formation = registry.find(group);
  if (formation == nullptr) {
    return map;
  }

  map["active"] = true;
  map["intent"] =
      QString::fromLatin1(Game::Formation::intent_to_string(formation->intent));
  map["intent_display_name"] = Game::Formation::intent_display_name(formation->intent);
  map["doctrine_display_name"] =
      QString::fromStdString(Game::Formation::DoctrineRegistry::instance()
                                 .get_or_neutral(formation->doctrine)
                                 .display_name);
  map["cohesion"] = formation->cohesion;
  map["phase"] = QString::fromLatin1(phase_to_string(formation->phase));
  map["member_count"] = static_cast<int>(formation->members.size());
  map["selected_in_group"] = in_group;
  map["mixed_groups"] = group_count > 0;
  map["blocked_slots"] = formation->blocked_slot_count();
  return map;
}

void ArmyFormationController::reset_formation_options() {
  m_formation_options = Game::Formation::ArmyFormationOptions{};
  m_formation_doctrine_override.clear();
  m_formation_intent = Game::Formation::ArmyFormationIntent::FactionDefault;
  m_formation_frontage = 0.0F;
  apply_formation_option_change();
}

auto ArmyFormationController::formation_preview_warning() const -> QString {
  if (!m_formation_preview.rejection_reason.empty()) {
    return QString::fromStdString(m_formation_preview.rejection_reason);
  }
  if (m_formation_preview.blocked_count > 0) {
    return tr("%1 of %2 positions do not fit on this ground.")
        .arg(m_formation_preview.blocked_count)
        .arg(static_cast<int>(m_formation_preview.slot_list.size()));
  }
  return {};
}

void ArmyFormationController::invalidate_formation_layout() {
  m_formation_layout_valid = false;
  m_formation_preview_dirty = true;
}

void ArmyFormationController::refresh_formation_preview() {
  if (m_world == nullptr || m_formation_units.empty() || !m_is_placing_formation) {
    m_formation_preview = Game::Formation::ArmyFormationPlan{};
    m_formation_members.clear();
    m_formation_layout_valid = false;
    m_formation_preview_dirty = true;
    emit formation_preview_changed();
    return;
  }

  Game::Formation::ArmyFormationRequest request;
  request.members = m_formation_units;
  request.anchor = m_formation_placement_position;
  request.facing = m_formation_facing_degrees;
  request.frontage = m_formation_frontage;
  request.intent = m_formation_intent;
  request.doctrine = m_formation_doctrine_override;
  request.options = m_formation_options;
  request.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
  request.group_id =
      Game::Formation::ArmyFormationRegistry::for_world(*m_world).group_of(
          m_formation_units.front());

  constexpr float k_anchor_epsilon = 0.05F;
  constexpr float k_facing_epsilon = 0.25F;
  if (!m_formation_preview_dirty && m_formation_preview.valid &&
      (request.anchor - m_formation_previewed_anchor).lengthSquared() <
          k_anchor_epsilon * k_anchor_epsilon &&
      std::abs(request.facing - m_formation_previewed_facing) < k_facing_epsilon) {
    return;
  }

  if (m_formation_members.empty()) {
    m_formation_members = Game::Formation::ArmyFormationPlanner::collect_members(
        *m_world, m_formation_units);
  }

  const auto* previous_group =
      request.group_id != Game::Formation::k_invalid_group
          ? Game::Formation::ArmyFormationRegistry::for_world(*m_world).find(
                request.group_id)
          : nullptr;
  auto const signature = Game::Formation::ArmyFormationPlanner::layout_signature(
      m_formation_members, request, previous_group);
  if (!m_formation_layout_valid || m_formation_layout.signature != signature) {
    m_formation_layout = Game::Formation::ArmyFormationPlanner::build_layout(
        m_formation_members, request, previous_group);
    m_formation_layout_valid = true;
  }

  m_formation_preview =
      Game::Formation::ArmyFormationPlanner::place(m_formation_layout, request);
  m_formation_previewed_anchor = request.anchor;
  m_formation_previewed_facing = request.facing;
  m_formation_preview_dirty = false;
  emit formation_preview_changed();
}

void ArmyFormationController::apply_formation_option_change() {
  invalidate_formation_layout();
  refresh_formation_preview();
  emit formation_placement_updated(m_formation_placement_position,
                                   m_formation_facing_degrees);
}

} // namespace App::Controllers
