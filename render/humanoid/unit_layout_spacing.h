#pragma once

#include "../../game/units/spawn_type.h"

namespace Render::GL {

[[nodiscard]] auto cavalry_formation_spacing(float mount_scale = 1.0F) -> float;

[[nodiscard]] auto resolve_formation_spacing(Game::Units::SpawnType spawn_type,
                                             float configured_spacing = 0.0F,
                                             float mount_scale = 1.0F) -> float;

} // namespace Render::GL
