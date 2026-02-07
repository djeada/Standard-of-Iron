#pragma once

#include <QImage>
#include <cstdint>
#include <memory>
#include <vector>

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

  void generate_for_map(const Game::Map::MapDefinition &map_def);
  void update_fog(float dt, int local_owner_id);
  void update_units(Engine::Core::World *world,
                    Game::Systems::SelectionSystem *selection_system,
                    int local_owner_id);
  void update_camera_viewport(const Render::GL::Camera *camera,
                              float screen_width, float screen_height);

  [[nodiscard]] bool consume_dirty_flag();

  [[nodiscard]] const QImage &get_image() const { return m_minimap_image; }
  [[nodiscard]] bool has_minimap() const {
    return !m_minimap_base_image.isNull();
  }
  [[nodiscard]] float get_world_width() const { return m_world_width; }
  [[nodiscard]] float get_world_height() const { return m_world_height; }
  [[nodiscard]] float get_tile_size() const { return m_tile_size; }

private:
  struct FogLookupEntry {
    int idx00 = 0;
    int idx10 = 0;
    int idx01 = 0;
    int idx11 = 0;
    float fx = 0.0F;
    float fy = 0.0F;
  };

  void rebuild_fog_lookup(int vis_width, int vis_height);
  void mark_dirty() { m_dirty = true; }

  QImage m_minimap_image;
  QImage m_minimap_base_image;
  QImage m_minimap_fog_image;
  std::uint64_t m_minimap_fog_version = 0;
  int m_fog_lookup_vis_width = 0;
  int m_fog_lookup_vis_height = 0;
  int m_fog_lookup_img_width = 0;
  int m_fog_lookup_img_height = 0;
  std::vector<FogLookupEntry> m_fog_lookup_entries;
  std::unique_ptr<Game::Map::Minimap::UnitLayer> m_unit_layer;
  std::unique_ptr<Game::Map::Minimap::CameraViewportLayer>
      m_camera_viewport_layer;
  float m_world_width = 0.0F;
  float m_world_height = 0.0F;
  float m_tile_size = 1.0F;
  float m_minimap_update_timer = 0.0F;
  static constexpr float MINIMAP_UPDATE_INTERVAL = 0.1F;

  bool m_dirty = false;

  std::uint64_t m_last_unit_hash = 0;
  float m_last_camera_x = 0.0F;
  float m_last_camera_z = 0.0F;
  float m_last_viewport_w = 0.0F;
  float m_last_viewport_h = 0.0F;
};
