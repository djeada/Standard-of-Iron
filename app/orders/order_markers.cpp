#include "app/orders/order_markers.h"

#include <algorithm>

#include "game/core/component_core.h"
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

namespace {

const QVector3D k_order_move_color(0.55F, 0.95F, 0.55F);
const QVector3D k_order_attack_color(1.0F, 0.32F, 0.22F);
const QVector3D k_order_guard_color(0.45F, 0.70F, 1.0F);
const QVector3D k_order_patrol_color(0.35F, 1.0F, 0.55F);
const QVector3D k_order_neutral_color(0.95F, 0.90F, 0.70F);
const QVector3D k_order_rejected_color(0.72F, 0.72F, 0.74F);

} // namespace

auto order_marker_color(OrderKind kind, bool rejected) -> QVector3D {
  if (rejected) {
    return k_order_rejected_color;
  }
  switch (kind) {
  case OrderKind::Move:
    return k_order_move_color;
  case OrderKind::Attack:
    return k_order_attack_color;
  case OrderKind::Guard:
  case OrderKind::Hold:
    return k_order_guard_color;
  case OrderKind::Patrol:
    return k_order_patrol_color;
  default:
    return k_order_neutral_color;
  }
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

auto OrderMarkerStore::push(const OrderOutcome& outcome,
                            Engine::Core::World* world) -> const OrderMarker* {
  if (!outcome.issued()) {
    return nullptr;
  }

  OrderMarker marker;
  marker.kind = outcome.kind;
  marker.rejected = outcome.rejected();
  marker.failure = outcome.failure;
  marker.lifetime = lifetime_for(outcome.kind, marker.rejected);

  if (outcome.target != 0 &&
      resolve_order_marker_anchor(world, outcome.target, marker.position)) {
    marker.target = outcome.target;
  } else if (outcome.has_destination) {
    marker.position = outcome.destination;
  } else {
    return nullptr;
  }

  if (m_markers.size() >= k_max_markers) {
    m_markers.erase(m_markers.begin());
  }
  m_markers.push_back(marker);
  return &m_markers.back();
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
