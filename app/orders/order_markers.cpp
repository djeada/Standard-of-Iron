#include "app/orders/order_markers.h"

#include <algorithm>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/core/world.h"

namespace App::Core {

auto resolve_order_marker_anchor(Engine::Core::World* world,
                                 Engine::Core::EntityID target,
                                 QVector3D& out_position) -> bool {
  if (world == nullptr || target == 0) {
    return false;
  }
  auto* entity = world->get_entity(target);
  if (entity == nullptr) {
    return false;
  }
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return false;
  }
  out_position =
      QVector3D(transform->position.x, transform->position.y, transform->position.z);
  return true;
}

auto OrderMarkerStore::lifetime_for(OrderKind kind, bool rejected) -> float {
  if (rejected) {
    return 1.2F;
  }
  switch (kind) {
  case OrderKind::Attack:
    return 1.6F;
  case OrderKind::Move:
    return 1.2F;
  case OrderKind::Guard:
  case OrderKind::Patrol:
    return 1.5F;
  default:
    return 1.2F;
  }
}

void OrderMarkerStore::push(const OrderOutcome& outcome, Engine::Core::World* world) {
  if (!outcome.issued()) {
    return;
  }

  OrderMarker marker;
  marker.kind = outcome.kind;
  marker.rejected = outcome.rejected();
  marker.lifetime = lifetime_for(outcome.kind, marker.rejected);

  if (outcome.target != 0 &&
      resolve_order_marker_anchor(world, outcome.target, marker.position)) {
    marker.target = outcome.target;
  } else if (outcome.has_destination) {
    marker.position = outcome.destination;
  } else {
    return;
  }

  if (m_markers.size() >= k_max_markers) {
    m_markers.erase(m_markers.begin());
  }
  m_markers.push_back(marker);
}

void OrderMarkerStore::update(float dt, Engine::Core::World* world) {
  if (dt < 0.0F) {
    dt = 0.0F;
  }
  for (auto& marker : m_markers) {
    marker.age += dt;
    if (marker.target != 0) {
      QVector3D anchor;
      if (resolve_order_marker_anchor(world, marker.target, anchor)) {
        marker.position = anchor;
      } else {
        marker.age = marker.lifetime;
      }
    }
  }
  m_markers.erase(std::remove_if(m_markers.begin(),
                                 m_markers.end(),
                                 [](const OrderMarker& marker) {
                                   return marker.age >= marker.lifetime;
                                 }),
                  m_markers.end());
}

} // namespace App::Core
