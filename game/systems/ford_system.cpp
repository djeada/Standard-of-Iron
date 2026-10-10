#include "ford_system.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "../core/component_core.h"
#include "../core/component_gameplay.h"
#include "../core/component_structures.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "game/core/ford_rules.h"

namespace Game::Systems {

namespace {

auto beside_ford(const Game::Map::TerrainService& terrain, float x, float z) -> bool {
  if (terrain.ford_profile_at(x, z) != nullptr) {
    return true;
  }
  constexpr float reach = FordSystem::k_beside_reach;
  constexpr std::array<std::pair<float, float>, 4> k_probes{
      {{reach, 0.0F}, {-reach, 0.0F}, {0.0F, reach}, {0.0F, -reach}}};
  for (auto const& [dx, dz] : k_probes) {
    if (terrain.ford_profile_at(x + dx, z + dz) != nullptr) {
      return true;
    }
  }
  return false;
}

} // namespace

FordSystem::FordSystem(Services services)
    : m_services(services) {
}

void FordSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  auto const& terrain = m_services.terrain;
  bool const fords = terrain.is_initialized() && terrain.has_fords();

  struct Change {
    Engine::Core::EntityID id;
    Engine::Core::WadingComponent state;
  };
  std::vector<Change> arrivals;
  std::vector<Engine::Core::EntityID> departures;

  if (!fords) {
    for (auto [id, wading] : world->view<Engine::Core::WadingComponent>()) {
      (void)wading;
      departures.push_back(id);
    }
    for (auto const id : departures) {
      world->remove<Engine::Core::WadingComponent>(id);
    }
    return;
  }

  for (auto [id, unit, transform] :
       world->view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
    auto* wading = world->try_get<Engine::Core::WadingComponent>(id);
    bool const eligible = unit.health > 0 &&
                          !world->has<Engine::Core::BuildingComponent>(id) &&
                          !world->has<Engine::Core::PendingRemovalComponent>(id) &&
                          !world->has<Engine::Core::RaftRiderComponent>(id);
    float const x = transform.position.x;
    float const z = transform.position.z;
    bool const near = eligible && beside_ford(terrain, x, z);

    Engine::Core::WadingComponent next =
        wading != nullptr ? *wading : Engine::Core::WadingComponent{};
    next.depth = 0.0F;
    next.speed = 1.0F;
    next.exposure = 1.0F;
    if (near) {
      if (auto const* profile = terrain.ford_profile_at(x, z)) {
        next.depth = terrain.ford_water_depth_at(x, z);
        next.speed = profile->speed;
        next.exposure = profile->exposure;
        next.cold = profile->cold;
      }
    }
    bool const in_water = next.in_water();
    next.chill = FordRules::next_chill(next.chill, in_water, next.cold, delta_time);
    if (!in_water && next.chill <= 0.0F) {
      next.cold = 0.0F;
    }

    if (in_water && next.cold > 0.0F) {
      if (auto* stamina = world->try_get<Engine::Core::StaminaComponent>(id)) {
        stamina->spend(FordRules::k_cold_stamina_drain_per_second * next.cold *
                       std::max(0.0F, delta_time));
      }
    }

    bool const keep = eligible && (near || next.chill > 0.0F);
    if (!keep) {
      if (wading != nullptr) {
        departures.push_back(id);
      }
      continue;
    }
    if (wading != nullptr) {
      *wading = next;
    } else {
      arrivals.push_back({id, next});
    }
  }

  for (auto const id : departures) {
    world->remove<Engine::Core::WadingComponent>(id);
  }
  for (auto const& arrival : arrivals) {
    if (auto* added = world->emplace<Engine::Core::WadingComponent>(arrival.id)) {
      *added = arrival.state;
    }
  }
}

} // namespace Game::Systems
