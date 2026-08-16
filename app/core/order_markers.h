#pragma once

#include <QVector3D>

#include <cstddef>
#include <vector>

#include "order_feedback.h"

namespace Engine::Core {
class World;
}

namespace App::Core {

struct OrderMarker {
  OrderKind kind = OrderKind::None;
  bool rejected = false;

  QVector3D position;

  Engine::Core::EntityID target = 0;

  float age = 0.0F;
  float lifetime = 1.0F;

  [[nodiscard]] auto progress() const -> float {
    return lifetime > 0.0F ? age / lifetime : 1.0F;
  }
};

class OrderMarkerStore {
public:
  static constexpr std::size_t k_max_markers = 24;

  void push(const OrderOutcome& outcome, Engine::Core::World* world);

  void update(float dt, Engine::Core::World* world);

  [[nodiscard]] auto markers() const -> const std::vector<OrderMarker>& {
    return m_markers;
  }

  void clear() { m_markers.clear(); }

  [[nodiscard]] static auto lifetime_for(OrderKind kind, bool rejected) -> float;

private:
  std::vector<OrderMarker> m_markers;
};

[[nodiscard]] auto resolve_order_marker_anchor(Engine::Core::World* world,
                                               Engine::Core::EntityID target,
                                               QVector3D& out_position) -> bool;

} // namespace App::Core
