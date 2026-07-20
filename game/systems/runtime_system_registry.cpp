#include "runtime_system_registry.h"

#include <memory>

#include "../core/world.h"
#include "ai_system.h"
#include "arrow_system.h"
#include "capture_system.h"
#include "civilian_delivery_system.h"
#include "cleanup_system.h"
#include "combat_status_effect_system.h"
#include "combat_system.h"
#include "commander_system.h"
#include "guard_system.h"
#include "healing_beam_system.h"
#include "healing_system.h"
#include "home_system.h"
#include "movement_system.h"
#include "patrol_system.h"
#include "production_system.h"
#include "projectile_system.h"
#include "selection_system.h"
#include "stamina_system.h"
#include "terrain_alignment_system.h"
#include "undead_awakening_system.h"

namespace Game::Systems {

void register_runtime_systems(Engine::Core::World& world) {
  world.add_system(std::make_unique<ArrowSystem>());
  world.add_system(std::make_unique<CombatStatusEffectSystem>());
  world.add_system(std::make_unique<ProjectileSystem>());
  world.add_system(std::make_unique<StaminaSystem>());
  world.add_system(std::make_unique<MovementSystem>());
  world.add_system(std::make_unique<PatrolSystem>());
  world.add_system(std::make_unique<GuardSystem>());
  world.add_system(std::make_unique<CombatSystem>());
  world.add_system(std::make_unique<CommanderSystem>());
  world.add_system(std::make_unique<HealingBeamSystem>());
  world.add_system(std::make_unique<HealingSystem>());
  world.add_system(std::make_unique<CaptureSystem>());
  world.add_system(std::make_unique<AISystem>());
  world.add_system(std::make_unique<UndeadAwakeningSystem>());
  world.add_system(std::make_unique<ProductionSystem>());
  world.add_system(std::make_unique<HomeSystem>());
  world.add_system(std::make_unique<CivilianDeliverySystem>());
  world.add_system(std::make_unique<TerrainAlignmentSystem>());
  world.add_system(std::make_unique<CleanupSystem>());
  world.add_system(std::make_unique<SelectionSystem>());
}

} // namespace Game::Systems
