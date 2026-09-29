#include "app/world/targeting_presentation.h"

#include <QMetaObject>
#include <QString>

#include <utility>

#include "app/economy/harvest_targeting.h"
#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/input/cursor_mode.h"
#include "app/input/hover_tracker.h"
#include "app/viewmodels/activity_view_model.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/units/spawn_type.h"
#include "scene/camera.h"

namespace App::World {

namespace {

constexpr float k_interaction_interval = 0.1F;

struct InteractionMode {
  bool armed = false;
  bool gathering = false;
  bool placing_construction = false;
};

auto interaction_mode(const TargetingInputs& inputs) -> InteractionMode {
  const CursorMode cursor_mode = inputs.attack.cursor != nullptr
                                     ? inputs.attack.cursor->mode()
                                     : CursorMode::Normal;
  const bool placing =
      inputs.production != nullptr && inputs.production->is_placing_construction();
  const QString pending_type =
      inputs.production != nullptr
          ? inputs.production->pending_builder_construction_type()
          : QString();
  return {.armed = App::Economy::interaction_highlights_armed(
              cursor_mode, placing, pending_type),
          .gathering =
              cursor_mode == CursorMode::Collect ||
              (placing && App::Economy::is_harvest_construction_item(pending_type)),
          .placing_construction = placing};
}

void note_selected_worker_kinds(const TargetingInputs& inputs,
                                Game::Systems::InteractionTargetingRequest& request) {
  auto* world = inputs.attack.world;
  const int local = inputs.attack.local_owner_id;
  std::vector<Engine::Core::EntityID> selection;
  if (auto* selection_system = &Game::Session::session_for(*world).selection()) {
    selection = selection_system->get_selected_units();
  }
  for (const auto id : selection) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || unit->owner_id != local || unit->health <= 0) {
      continue;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Builder) {
      request.has_builders = true;
    } else if (unit->spawn_type == Game::Units::SpawnType::Civilian) {
      request.has_civilians = true;
    }
  }
}

void publish_hint(
    App::ViewModels::ActivityViewModel* activity,
    QVariantMap& stored,
    const QVariantMap& next,
    void (App::ViewModels::ActivityViewModel::*setter)(const QVariantMap&)) {
  if (activity == nullptr || stored == next) {
    return;
  }
  stored = next;
  QMetaObject::invokeMethod(
      activity,
      [activity, setter, hint = next]() { (activity->*setter)(hint); },
      Qt::QueuedConnection);
}

} // namespace

void TargetingPresentation::sync_attack_targeting(const TargetingInputs& inputs) {
  auto result = App::Core::PresentationSync::collect_attack_targeting(inputs.attack);
  m_attack_targeting = std::move(result.highlights);
  publish_hint(inputs.activity,
               m_attack_hint,
               result.hint,
               &App::ViewModels::ActivityViewModel::set_attack_target_hint);
}

void TargetingPresentation::sync_attack_range_rings(const TargetingInputs& inputs) {
  if (inputs.commander_active) {
    m_attack_range_rings.clear();
    return;
  }
  m_attack_range_rings =
      App::Core::PresentationSync::collect_attack_range_rings(inputs.attack);
}

void TargetingPresentation::reset_interaction() {
  m_interaction_targeting = {};
  m_interaction_accumulator = 0.0F;
  m_interaction_hint.clear();
}

auto TargetingPresentation::collect_interaction(const TargetingInputs& inputs,
                                                QVariantMap& hint)
    -> Game::Systems::InteractionTargetingHighlights {
  const InteractionMode mode = interaction_mode(inputs);
  if (inputs.attack.world == nullptr || inputs.attack.spectator_mode || !mode.armed) {
    return {};
  }

  Game::Systems::InteractionTargetingRequest request;
  request.world = inputs.attack.world;
  request.local_owner_id = inputs.attack.local_owner_id;
  note_selected_worker_kinds(inputs, request);
  if (!request.has_builders && !request.has_civilians) {
    return {};
  }

  auto& visibility = inputs.session->visibility();
  const auto snapshot =
      visibility.is_initialized() ? visibility.snapshot_ptr() : nullptr;

  request.hovered_entity_id = inputs.attack.hover != nullptr
                                  ? inputs.attack.hover->get_last_hovered_entity()
                                  : 0;
  if (inputs.attack.camera != nullptr) {
    const QVector3D anchor = inputs.attack.camera->get_target();
    request.anchor_x = anchor.x();
    request.anchor_z = anchor.z();
  }
  request.max_distance = mode.gathering
                             ? Game::Systems::k_gather_highlight_max_distance
                             : Game::Systems::k_interaction_highlight_max_distance;
  request.max_markers = mode.gathering
                            ? Game::Systems::k_gather_highlight_max_markers
                            : Game::Systems::k_interaction_highlight_max_markers;
  request.visibility = snapshot.get();
  if (mode.gathering && mode.placing_construction) {
    request.gather_only = true;
    request.hover_from_placement = true;
    request.placement_world_prop_id = inputs.production->pending_harvest_target_id();
    request.placement_entity_id = inputs.production->pending_food_target_id();
  }

  QVector3D ground;
  if (inputs.screen_to_ground(inputs.cursor_screen, ground)) {
    request.has_hovered_ground = true;
    request.hovered_ground_x = ground.x();
    request.hovered_ground_z = ground.z();
  }

  auto highlights = Game::Systems::collect_interaction_target_highlights(request);

  const auto action_key =
      Game::Systems::interaction_action_key(highlights.hovered_action);
  hint[QStringLiteral("action")] =
      QString::fromLatin1(action_key.data(), static_cast<qsizetype>(action_key.size()));
  hint[QStringLiteral("resource")] =
      QString::fromLatin1(highlights.hovered_resource.data(),
                          static_cast<qsizetype>(highlights.hovered_resource.size()));
  return highlights;
}

void TargetingPresentation::sync_interaction_targeting(float delta_time,
                                                       const TargetingInputs& inputs) {
  m_interaction_accumulator += delta_time;
  if (m_interaction_accumulator < k_interaction_interval) {
    return;
  }
  m_interaction_accumulator = 0.0F;

  QVariantMap hint;
  hint[QStringLiteral("action")] = QStringLiteral("none");
  m_interaction_targeting = collect_interaction(inputs, hint);

  publish_hint(inputs.activity,
               m_interaction_hint,
               hint,
               &App::ViewModels::ActivityViewModel::set_interaction_target_hint);
}

} // namespace App::World
