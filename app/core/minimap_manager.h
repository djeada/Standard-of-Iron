#pragma once

#include <QImage>
#include <memory>
#include <vector>

namespace Game::Map {
struct MapDefinition;
namespace Minimap {
class UnitLayer;
}
} // namespace Game::Map

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Game::Systems {
class SelectionSystem;
}

class MinimapManager {
public:
  MinimapManager();
  ~MinimapManager();

  void generate_for_map(const Game::Map::MapDefinition &map_def);
  void update_fog(float dt, int local_owner_id);
  void update_units(Engine::Core::World *world,
                    Game::Systems::SelectionSystem *selection_system);

  [[nodiscard]] const QImage &get_image() const { return m_minimap_image; }
  [[nodiscard]] bool has_minimap() const { return !m_minimap_base_image.isNull(); }

private:
  QImage m_minimap_image;
  QImage m_minimap_base_image;
  std::uint64_t m_minimap_fog_version = 0;
  std::unique_ptr<Game::Map::Minimap::UnitLayer> m_unit_layer;
  float m_world_width = 0.0F;
  float m_world_height = 0.0F;
  float m_minimap_update_timer = 0.0F;
  static constexpr float MINIMAP_UPDATE_INTERVAL = 0.1F;
};
