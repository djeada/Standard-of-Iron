#include "renderer_bootstrap.h"

#include "game/core/world.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/capture_system.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/commander_system.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/home_system.h"
#include "game/systems/local_avoidance_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/patrol_system.h"
#include "game/systems/production_system.h"
#include "game/systems/projectile_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/stamina_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/undead_awakening_system.h"
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
  world.add_system(std::make_unique<Game::Systems::ArrowSystem>());
  world.add_system(std::make_unique<Game::Systems::ProjectileSystem>());
  world.add_system(std::make_unique<Game::Systems::StaminaSystem>());
  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.add_system(std::make_unique<Game::Systems::LocalAvoidanceSystem>());
  world.add_system(std::make_unique<Game::Systems::PatrolSystem>());
  world.add_system(std::make_unique<Game::Systems::GuardSystem>());
  world.add_system(std::make_unique<Game::Systems::CombatSystem>());
  world.add_system(std::make_unique<Game::Systems::CommanderSystem>());
  world.add_system(std::make_unique<Game::Systems::HealingBeamSystem>());
  world.add_system(std::make_unique<Game::Systems::HealingSystem>());
  world.add_system(std::make_unique<Game::Systems::CaptureSystem>());
  world.add_system(std::make_unique<Game::Systems::AISystem>());
  world.add_system(std::make_unique<Game::Systems::UndeadAwakeningSystem>());
  world.add_system(std::make_unique<Game::Systems::ProductionSystem>());
  world.add_system(std::make_unique<Game::Systems::HomeSystem>());
  world.add_system(std::make_unique<Game::Systems::CivilianDeliverySystem>());
  world.add_system(std::make_unique<Game::Systems::TerrainAlignmentSystem>());
  world.add_system(std::make_unique<Game::Systems::CleanupSystem>());
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
}
