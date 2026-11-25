#include "town_growth_system.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "../core/component_core.h"
#include "../core/component_economy.h"
#include "../core/component_structures.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "../visuals/building_asset_key.h"

namespace Game::Systems {

namespace {

constexpr std::string_view k_home_type = "home";
constexpr std::string_view k_tall_style = "tenement";

struct TownHome {
  Engine::Core::EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  bool tall{false};
  bool plain{false};
};

struct Town {
  std::vector<TownHome> homes;
  float sum_x{0.0F};
  float sum_z{0.0F};
};

} // namespace

void TownGrowthSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  m_cooldown -= delta_time;
  if (m_cooldown > 0.0F) {
    return;
  }
  m_cooldown = k_update_interval;

  std::map<int, Town> towns;
  for (auto [home_id, home, transform, unit, building] :
       world->view<const Engine::Core::HomeComponent,
                   const Engine::Core::TransformComponent,
                   const Engine::Core::UnitComponent,
                   const Engine::Core::BuildingComponent>()) {
    (void)home;
    if (unit.health <= 0 || unit.spawn_type != Game::Units::SpawnType::Home) {
      continue;
    }
    Town& town = towns[unit.owner_id];
    town.homes.push_back({.id = home_id,
                          .x = transform.position.x,
                          .z = transform.position.z,
                          .tall = building.visual_style == k_tall_style,
                          .plain = building.visual_style.empty()});
    town.sum_x += transform.position.x;
    town.sum_z += transform.position.z;
  }

  for (auto& [owner_id, town] : towns) {
    (void)owner_id;
    const int home_count = static_cast<int>(town.homes.size());
    const int tall_count = static_cast<int>(
        std::count_if(town.homes.begin(), town.homes.end(), [](const TownHome& home) {
          return home.tall;
        }));
    if (tall_count >= tall_home_target(home_count)) {
      continue;
    }

    const float centre_x = town.sum_x / static_cast<float>(home_count);
    const float centre_z = town.sum_z / static_cast<float>(home_count);
    const TownHome* chosen = nullptr;
    float chosen_distance = std::numeric_limits<float>::max();
    for (const TownHome& home : town.homes) {
      if (!home.plain) {
        continue;
      }
      const float dx = home.x - centre_x;
      const float dz = home.z - centre_z;
      const float distance = dx * dx + dz * dz;
      if (distance < chosen_distance ||
          (distance == chosen_distance && chosen != nullptr && home.id < chosen->id)) {
        chosen = &home;
        chosen_distance = distance;
      }
    }
    if (chosen == nullptr) {
      continue;
    }

    auto* building = world->try_get<Engine::Core::BuildingComponent>(chosen->id);
    auto* renderable = world->try_get<Engine::Core::RenderableComponent>(chosen->id);
    if (building == nullptr || renderable == nullptr) {
      continue;
    }
    building->visual_style = std::string(k_tall_style);
    renderable->renderer_id = Game::Visuals::styled_building_asset_key(
        building->original_nation_id, k_home_type, k_tall_style);
  }
}

auto TownGrowthSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<HomeComponent, TransformComponent, UnitComponent>{},
      Writes<BuildingComponent, RenderableComponent>{});
}

} // namespace Game::Systems
