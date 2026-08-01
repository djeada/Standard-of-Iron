#pragma once

#include <QVector3D>

#include <string>
#include <vector>

#include "army_formation_planner.h"
#include "army_formation_types.h"

namespace Engine::Core {
class World;
} // namespace Engine::Core

namespace Game::Formation {

struct ArmyFormationResult {
  std::vector<QVector3D> positions;
  std::vector<float> facing_angles;
  std::vector<int> stable_slot_ids;
  std::vector<int> stable_ranks;
  std::vector<int> stable_files;
  std::vector<SlotStatus> slot_status;

  FormationGroupID group_id{k_invalid_group};
  FormationDoctrineId doctrine{k_neutral_doctrine};
  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};

  float formation_facing{0.0F};
  float frontage{0.0F};
  float depth{0.0F};

  bool valid{false};
  bool used_army_formation{false};
  int blocked_count{0};
  std::string rejection_reason;
};

class ArmyFormationService {
public:
  [[nodiscard]] static auto spread(int count,
                                   const QVector3D& centre,
                                   float spacing = 1.0F) -> std::vector<QVector3D>;

  [[nodiscard]] static auto
  positions_for(const std::vector<ArmyFormationMember>& members,
                const ArmyFormationRequest& request) -> std::vector<QVector3D>;

  [[nodiscard]] static auto
  preview(Engine::Core::World& world,
          const ArmyFormationRequest& request) -> ArmyFormationResult;

  [[nodiscard]] static auto
  commit(Engine::Core::World& world,
         const ArmyFormationRequest& request) -> ArmyFormationResult;

  [[nodiscard]] static auto auto_facing(Engine::Core::World& world,
                                        const std::vector<EntityID>& members,
                                        const QVector3D& anchor) -> float;

  [[nodiscard]] static auto
  availability(Engine::Core::World& world,
               const std::vector<EntityID>& members,
               ArmyFormationIntent intent,
               const FormationDoctrineId& doctrine = {}) -> std::string;

  [[nodiscard]] static auto
  doctrine_for_selection(Engine::Core::World& world,
                         const std::vector<EntityID>& members,
                         MixedDoctrinePolicy policy) -> FormationDoctrineId;

  static void release(Engine::Core::World& world, const std::vector<EntityID>& members);

private:
  [[nodiscard]] static auto build(Engine::Core::World& world,
                                  const ArmyFormationRequest& request,
                                  bool commit_group) -> ArmyFormationResult;
};

} // namespace Game::Formation
