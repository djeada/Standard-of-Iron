#pragma once

#include <QImage>

#include <cstdint>
#include <limits>
#include <memory>

#include "game/map/minimap/minimap_fog_compositor.h"
#include "game/map/visibility_service.h"

namespace Game::Map {
struct MapDefinition;
namespace Minimap {
class UnitLayer;
class CameraViewportLayer;
} // namespace Minimap
} // namespace Game::Map

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Game::Systems {
class SelectionSystem;
}

namespace Render::GL {
class Camera;
}

class MinimapManager {
public:
  MinimapManager();
  ~MinimapManager();

  void generate_for_map(const Game::Map::MapDefinition& map_def);
  void update_fog(const Game::Map::VisibilityService::Snapshot& snapshot);
  void clear_fog();
  void update_units(Engine::Core::World* world,
                    Game::Systems::SelectionSystem* selection_system,
                    int local_owner_id);
  void update_camera_viewport(const Render::GL::Camera* camera,
                              float screen_width,
                              float screen_height);

  [[nodiscard]] bool consume_dirty_flag();

  [[nodiscard]] const QImage& get_image() const { return m_minimap_image; }
  [[nodiscard]] bool has_minimap() const { return !m_minimap_base_image.isNull(); }
  [[nodiscard]] float get_world_width() const { return m_world_width; }
  [[nodiscard]] float get_world_height() const { return m_world_height; }
  [[nodiscard]] float get_tile_size() const { return m_tile_size; }
  [[nodiscard]] std::uint64_t fog_version() const { return m_fog_compositor.version(); }
  [[nodiscard]] bool unit_overlay_stale() const {
    return fog_version() != m_last_fog_composite_version;
  }

private:
  void mark_dirty() { m_dirty = true; }

  QImage m_minimap_image;
  QImage m_minimap_base_image;
  QImage m_minimap_fog_image;
  QImage m_minimap_units_image;
  Game::Map::Minimap::MinimapFogCompositor m_fog_compositor;
  std::unique_ptr<Game::Map::Minimap::UnitLayer> m_unit_layer;
  std::unique_ptr<Game::Map::Minimap::CameraViewportLayer> m_camera_viewport_layer;
  float m_world_width = 0.0F;
  float m_world_height = 0.0F;
  float m_tile_size = 1.0F;

  bool m_dirty = false;
  bool m_viewport_composite_dirty = false;

  std::uint64_t m_last_unit_hash = 0;

  std::uint64_t m_last_fog_composite_version =
      std::numeric_limits<std::uint64_t>::max();
  float m_last_camera_x = 0.0F;
  float m_last_camera_z = 0.0F;
  float m_last_viewport_w = 0.0F;
  float m_last_viewport_h = 0.0F;
};
