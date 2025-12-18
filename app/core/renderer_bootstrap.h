#pragma once

#include <memory>
#include <vector>

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class RiverRenderer;
class RoadRenderer;
class RiverbankRenderer;
class BridgeRenderer;
class FogRenderer;
class StoneRenderer;
class PlantRenderer;
class PineRenderer;
class OliveRenderer;
class FireCampRenderer;
class RainRenderer;
struct IRenderPass;
} 

namespace Engine::Core {
class World;
}

class RendererBootstrap {
public:
  struct RenderingComponents {
    std::unique_ptr<Render::GL::Renderer> renderer;
    std::unique_ptr<Render::GL::Camera> camera;
    std::unique_ptr<Render::GL::GroundRenderer> ground;
    std::unique_ptr<Render::GL::TerrainRenderer> terrain;
    std::unique_ptr<Render::GL::BiomeRenderer> biome;
    std::unique_ptr<Render::GL::RiverRenderer> river;
    std::unique_ptr<Render::GL::RoadRenderer> road;
    std::unique_ptr<Render::GL::RiverbankRenderer> riverbank;
    std::unique_ptr<Render::GL::BridgeRenderer> bridge;
    std::unique_ptr<Render::GL::FogRenderer> fog;
    std::unique_ptr<Render::GL::StoneRenderer> stone;
    std::unique_ptr<Render::GL::PlantRenderer> plant;
    std::unique_ptr<Render::GL::PineRenderer> pine;
    std::unique_ptr<Render::GL::OliveRenderer> olive;
    std::unique_ptr<Render::GL::FireCampRenderer> firecamp;
    std::unique_ptr<Render::GL::RainRenderer> rain;
    std::vector<Render::GL::IRenderPass *> passes;
  };

  static RenderingComponents initialize_rendering();
  static void initialize_world_systems(Engine::Core::World &world);
};
