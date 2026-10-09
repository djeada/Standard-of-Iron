#include "factory.h"

#include "../core/component_commander.h"
#include "../core/component_core.h"
#include "../core/world.h"
#include "archer.h"
#include "ballista.h"
#include "barracks.h"
#include "builder.h"
#include "catapult.h"
#include "commander_catalog.h"
#include "civilian.h"
#include "defense_tower.h"
#include "elephant.h"
#include "farm.h"
#include "grave_priest.h"
#include "healer.h"
#include "home.h"
#include "horse_archer.h"
#include "horse_spearman.h"
#include "horse_swordsman.h"
#include "marketplace.h"
#include "ram.h"
#include "sheep.h"
#include "siege_tower.h"
#include "skeleton_archer.h"
#include "skeleton_swordsman.h"
#include "spearman.h"
#include "swordsman.h"
#include "temple.h"
#include "units/spawn_type.h"
#include "units/unit.h"
#include "wall_gate.h"
#include "wall_ladder.h"
#include "wall_segment.h"
#include "wolf.h"

namespace Game::Units {
namespace {

auto owner_has_living_commander(Engine::Core::World& world, int owner_id) -> bool {
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->owner_id != owner_id || unit->health <= 0) {
      continue;
    }
    // Historical cameos serve beside an owner's commander and never occupy the
    // owner's single commander slot.
    if (const auto* commander = entity->get_component<Engine::Core::CommanderComponent>();
        commander != nullptr && is_historical_commander_id(commander->commander_id)) {
      continue;
    }
    const auto troop_type = spawn_typeToTroopType(unit->spawn_type);
    if (troop_type.has_value() && is_commander_troop(*troop_type)) {
      return true;
    }
  }
  return false;
}

auto resolve_historical_spawn(const SpawnParams& params, SpawnType requested)
    -> SpawnType {
  const auto* definition = historical_commander_definition(params.commander_id);
  if (definition == nullptr) {
    return requested;
  }
  return spawn_typeFromTroopType(
      historical_commander_troop_for_nation(*definition, params.nation_id));
}

} // namespace

auto UnitFactoryRegistry::create(SpawnType type,
                                 Engine::Core::World& world,
                                 const SpawnParams& params) const
    -> std::unique_ptr<Unit> {
  SpawnParams resolved_params = params;
  if (!params.commander_id.empty()) {
    type = resolve_historical_spawn(params, type);
    resolved_params.spawn_type = type;
  }
  auto it = m_factories.find(type);
  if (it == m_factories.end()) {
    return nullptr;
  }
  auto unit = it->second(world, resolved_params);

  apply_forest_passability(world, unit.get(), type);
  apply_historical_commander(world, unit.get(), resolved_params);
  return unit;
}

void UnitFactoryRegistry::apply_historical_commander(Engine::Core::World& world,
                                                     Unit* unit,
                                                     const SpawnParams& params) {
  if (unit == nullptr || params.commander_id.empty()) {
    return;
  }
  auto* entity = world.get_entity(unit->id());
  if (entity == nullptr) {
    return;
  }
  (void)Game::Units::apply_historical_commander(*entity, params.commander_id);
}

void UnitFactoryRegistry::apply_forest_passability(Engine::Core::World& world,
                                                   Unit* unit,
                                                   SpawnType type) {
  if (unit == nullptr) {
    return;
  }
  auto* entity = world.get_entity(unit->id());
  if (entity == nullptr) {
    return;
  }
  if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
    movement->set_can_enter_forest(can_enter_forest(type));
  }
}

