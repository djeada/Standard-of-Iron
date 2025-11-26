#include "firecamp_renderer.h"
#include "../../game/map/terrain_service.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/building_collision_registry.h"
#include "../gl/buffer.h"
#include "../scene_renderer.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground/firecamp_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include <QDebug>
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <qglobal.h>
#include <qvectornd.h>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

inline auto valueNoise(float x, float z, uint32_t seed) -> float {
  int const ix = static_cast<int>(std::floor(x));
  int const iz = static_cast<int>(std::floor(z));
  float fx = x - static_cast<float>(ix);
  float fz = z - static_cast<float>(iz);

  fx = fx * fx * (3.0F - 2.0F * fx);
  fz = fz * fz * (3.0F - 2.0F * fz);

  uint32_t s00 = hash_coords(ix, iz, seed);
  uint32_t s10 = hash_coords(ix + 1, iz, seed);
  uint32_t s01 = hash_coords(ix, iz + 1, seed);
  uint32_t s11 = hash_coords(ix + 1, iz + 1, seed);

  float const v00 = rand_01(s00);
  float const v10 = rand_01(s10);
  float const v01 = rand_01(s01);
  float const v11 = rand_01(s11);

  float const v0 = v00 * (1.0F - fx) + v10 * fx;
  float const v1 = v01 * (1.0F - fx) + v11 * fx;
  return v0 * (1.0F - fz) + v1 * fz;
}

} // namespace

namespace Render::GL {

FireCampRenderer::FireCampRenderer() = default;
FireCampRenderer::~FireCampRenderer() = default;

void FireCampRenderer::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biomeSettings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_heightData = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biomeSettings = biomeSettings;
  m_noiseSeed = biomeSettings.seed;

  m_fireCampInstances.clear();
  m_fireCampInstanceBuffer.reset();
  m_fireCampInstanceCount = 0;
  m_fireCampInstancesDirty = false;

  m_fireCampParams.time = 0.0F;
  m_fireCampParams.flickerSpeed = 5.0F;
  m_fireCampParams.flickerAmount = 0.02F;
  m_fireCampParams.glowStrength = 1.1F;

  generateFireCampInstances();
}

void FireCampRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  m_fireCampInstanceCount = static_cast<uint32_t>(m_fireCampInstances.size());

  if (m_fireCampInstanceCount == 0) {
    m_fireCampInstanceBuffer.reset();
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.isInitialized();

  std::vector<FireCampInstanceGpu> visible_instances;
  if (use_visibility) {
    visible_instances.reserve(m_fireCampInstanceCount);
    for (const auto &instance : m_fireCampInstances) {
      float const world_x = instance.pos_intensity.x();
      float const world_z = instance.pos_intensity.z();
      if (visibility.isVisibleWorld(world_x, world_z)) {
        visible_instances.push_back(instance);
      }
    }
  } else {
    visible_instances = m_fireCampInstances;
  }

  const auto visible_count = static_cast<uint32_t>(visible_instances.size());
  if (visible_count == 0) {
    m_fireCampInstanceBuffer.reset();
    return;
  }

  if (!m_fireCampInstanceBuffer) {
    m_fireCampInstanceBuffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
  }

  m_fireCampInstanceBuffer->setData(visible_instances, Buffer::Usage::Static);

  FireCampBatchParams params = m_fireCampParams;
  params.time = renderer.getAnimationTime();
  params.flickerAmount = m_fireCampParams.flickerAmount *
                         (0.9F + 0.25F * std::sin(params.time * 1.3F));
  params.glowStrength = m_fireCampParams.glowStrength *
                        (0.85F + 0.2F * std::sin(params.time * 1.7F + 1.2F));
  renderer.firecampBatch(m_fireCampInstanceBuffer.get(), visible_count, params);

  const QVector3D log_color(0.26F, 0.15F, 0.08F);
  const QVector3D char_color(0.08F, 0.05F, 0.03F);

  for (const auto &instance : visible_instances) {
    const QVector4D pos_intensity = instance.pos_intensity;
    const QVector4D radius_phase = instance.radius_phase;

    const QVector3D camp_pos = pos_intensity.toVector3D();
    const float intensity = std::clamp(pos_intensity.w(), 0.6F, 1.6F);
    const float base_radius = std::max(radius_phase.x(), 1.0F);

    uint32_t state = hash_coords(
        static_cast<int>(std::floor(camp_pos.x())),
        static_cast<int>(std::floor(camp_pos.z())),
        static_cast<uint32_t>(radius_phase.y() *
                              HashConstants::k_temporal_variation_frequency));

    const float time = params.time;
    const float char_amount =
        std::clamp(time * 0.015F + rand_01(state) * 0.05F, 0.0F, 1.0F);

    const QVector3D blended_log_color =
        log_color * (1.0F - char_amount) + char_color * (char_amount + 0.15F);

    const float log_length = std::clamp(base_radius * 0.85F, 0.45F, 1.1F);
    const float log_radius = std::clamp(base_radius * 0.08F, 0.03F, 0.08F);

    const float base_yaw = (rand_01(state) - 0.5F) * 0.35F;
    const float cos_base = std::cos(base_yaw);
    const float sin_base = std::sin(base_yaw);
    const QVector3D axis_a(cos_base, 0.0F, sin_base);
    const QVector3D axis_b(-axis_a.z(), 0.0F, axis_a.x());

    const QVector3D base_center = camp_pos + QVector3D(0.0F, -0.02F, 0.0F);
    const QVector3D base_half_a = axis_a * (log_length * 0.5F);
    const QVector3D base_half_b = axis_b * (log_length * 0.45F);

    renderer.cylinder(base_center - base_half_a, base_center + base_half_a,
                      log_radius, blended_log_color, 1.0F);
    renderer.cylinder(base_center - base_half_b, base_center + base_half_b,
                      log_radius, blended_log_color, 1.0F);

    if (rand_01(state) > 0.25F) {
      float const top_yaw = base_yaw + 0.6F + (rand_01(state) - 0.5F) * 0.35F;
      QVector3D const top_axis(std::cos(top_yaw), 0.0F, std::sin(top_yaw));
      QVector3D const top_half = top_axis * (log_length * 0.35F);
      QVector3D const top_center =
          camp_pos + QVector3D(0.0F, log_radius * 1.6F, 0.0F);
      float const top_radius = log_radius * 0.85F;
      renderer.cylinder(top_center - top_half, top_center + top_half,
                        top_radius, blended_log_color, 1.0F);
    }
  }
}

void FireCampRenderer::clear() {
  m_fireCampInstances.clear();
  m_fireCampInstanceBuffer.reset();
  m_fireCampInstanceCount = 0;
  m_fireCampInstancesDirty = false;
  m_explicitPositions.clear();
  m_explicitIntensities.clear();
  m_explicitRadii.clear();
}

void FireCampRenderer::setExplicitFireCamps(
    const std::vector<QVector3D> &positions,
    const std::vector<float> &intensities, const std::vector<float> &radii) {
  m_explicitPositions = positions;
  m_explicitIntensities = intensities;
  m_explicitRadii = radii;
  m_fireCampInstancesDirty = true;
  if (m_width > 0 && m_height > 0 && !m_heightData.empty()) {
    generateFireCampInstances();
  }
}

void FireCampRenderer::addExplicitFireCamps() {
  if (m_explicitPositions.empty()) {
    return;
  }

  for (size_t i = 0; i < m_explicitPositions.size(); ++i) {
    const QVector3D &pos = m_explicitPositions[i];

    float intensity = 1.0F;
    if (i < m_explicitIntensities.size()) {
      intensity = m_explicitIntensities[i];
    }

    float radius = 3.0F;
    if (i < m_explicitRadii.size()) {
      radius = m_explicitRadii[i];
    }

    float const phase = static_cast<float>(i) * 1.234567F;

    FireCampInstanceGpu instance;
    instance.pos_intensity = QVector4D(pos.x(), pos.y(), pos.z(), intensity);
    instance.radius_phase = QVector4D(radius, phase, 1.0F, 0.0F);
    m_fireCampInstances.push_back(instance);
  }
}

