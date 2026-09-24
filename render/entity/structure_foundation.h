#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "game/units/spawn_type.h"

namespace Game::Map {
class TerrainService;
}

namespace Render::GL {

class ISubmitter;
class Mesh;
class Texture;

// Structures stand upright, seated at the terrain under their centre. On a
// slope the downhill side of the base would hover above the ground and the
// uphill side is buried, so a structure whose footprint falls away gets a
// fieldstone foundation reaching down to the lowest ground under its drawn
// body. Completed buildings, construction sites and placement ghosts all go
// through here, so they rest on the ground the same way.
struct StructureFoundation {
  // World-space distance from the seat (the model origin) down to the lowest
  // terrain under the drawn body; zero on level ground.
  float depth{0.0F};
  // Local (mesh-unit) centre and half extents of the drawn body.
  float center_x{0.0F};
  float center_z{0.0F};
  float half_width{0.0F};
  float half_depth{0.0F};
};

// Below this the downhill gap cannot be seen at gameplay zoom, and the gentle
// relief of level maps would otherwise put a stone rim under every building.
inline constexpr float k_structure_foundation_min_depth = 0.06F;
// Deep enough to reach the foot of any ridge edge a structure can be put on;
// a foundation that stops short hangs in the air, which reads worse than a tall
// retaining wall.
inline constexpr float k_structure_foundation_max_depth = 4.0F;

[[nodiscard]] auto
resolve_structure_foundation(const Game::Map::TerrainService& terrain,
                             Game::Units::SpawnType spawn_type,
                             const QMatrix4x4& model) -> StructureFoundation;

void submit_structure_foundation(const StructureFoundation& foundation,
                                 const QMatrix4x4& model,
                                 ISubmitter& out,
                                 Mesh* unit_cube,
                                 Texture* white,
                                 float alpha = 1.0F);

} // namespace Render::GL
