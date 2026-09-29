#include "app/mission/tutorial_runtime.h"

#include <algorithm>

#include "app/world/minimap_manager.h"
#include "game/map/mission_definition.h"
#include "game/mission/campaign_manager.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"

namespace App::Mission {

namespace {

constexpr float k_observe_interval = 0.2F;

} // namespace

TutorialRuntime::TutorialRuntime(QObject* parent)
    : m_director(std::make_unique<Game::Mission::TutorialDirector>(parent)) {
}

TutorialRuntime::~TutorialRuntime() = default;

auto TutorialRuntime::holds_mission_clock() const -> bool {
  return m_director && m_director->holds_mission_clock();
}

auto TutorialRuntime::serialize() const -> QJsonObject {
  return m_director ? m_director->serialize() : QJsonObject{};
}

void TutorialRuntime::end_match() {
  if (m_director) {
    m_director->end();
  }
  m_notes.reset();
  m_observe_accumulator = 0.0F;
}

void TutorialRuntime::activate_if_configured(const CampaignManager* campaign) {
  if (!m_director) {
    return;
  }
  const bool tutorial_mission = campaign != nullptr &&
                                campaign->current_mission_definition().has_value() &&
                                campaign->current_mission_definition()->tutorial;
  m_notes.reset();
  m_observe_accumulator = 0.0F;
  if (tutorial_mission) {
    m_director->begin();
  } else {
    m_director->end();
  }
}

void TutorialRuntime::restore(const CampaignManager* campaign,
                              const QJsonObject& state,
                              int cleared_wave_count) {
  activate_if_configured(campaign);
  if (m_director && m_director->active()) {
    m_director->restore(state, cleared_wave_count);
  }
}

void TutorialRuntime::note_order_outcome(const App::Core::OrderOutcome& outcome) {
  if (!outcome.accepted()) {
    m_notes.last_rejection_reason = outcome.reason;
    return;
  }
  switch (outcome.kind) {
  case App::Core::OrderKind::Move:
  case App::Core::OrderKind::Formation:
    m_notes.move_accepted = true;
    break;
  case App::Core::OrderKind::Attack:
    m_notes.attack_accepted = true;
    break;
  case App::Core::OrderKind::Hold:
    m_notes.hold_accepted = true;
    break;
  case App::Core::OrderKind::Guard:
    m_notes.guard_accepted = true;
    break;
  case App::Core::OrderKind::Patrol:
    m_notes.patrol_accepted = true;
    break;
  case App::Core::OrderKind::Gather:
    m_notes.gather_accepted = true;
    break;
  case App::Core::OrderKind::Build:
    m_notes.build_accepted = true;
    break;
  default:
    break;
  }
  m_notes.last_rejection_reason.clear();
}

void TutorialRuntime::update(float real_dt, const TutorialTick& tick) {
  if (!m_director || !m_director->active()) {
    m_notes.reset();
    return;
  }

  m_observe_accumulator += std::max(0.0F, real_dt);
  if (m_observe_accumulator < k_observe_interval) {
    return;
  }
  const float elapsed = m_observe_accumulator;
  m_observe_accumulator = 0.0F;
  const QVariantMap wave_status =
      tick.waves != nullptr ? tick.waves->status() : QVariantMap{};
  m_director->advance(
      observe_tutorial_frame(
          {.world = tick.world,
           .notes = m_notes,
           .local_owner_id = tick.local_owner_id,
           .victory_state = tick.victory_state,
           .enemy_units_defeated = tick.enemy_units_defeated,
           .mission_running = tick.mission_running,
           .placement = tick.placement,
           .wave_status = wave_status,
           .resources = tick.session != nullptr ? &tick.session->economy() : nullptr,
           .owners = tick.session != nullptr ? &tick.session->owners() : nullptr}),
      elapsed);
  m_notes.reset();
  publish_focus_points(tick, wave_status);
}

void TutorialRuntime::publish_focus_points(const TutorialTick& tick,
                                           const QVariantMap& wave_status) {
  QVariantList points = resolve_tutorial_focus_points(
      {.world = tick.world,
       .local_owner_id = tick.local_owner_id,
       .target = m_director->focus_target_id(),
       .wave_alerts = wave_status.value(QStringLiteral("alerts")).toList(),
       .owners = tick.session != nullptr ? &tick.session->owners() : nullptr,
       .terrain = tick.session != nullptr ? &tick.session->terrain() : nullptr});

  if (!points.isEmpty() && tick.minimap != nullptr && tick.minimap->has_minimap()) {
    for (auto& value : points) {
      QVariantMap point = value.toMap();
      float nx = 0.0F;
      float ny = 0.0F;
      (void)tick.minimap->world_to_normalized(
          point.value("world_x").toFloat(), point.value("world_z").toFloat(), nx, ny);
      point["nx"] = std::clamp(nx, 0.0F, 1.0F);
      point["ny"] = std::clamp(ny, 0.0F, 1.0F);
      value = point;
    }
  }
  m_director->set_focus_points(points);
}

} // namespace App::Mission
