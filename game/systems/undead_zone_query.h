#pragma once

#include <QString>

namespace Game::Systems {

class UndeadZoneQuery {
public:
  virtual ~UndeadZoneQuery() = default;

  [[nodiscard]] virtual auto has_zone(const QString& zone_id) const -> bool = 0;
  [[nodiscard]] virtual auto is_zone_cleared(const QString& zone_id) const -> bool = 0;
  [[nodiscard]] virtual auto
  is_shrine_purified(const QString& zone_id) const -> bool = 0;
  [[nodiscard]] virtual auto
  completed_wave_count(const QString& zone_id) const -> int = 0;
};

} // namespace Game::Systems
