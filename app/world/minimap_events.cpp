#include "app/world/minimap_events.h"

#include <algorithm>

#include "app/viewmodels/minimap_view_model.h"
#include "app/world/minimap_manager.h"
#include "game/core/component_gameplay.h"
#include "game/core/local_audience.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/cursed_gold_vein_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/spawn_type.h"

namespace App::World {

namespace {

constexpr float k_landmark_poll_interval = 0.5F;

} // namespace

MinimapEvents::MinimapEvents(const MinimapEventSources& sources)
    : m_sources(sources) {
  m_undead_zone_awakened_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UndeadZoneAwakenedEvent>(
          [this](const Engine::Core::UndeadZoneAwakenedEvent& e) {
            note_shrine_stirred(e);
          });
  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent& e) { note_unit_died(e); });
  m_barrack_captured_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>(
          [this](const Engine::Core::BarrackCapturedEvent& e) {
            note_barrack_captured(e);
          });
}

auto MinimapEvents::minimap_ready() const -> bool {
  return m_sources.view_model != nullptr && m_sources.manager != nullptr &&
         m_sources.manager->has_minimap();
}

void MinimapEvents::reset() {
  m_landmark_poll_accumulator = 0.0F;
  if (m_sources.view_model != nullptr) {
    m_sources.view_model->clear_overlays();
  }
}

void MinimapEvents::note_combat_hit(const Engine::Core::CombatHitEvent& event) {
  if (m_sources.world == nullptr || !minimap_ready()) {
    return;
  }
  const auto* transform =
      m_sources.world->try_get<Engine::Core::TransformComponent>(event.target_id);
  const auto* unit =
      m_sources.world->try_get<Engine::Core::UnitComponent>(event.target_id);
  if (transform == nullptr || unit == nullptr ||
      Game::Units::is_wildlife_spawn(unit->spawn_type)) {
    return;
  }

  const int local = *m_sources.local_owner_id;
  const bool involves_local =
      unit->owner_id == local || event.attacker_owner_id == local;
  if (!involves_local && !m_sources.view_model->consume_alert_budget()) {
    return;
  }

  const bool is_building = Game::Units::is_building_spawn(unit->spawn_type);
  m_sources.view_model->note_alert(
      is_building ? App::ViewModels::MinimapAlert::StructureAttacked
                  : App::ViewModels::MinimapAlert::TroopsAttacked,
      transform->position.x,
      transform->position.z,
      unit->owner_id,
      event.attacker_owner_id);
}

void MinimapEvents::note_unit_died(const Engine::Core::UnitDiedEvent& event) {
  if (m_sources.world == nullptr || !minimap_ready() ||
      Game::Units::is_wildlife_spawn(event.spawn_type)) {
    return;
  }
  const auto* transform =
      m_sources.world->try_get<Engine::Core::TransformComponent>(event.unit_id);
  if (transform == nullptr) {
    return;
  }

  const int local = *m_sources.local_owner_id;
  const bool is_building = Game::Units::is_building_spawn(event.spawn_type);
  const bool lost_by_local = event.owner_id == local;
  const bool taken_by_local = event.killer_owner_id == local;

  if (!lost_by_local && !(is_building && taken_by_local)) {
    return;
  }
  m_sources.view_model->note_alert(is_building
                                       ? App::ViewModels::MinimapAlert::StructureLost
                                       : App::ViewModels::MinimapAlert::UnitLost,
                                   transform->position.x,
                                   transform->position.z,
                                   event.owner_id,
                                   event.killer_owner_id);
}

void MinimapEvents::note_shrine_stirred(
    const Engine::Core::UndeadZoneAwakenedEvent& event) {
  if (!minimap_ready()) {
    return;
  }
  m_sources.view_model->note_alert(App::ViewModels::MinimapAlert::ShrineStirred,
                                   event.world_x,
                                   event.world_z,
                                   event.zone_owner_id,
                                   event.woken_by_owner_id);
}

void MinimapEvents::note_barrack_captured(
    const Engine::Core::BarrackCapturedEvent& event) {
  if (m_sources.view_model == nullptr || m_sources.world == nullptr) {
    return;
  }
  const auto* transform =
      m_sources.world->try_get<Engine::Core::TransformComponent>(event.barrack_id);
  if (transform == nullptr) {
    return;
  }
  m_sources.view_model->note_alert(App::ViewModels::MinimapAlert::CaptureFinished,
                                   transform->position.x,
                                   transform->position.z,
                                   event.previous_owner_id,
                                   event.new_owner_id);
}

void MinimapEvents::publish_overlays(float dt) {
  if (!minimap_ready()) {
    return;
  }
  auto& manager = *m_sources.manager;
  auto& view_model = *m_sources.view_model;

  for (const auto& alert : manager.capture_alerts()) {
    view_model.note_alert(alert.contested
                              ? App::ViewModels::MinimapAlert::CaptureContested
                              : App::ViewModels::MinimapAlert::CaptureStarted,
                          alert.world_x,
                          alert.world_z,
                          alert.site_owner_id,
                          alert.capturing_owner_id);
  }
  manager.clear_capture_alerts();

  if (manager.consume_destinations_dirty()) {
    QVariantList destinations;
    for (const auto& destination : manager.destinations()) {
      QVariantMap entry;
      entry["nx"] = destination.nx;
      entry["ny"] = destination.ny;
      destinations.append(entry);
    }
    view_model.set_destinations(destinations);
  }

  m_landmark_poll_accumulator += std::max(dt, 0.0F);
  if (m_landmark_poll_accumulator < k_landmark_poll_interval) {
    return;
  }
  m_landmark_poll_accumulator = 0.0F;
  publish_landmarks();
}

void MinimapEvents::publish_landmarks() {
  auto& manager = *m_sources.manager;
  QVariantList landmarks;
  if (m_sources.world != nullptr) {
    if (auto* undead =
            m_sources.world->get_system<Game::Systems::UndeadAwakeningSystem>()) {
      for (const auto& shrine : undead->shrine_markers()) {
        float nx = 0.0F;
        float ny = 0.0F;
        if (!manager.world_to_normalized(
                shrine.world_position.x(), shrine.world_position.z(), nx, ny)) {
          continue;
        }
        QVariantMap entry;
        entry["nx"] = nx;
        entry["ny"] = ny;
        entry["kind"] = QStringLiteral("shrine");
        entry["state"] = shrine.cleared    ? QStringLiteral("cleared")
                         : shrine.awakened ? QStringLiteral("awakened")
                                           : QStringLiteral("dormant");
        landmarks.append(entry);
      }
    }
    if (auto* veins =
            m_sources.world->get_system<Game::Systems::CursedGoldVeinSystem>()) {
      const int local_owner = m_sources.session != nullptr
                                  ? m_sources.session->owners().get_local_player_id()
                                  : 0;
      for (const auto& vein : veins->vein_markers()) {
        float nx = 0.0F;
        float ny = 0.0F;
        if (!manager.world_to_normalized(
                vein.world_position.x(), vein.world_position.z(), nx, ny)) {
          continue;
        }
        QVariantMap entry;
        entry["nx"] = nx;
        entry["ny"] = ny;
        entry["kind"] = QStringLiteral("gold_vein");
        entry["state"] = vein.destroyed ? QStringLiteral("destroyed")
                         : Game::Core::is_neutral_owner(vein.owner_id)
                             ? QStringLiteral("neutral")
                         : vein.owner_id == local_owner ? QStringLiteral("owned")
                                                        : QStringLiteral("enemy");
        landmarks.append(entry);
      }
    }
  }
  m_sources.view_model->set_landmarks(landmarks);
}

} // namespace App::World
