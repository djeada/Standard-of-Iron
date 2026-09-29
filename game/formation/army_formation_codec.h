#pragma once

#include <QJsonObject>

#include "army_formation_types.h"

namespace Game::Formation::Codec {

[[nodiscard]] auto formation_to_json(const ArmyFormation& formation) -> QJsonObject;

[[nodiscard]] auto formation_from_json(const QJsonObject& obj) -> ArmyFormation;

} // namespace Game::Formation::Codec
