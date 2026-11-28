#pragma once

namespace Game::Systems {

class ConvoyQuery {
public:
  virtual ~ConvoyQuery() = default;
  [[nodiscard]] virtual auto delivered_wagons() const -> int = 0;
  [[nodiscard]] virtual auto available_wagons() const -> int = 0;
};

} // namespace Game::Systems
