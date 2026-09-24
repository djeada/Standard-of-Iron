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

struct StructureFoundation {

  float depth{0.0F};

  float center_x{0.0F};
  float center_z{0.0F};
  float half_width{0.0F};
  float half_depth{0.0F};
};

inline constexpr float k_structure_foundation_min_depth = 0.06F;

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
