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

auto WorldView::shipped_content() -> Content {
  return Content{.troop_profiles = &Game::Systems::TroopProfileService::instance(),
                 .troop_config = &Game::Units::TroopConfig::instance(),
                 .troop_catalog = &Game::Units::TroopCatalog::instance(),
                 .unit_layouts = &Game::Formation::UnitLayoutLibrary::instance(),
                 .soldier_offsets = &Game::Formation::UnitLayoutSystem::instance()};
}

WorldView::WorldView()
    : WorldView(shipped_content()) {
}

WorldView::WorldView(const Content& content)
    : m_troop_profiles(content.troop_profiles)
    , m_troop_config(content.troop_config)
    , m_troop_catalog(content.troop_catalog)
    , m_unit_layouts(content.unit_layouts)
    , m_soldier_offsets(content.soldier_offsets) {
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

auto WorldView::of(const Game::Session::SessionContext& session,
                   const Content& content) -> WorldView {
  WorldView view(content);

  view.m_terrain = &session.terrain();
  view.m_visibility = &session.visibility();
  view.m_owners = &session.owners();
  view.m_nations = &session.nations();
  view.m_birds = &session.birds();

  return view;
}

auto WorldView::of(const Game::Session::SessionContext& session) -> WorldView {
  return of(session, shipped_content());
}

} // namespace Render
