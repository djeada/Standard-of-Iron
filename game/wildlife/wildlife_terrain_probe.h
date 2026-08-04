#pragma once

namespace Game::Wildlife {

struct WorldBounds {
  float min_x{-1000.0F};
  float min_z{-1000.0F};
  float max_x{1000.0F};
  float max_z{1000.0F};

  [[nodiscard]] auto contains(float x, float z) const noexcept -> bool {
    return x >= min_x && x <= max_x && z >= min_z && z <= max_z;
  }
};

class ITerrainProbe {
public:
  ITerrainProbe() = default;
  ITerrainProbe(const ITerrainProbe&) = delete;
  ITerrainProbe(ITerrainProbe&&) = delete;
  auto operator=(const ITerrainProbe&) -> ITerrainProbe& = delete;
  auto operator=(ITerrainProbe&&) -> ITerrainProbe& = delete;
  virtual ~ITerrainProbe() = default;

  [[nodiscard]] virtual auto is_blocked(float world_x, float world_z) const -> bool = 0;
  [[nodiscard]] virtual auto ground_height(float world_x,
                                           float world_z) const -> float = 0;
  [[nodiscard]] virtual auto bounds() const -> WorldBounds = 0;
};

class FlatTerrainProbe final : public ITerrainProbe {
public:
  explicit FlatTerrainProbe(WorldBounds bounds = {}, float height = 0.0F) noexcept
      : m_bounds(bounds)
      , m_height(height) {}

  [[nodiscard]] auto is_blocked(float world_x, float world_z) const -> bool override {
    return !m_bounds.contains(world_x, world_z);
  }

  [[nodiscard]] auto ground_height(float world_x,
                                   float world_z) const -> float override {
    (void)world_x;
    (void)world_z;
    return m_height;
  }

  [[nodiscard]] auto bounds() const -> WorldBounds override { return m_bounds; }

private:
  WorldBounds m_bounds;
  float m_height;
};

[[nodiscard]] auto terrain_service_probe() -> ITerrainProbe&;

} // namespace Game::Wildlife
