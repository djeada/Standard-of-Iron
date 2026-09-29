#pragma once

#include <QJsonObject>

#include "undead_zone_runtime.h"

namespace Game::Systems {

[[nodiscard]] auto serialize_undead_zone(const UndeadRuntimeZone& zone) -> QJsonObject;

void restore_undead_zone(UndeadRuntimeZone& zone, const QJsonObject& saved);

} // namespace Game::Systems
