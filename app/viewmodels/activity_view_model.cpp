#include "app/viewmodels/activity_view_model.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "app/core/client_context.h"
#include "app/core/player_feedback.h"
#include "app/economy/unit_profile.h"
#include "app/input/cursor_manager.h"
#include "app/input/input_command_handler.h"
#include "app/world/focus_target.h"
#include "app/world/unit_queries.h"
#include "app/world/visibility_coordinator.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/map/render_visibility_rules.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/session_context.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/selection_system.h"

namespace App::ViewModels {
namespace {

auto activity_to_variant(const Game::Systems::UnitActivity& activity) -> QVariantMap {
  const auto kind = Game::Systems::activity_kind_id(activity.kind);
  const auto state = Game::Systems::activity_state_id(activity.state);
  QVariantMap result;
  result[QStringLiteral("activity")] =
      QString::fromUtf8(kind.data(), static_cast<int>(kind.size()));
  result[QStringLiteral("state")] =
      QString::fromUtf8(state.data(), static_cast<int>(state.size()));
  result[QStringLiteral("queued")] = activity.queued_orders;
  return result;
}

} // namespace

ActivityViewModel::ActivityViewModel(const App::Core::ClientContext& context,
                                     App::Core::ClientHost& host,
                                     QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host) {
}

auto ActivityViewModel::unit(qulonglong unit_id) const -> QVariantMap {
  const auto frame_lock = m_host.lock_frame();
  return activity_to_variant(App::World::unit_activity(
      m_context.world, static_cast<Engine::Core::EntityID>(unit_id)));
}

auto ActivityViewModel::unit_profile(const QString& unit_type,
                                     const QString& nation_id) const -> QVariantMap {
  const auto frame_lock = m_host.lock_frame();
  return App::Economy::unit_profile(m_context.session->nations(), unit_type, nation_id);
}

auto ActivityViewModel::selection_summary() const -> QVariantMap {
  const auto frame_lock = m_host.lock_frame();
  QVariantMap summary;
  summary[QStringLiteral("activity")] = QStringLiteral("idle");
  summary[QStringLiteral("state")] = QStringLiteral("active");
  summary[QStringLiteral("count")] = 0;
  summary[QStringLiteral("total")] = 0;
  summary[QStringLiteral("mixed")] = false;
  if (m_context.world == nullptr || m_context.selection == nullptr) {
    return summary;
  }

  std::vector<Engine::Core::EntityID> selected;
  m_context.selection->get_selected_unit_ids(selected);
  if (selected.empty()) {
    return summary;
  }

  std::map<std::pair<QString, QString>, int> tally;
  for (const auto id : selected) {
    const auto entry =
        activity_to_variant(App::World::unit_activity(m_context.world, id));
    tally[{entry[QStringLiteral("activity")].toString(),
           entry[QStringLiteral("state")].toString()}] += 1;
  }

  auto best = tally.begin();
  for (auto it = tally.begin(); it != tally.end(); ++it) {
    if (it->second > best->second) {
      best = it;
    }
  }

  summary[QStringLiteral("activity")] = best->first.first;
  summary[QStringLiteral("state")] = best->first.second;
  summary[QStringLiteral("count")] = best->second;
  summary[QStringLiteral("total")] = static_cast<int>(selected.size());
  summary[QStringLiteral("mixed")] = tally.size() > 1;
  return summary;
}

void ActivityViewModel::begin_repair_order() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* cursor = m_context.cursor;
  if (cursor == nullptr) {
    return;
  }
  m_host.set_cursor_mode(cursor->mode() == CursorMode::Repair ? CursorMode::Normal
                                                              : CursorMode::Repair);
}

void ActivityViewModel::begin_dismantle_order() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* cursor = m_context.cursor;
  if (cursor == nullptr) {
    return;
  }
  m_host.set_cursor_mode(cursor->mode() == CursorMode::Dismantle
                             ? CursorMode::Normal
                             : CursorMode::Dismantle);
}

