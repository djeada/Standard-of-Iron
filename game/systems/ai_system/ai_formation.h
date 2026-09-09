#pragma once

#include <QVector3D>

#include <optional>
#include <vector>

#include "../../formation/army_formation_service.h"
#include "ai_types.h"

namespace Game::Systems {
struct Nation;
}

namespace Game::Systems::AI {

struct AIFormationRequest {
  int player_id{0};
  const Game::Systems::Nation* nation{nullptr};
  QVector3D anchor;
  float spacing{2.0F};
  Game::Formation::ArmyFormationIntent intent{
      Game::Formation::ArmyFormationIntent::FactionDefault};
  Game::Formation::MovementPolicy movement{
      Game::Formation::MovementPolicy::ReformAtDestination};
  std::optional<float> facing;
  bool preserve_member_order{false};
  bool resolve_terrain{true};
};

struct AIFormationSlot {
  QVector3D position;
  bool usable{false};
};

struct MusterFootprint {
  float frontage{0.0F};
  float depth{0.0F};
  bool valid{false};
};

[[nodiscard]] auto
muster_footprint(const AIFormationRequest& request,
                 const std::vector<const EntitySnapshot*>& units) -> MusterFootprint;

[[nodiscard]] auto doctrine_for_nation(const Game::Systems::Nation* nation)
    -> Game::Formation::FormationDoctrineId;

[[nodiscard]] auto plan_ai_formation(const AIFormationRequest& request,
                                     const std::vector<const EntitySnapshot*>& units)
    -> std::vector<AIFormationSlot>;

[[nodiscard]] auto
plan_ai_formation(const AIFormationRequest& request,
                  const std::vector<Engine::Core::EntityID>& unit_ids,
                  const AISnapshot& snapshot) -> std::vector<AIFormationSlot>;

[[nodiscard]] auto
move_to_slots(const std::vector<Engine::Core::EntityID>& unit_ids,
              const std::vector<AIFormationSlot>& slot_list) -> AICommand;

[[nodiscard]] auto
select_ai_intent(const AISnapshot& snapshot,
                 const AIContext& context,
                 bool defensive_posture,
                 bool escorting_siege) -> Game::Formation::ArmyFormationIntent;

} // namespace Game::Systems::AI
