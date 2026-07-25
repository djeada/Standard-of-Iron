#pragma once

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class TerrainFeatureManager;
class TerrainScatterManager;
class FogRenderer;
class MapBoundaryFogRenderer;
class AmbientFogRenderer;
class RainRenderer;
} // namespace Render::GL

namespace Game::Systems {
class VictoryService;
class RainManager;
} // namespace Game::Systems

class MinimapManager;
class VisibilityCoordinator;

struct AppSceneContext {
  Engine::Core::World* world = nullptr;
  Render::GL::Renderer* renderer = nullptr;
  Render::GL::Camera* active_camera = nullptr;
  Render::GL::GroundRenderer* ground = nullptr;
  Render::GL::TerrainRenderer* terrain = nullptr;
  Render::GL::TerrainFeatureManager* features = nullptr;
  Render::GL::TerrainScatterManager* scatter = nullptr;
  Render::GL::FogRenderer* fog = nullptr;
  Render::GL::MapBoundaryFogRenderer* boundary_fog = nullptr;
  Render::GL::AmbientFogRenderer* ambient_fog = nullptr;
  Render::GL::RainRenderer* rain = nullptr;
  MinimapManager* minimap_manager = nullptr;
  VisibilityCoordinator* visibility_coordinator = nullptr;
  Game::Systems::VictoryService* victory_service = nullptr;
  Game::Systems::RainManager* rain_manager = nullptr;
};
