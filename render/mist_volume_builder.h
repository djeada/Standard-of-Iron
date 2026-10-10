#pragma once

#include <functional>
#include <vector>

#include "game/map/map_definition.h"
#include "game/map/terrain_features.h"
#include "mist_volume.h"

namespace Render {

inline constexpr float k_water_mist_strength = 0.42F;
inline constexpr float k_miasma_strength_base = 0.30F;
inline constexpr float k_miasma_strength_per_density = 0.55F;

using MistSurfaceHeight = std::function<float(float world_x, float world_z)>;

struct MistSources {
  const std::vector<Game::Map::FogZone>* fog_zones = nullptr;
  const std::vector<Game::Map::RiverSegment>* rivers = nullptr;
  const std::vector<Game::Map::Lake>* lakes = nullptr;
};

[[nodiscard]] auto
build_mist_volumes(const MistSources& sources,
                   const MistSurfaceHeight& surface_y) -> std::vector<MistVolume>;

[[nodiscard]] auto
merge_mist_volumes(std::vector<MistVolume> priority,
                   const std::vector<MistVolume>& rest) -> std::vector<MistVolume>;

} // namespace Render
