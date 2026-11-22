#include "render/ground/biome_renderer.h"

#include <QDebug>
#include <QVector2D>
#include <qglobal.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_resources.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/graphics_settings.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/scatter_submission.h"
#include "render/math/render_constants.h"

namespace {

using namespace Render::Ground;

constexpr float k_grass_full_screen_fraction = 0.0040F;
constexpr float k_grass_min_screen_fraction = 0.0018F;
constexpr float k_grass_cull_screen_fraction = 0.0010F;
constexpr float k_grass_min_density = 0.04F;

} // namespace

namespace Render::GL {

BiomeRenderer::BiomeRenderer() = default;
BiomeRenderer::~BiomeRenderer() = default;

void BiomeRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings) {
  configure_height_scatter_common(height_map, biome_settings, {});
  const auto profiles = Game::Map::make_biome_profiles(m_biome_settings);
  const auto& scatter_profile = profiles.scatter;
  const auto& wind_profile = profiles.wind;
  auto& grass_params = m_state.params;

  grass_params.soil_color = scatter_profile.soil_color;
  grass_params.wind_strength = wind_profile.sway_strength;
  grass_params.wind_speed = wind_profile.sway_speed;
  grass_params.light_direction = m_light_direction;
  grass_params.time = 0.0F;
  grass_params.ambient_boost = profiles.surface.terrain_ambient_boost * 0.95F;

  generate_grass_instances();
}

void BiomeRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction = dir.isNull() ? QVector3D(0.35F, 0.8F, 0.45F) : dir.normalized();
  m_state.params.light_direction = m_light_direction;
}

void BiomeRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  Q_UNUSED(resources);
  const auto visible_count = Scatter::sync_filtered_state(
      m_state,
      [](const GrassInstanceGpu& instance) -> const QVector4D& {
        return instance.pos_height;
      },
      nullptr,
      Scatter::ScatterMemoryMode::Remembered);
  if (visible_count == 0) {
    return;
  }

  GrassBatchParams params = m_state.params;
  params.time = renderer.get_animation_time();
  TerrainScatterCmd cmd;
  cmd.visibility = renderer.visibility_mask();
  cmd.species = TerrainScatterCmd::Species::Grass;
  cmd.grass = params;

  const auto* camera = renderer.camera();
  if (camera == nullptr || m_typical_blade_height <= 0.0F) {
    Scatter::submit_visible_chunks(renderer, m_state, cmd);
    return;
  }

  const QVector3D camera_position = camera->get_position();
  const float tan_half_fov = std::tan(std::clamp(camera->get_fov(), 1.0F, 179.0F) *
                                      0.5F * std::numbers::pi_v<float> / 180.0F);
  const float blade_height = m_typical_blade_height;

  Scatter::submit_visible_chunks_lod(
      renderer,
      m_state,
      cmd,
      [&](const QVector3D& chunk_center, std::size_t chunk_size) -> std::size_t {
        const float distance =
            std::max((chunk_center - camera_position).length(), 0.001F);
        const float screen_fraction =
            blade_height / std::max(2.0F * distance * tan_half_fov, 1e-6F);
        if (screen_fraction < k_grass_cull_screen_fraction) {
          return 0;
        }
        const float detail = smoothstep(
            k_grass_min_screen_fraction, k_grass_full_screen_fraction, screen_fraction);
        const float keep = k_grass_min_density + (1.0F - k_grass_min_density) * detail;
        if (keep >= 0.999F) {
          return chunk_size;
        }
        return std::max<std::size_t>(
            1U, static_cast<std::size_t>(float(chunk_size) * keep));
      });
}

void BiomeRenderer::refresh_grass() {
  generate_grass_instances();
}

auto BiomeRenderer::grass_matches_graphics_profile() const noexcept -> bool {
  const float wanted =
      std::clamp(GraphicsSettings::instance().profile().grass_density, 0.0F, 1.0F);
  return m_generated_grass_density < 0.0F ||
         std::abs(m_generated_grass_density - wanted) < 1e-4F;
}

void BiomeRenderer::generate_grass_instances() {
  auto grass = Ground::Scatter::generate_grass(
      {scatter_grid(), nullptr, m_biome_settings, world().terrain_or_empty(), {}},
      GraphicsSettings::instance().profile().grass_density);
  if (grass.generated_density >= 0.0F) {
    m_generated_grass_density = grass.generated_density;
  }
  if (grass.typical_blade_height >= 0.0F) {
    m_typical_blade_height = grass.typical_blade_height;
  }
  adopt_generated(std::move(grass.instances));
  if (m_state.instance_count > 0) {
    m_state.visibility_dirty = true;
  }
}

} // namespace Render::GL
