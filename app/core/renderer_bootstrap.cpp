#include "renderer_bootstrap.h"

#include "game/core/world.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/ballista_attack_system.h"
#include "game/systems/capture_system.h"
#include "game/systems/catapult_attack_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/patrol_system.h"
#include "game/systems/guard_system.h"
#include "game/systems/production_system.h"
#include "game/systems/projectile_system.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "render/gl/camera.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/bridge_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
#include "render/ground/road_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"

auto RendererBootstrap::initialize_rendering() -> RenderingComponents {
  RenderingComponents components;

  components.renderer = std::make_unique<Render::GL::Renderer>();
  components.camera = std::make_unique<Render::GL::Camera>();
  components.ground = std::make_unique<Render::GL::GroundRenderer>();
  components.terrain = std::make_unique<Render::GL::TerrainRenderer>();
  components.biome = std::make_unique<Render::GL::BiomeRenderer>();
  components.river = std::make_unique<Render::GL::RiverRenderer>();
  components.road = std::make_unique<Render::GL::RoadRenderer>();
  components.riverbank = std::make_unique<Render::GL::RiverbankRenderer>();
  components.bridge = std::make_unique<Render::GL::BridgeRenderer>();
  components.fog = std::make_unique<Render::GL::FogRenderer>();
  components.stone = std::make_unique<Render::GL::StoneRenderer>();
  components.plant = std::make_unique<Render::GL::PlantRenderer>();
  components.pine = std::make_unique<Render::GL::PineRenderer>();
  components.olive = std::make_unique<Render::GL::OliveRenderer>();
  components.firecamp = std::make_unique<Render::GL::FireCampRenderer>();
  components.rain = std::make_unique<Render::GL::RainRenderer>();

  components.passes = {components.ground.get(),    components.terrain.get(),
                       components.river.get(),     components.road.get(),
                       components.riverbank.get(), components.bridge.get(),
                       components.biome.get(),     components.stone.get(),
                       components.plant.get(),     components.pine.get(),
                       components.olive.get(),     components.firecamp.get(),
                       components.rain.get(),      components.fog.get()};

  return components;
}

void RendererBootstrap::initialize_world_systems(Engine::Core::World &world) {
  world.add_system(std::make_unique<Game::Systems::ArrowSystem>());
  world.add_system(std::make_unique<Game::Systems::ProjectileSystem>());
  world.add_system(std::make_unique<Game::Systems::MovementSystem>());
  world.add_system(std::make_unique<Game::Systems::PatrolSystem>());
  world.add_system(std::make_unique<Game::Systems::GuardSystem>());
  world.add_system(std::make_unique<Game::Systems::CombatSystem>());
  world.add_system(std::make_unique<Game::Systems::CatapultAttackSystem>());
  world.add_system(std::make_unique<Game::Systems::BallistaAttackSystem>());
  world.add_system(std::make_unique<Game::Systems::HealingBeamSystem>());
  world.add_system(std::make_unique<Game::Systems::HealingSystem>());
  world.add_system(std::make_unique<Game::Systems::CaptureSystem>());
  world.add_system(std::make_unique<Game::Systems::AISystem>());
  world.add_system(std::make_unique<Game::Systems::ProductionSystem>());
  world.add_system(std::make_unique<Game::Systems::TerrainAlignmentSystem>());
  world.add_system(std::make_unique<Game::Systems::CleanupSystem>());
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
}