void FireCampRenderer::generateFireCampInstances() {
  m_fireCampInstances.clear();

  if (m_width < 2 || m_height < 2 || m_heightData.empty()) {
    return;
  }

  const float half_width = static_cast<float>(m_width) * 0.5F;
  const float half_height = static_cast<float>(m_height) * 0.5F;
  const float tile_safe = std::max(0.1F, m_tile_size);

  const float edge_padding =
      std::clamp(m_biomeSettings.spawnEdgePadding, 0.0F, 0.5F);
  const float edge_margin_x = static_cast<float>(m_width) * edge_padding;
  const float edge_margin_z = static_cast<float>(m_height) * edge_padding;

  float const fire_camp_density = 0.02F;

  std::vector<QVector3D> normals(static_cast<qsizetype>(m_width * m_height),
                                 QVector3D(0, 1, 0));
  for (int z = 1; z < m_height - 1; ++z) {
    for (int x = 1; x < m_width - 1; ++x) {
      int const idx = z * m_width + x;
      float const h_l = m_heightData[(z)*m_width + (x - 1)];
      float const h_r = m_heightData[(z)*m_width + (x + 1)];
      float const h_d = m_heightData[(z - 1) * m_width + (x)];
      float const h_u = m_heightData[(z + 1) * m_width + (x)];

      QVector3D n = QVector3D(h_l - h_r, 2.0F * tile_safe, h_d - h_u);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[idx] = n;
    }
  }

  auto add_fire_camp = [&](float gx, float gz, uint32_t &state) -> bool {
    if (gx < edge_margin_x || gx > m_width - 1 - edge_margin_x ||
        gz < edge_margin_z || gz > m_height - 1 - edge_margin_z) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);
    int const normal_idx = iz * m_width + ix;

    QVector3D const normal = normals[normal_idx];
    float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);

    if (slope > 0.3F) {
      return false;
    }

    float const world_x = (gx - half_width) * m_tile_size;
    float const world_z = (gz - half_height) * m_tile_size;
    float const world_y = m_heightData[normal_idx];

    auto &building_registry =
        Game::Systems::BuildingCollisionRegistry::instance();
    if (building_registry.isPointInBuilding(world_x, world_z)) {
      return false;
    }

    // Avoid placing fire camps on roads
    auto &terrain_service = Game::Map::TerrainService::instance();
    if (terrain_service.is_point_on_road(world_x, world_z)) {
      return false;
    }

    float const intensity = remap(rand_01(state), 0.8F, 1.2F);
    float const radius = remap(rand_01(state), 2.0F, 4.0F) * tile_safe;

    float const phase = rand_01(state) * MathConstants::k_two_pi;

    float const duration = 1.0F;

    FireCampInstanceGpu instance;
    instance.pos_intensity = QVector4D(world_x, world_y, world_z, intensity);
    instance.radius_phase = QVector4D(radius, phase, duration, 0.0F);
    m_fireCampInstances.push_back(instance);
    return true;
  };

  constexpr int k_grid_spacing = 20;

  for (int z = 0; z < m_height; z += k_grid_spacing) {
    for (int x = 0; x < m_width; x += k_grid_spacing) {
      int const idx = z * m_width + x;

      QVector3D const normal = normals[idx];
      float const slope = 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);
      if (slope > 0.3F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0xF12ECA3FU ^ static_cast<uint32_t>(idx));

      float const world_x = (x - half_width) * m_tile_size;
      float const world_z = (z - half_height) * m_tile_size;

      float const cluster_noise = valueNoise(world_x * 0.02F, world_z * 0.02F,
                                             m_noiseSeed ^ 0xCA3F12E0U);

      if (cluster_noise < 0.4F) {
        continue;
      }

      float density_mult = 1.0F;
      if (m_terrain_types[idx] == Game::Map::TerrainType::Hill) {
        density_mult = 0.5F;
      } else if (m_terrain_types[idx] == Game::Map::TerrainType::Mountain) {
        density_mult = 0.0F;
      }

      float const effective_density = fire_camp_density * density_mult;
      if (rand_01(state) < effective_density) {
        float const gx = float(x) + rand_01(state) * float(k_grid_spacing);
        float const gz = float(z) + rand_01(state) * float(k_grid_spacing);
        add_fire_camp(gx, gz, state);
      }
    }
  }

  addExplicitFireCamps();

  m_fireCampInstanceCount = m_fireCampInstances.size();
  m_fireCampInstancesDirty = m_fireCampInstanceCount > 0;

  qDebug() << "FireCampRenderer: Generated" << m_fireCampInstanceCount
           << "total instances";
}

} // namespace Render::GL
