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

auto generate_statues(const ScatterRequest& request) -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Statue) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0x1D7B4E63U);

    QVector3D const pentelic_marble(0.955F, 0.940F, 0.900F);
    QVector3D const travertine(0.905F, 0.845F, 0.735F);
    QVector3D const grey_limestone(0.790F, 0.795F, 0.780F);

    float const quarry = rand_01(state);
    QVector3D color =
        quarry < 0.30F
            ? pentelic_marble
            : (quarry < 0.74F ? pentelic_marble * 0.45F + travertine * 0.55F
                              : pentelic_marble * 0.35F + grey_limestone * 0.65F);
    float const weathering = remap(rand_01(state), 0.0F, 0.14F);
    color *= 1.0F - weathering;
    color *= remap(rand_01(state), 0.96F, 1.04F);

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Statue));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
