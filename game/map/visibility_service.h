#pragma once

#include <cstdint>
#include <vector>

namespace Engine {
namespace Core {
class World;
}
} // namespace Engine

namespace Game {
namespace Map {

enum class VisibilityState : std::uint8_t {
  Unseen = 0,
  Explored = 1,
  Visible = 2
};

class VisibilityService {
public:
  static VisibilityService &instance();

  void initialize(int width, int height, float tileSize);
  void reset();
  void update(Engine::Core::World &world, int playerId);

  bool isInitialized() const { return m_initialized; }

  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }
  float getTileSize() const { return m_tileSize; }

  VisibilityState stateAt(int gridX, int gridZ) const;
  bool isVisibleWorld(float worldX, float worldZ) const;
  bool isExploredWorld(float worldX, float worldZ) const;

  const std::vector<std::uint8_t> &cells() const { return m_cells; }
  std::uint64_t version() const { return m_version; }

private:
  bool inBounds(int x, int z) const;
  int index(int x, int z) const;
  int worldToGrid(float worldCoord, float half) const;

  VisibilityService() = default;

  bool m_initialized = false;
  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;
  float m_halfWidth = 0.0f;
  float m_halfHeight = 0.0f;

  std::vector<std::uint8_t> m_cells;
  std::vector<std::uint8_t> m_currentVisible;
  std::uint64_t m_version = 0;
};

} // namespace Map
} // namespace Game
