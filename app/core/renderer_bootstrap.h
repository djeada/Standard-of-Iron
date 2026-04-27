#pragma once

#include <memory>

namespace Render::GL {
class Renderer;
class Camera;
class TerrainSceneProxy;
class TerrainSurfaceManager;
class TerrainFeatureManager;
class TerrainScatterManager;
class FogRenderer;
class RainRenderer;
} // namespace Render::GL

namespace Engine::Core {
class World;
}

class RendererBootstrap {
public:
  struct RenderingComponents {
    std::unique_ptr<Render::GL::Renderer> renderer;
    std::unique_ptr<Render::GL::Camera> camera;
    std::unique_ptr<Render::GL::TerrainSceneProxy> terrain_scene;
    std::unique_ptr<Render::GL::TerrainSurfaceManager> surface;
    std::unique_ptr<Render::GL::TerrainFeatureManager> features;
    std::unique_ptr<Render::GL::TerrainScatterManager> scatter;
    std::unique_ptr<Render::GL::FogRenderer> fog;
    std::unique_ptr<Render::GL::RainRenderer> rain;
  };

  static RenderingComponents initialize_rendering();
  static void initialize_world_systems(Engine::Core::World &world);
};
