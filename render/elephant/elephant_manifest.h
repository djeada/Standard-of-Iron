#pragma once

#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Elephant {

[[nodiscard]] auto elephant_runtime_manifest() noexcept
    -> const Render::Creature::CreatureRuntimeManifest&;

}
