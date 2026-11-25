#pragma once

#include <QVariantMap>

namespace Engine::Core {
class World;
}

namespace App::Economy {

// The selected Mercenary Camp or Elephant Stables (NATION_ASYMMETRY.md 4.2,
// 4.3): what it is hiring, the bands or beasts it offers with their gold price
// and upkeep, and what the owner's hired bands cost a minute.
[[nodiscard]] auto selected_hiring_state(Engine::Core::World* world,
                                         int local_owner_id) -> QVariantMap;

} // namespace App::Economy