void register_built_in_units(UnitFactoryRegistry& reg) {
  reg.register_factory(SpawnType::Archer,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Archer::Create(world, params);
                       });

  reg.register_factory(SpawnType::Slinger,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Archer::Create(world, params);
                       });

  reg.register_factory(SpawnType::Velites,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Archer::Create(world, params);
                       });

  reg.register_factory(SpawnType::Swordsman,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Swordsman::Create(world, params);
                       });

  reg.register_factory(SpawnType::MountedSwordsman,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return MountedSwordsman::Create(world, params);
                       });

  reg.register_factory(SpawnType::Spearman,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Spearman::Create(world, params);
                       });

  reg.register_factory(SpawnType::SkeletonSwordsman,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return SkeletonSwordsman::Create(world, params);
                       });

  reg.register_factory(SpawnType::SkeletonArcher,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return SkeletonArcher::Create(world, params);
                       });

  reg.register_factory(SpawnType::GravePriest,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return GravePriest::Create(world, params);
                       });

  reg.register_factory(SpawnType::HorseArcher,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return HorseArcher::Create(world, params);
                       });

  reg.register_factory(SpawnType::HorseSpearman,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return HorseSpearman::Create(world, params);
                       });

  reg.register_factory(SpawnType::Healer,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Healer::Create(world, params);
                       });

  reg.register_factory(SpawnType::Catapult,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Catapult::Create(world, params);
                       });

  reg.register_factory(SpawnType::Ballista,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Ballista::Create(world, params);
                       });

  reg.register_factory(SpawnType::Ram,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Ram::Create(world, params);
                       });

  reg.register_factory(SpawnType::SiegeTower,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return SiegeTower::Create(world, params);
                       });

  reg.register_factory(SpawnType::Elephant,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Elephant::Create(world, params);
                       });

  auto can_spawn_commander = [](Engine::Core::World& world, const SpawnParams& params) {
    return is_historical_commander_id(params.commander_id) ||
           !owner_has_living_commander(world, params.player_id);
  };
  auto sword_commander_factory =
      [can_spawn_commander](Engine::Core::World& world,
                            const SpawnParams& params) -> std::unique_ptr<Unit> {
    if (!can_spawn_commander(world, params)) {
      return {};
    }
    return Swordsman::Create(world, params);
  };
  auto spear_commander_factory =
      [can_spawn_commander](Engine::Core::World& world,
                            const SpawnParams& params) -> std::unique_ptr<Unit> {
    if (!can_spawn_commander(world, params)) {
      return {};
    }
    return Spearman::Create(world, params);
  };
  auto bow_commander_factory =
      [can_spawn_commander](Engine::Core::World& world,
                            const SpawnParams& params) -> std::unique_ptr<Unit> {
    if (!can_spawn_commander(world, params)) {
      return {};
    }
    return Archer::Create(world, params);
  };
  reg.register_factory(SpawnType::RomanLegionOrganizer, spear_commander_factory);
  reg.register_factory(SpawnType::RomanVeteranConsul, sword_commander_factory);
  reg.register_factory(SpawnType::RomanFieldCommander, bow_commander_factory);
  reg.register_factory(SpawnType::CarthageSpearCommander, spear_commander_factory);
  reg.register_factory(SpawnType::CarthageBowCommander, bow_commander_factory);
  reg.register_factory(SpawnType::CarthageSwordCommander, sword_commander_factory);

  reg.register_factory(SpawnType::Civilian,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Civilian::Create(world, params);
                       });

  reg.register_factory(SpawnType::Builder,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Builder::Create(world, params);
                       });

  reg.register_factory(SpawnType::Barracks,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Barracks::Create(world, params);
                       });

  reg.register_factory(SpawnType::DefenseTower,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return DefenseTower::create(world, params);
                       });

  reg.register_factory(SpawnType::Home,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Home::Create(world, params);
                       });

  reg.register_factory(SpawnType::WallSegment,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return WallSegment::create(world, params);
                       });
  reg.register_factory(SpawnType::WallGate,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return WallGate::create(world, params);
                       });
  reg.register_factory(SpawnType::WallLadder,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return WallLadder::create(world, params);
                       });

  reg.register_factory(SpawnType::Marketplace,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Marketplace::Create(world, params);
                       });

  reg.register_factory(SpawnType::Temple,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Temple::Create(world, params);
                       });

  reg.register_factory(SpawnType::Farm,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Farm::Create(world, params);
                       });

  reg.register_factory(SpawnType::Sheep,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Sheep::Create(world, params);
                       });

  reg.register_factory(SpawnType::Wolf,
                       [](Engine::Core::World& world, const SpawnParams& params) {
                         return Wolf::Create(world, params);
                       });
}

} // namespace Game::Units
