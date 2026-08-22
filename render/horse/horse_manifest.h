#pragma once

#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Horse {

[[nodiscard]] auto
horse_runtime_manifest() noexcept -> const Render::Creature::CreatureRuntimeManifest&;

}
