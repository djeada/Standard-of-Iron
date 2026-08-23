#include "world_view.h"

#include "game/formation/unit_layout.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/session/session_context.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_config.h"
#include "game/wildlife/bird_flock.h"

namespace Render {

namespace {

auto empty_terrain() -> const Game::Map::TerrainService& {
  static const Game::Map::TerrainService instance;
  return instance;
}

} // namespace

WorldView::WorldView()
    : m_troop_profiles(&Game::Systems::TroopProfileService::instance())
    , m_troop_config(&Game::Units::TroopConfig::instance())
    , m_troop_catalog(&Game::Units::TroopCatalog::instance())
    , m_unit_layouts(&Game::Formation::UnitLayoutLibrary::instance())
    , m_soldier_offsets(&Game::Formation::UnitLayoutSystem::instance()) {
}

auto WorldView::has_terrain() const noexcept -> bool {
  return m_terrain != nullptr && m_terrain->is_initialized();
}

auto WorldView::terrain_or_empty() const noexcept -> const Game::Map::TerrainService& {
  return m_terrain != nullptr ? *m_terrain : empty_terrain();
}

auto WorldView::visibility_or_empty() const noexcept
    -> const Game::Map::VisibilityService& {
  static const Game::Map::VisibilityService k_empty;
  return m_visibility != nullptr ? *m_visibility : k_empty;
}

auto WorldView::find_troop_profile(Game::Systems::NationID nation_id,
                                   Game::Units::TroopType type) const
    -> const Game::Systems::TroopProfile* {
  return m_troop_profiles != nullptr ? m_troop_profiles->find_profile(nation_id, type)
                                     : nullptr;
}

auto WorldView::has_visibility() const noexcept -> bool {
  return m_visibility != nullptr && m_visibility->is_initialized();
}

auto WorldView::of(const Game::Session::SessionContext& session) -> WorldView {
  WorldView view;

  view.m_terrain = &session.terrain();
  view.m_visibility = &session.visibility();
  view.m_owners = &session.owners();
  view.m_nations = &session.nations();

  view.m_birds = &Game::Wildlife::BirdFlockManager::instance();

  return view;
}

} // namespace Render
