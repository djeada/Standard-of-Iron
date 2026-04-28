#pragma once

#include "../creature/species_manifest.h"

namespace Render::Horse {

[[nodiscard]] auto
horse_manifest() noexcept -> const Render::Creature::SpeciesManifest &;

}
