#include "app/orders/context_intent.h"

#include <QObject>

#include "game/core/component.h"
#include "game/core/world.h"

namespace App::Core {
namespace {

auto rally_placement_mode(CursorMode mode) -> bool {
  return mode == CursorMode::PlaceCommanderRally ||
         mode == CursorMode::PlaceBarracksRally;
}

auto interact_mode(CursorMode mode) -> bool {
  switch (mode) {
  case CursorMode::Heal:
  case CursorMode::Build:
  case CursorMode::Deliver:
  case CursorMode::Collect:
  case CursorMode::Repair:
  case CursorMode::Dismantle:
  case CursorMode::Guard:
  case CursorMode::PlaceBuilding:
    return true;
  case CursorMode::Normal:
  case CursorMode::Patrol:
  case CursorMode::Attack:
  case CursorMode::PlaceCommanderRally:
  case CursorMode::PlaceBarracksRally:
    return false;
  }
  return false;
}

auto invalid(const QString& reason) -> ContextIntentResolution {
  ContextIntentResolution resolution;
  resolution.intent = ContextIntent::Invalid;
  resolution.reason = reason;
  return resolution;
}

auto ground_move(const ContextIntentRequest& request) -> ContextIntentResolution {
  if (!request.has_ground) {
    return invalid(QObject::tr("No ground under the cursor"));
  }
  if (!request.ground_is_walkable) {
    return invalid(QObject::tr("Cannot reach"));
  }
  ContextIntentResolution resolution;
  resolution.intent = ContextIntent::Move;
  resolution.has_position = true;
  resolution.position = request.ground;
  return resolution;
}

} // namespace

auto context_intent_name(ContextIntent intent) -> const char* {
  switch (intent) {
  case ContextIntent::Invalid:
    return "invalid";
  case ContextIntent::Move:
    return "move";
  case ContextIntent::Attack:
    return "attack";
  case ContextIntent::Interact:
    return "interact";
  case ContextIntent::SetRally:
    return "rally";
  }
  return "invalid";
}

auto resolve_context_intent(const ContextIntentRequest& request)
    -> ContextIntentResolution {
  if (request.world == nullptr) {
    return invalid(QObject::tr("No match is running"));
  }
  if (request.spectator_mode) {
    return invalid(QObject::tr("Spectating"));
  }

  if (rally_placement_mode(request.cursor_mode)) {
    if (!request.has_ground) {
      return invalid(QObject::tr("No ground under the cursor"));
    }
    ContextIntentResolution resolution;
    resolution.intent = ContextIntent::SetRally;
    resolution.has_position = true;
    resolution.position = request.ground;
    return resolution;
  }

  if (request.placing_construction || request.placing_formation) {
    if (!request.has_ground) {
      return invalid(QObject::tr("No ground under the cursor"));
    }
    ContextIntentResolution resolution;
    resolution.intent = ContextIntent::Interact;
    resolution.has_position = true;
    resolution.position = request.ground;
    return resolution;
  }

  const bool has_selection =
      request.selection != nullptr && !request.selection->empty();
  if (!has_selection) {
    return invalid(QObject::tr("Nothing selected"));
  }

  if (interact_mode(request.cursor_mode)) {
    if (request.hovered_entity_id == 0) {
      return invalid(QObject::tr("No target under the cursor"));
    }
    ContextIntentResolution resolution;
    resolution.intent = ContextIntent::Interact;
    resolution.target = request.hovered_entity_id;
    if (request.has_ground) {
      resolution.has_position = true;
      resolution.position = request.ground;
    }
    return resolution;
  }

  if (request.cursor_mode == CursorMode::Attack) {
    ContextIntentResolution resolution;
    resolution.intent = ContextIntent::Attack;
    resolution.target = request.hovered_is_enemy_unit ? request.hovered_entity_id : 0;
    if (request.has_ground) {
      resolution.has_position = true;
      resolution.position = request.ground;
    }
    return resolution;
  }

  if (request.hovered_is_enemy_unit && request.hovered_entity_id != 0) {
    ContextIntentResolution resolution;
    resolution.intent = ContextIntent::Attack;
    resolution.target = request.hovered_entity_id;
    if (request.has_ground) {
      resolution.has_position = true;
      resolution.position = request.ground;
    }
    return resolution;
  }

  if (request.interaction_available) {
    ContextIntentResolution resolution;
    resolution.intent = ContextIntent::Interact;
    resolution.target = request.hovered_entity_id;
    if (request.has_ground) {
      resolution.has_position = true;
      resolution.position = request.ground;
    }
    return resolution;
  }

  return ground_move(request);
}

auto to_variant_map(const ContextIntentResolution& resolution) -> QVariantMap {
  QVariantMap map;
  map[QStringLiteral("intent")] =
      QString::fromLatin1(context_intent_name(resolution.intent));
  map[QStringLiteral("valid")] = resolution.valid();
  map[QStringLiteral("targetId")] = static_cast<qulonglong>(resolution.target);
  map[QStringLiteral("hasPosition")] = resolution.has_position;
  map[QStringLiteral("x")] = resolution.position.x();
  map[QStringLiteral("y")] = resolution.position.y();
  map[QStringLiteral("z")] = resolution.position.z();
  map[QStringLiteral("reason")] = resolution.reason;
  return map;
}

} // namespace App::Core
