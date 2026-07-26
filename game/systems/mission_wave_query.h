#pragma once

namespace Game::Systems {

class MissionWaveQuery {
public:
  virtual ~MissionWaveQuery() = default;

  [[nodiscard]] virtual auto total_wave_count() const -> int = 0;
  [[nodiscard]] virtual auto cleared_wave_count() const -> int = 0;
};

} // namespace Game::Systems
