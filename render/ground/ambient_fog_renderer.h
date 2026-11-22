#pragma once

#include <QVector3D>

#include <cstddef>
#include <memory>
#include <vector>

#include "game/map/map_definition.h"
#include "render/frame/draw_commands.h"
#include "render/frame/i_draw_source.h"
#include "render/geom/instance_block.h"

namespace Render::GL {
class FrameSubmitter;
class DrawResources;

class AmbientFogRenderer : public IDrawSource {
public:
  AmbientFogRenderer() = default;
  ~AmbientFogRenderer() override = default;

  void set_enabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] auto is_enabled() const -> bool { return m_enabled; }

  void configure(const std::vector<Game::Map::FogZone>& zones);

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_instances.size();
  }

  [[nodiscard]] auto zones() const -> const std::vector<Game::Map::FogZone>& {
    return m_zones;
  }

  [[nodiscard]] auto is_point_in_fog(float world_x, float world_z) const -> bool;

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

private:
  void build_instances();

  bool m_enabled = true;
  std::vector<Game::Map::FogZone> m_zones;
  std::vector<FogInstanceData> m_instances;
  std::unique_ptr<InstanceBlock> m_instance_block;
  bool m_dirty = false;
};

} // namespace Render::GL
