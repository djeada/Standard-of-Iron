#pragma once

#include <QString>
#include <QVariantMap>

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class NationRegistry;
}

namespace App::Economy {

[[nodiscard]] auto selected_barracks_state(Engine::Core::World* world,
                                           int local_owner_id) -> QVariantMap;

[[nodiscard]] auto selected_builder_state(Engine::Core::World* world) -> QVariantMap;

[[nodiscard]] auto selected_farm_state(Engine::Core::World* world,
                                       int local_owner_id) -> QVariantMap;

[[nodiscard]] auto selected_marketplace_state(Engine::Core::World* world,
                                              int local_owner_id) -> QVariantMap;

[[nodiscard]] auto selected_home_state(Engine::Core::World* world,
                                       int local_owner_id) -> QVariantMap;

[[nodiscard]] auto selected_temple_state(Engine::Core::World* world,
                                         int local_owner_id) -> QVariantMap;

[[nodiscard]] auto unit_production_info(const Game::Systems::NationRegistry& nations,
                                        const QString& unit_type,
                                        const QString& nation_id) -> QVariantMap;

[[nodiscard]] auto construction_info(const QString& item_type) -> QVariantMap;

} // namespace App::Economy
