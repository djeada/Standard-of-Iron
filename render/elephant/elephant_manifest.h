#pragma once

#include "render/creature/species_manifest.h"

namespace Render::Elephant {

[[nodiscard]] auto
elephant_manifest() noexcept -> const Render::Creature::SpeciesManifest&;

}
