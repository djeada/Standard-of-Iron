#pragma once

#include "unit.h"

namespace Game { namespace Units {

class Archer : public Unit {
public:
    // Create a new Archer unit in the given world with spawn params
    static std::unique_ptr<Archer> Create(Engine::Core::World& world, const SpawnParams& params);

private:
    Archer(Engine::Core::World& world);
    void init(const SpawnParams& params);
};

} } // namespace Game::Units
