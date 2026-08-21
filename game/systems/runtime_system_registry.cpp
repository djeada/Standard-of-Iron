#include "runtime_system_registry.h"

#include <memory>

#include "../command/command_system.h"
#include "../core/system_schedule.h"
#include "../core/world.h"
#include "../formation/army_formation_registry.h"
#include "../formation/unit_layout_state_system.h"
#include "../wildlife/wildlife_system.h"
#include "ai_system.h"
#include "arrow_system.h"
#include "capture_system.h"
#include "civilian_delivery_system.h"
#include "cleanup_system.h"
#include "combat_status_effect_system.h"
#include "combat_system.h"
#include "commander_system.h"
#include "dismantle_system.h"
#include "engagement_slot_system.h"
#include "farm_system.h"
#include "formation_move_dispatch_system.h"
#include "gate_system.h"
#include "gather_loop_system.h"
#include "guard_system.h"
#include "healing_beam_system.h"
#include "healing_system.h"
#include "home_system.h"
#include "local_avoidance_system.h"
#include "movement_system.h"
#include "patrol_system.h"
#include "production_system.h"
#include "projectile_system.h"
#include "resource_delivery_system.h"
#include "rpg_combat_system/rpg_engagement_system.h"
#include "selection_system.h"
#include "settlement_life_system.h"
#include "showcase_routine_system.h"
#include "stamina_system.h"
#include "terrain_alignment_system.h"
#include "undead_awakening_system.h"

namespace Game::Systems {

void register_runtime_systems(Engine::Core::World& world) {

  world.add_system(std::make_unique<Game::Command::CommandSystem>(),
                   Engine::Core::SystemPhase::Input);
  world.add_system(std::make_unique<ArrowSystem>(), Engine::Core::SystemPhase::Input);
  world.add_system(std::make_unique<CombatStatusEffectSystem>(),
                   Engine::Core::SystemPhase::Input);
  world.add_system(std::make_unique<ProjectileSystem>(),
                   Engine::Core::SystemPhase::Input);
  world.add_system(std::make_unique<StaminaSystem>(), Engine::Core::SystemPhase::Input);

  world.add_system(std::make_unique<GateSystem>(), Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<LocalAvoidanceSystem>(),
                   Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<MovementSystem>(),
                   Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<PatrolSystem>(),
                   Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<GuardSystem>(),
                   Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<Game::Formation::ArmyFormationRuntime>(),
                   Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<FormationMoveDispatchSystem>(),
                   Engine::Core::SystemPhase::Movement);
  world.add_system(std::make_unique<Game::Formation::UnitLayoutStateSystem>(),
                   Engine::Core::SystemPhase::Movement);

  world.add_system(std::make_unique<EngagementSlotSystem>(),
                   Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<RpgEngagementSystem>(),
                   Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<CombatSystem>(), Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<CommanderSystem>(),
                   Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<HealingBeamSystem>(),
                   Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<HealingSystem>(),
                   Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<CaptureSystem>(),
                   Engine::Core::SystemPhase::Combat);
  world.add_system(std::make_unique<AISystem>(), Engine::Core::SystemPhase::Strategy);
  world.add_system(std::make_unique<UndeadAwakeningSystem>(),
                   Engine::Core::SystemPhase::Strategy);
  world.add_system(std::make_unique<ProductionSystem>(),
                   Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<DismantleSystem>(),
                   Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<HomeSystem>(), Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<FarmSystem>(), Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<CivilianDeliverySystem>(),
                   Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<ResourceDeliverySystem>(),
                   Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<GatherLoopSystem>(),
                   Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<SettlementLifeSystem>(),
                   Engine::Core::SystemPhase::Economy);
  world.add_system(std::make_unique<Game::Wildlife::WildlifeSystem>(),
                   Engine::Core::SystemPhase::Ambient);
  world.add_system(std::make_unique<ShowcaseRoutineSystem>(),
                   Engine::Core::SystemPhase::Ambient);
  world.add_system(std::make_unique<TerrainAlignmentSystem>(),
                   Engine::Core::SystemPhase::Presentation);
  world.add_system(std::make_unique<CleanupSystem>(),
                   Engine::Core::SystemPhase::Cleanup);
  world.add_system(std::make_unique<SelectionSystem>(),
                   Engine::Core::SystemPhase::Cleanup);
}

} // namespace Game::Systems
