#pragma once

#include "resource_types.h"

class QJsonObject;

namespace Game::Systems {

[[nodiscard]] auto read_resource_overlay(const QJsonObject& obj) -> ResourceOverlay;

} // namespace Game::Systems
