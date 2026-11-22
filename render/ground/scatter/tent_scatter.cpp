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

constexpr float k_tent_footprint_radius = 1.15F;

} // namespace

auto generate_tents(const ScatterRequest& request) -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Tent) {
      continue;
    }
    const float tent_scale = prop.scale * Game::Map::world_prop_render_scale(
                                              Game::Map::WorldProp::Type::Tent);
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, tent_scale * k_tent_footprint_radius);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0xA7124D93U);

    float const dye = rand_01(state);
    QVector3D canvas_color;
    if (dye < 0.16F) {
      canvas_color = QVector3D(0.46F, 0.15F, 0.11F);
    } else if (dye < 0.46F) {
      canvas_color = QVector3D(0.58F, 0.44F, 0.26F);
    } else if (dye < 0.58F) {
      canvas_color = QVector3D(0.18F, 0.25F, 0.32F);
    } else if (dye < 0.70F) {
      canvas_color = QVector3D(0.30F, 0.33F, 0.20F);
    } else {
      canvas_color = QVector3D(0.44F, 0.30F, 0.19F);
    }
    canvas_color *= remap(rand_01(state), 0.88F, 1.06F);

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(), resolved.y(), resolved.z(), tent_scale);
    inst.color_rot =
        QVector4D(canvas_color.x(), canvas_color.y(), canvas_color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
