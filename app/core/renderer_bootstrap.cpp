#include "renderer_bootstrap.h"

#include "game/core/world.h"
#include "game/systems/runtime_system_registry.h"
#include "render/gl/camera.h"
#include "render/graphics_settings.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"

auto RendererBootstrap::initialize_rendering() -> RenderingComponents {
  RenderingComponents components;

  components.renderer = std::make_unique<Render::GL::Renderer>(
      Render::GraphicsSettings::instance().features().shader_quality);
  components.camera = std::make_unique<Render::GL::Camera>();
  components.surface = std::make_unique<Render::GL::TerrainSurfaceManager>();
  components.features = std::make_unique<Render::GL::TerrainFeatureManager>();
  components.scatter = std::make_unique<Render::GL::TerrainScatterManager>();
  components.fog = std::make_unique<Render::GL::FogRenderer>();
  components.boundary_fog = std::make_unique<Render::GL::MapBoundaryFogRenderer>();
  components.ambient_fog = std::make_unique<Render::GL::AmbientFogRenderer>();
  components.rain = std::make_unique<Render::GL::RainRenderer>();
  components.terrain_scene =
      std::make_unique<Render::GL::TerrainSceneProxy>(components.surface.get(),
                                                      components.features.get(),
                                                      components.scatter.get(),
                                                      components.rain.get(),
                                                      components.fog.get(),
                                                      components.boundary_fog.get(),
                                                      components.ambient_fog.get());

  return components;
}

void RendererBootstrap::initialize_world_systems(Engine::Core::World& world) {
  Game::Systems::register_runtime_systems(world);
}
