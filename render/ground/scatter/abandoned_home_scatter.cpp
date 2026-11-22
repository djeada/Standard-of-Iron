#include <QVector3D>
#include <QVector4D>

#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace Render::Ground::Scatter {

namespace {

using namespace Render::Ground;
using Render::GL::PropInstanceGpu;
using std::uint32_t;

} // namespace

auto generate_abandoned_homes(const ScatterRequest& request)
    -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::AbandonedHome) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0x5C21A93FU);

    QVector3D const daub(1.16F, 1.02F, 0.82F);
    QVector3D const soot(0.58F, 0.52F, 0.46F);
    float const burn_mix = remap(rand_01(state), 0.10F, 0.40F);
    QVector3D color = daub * (1.0F - burn_mix) + soot * burn_mix;
    color *= remap(rand_01(state), 0.90F, 1.08F);

    PropInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(),
                  resolved.y(),
                  resolved.z(),
                  prop.scale * Game::Map::world_prop_render_scale(
                                   Game::Map::WorldProp::Type::AbandonedHome));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
