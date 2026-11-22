#include <QVector4D>

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "game/map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/math/render_constants.h"

namespace Render::Ground::Scatter {

using Render::GL::FireCampInstanceGpu;
using Render::GL::FireCampRenderer;
using Render::GL::MathConstants::k_two_pi;

auto generate_firecamps(const ScatterRequest& request) -> FireCampScatter {
  FireCampScatter out;
  if (request.grid.empty()) {
    return out;
  }
  const int grid_width = request.grid.width;
  const int grid_height = request.grid.height;
  const auto& height_data = *request.grid.heights;
  const auto& world_props = request.world_props;
  auto& firecamp_instances = out.instances;

  if (grid_width < 2 || grid_height < 2 || height_data.empty()) {
    return out;
  }
  const auto& terrain_service = request.terrain;

  for (size_t i = 0; i < world_props.size(); ++i) {
    const auto& prop = world_props[i];
    if (prop.type != Game::Map::WorldProp::Type::FireCamp) {
      continue;
    }

    const auto [world_x, world_z] = terrain_service.world_prop_world_xz(prop);
    const float hearth = FireCampRenderer::hearth_radius(prop.scale);
    const float centre_y = terrain_service.resolve_surface_world_y(world_x, world_z);

    constexpr int k_spokes = 8;
    float slope_x = 0.0F;
    float slope_z = 0.0F;
    std::array<float, k_spokes> rim_heights{};
    for (int spoke = 0; spoke < k_spokes; ++spoke) {
      const float angle =
          k_two_pi * static_cast<float>(spoke) / static_cast<float>(k_spokes);
      const float rise =
          terrain_service.resolve_surface_world_y(world_x + std::cos(angle) * hearth,
                                                  world_z + std::sin(angle) * hearth) -
          centre_y;
      rim_heights[static_cast<std::size_t>(spoke)] = rise;
      slope_x += rise * std::cos(angle);
      slope_z += rise * std::sin(angle);
    }
    slope_x /= hearth * static_cast<float>(k_spokes) * 0.5F;
    slope_z /= hearth * static_cast<float>(k_spokes) * 0.5F;
    float bulge = 0.0F;
    for (int spoke = 0; spoke < k_spokes; ++spoke) {
      const float angle =
          k_two_pi * static_cast<float>(spoke) / static_cast<float>(k_spokes);
      const float plane =
          (slope_x * std::cos(angle) + slope_z * std::sin(angle)) * hearth;
      bulge = std::max(bulge, rim_heights[static_cast<std::size_t>(spoke)] - plane);
    }

    const float base_radius = std::max(prop.radius, 1.0F);
    const float phase = static_cast<float>(i) * 1.234567F;

    const auto decor_index = static_cast<float>(out.decor.size());
    out.decor.push_back(FireCampRenderer::build_camp_decor(
        terrain_service, world_x, world_z, hearth, phase));

    FireCampInstanceGpu instance;
    instance.pos_intensity = QVector4D(world_x, centre_y, world_z, prop.intensity);
    instance.radius_phase = QVector4D(base_radius, phase, prop.scale, decor_index);
    instance.ground = QVector4D(slope_x, slope_z, hearth, std::min(bulge, 0.3F));
    firecamp_instances.push_back(instance);
  }
  return out;
}

} // namespace Render::Ground::Scatter
