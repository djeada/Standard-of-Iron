#pragma once

#include <cstdint>
#include <memory>

namespace Engine::Core {
class EventManager;
class World;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
class VisibilityService;
} // namespace Game::Map

namespace Game::Systems {
class BuildingCollisionRegistry;
class GlobalStatsRegistry;
class MarketplaceSystem;
class NationRegistry;
class NavigationService;
class OwnerRegistry;
class PlayerResourceRegistry;
class TroopCountRegistry;
} // namespace Game::Systems

namespace Game::Formation {
class ArmyFormationRegistry;
}

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Wildlife {
class BirdFlockManager;
}

namespace Game::Command {
class CommandQueue;
}

namespace Game::Session {

class DeterministicRng;
class SessionContext;
class SimulationClock;

struct AmbientServices {
  SessionContext* session = nullptr;
  Engine::Core::World* world = nullptr;
  Engine::Core::EventManager* events = nullptr;
  Game::Map::TerrainService* terrain = nullptr;
  Game::Map::VisibilityService* visibility = nullptr;
  Game::Systems::OwnerRegistry* owners = nullptr;
  Game::Systems::PlayerResourceRegistry* economy = nullptr;
  Game::Systems::NationRegistry* nations = nullptr;
  Game::Systems::GlobalStatsRegistry* stats = nullptr;
  Game::Systems::TroopCountRegistry* troop_counts = nullptr;
  Game::Systems::BuildingCollisionRegistry* building_collision = nullptr;
  Game::Systems::MarketplaceSystem* marketplace = nullptr;
  Game::Systems::NavigationService* navigation = nullptr;
  Game::Formation::ArmyFormationRegistry* army_formations = nullptr;
  std::shared_ptr<Game::Units::UnitFactoryRegistry>* units = nullptr;
  Game::Wildlife::BirdFlockManager* birds = nullptr;
  SimulationClock* clock = nullptr;
  DeterministicRng* rng = nullptr;
  Game::Command::CommandQueue* commands = nullptr;
};

[[nodiscard]] auto ambient_services() -> const AmbientServices&;

[[nodiscard]] auto
services_for(const Engine::Core::World& world) -> const AmbientServices&;

[[nodiscard]] auto
services_for_or_null(const Engine::Core::World& world) -> const AmbientServices*;

void bind_world_services(const Engine::Core::World& world,
                         const AmbientServices* services);

void unbind_world_services(const Engine::Core::World& world);

[[nodiscard]] auto unbound_world_lookups() -> std::uint64_t;

[[nodiscard]] auto strict_world_binding() -> bool;

void set_strict_world_binding(bool strict);

void reset_unbound_world_lookups();

[[nodiscard]] auto ambient_services_or_null() -> const AmbientServices*;

auto set_ambient_services(const AmbientServices* services) -> const AmbientServices*;

auto set_thread_ambient_services(const AmbientServices* services)
    -> const AmbientServices*;

void unbind_ambient_services(const AmbientServices* services);

} // namespace Game::Session
