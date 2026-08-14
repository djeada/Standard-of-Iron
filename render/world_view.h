#pragma once

#include <cstdint>

#include "game/systems/nation_id.h"
#include "game/units/troop_type.h"

namespace Game::Formation {
class UnitLayoutLibrary;
class UnitLayoutSystem;
} // namespace Game::Formation

namespace Game::Map {
class TerrainService;
class VisibilityService;
} // namespace Game::Map

namespace Game::Session {
class SessionContext;
} // namespace Game::Session

namespace Game::Systems {
struct TroopProfile;
class NationRegistry;
class OwnerRegistry;
class TroopProfileService;
} // namespace Game::Systems

namespace Game::Units {
class TroopCatalog;
class TroopConfig;
} // namespace Game::Units

namespace Game::Wildlife {
class BirdFlockManager;
} // namespace Game::Wildlife

namespace Render {

class WorldView {
public:
  WorldView();

  [[nodiscard]] static auto
  of(const Game::Session::SessionContext& session) -> WorldView;

  [[nodiscard]] static auto of_active_session() -> WorldView;

  [[nodiscard]] auto terrain() const noexcept -> const Game::Map::TerrainService* {
    return m_terrain;
  }

  [[nodiscard]] auto has_terrain() const noexcept -> bool;

  [[nodiscard]] auto
  terrain_or_empty() const noexcept -> const Game::Map::TerrainService&;

  [[nodiscard]] auto
  visibility() const noexcept -> const Game::Map::VisibilityService* {
    return m_visibility;
  }
  [[nodiscard]] auto has_visibility() const noexcept -> bool;

  [[nodiscard]] auto
  visibility_or_empty() const noexcept -> const Game::Map::VisibilityService&;

  [[nodiscard]] auto find_troop_profile(Game::Systems::NationID nation_id,
                                        Game::Units::TroopType type) const
      -> const Game::Systems::TroopProfile*;

  [[nodiscard]] auto owners() const noexcept -> const Game::Systems::OwnerRegistry* {
    return m_owners;
  }
  [[nodiscard]] auto nations() const noexcept -> const Game::Systems::NationRegistry* {
    return m_nations;
  }

  [[nodiscard]] auto birds() const noexcept -> const Game::Wildlife::BirdFlockManager* {
    return m_birds;
  }

  [[nodiscard]] auto
  troop_profiles() const noexcept -> const Game::Systems::TroopProfileService* {
    return m_troop_profiles;
  }
  [[nodiscard]] auto troop_config() const noexcept -> const Game::Units::TroopConfig* {
    return m_troop_config;
  }
  [[nodiscard]] auto
  troop_catalog() const noexcept -> const Game::Units::TroopCatalog* {
    return m_troop_catalog;
  }
  [[nodiscard]] auto
  unit_layouts() const noexcept -> const Game::Formation::UnitLayoutLibrary* {
    return m_unit_layouts;
  }
  [[nodiscard]] auto
  soldier_offsets() const noexcept -> const Game::Formation::UnitLayoutSystem* {
    return m_soldier_offsets;
  }

  [[nodiscard]] auto is_bound() const noexcept -> bool { return m_terrain != nullptr; }

private:
  const Game::Map::TerrainService* m_terrain = nullptr;
  const Game::Map::VisibilityService* m_visibility = nullptr;
  const Game::Systems::OwnerRegistry* m_owners = nullptr;
  const Game::Systems::NationRegistry* m_nations = nullptr;
  const Game::Wildlife::BirdFlockManager* m_birds = nullptr;

  const Game::Systems::TroopProfileService* m_troop_profiles = nullptr;
  const Game::Units::TroopConfig* m_troop_config = nullptr;
  const Game::Units::TroopCatalog* m_troop_catalog = nullptr;
  const Game::Formation::UnitLayoutLibrary* m_unit_layouts = nullptr;
  const Game::Formation::UnitLayoutSystem* m_soldier_offsets = nullptr;
};

} // namespace Render
