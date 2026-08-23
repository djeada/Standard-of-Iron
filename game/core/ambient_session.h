#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Map {
class TerrainService;
class VisibilityService;
} // namespace Game::Map

namespace Game::Systems {
class BuildingCollisionRegistry;
class GlobalStatsRegistry;
class MarketplaceSystem;
class NationRegistry;
class OwnerRegistry;
class PlayerResourceRegistry;
class TroopCountRegistry;
} // namespace Game::Systems

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
  Game::Map::TerrainService* terrain = nullptr;
  Game::Map::VisibilityService* visibility = nullptr;
  Game::Systems::OwnerRegistry* owners = nullptr;
  Game::Systems::PlayerResourceRegistry* economy = nullptr;
  Game::Systems::NationRegistry* nations = nullptr;
  Game::Systems::GlobalStatsRegistry* stats = nullptr;
  Game::Systems::TroopCountRegistry* troop_counts = nullptr;
  Game::Systems::BuildingCollisionRegistry* building_collision = nullptr;
  Game::Systems::MarketplaceSystem* marketplace = nullptr;
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

[[nodiscard]] auto ambient_services_or_null() -> const AmbientServices*;

auto set_ambient_services(const AmbientServices* services) -> const AmbientServices*;

auto set_thread_ambient_services(const AmbientServices* services)
    -> const AmbientServices*;

void unbind_ambient_services(const AmbientServices* services);

} // namespace Game::Session
