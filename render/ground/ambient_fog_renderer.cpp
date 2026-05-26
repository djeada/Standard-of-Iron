#include "ambient_fog_renderer.h"

#include <algorithm>
#include <cmath>

#include "../scene_renderer.h"

namespace Render::GL {

namespace {

constexpr float k_fog_y = 0.08F;
constexpr float k_max_patch_size = 12.0F;
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

void AmbientFogRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (!m_enabled || m_instances.empty()) {
    return;
  }
  (void)resources;

  if (m_dirty) {
    if (!m_instance_buffer) {
      m_instance_buffer = std::make_unique<Buffer>(Buffer::Type::Vertex);
    }
    m_instance_buffer->set_data(m_instances, Buffer::Usage::Static);
    m_dirty = false;
  }

  renderer.fog_batch(m_instance_buffer.get(), m_instances.size());
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
                                k_fog_y,
                                start_z + row * actual_step_z);
        inst.size = std::max(actual_step_x, actual_step_z) * 1.4F;
        inst.color = QVector3D(0.62F, 0.65F, 0.70F);
        inst.alpha = 0.18F + 0.22F * density;
        m_instances.push_back(inst);
      }
    }
  }
}

} // namespace Render::GL
