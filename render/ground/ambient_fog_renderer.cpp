#include "render/ground/ambient_fog_renderer.h"

#include <algorithm>
#include <cmath>

#include "render/frame/frame_submitter.h"

namespace Render::GL {

namespace {

constexpr float k_fog_y = 0.08F;
constexpr float k_max_patch_size = 6.0F;
constexpr float k_fade_start = 0.30F;
constexpr float k_overlap_factor = 0.7F;

} // namespace

void AmbientFogRenderer::configure(const std::vector<Game::Map::FogZone>& zones) {
  m_zones = zones;
  build_instances();
  m_dirty = true;
}

auto AmbientFogRenderer::is_point_in_fog(float world_x, float world_z) const -> bool {
  for (const auto& zone : m_zones) {
    const float half_w = zone.width * 0.5F;
    const float half_h = zone.height * 0.5F;
    if (world_x >= zone.x - half_w && world_x <= zone.x + half_w &&
        world_z >= zone.z - half_h && world_z <= zone.z + half_h) {
      return true;
    }
  }
  return false;
}

void AmbientFogRenderer::submit(FrameSubmitter& renderer,
                                const DrawResources* resources) {
  if (!m_enabled || m_instances.empty()) {
    return;
  }
  (void)resources;

  if (m_dirty) {
    if (!m_instance_block) {
      m_instance_block = std::make_unique<InstanceBlock>();
    }
    m_instance_block->assign(m_instances);
    m_dirty = false;
  }

  renderer.fog_batch(m_instance_block.get(), m_instances.size());
}

void AmbientFogRenderer::build_instances() {
  m_instances.clear();

  for (const auto& zone : m_zones) {
    const float zone_w = std::max(zone.width, 0.1F);
    const float zone_h = std::max(zone.height, 0.1F);
    const float density = std::clamp(zone.density, 0.05F, 1.0F);

    const float patch_size = std::min(std::max(zone_w, zone_h), k_max_patch_size);
    const float step = patch_size * k_overlap_factor;

    const int cols = std::max(1, static_cast<int>(std::ceil(zone_w / step)));
    const int rows = std::max(1, static_cast<int>(std::ceil(zone_h / step)));

    const float actual_step_x = zone_w / static_cast<float>(cols);
    const float actual_step_z = zone_h / static_cast<float>(rows);
    const float start_x = zone.x - zone_w * 0.5F + actual_step_x * 0.5F;
    const float start_z = zone.z - zone_h * 0.5F + actual_step_z * 0.5F;

    for (int row = 0; row < rows; ++row) {
      for (int col = 0; col < cols; ++col) {
        FogInstanceData inst;
        inst.center = QVector3D(start_x + col * actual_step_x,
                                zone.y + k_fog_y,
                                start_z + row * actual_step_z);
        inst.size = std::max(actual_step_x, actual_step_z) * 1.4F;
        inst.color = QVector3D(0.62F, 0.65F, 0.70F);

        const float rx = (inst.center.x() - zone.x) / (zone_w * 0.5F);
        const float rz = (inst.center.z() - zone.z) / (zone_h * 0.5F);
        const float reach = std::clamp((std::sqrt(rx * rx + rz * rz) - k_fade_start) /
                                           (1.0F - k_fade_start),
                                       0.0F,
                                       1.0F);
        const float thinning = 1.0F - reach * reach * (3.0F - 2.0F * reach);
        inst.alpha = (0.18F + 0.22F * density) * thinning;
        if (inst.alpha < 0.01F) {
          continue;
        }
        m_instances.push_back(inst);
      }
    }
  }
}

} // namespace Render::GL
