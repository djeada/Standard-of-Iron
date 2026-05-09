#include "factory.h"
#include "archer.h"
#include "ballista.h"
#include "barracks.h"
#include "builder.h"
#include "catapult.h"
#include "civilian.h"
#include "defense_tower.h"
#include "elephant.h"
#include "healer.h"
#include "home.h"
#include "horse_archer.h"
#include "horse_spearman.h"
#include "horse_swordsman.h"
#include "spearman.h"
#include "swordsman.h"
#include "units/spawn_type.h"
#include "units/unit.h"

namespace Game::Units {

void register_built_in_units(UnitFactoryRegistry &reg) {
  reg.register_factory(SpawnType::Archer, [](Engine::Core::World &world,
                                            const SpawnParams &params) {
    return Archer::Create(world, params);
  });

  reg.register_factory(SpawnType::Knight, [](Engine::Core::World &world,
                                            const SpawnParams &params) {
    return Swordsman::Create(world, params);
  });

  reg.register_factory(SpawnType::MountedKnight, [](Engine::Core::World &world,
                                                   const SpawnParams &params) {
    return MountedKnight::Create(world, params);
  });

  reg.register_factory(SpawnType::Spearman, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Spearman::Create(world, params);
  });

  reg.register_factory(SpawnType::HorseArcher, [](Engine::Core::World &world,
                                                 const SpawnParams &params) {
    return HorseArcher::Create(world, params);
  });

  reg.register_factory(SpawnType::HorseSpearman, [](Engine::Core::World &world,
                                                   const SpawnParams &params) {
    return HorseSpearman::Create(world, params);
  });

  reg.register_factory(SpawnType::Healer, [](Engine::Core::World &world,
                                            const SpawnParams &params) {
    return Healer::Create(world, params);
  });

  reg.register_factory(SpawnType::Catapult, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Catapult::Create(world, params);
  });

  reg.register_factory(SpawnType::Ballista, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Ballista::Create(world, params);
  });

  reg.register_factory(SpawnType::Elephant, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Elephant::Create(world, params);
  });

  auto commander_factory = [](Engine::Core::World &world,
                              const SpawnParams &params) {
    return Swordsman::Create(world, params);
  };
  reg.register_factory(SpawnType::RomanLegionOrganizer, commander_factory);
  reg.register_factory(SpawnType::RomanVeteranConsul, commander_factory);
  reg.register_factory(SpawnType::RomanFieldCommander, commander_factory);
  reg.register_factory(SpawnType::CarthageMercenaryBroker, commander_factory);
  reg.register_factory(SpawnType::CarthageCavalryPatron, commander_factory);
  reg.register_factory(SpawnType::CarthageElephantMaster, commander_factory);

  reg.register_factory(SpawnType::Civilian, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Civilian::Create(world, params);
  });

  reg.register_factory(SpawnType::Builder, [](Engine::Core::World &world,
                                             const SpawnParams &params) {
    return Builder::Create(world, params);
  });

  reg.register_factory(SpawnType::Barracks, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Barracks::Create(world, params);
  });

  reg.register_factory(SpawnType::DefenseTower, [](Engine::Core::World &world,
                                                  const SpawnParams &params) {
    return DefenseTower::create(world, params);
  });

  reg.register_factory(SpawnType::Home, [](Engine::Core::World &world,
                                          const SpawnParams &params) {
    return Home::Create(world, params);
  });
}

} // namespace Game::Units
