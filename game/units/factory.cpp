#include "factory.h"
#include "archer.h"
#include "ballista.h"
#include "barracks.h"
#include "builder.h"
#include "catapult.h"
#include "defense_tower.h"
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

void registerBuiltInUnits(UnitFactoryRegistry &reg) {
  reg.registerFactory(SpawnType::Archer, [](Engine::Core::World &world,
                                            const SpawnParams &params) {
    return Archer::Create(world, params);
  });

  reg.registerFactory(SpawnType::Knight, [](Engine::Core::World &world,
                                            const SpawnParams &params) {
    return Swordsman::Create(world, params);
  });

  reg.registerFactory(SpawnType::MountedKnight, [](Engine::Core::World &world,
                                                   const SpawnParams &params) {
    return MountedKnight::Create(world, params);
  });

  reg.registerFactory(SpawnType::Spearman, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Spearman::Create(world, params);
  });

  reg.registerFactory(SpawnType::HorseArcher, [](Engine::Core::World &world,
                                                 const SpawnParams &params) {
    return HorseArcher::Create(world, params);
  });

  reg.registerFactory(SpawnType::HorseSpearman, [](Engine::Core::World &world,
                                                   const SpawnParams &params) {
    return HorseSpearman::Create(world, params);
  });

  reg.registerFactory(SpawnType::Healer, [](Engine::Core::World &world,
                                            const SpawnParams &params) {
    return Healer::Create(world, params);
  });

  reg.registerFactory(SpawnType::Catapult, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Catapult::Create(world, params);
  });

  reg.registerFactory(SpawnType::Ballista, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Ballista::Create(world, params);
  });

  reg.registerFactory(SpawnType::Builder, [](Engine::Core::World &world,
                                             const SpawnParams &params) {
    return Builder::Create(world, params);
  });

  reg.registerFactory(SpawnType::Barracks, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Barracks::Create(world, params);
  });

  reg.registerFactory(SpawnType::DefenseTower, [](Engine::Core::World &world,
                                                  const SpawnParams &params) {
    return DefenseTower::create(world, params);
  });

  reg.registerFactory(SpawnType::Home, [](Engine::Core::World &world,
                                          const SpawnParams &params) {
    return Home::Create(world, params);
  });
}

} // namespace Game::Units
