#include "app/input/context_interaction_resolver.h"

#include <QPointF>
#include <QVector3D>

#include "app/input/hover_tracker.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/interaction_targeting.h"
#include "scene/camera.h"

namespace {

using Game::Systems::InteractionAction;
using Game::Systems::InteractionTargetMarker;

auto selected_worker_request(Engine::Core::World& world)
    -> Game::Systems::InteractionTargetingRequest {
  Game::Systems::InteractionTargetingRequest request;
  request.world = &world;
  for (auto const id :
       Game::Session::session_for(world).selection().get_selected_units()) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    request.local_owner_id = unit->owner_id;
    if (unit->spawn_type == Game::Units::SpawnType::Builder) {
      request.has_builders = true;
    } else if (unit->spawn_type == Game::Units::SpawnType::Civilian) {
      request.has_civilians = true;
    }
  }
  return request;
}

auto hovered_marker(const Game::Systems::InteractionTargetingHighlights& highlights)
    -> const InteractionTargetMarker* {
  for (const auto& marker : highlights.markers) {
    if (marker.hovered) {
      return &marker;
    }
  }
  return nullptr;
}

auto latin1(std::string_view text) -> QString {
  return QString::fromLatin1(text.data(), static_cast<qsizetype>(text.size()));
}

auto interaction_for(const Game::Systems::InteractionTargetingHighlights& highlights)
    -> ContextInteraction {
  ContextInteraction interaction;
  const InteractionTargetMarker* hovered = hovered_marker(highlights);
  switch (highlights.hovered_action) {
  case InteractionAction::Gather:
    if (hovered != nullptr) {
      interaction.gather_product_type =
          latin1(Game::Systems::harvest_product_for_prop(hovered->prop_type));
    }
    break;
  case InteractionAction::Harvest:
    if (hovered != nullptr && hovered->entity_id != 0) {
      interaction.food_product_type =
          latin1(Game::Systems::k_builder_product_harvest_grain);
      interaction.target = hovered->entity_id;
    }
    break;
  case InteractionAction::Slaughter:
    if (hovered != nullptr && hovered->entity_id != 0) {
      interaction.food_product_type =
          latin1(Game::Systems::k_builder_product_slaughter_sheep);
      interaction.target = hovered->entity_id;
    }
    break;
  case InteractionAction::Repair:
    interaction.target = highlights.hovered_entity_id;
    break;
  case InteractionAction::Deliver:
  case InteractionAction::None:
    break;
  }
  return interaction;
}

} // namespace

ContextInteractionResolver::ContextInteractionResolver(
    Engine::Core::World* world,
    Render::GL::Camera* camera,
    Game::Systems::PickingService* picking_service,
    HoverTracker* hover_tracker)
    : m_world(world)
    , m_camera(camera)
    , m_picking_service(picking_service)
    , m_hover_tracker(hover_tracker) {
}

auto ContextInteractionResolver::resolve(
    qreal sx, qreal sy, const ViewportState& viewport) const -> ContextInteraction {
  if (m_world == nullptr || m_camera == nullptr || m_picking_service == nullptr) {
    return {};
  }

  auto request = selected_worker_request(*m_world);
  if (!request.has_builders && !request.has_civilians) {
    return {};
  }

  request.hovered_entity_id =
      m_hover_tracker != nullptr ? m_hover_tracker->get_last_hovered_entity() : 0;

  QVector3D ground;
  if (m_picking_service->screen_to_ground(
          QPointF(sx, sy), *m_camera, viewport.width, viewport.height, ground)) {
    request.has_hovered_ground = true;
    request.hovered_ground_x = ground.x();
    request.hovered_ground_z = ground.z();
    request.anchor_x = ground.x();
    request.anchor_z = ground.z();
  }
  request.max_distance = Game::Systems::k_interaction_highlight_max_distance;
  request.max_markers = Game::Systems::k_interaction_highlight_max_markers;

  return interaction_for(Game::Systems::collect_interaction_target_highlights(request));
}
