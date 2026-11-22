#include "render/ground/plant_renderer.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_resources.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/terrain_contact.h"
#include "render/ground/ground_ecology.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/scatter_submission.h"
#include "render/math/render_constants.h"

namespace Render::GL {

PlantRenderer::PlantRenderer() = default;
PlantRenderer::~PlantRenderer() = default;

void PlantRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings,
                              const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);

  const auto profiles = Game::Map::make_biome_profiles(m_biome_settings);
  const auto& wind_profile = profiles.wind;
  auto& plant_params = m_state.params;
  plant_params.light_direction = m_light_direction;
  plant_params.time = 0.0F;
  plant_params.wind_strength = wind_profile.sway_strength;
  plant_params.wind_speed = wind_profile.sway_speed;

  m_ecology = Render::Ground::shared_ground_ecology(
      world().terrain(), height_map, m_noise_seed);
  generate_plant_instances();
}

void PlantRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, FoliageBatchParams::default_light_direction());
}

void PlantRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  submit_filtered_common<true>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Plant,
      [](TerrainScatterCmd& cmd, const FoliageBatchParams& params) {
        cmd.foliage = params;
      });
}

void PlantRenderer::generate_plant_instances() {
  adopt_generated(Ground::Scatter::generate_plants({scatter_grid(),
                                                    m_ecology.get(),
                                                    m_biome_settings,
                                                    world().terrain_or_empty(),
                                                    m_world_props}));
}

} // namespace Render::GL
