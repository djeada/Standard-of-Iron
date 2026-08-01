#pragma once

#include <QVector3D>

#include <vector>

#include "../../formation/army_formation_service.h"
#include "ai_types.h"

namespace Game::Systems::AI {

struct AIFormationRequest {
  int player_id{0};
  QVector3D anchor;
  float spacing{2.0F};
  Game::Formation::ArmyFormationIntent intent{
      Game::Formation::ArmyFormationIntent::FactionDefault};
  Game::Formation::MovementPolicy movement{
      Game::Formation::MovementPolicy::ReformAtDestination};
  bool resolve_terrain{true};
};

[[nodiscard]] auto
doctrine_for_player(int player_id) -> Game::Formation::FormationDoctrineId;

[[nodiscard]] auto plan_ai_formation(const AIFormationRequest& request,
                                     const std::vector<const EntitySnapshot*>& units)
    -> std::vector<QVector3D>;

[[nodiscard]] auto
plan_ai_formation(const AIFormationRequest& request,
                  const std::vector<Engine::Core::EntityID>& unit_ids,
                  const AISnapshot& snapshot) -> std::vector<QVector3D>;

[[nodiscard]] auto
select_ai_intent(const AISnapshot& snapshot,
                 const AIContext& context,
                 bool defensive_posture,
                 bool escorting_siege) -> Game::Formation::ArmyFormationIntent;

} // namespace Game::Systems::AI
