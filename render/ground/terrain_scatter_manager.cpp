#include "terrain_scatter_manager.h"

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain_service.h"
#include "../ground/biome_renderer.h"
#include "../ground/firecamp_renderer.h"
#include "../ground/olive_renderer.h"
#include "../ground/pine_renderer.h"
#include "../ground/plant_renderer.h"
#include "../ground/stone_renderer.h"
#include <QVector3D>
#include <tuple>
#include <utility>

namespace Render::GL {

namespace {

auto convert_fire_camps(const std::vector<Game::Map::FireCamp> &fire_camps,
                        const Game::Map::TerrainHeightMap &height_map)
    -> std::tuple<std::vector<QVector3D>, std::vector<float>,
                  std::vector<float>> {
  std::vector<QVector3D> positions;
  std::vector<float> intensities;
  std::vector<float> radii;

  positions.reserve(fire_camps.size());
  intensities.reserve(fire_camps.size());
  radii.reserve(fire_camps.size());

  const float tile_size = height_map.getTileSize();
  const int width = height_map.getWidth();
  const int height = height_map.getHeight();
  const float half_width = width * 0.5F;
  const float half_height = height * 0.5F;
  auto &terrain_service = Game::Map::TerrainService::instance();

  for (const auto &fire_camp : fire_camps) {
    const float world_x = (fire_camp.x - half_width) * tile_size;
    const float world_z = (fire_camp.z - half_height) * tile_size;
    positions.push_back(terrain_service.resolve_surface_world_position(
        world_x, world_z, 0.0F, 0.0F));
    intensities.push_back(fire_camp.intensity);
    radii.push_back(fire_camp.radius);
  }

  return {std::move(positions), std::move(intensities), std::move(radii)};
}

} // namespace

TerrainScatterManager::TerrainScatterManager()
    : m_biome(std::make_unique<BiomeRenderer>()),
      m_stone(std::make_unique<StoneRenderer>()),
      m_plant(std::make_unique<PlantRenderer>()),
      m_pine(std::make_unique<PineRenderer>()),
      m_olive(std::make_unique<OliveRenderer>()),
      m_firecamp(std::make_unique<FireCampRenderer>()),
      m_passes{m_biome.get(), m_stone.get(), m_plant.get(),
               m_pine.get(),  m_olive.get(), m_firecamp.get()} {}

TerrainScatterManager::~TerrainScatterManager() = default;

void TerrainScatterManager::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings,
    const std::vector<Game::Map::FireCamp> &fire_camps) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_biome->configure(height_map, biome_settings);
  m_stone->configure(height_map, biome_settings);
  m_plant->configure(height_map, biome_settings);
  m_pine->configure(height_map, biome_settings);
  m_olive->configure(height_map, biome_settings);
  m_firecamp->configure(height_map, biome_settings);

  const auto [positions, intensities, radii] =
      convert_fire_camps(fire_camps, height_map);
  m_firecamp->setExplicitFireCamps(positions, intensities, radii);
}

void TerrainScatterManager::submit(Renderer &renderer,
                                   ResourceManager *resources) {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (auto *pass : m_passes) {
    if (pass != nullptr) {
      pass->submit(renderer, resources);
    }
  }
}

void TerrainScatterManager::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_biome->clear();
  m_stone->clear();
  m_plant->clear();
  m_pine->clear();
  m_olive->clear();
  m_firecamp->clear();
}

void TerrainScatterManager::refresh_grass() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_biome->refresh_grass();
}

auto TerrainScatterManager::is_gpu_ready() const -> bool {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_biome->is_gpu_ready() && m_stone->is_gpu_ready() &&
         m_plant->is_gpu_ready() && m_pine->is_gpu_ready() &&
         m_olive->is_gpu_ready() && m_firecamp->is_gpu_ready();
}

auto TerrainScatterManager::biome() const -> BiomeRenderer * {
  return m_biome.get();
}

auto TerrainScatterManager::stone() const -> StoneRenderer * {
  return m_stone.get();
}

auto TerrainScatterManager::plant() const -> PlantRenderer * {
  return m_plant.get();
}

auto TerrainScatterManager::pine() const -> PineRenderer * {
  return m_pine.get();
}

auto TerrainScatterManager::olive() const -> OliveRenderer * {
  return m_olive.get();
}

auto TerrainScatterManager::firecamp() const -> FireCampRenderer * {
  return m_firecamp.get();
}

auto TerrainScatterManager::passes() const
    -> const std::vector<IRenderPass *> & {
  return m_passes;
}

} // namespace Render::GL
