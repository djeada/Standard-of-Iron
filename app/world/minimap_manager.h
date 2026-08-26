#pragma once

#include <QImage>

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "game/map/visibility_service.h"
#include "game/render_bridge/minimap/minimap_fog_compositor.h"
#include "game/render_bridge/minimap/unit_layer.h"

namespace Game::Map {
struct MapDefinition;
namespace Minimap {
class CameraViewportLayer;
} // namespace Minimap
} // namespace Game::Map

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {
class SelectionSystem;
}

namespace Render::GL {
class Camera;
}

class MinimapManager {
public:
  struct DestinationMarker {
    float nx = 0.0F;
    float ny = 0.0F;
    float origin_nx = 0.0F;
    float origin_ny = 0.0F;
    int owner_id = 0;
  };

  struct CaptureAlert {
    float world_x = 0.0F;
    float world_z = 0.0F;
    int site_owner_id = 0;
    int capturing_owner_id = 0;
    bool contested = false;
  };

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

  [[nodiscard]] bool
  world_to_normalized(float world_x, float world_z, float& nx, float& ny) const;

  [[nodiscard]] const std::vector<DestinationMarker>& destinations() const {
    return m_destinations;
  }
  [[nodiscard]] bool consume_destinations_dirty();

  [[nodiscard]] const std::vector<CaptureAlert>& capture_alerts() const {
    return m_capture_alerts;
  }
  void clear_capture_alerts() { m_capture_alerts.clear(); }

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
  bool m_camera_viewport_valid = false;
  float m_last_camera_x = 0.0F;
  float m_last_camera_z = 0.0F;
  float m_last_viewport_w = 0.0F;
  float m_last_viewport_h = 0.0F;

  std::vector<Game::Map::Minimap::UnitMarker> m_marker_scratch;
  std::vector<Engine::Core::EntityID> m_selected_scratch;

  struct CaptureWatch {
    Engine::Core::EntityID entity_id = 0;
    int capturing_owner_id = 0;
    int site_owner_id = 0;
    float world_x = 0.0F;
    float world_z = 0.0F;
    bool contested = false;
  };

  void collect_capture_alerts(const std::vector<CaptureWatch>& current);

  std::vector<CaptureWatch> m_capture_watch;
  std::vector<CaptureWatch> m_capture_watch_scratch;
  std::vector<CaptureAlert> m_capture_alerts;
  std::vector<DestinationMarker> m_destinations;
  std::vector<DestinationMarker> m_destination_scratch;
  std::vector<std::uint64_t> m_destination_cell_scratch;
  std::uint64_t m_destination_hash = 0;
  bool m_destinations_dirty = false;
  float m_hash_position_scale = 1.0F;
};
