#include "factory.h"
#include "archer.h"
#include "barracks.h"
#include "knight.h"
#include "mounted_knight.h"
#include "spearman.h"

namespace Game {
namespace Units {

void registerBuiltInUnits(UnitFactoryRegistry &reg) {
  reg.registerFactory(
      "archer", [](Engine::Core::World &world, const SpawnParams &params) {
        return Archer::Create(world, params);
      });
  reg.registerFactory(
      "knight", [](Engine::Core::World &world, const SpawnParams &params) {
        return Knight::Create(world, params);
      });
  reg.registerFactory("mounted_knight", [](Engine::Core::World &world,
                                           const SpawnParams &params) {
    return MountedKnight::Create(world, params);
  });
  reg.registerFactory(
      "spearman", [](Engine::Core::World &world, const SpawnParams &params) {
        return Spearman::Create(world, params);
      });
  reg.registerFactory(
      "barracks", [](Engine::Core::World &world, const SpawnParams &params) {
        return Barracks::Create(world, params);
      });
}

} // namespace Units
} // namespace Game
