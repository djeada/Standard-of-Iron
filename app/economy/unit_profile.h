#pragma once

#include <QString>
#include <QVariantMap>

namespace Game::Systems {
class NationRegistry;
}

namespace App::Economy {

[[nodiscard]] auto unit_profile(const Game::Systems::NationRegistry& nations,
                                const QString& unit_type,
                                const QString& nation_id) -> QVariantMap;

} // namespace App::Economy
