#include "factory.h"
#include "archer.h"
#include "barracks.h"
#include "knight.h"
#include "mounted_knight.h"
#include "spearman.h"
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
    return Knight::Create(world, params);
  });

  reg.registerFactory(SpawnType::MountedKnight, [](Engine::Core::World &world,
                                                   const SpawnParams &params) {
    return MountedKnight::Create(world, params);
  });

  reg.registerFactory(SpawnType::Spearman, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Spearman::Create(world, params);
  });

  reg.registerFactory(SpawnType::Barracks, [](Engine::Core::World &world,
                                              const SpawnParams &params) {
    return Barracks::Create(world, params);
  });
}

} // namespace Game::Units
