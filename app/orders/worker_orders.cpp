#include "app/orders/worker_orders.h"

#include <algorithm>
#include <limits>

#include "game/core/component_gameplay.h"
#include "game/core/world.h"

namespace App::Orders {

auto harvest_order(std::vector<Engine::Core::EntityID> crew,
                   std::string product_type,
                   Engine::Core::EntityID resource_target,
                   float x,
                   float z) -> App::Core::OrderRequest {
  const QVector3D site(x, 0.0F, z);
  App::Core::OrderRequest request;
  request.kind = App::Core::OrderKind::Gather;
  request.payload =
      Game::Command::StartHarvest{.units = std::move(crew),
                                  .construction_type = std::move(product_type),
                                  .resource_target = resource_target,
                                  .site = site};
  request.has_destination = true;
  request.destination = site;
  return request;
}

auto builder_crew_of(Engine::Core::World& world,
                     const std::vector<Engine::Core::EntityID>& selected)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> crew;
  crew.reserve(selected.size());
  for (auto const id : selected) {
    if (world.try_get<Engine::Core::BuilderProductionComponent>(id) != nullptr) {
      crew.push_back(id);
    }
  }
  return crew;
}

void sort_nearest_first(Engine::Core::World& world,
                        std::vector<Engine::Core::EntityID>& crew,
                        float x,
                        float z) {
  const auto distance_to = [&](Engine::Core::EntityID id) {
    auto* entity = world.get_entity(id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      return std::numeric_limits<float>::infinity();
    }
    float const dx = transform->position.x - x;
    float const dz = transform->position.z - z;
    return dx * dx + dz * dz;
  };
  std::stable_sort(crew.begin(), crew.end(), [&](auto lhs, auto rhs) {
    return distance_to(lhs) < distance_to(rhs);
  });
}

} // namespace App::Orders
