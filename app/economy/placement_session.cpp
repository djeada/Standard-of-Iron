#include "app/economy/placement_session.h"

#include "app/economy/harvest_targeting.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/nation_registry.h"

namespace App::Economy {

auto placement_kind_for(const QString& item_type) -> PlacementKind {
  if (item_type == QStringLiteral("wall_gate")) {
    return PlacementKind::Gate;
  }
  if (item_type == QStringLiteral("wall_ladder")) {
    return PlacementKind::Ladder;
  }
  if (item_type == QStringLiteral("wall_segment")) {
    return PlacementKind::Wall;
  }
  if (is_harvest_construction_item(item_type)) {
    return PlacementKind::Harvest;
  }
  return PlacementKind::Structure;
}

auto is_previewable_structure_item(const QString& item_type) -> bool {
  return item_type == QStringLiteral("defense_tower") ||
         item_type == QStringLiteral("barracks") ||
         item_type == QStringLiteral("home") ||
         item_type == QStringLiteral("marketplace") ||
         item_type == QStringLiteral("temple") || item_type == QStringLiteral("farm");
}

auto item_supports_preview_rotation(const QString& item_type) -> bool {
  return is_previewable_structure_item(item_type);
}

auto placement_phase(bool placing,
                     bool wall_drag_active,
                     bool preview_active) -> PlacementPhase {
  if (!placing) {
    return PlacementPhase::Idle;
  }
  if (preview_active) {
    return PlacementPhase::Previewing;
  }
  return wall_drag_active ? PlacementPhase::DraggingWall : PlacementPhase::Aiming;
}

auto placement_allows(PlacementPhase phase,
                      PlacementKind kind,
                      PlacementEvent event) -> bool {
  const bool placing = phase != PlacementPhase::Idle;
  const bool wall = kind == PlacementKind::Wall || kind == PlacementKind::Gate ||
                    kind == PlacementKind::Ladder;
  switch (event) {
  case PlacementEvent::Start:
  case PlacementEvent::MatchReset:
  case PlacementEvent::Confirm:
    return true;
  case PlacementEvent::PointerMotion:
  case PlacementEvent::PointerRelease:
  case PlacementEvent::Cancel:
    return placing;
  case PlacementEvent::PointerPress:
    return placing && wall;
  case PlacementEvent::Rotate:
    return phase == PlacementPhase::Previewing;
  }
  return false;
}

void PlacementSession::begin_direct(const QString& building_type,
                                    int owner_id,
                                    Game::Systems::NationID nation_id) {
  end();
  m_pending_building_type = building_type;
  m_construction_type = building_type;
  m_active = true;
  m_direct = true;
  m_owner_id = owner_id;
  m_nation_id = nation_id;
}

void PlacementSession::begin_with_builders(const QString& item_type,
                                           std::vector<Engine::Core::EntityID> builders,
                                           const QVector3D& center,
                                           Engine::Core::World* world) {
  end();
  m_builders = std::move(builders);
  m_construction_type = item_type;
  m_active = true;
  m_direct = false;
  m_position = center;
  m_owner_id = owner_id(world);
  m_nation_id = nation_id(world);
}

void PlacementSession::end() {
  m_pending_building_type.clear();
  m_construction_type.clear();
  m_builders.clear();
  m_active = false;
  m_direct = false;
  m_owner_id = 0;
  m_nation_id = Game::Systems::NationID::RomanRepublic;
  m_rotation_y = 0.0F;
  m_harvest_target_id = 0;
  m_food_target_id = 0;
}

auto PlacementSession::is_wall() const -> bool {
  const auto placement_kind = kind();
  return placement_kind == PlacementKind::Wall ||
         placement_kind == PlacementKind::Gate ||
         placement_kind == PlacementKind::Ladder;
}

auto PlacementSession::is_gate() const -> bool {
  return kind() == PlacementKind::Gate;
}

auto PlacementSession::is_ladder() const -> bool {
  return kind() == PlacementKind::Ladder;
}

auto PlacementSession::effective_rotation_y() const -> float {
  return item_supports_preview_rotation(m_construction_type) ? m_rotation_y : 0.0F;
}

void PlacementSession::set_targets(std::uint64_t harvest_target,
                                   Engine::Core::EntityID food_target) {
  m_harvest_target_id = harvest_target;
  m_food_target_id = food_target;
}

auto PlacementSession::owner_id(Engine::Core::World* world) const -> int {
  if (m_owner_id > 0) {
    return m_owner_id;
  }
  if (world == nullptr) {
    return 0;
  }
  for (auto builder_id : m_builders) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(builder_id);
    if (unit != nullptr) {
      return unit->owner_id;
    }
  }
  return 0;
}

auto PlacementSession::nation_id(Engine::Core::World* world) const
    -> Game::Systems::NationID {
  if (m_owner_id > 0) {
    return m_nation_id;
  }
  if (world == nullptr) {
    return Game::Systems::NationID::RomanRepublic;
  }
  for (auto builder_id : m_builders) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(builder_id);
    if (unit != nullptr) {
      return unit->nation_id;
    }
  }
  return Game::Session::session_for(*world).nations().default_nation_id();
}

} // namespace App::Economy
