#include "factory.h"
#include "archer.h"

namespace Game { namespace Units {

void registerBuiltInUnits(UnitFactoryRegistry& reg) {
    reg.registerFactory("archer", [](Engine::Core::World& world, const SpawnParams& params){
        return Archer::Create(world, params);
    });
}

} } // namespace Game::Units
