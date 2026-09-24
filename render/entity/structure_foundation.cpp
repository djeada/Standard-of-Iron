#include "structure_foundation.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "game/map/terrain_service.h"
#include "game/units/building_body.h"
#include "render/gl/primitives.h"
#include "render/submitter.h"

namespace Render::GL {

namespace {

constexpr int k_foundation_samples = 5;

constexpr float k_foundation_inset = 0.96F;

constexpr float k_foundation_top_lift = 0.012F;
constexpr float k_foundation_bottom_margin = 0.08F;

const QVector3D k_foundation_color(0.47F, 0.43F, 0.37F);

} // namespace

auto resolve_structure_foundation(const Game::Map::TerrainService& terrain,
                                  Game::Units::SpawnType spawn_type,
                                  const QMatrix4x4& model) -> StructureFoundation {
  StructureFoundation foundation;
  if (!terrain.is_initialized() || !Game::Units::is_building_spawn(spawn_type)) {
    return foundation;
  }

  auto const body =
      Game::Units::find_building_body_extent(Game::Units::spawn_type_name(spawn_type));
  if (!body.has_value()) {
    return foundation;
  }
  foundation.center_x = body->offset_x;
  foundation.center_z = body->offset_z;
  foundation.half_width = std::max(body->width, 0.0F) * 0.5F;
  foundation.half_depth = std::max(body->depth, 0.0F) * 0.5F;
  if (foundation.half_width <= 0.0F || foundation.half_depth <= 0.0F) {
    return foundation;
  }

  QVector3D const seat = model.map(QVector3D(0.0F, 0.0F, 0.0F));
  float lowest = seat.y();
  for (int iz = 0; iz < k_foundation_samples; ++iz) {
    float const v = -1.0F + 2.0F * static_cast<float>(iz) /
                                static_cast<float>(k_foundation_samples - 1);
    for (int ix = 0; ix < k_foundation_samples; ++ix) {
      float const u = -1.0F + 2.0F * static_cast<float>(ix) /
                                  static_cast<float>(k_foundation_samples - 1);
      QVector3D const local(foundation.center_x + u * foundation.half_width,
                            0.0F,
                            foundation.center_z + v * foundation.half_depth);
      QVector3D const world = model.map(local);
      lowest = std::min(
          lowest,
          terrain.resolve_surface_world_y(world.x(), world.z(), 0.0F, seat.y()));
    }
  }

  float const depth = seat.y() - lowest;
  if (depth >= k_structure_foundation_min_depth) {
    foundation.depth = std::min(depth, k_structure_foundation_max_depth);
  }
  return foundation;
}

void submit_structure_foundation(const StructureFoundation& foundation,
                                 const QMatrix4x4& model,
                                 ISubmitter& out,
                                 Mesh* unit_cube,
                                 Texture* white,
                                 float alpha) {
  if (foundation.depth <= 0.0F || foundation.half_width <= 0.0F ||
      foundation.half_depth <= 0.0F) {
    return;
  }
  Mesh* const cube = unit_cube != nullptr ? unit_cube : get_unit_cube();
  if (cube == nullptr) {
    return;
  }

  float const up_scale = std::max(model.column(1).toVector3D().length(), 1.0e-4F);
  float const top = k_foundation_top_lift / up_scale;
  float const bottom = -(foundation.depth + k_foundation_bottom_margin) / up_scale;

  QMatrix4x4 local;
  local.translate(foundation.center_x, (top + bottom) * 0.5F, foundation.center_z);
  local.scale(foundation.half_width * k_foundation_inset,
              (top - bottom) * 0.5F,
              foundation.half_depth * k_foundation_inset);
  out.mesh(cube, model * local, k_foundation_color, white, alpha);
}

} // namespace Render::GL
