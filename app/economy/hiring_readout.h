#pragma once

#include <QVariantMap>

namespace Engine::Core {
class World;
}

namespace App::Economy {

[[nodiscard]] auto selected_hiring_state(Engine::Core::World* world,
                                         int local_owner_id) -> QVariantMap;

} // namespace App::Economy
