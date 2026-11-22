#include <QVector3D>
#include <QVector4D>

#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/terrain_contact.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace Render::Ground::Scatter {

namespace {

using namespace Render::Ground;
using Render::GL::PropInstanceGpu;
using std::uint32_t;

} // namespace

auto generate_iron_ore(const ScatterRequest& request) -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;

  const auto surface_profile = Game::Map::make_surface_profile(request.biome);

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::IronOre) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_world_position(prop);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0xB7C3D1A4U);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D color = surface_profile.rock_low * (1.0F - color_var) +
                      surface_profile.rock_high * color_var;

    color *= 0.95F;
    QVector3D const iron_tint(0.24F, 0.25F, 0.30F);
    float const iron_mix = remap(rand_01(state), 0.32F, 0.54F);
    color = color * (1.0F - iron_mix) + iron_tint * iron_mix;

    float const ground_radius = Game::Map::world_prop_ground_radius(
        Game::Map::WorldProp::Type::IronOre, prop.scale);
    float const bedded_y = Render::bedded_prop_world_y(terrain_service,
                                                       resolved.x(),
                                                       resolved.z(),
                                                       resolved.y(),
                                                       ground_radius * 0.8F,
                                                       ground_radius * 0.6F);

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               bedded_y,
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::IronOre));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