void ActivityViewModel::confirm_dismantle_at(qreal sx, qreal sy) {
  if (m_context.input == nullptr || m_context.active_camera == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_builder_dismantle_click(
      sx, sy, m_context.local_owner_id, *m_context.viewport);
}

void ActivityViewModel::confirm_repair_at(qreal sx, qreal sy) {
  if (m_context.input == nullptr || m_context.active_camera == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_builder_repair_click(
      sx, sy, m_context.local_owner_id, *m_context.viewport);
}

void ActivityViewModel::toggle_auto_gather(const QString& priority_product_type) {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->on_auto_gather_command(priority_product_type);
}

void ActivityViewModel::set_auto_gather(bool active,
                                        const QString& priority_product_type) {
  if (m_context.input == nullptr) {
    return;
  }
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  m_context.input->set_auto_gather(active, priority_product_type);
}

void ActivityViewModel::clear_inspect_target() {
  {
    const auto frame_lock = m_host.lock_frame();
    auto* world = m_context.world;
    if (world == nullptr) {
      return;
    }
    auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
    if (selection_system == nullptr) {
      return;
    }
    if (selection_system->inspected_entity() == Engine::Core::NULL_ENTITY) {
      return;
    }
    selection_system->clear_inspected_entity();
  }
  emit inspect_target_cleared();
}

auto ActivityViewModel::pop_player_feedback_events() -> QVariantList {
  QVariantList out;
  if (m_context.feedback == nullptr) {
    return out;
  }

  const auto events = m_context.feedback->drain();
  out.reserve(static_cast<qsizetype>(events.size()));
  for (const auto& event : events) {
    out.append(App::Core::to_variant_map(event));
  }
  return out;
}

auto ActivityViewModel::pop_feedback_ticks() -> QVariantList {
  return App::Core::WorldFeedbackStore::to_variant(m_feedback.pop_ready());
}

void ActivityViewModel::record_hit(const Engine::Core::CombatHitEvent& event,
                                   App::Core::FeedbackStyle style) {
  auto* world = m_context.world;
  if (world == nullptr ||
      (m_context.level != nullptr && m_context.level->is_spectator_mode)) {
    return;
  }
  auto* target = world->get_entity(event.target_id);
  if (target == nullptr) {
    return;
  }
  const auto* transform = target->get_component<Engine::Core::TransformComponent>();
  const auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (transform == nullptr || target_unit == nullptr) {
    return;
  }

  const int owner = m_context.local_owner_id;
  const bool fog_exempt = style == App::Core::FeedbackStyle::Burst;
  const bool target_visible = [&]() {
    if (fog_exempt || target_unit->owner_id == owner ||
        m_context.visibility == nullptr) {
      return true;
    }
    const auto snapshot = m_context.visibility->current_snapshot();
    if (snapshot == nullptr || !snapshot->initialized) {
      return true;
    }
    return Game::Map::should_render_non_local_unit(
        *snapshot, transform->position.x, transform->position.z);
  }();
  if (!target_visible) {
    return;
  }

  int attacker_owner = 0;
  if (auto* attacker = world->get_entity(event.attacker_id)) {
    if (const auto* attacker_unit =
            attacker->get_component<Engine::Core::UnitComponent>()) {
      attacker_owner = attacker_unit->owner_id;
    }
  }

  App::Core::WorldFeedbackTick hit;
  hit.anchor = event.target_id;
  hit.kind = App::Core::FeedbackKind::Damage;
  hit.style = style;
  const bool is_building = target->has_component<Engine::Core::BuildingComponent>();
  hit.x = transform->position.x;
  hit.y = transform->position.y +
          (is_building ? std::max(2.4F, transform->scale.y * 0.9F) : 1.8F);
  hit.z = transform->position.z;
  hit.amount = event.damage;
  hit.severity = target_unit->max_health > 0
                     ? std::clamp(static_cast<float>(event.damage) /
                                      static_cast<float>(target_unit->max_health),
                                  0.0F,
                                  1.5F)
                     : 0.0F;
  hit.killing_blow = event.is_killing_blow;
  hit.incoming = target_unit->owner_id == owner;
  hit.outgoing = attacker_owner == owner;
  if (!hit.incoming && !hit.outgoing) {
    return;
  }

  if (auto* selection_system = world->get_system<Game::Systems::SelectionSystem>()) {
    const auto& selected = selection_system->get_selected_units();
    hit.focused = selection_system->inspected_entity() == event.target_id ||
                  std::find(selected.begin(), selected.end(), event.target_id) !=
                      selected.end() ||
                  App::Core::primary_attack_target(world, selected) == event.target_id;
  }
  m_feedback.push(hit);
}

void ActivityViewModel::record_economy(
    const Engine::Core::EconomyFeedbackEvent& event) {
  auto* world = m_context.world;
  if (world == nullptr || event.amount == 0) {
    return;
  }
  if (event.owner_id != m_context.local_owner_id) {
    return;
  }
  if (m_context.level != nullptr && m_context.level->is_spectator_mode) {
    return;
  }

  App::Core::WorldFeedbackTick tick;
  tick.anchor = event.anchor_id;
  tick.kind = event.kind == Engine::Core::EconomyFeedbackKind::Population
                  ? App::Core::FeedbackKind::Population
                  : App::Core::FeedbackKind::Resource;
  tick.style = App::Core::FeedbackStyle::Tick;
  tick.amount = event.amount;
  tick.resource = event.resource;
  tick.paired_resource = event.paired_resource;
  tick.paired_amount = event.paired_amount;
  tick.outgoing = event.amount < 0;
  tick.incoming = event.amount > 0;

  bool positioned = false;
  if (event.anchor_id != Engine::Core::NULL_ENTITY) {
    if (auto* anchor = world->get_entity(event.anchor_id)) {
      if (const auto* transform =
              anchor->get_component<Engine::Core::TransformComponent>()) {
        const bool is_building =
            anchor->has_component<Engine::Core::BuildingComponent>();
        tick.x = transform->position.x;
        tick.y = transform->position.y +
                 (is_building ? std::max(2.4F, transform->scale.y * 0.9F) : 1.8F);
        tick.z = transform->position.z;
        positioned = true;
      }
    }
  }
  if (!positioned) {
    if (!event.has_position) {
      return;
    }
    tick.anchor = Engine::Core::NULL_ENTITY;
    tick.x = event.x;
    tick.y = event.y + 1.8F;
    tick.z = event.z;
  }

  if (auto* selection_system = world->get_system<Game::Systems::SelectionSystem>();
      selection_system != nullptr && tick.anchor != Engine::Core::NULL_ENTITY) {
    const auto& selected = selection_system->get_selected_units();
    tick.focused =
        selection_system->inspected_entity() == tick.anchor ||
        std::find(selected.begin(), selected.end(), tick.anchor) != selected.end();
  }
  m_feedback.push(tick);
}

void ActivityViewModel::set_focus_targets(const QVariantMap& inspect,
                                          const QVariantMap& target) {
  if (m_inspect_target == inspect && m_selection_target == target) {
    return;
  }
  m_inspect_target = inspect;
  m_selection_target = target;
  emit focus_targets_changed();
}

void ActivityViewModel::set_attack_target_hint(const QVariantMap& hint) {
  if (m_attack_target_hint == hint) {
    return;
  }
  m_attack_target_hint = hint;
  emit attack_target_hint_changed();
}

void ActivityViewModel::set_interaction_target_hint(const QVariantMap& hint) {
  if (m_interaction_target_hint == hint) {
    return;
  }
  m_interaction_target_hint = hint;
  emit interaction_target_hint_changed();
}

} // namespace App::ViewModels
