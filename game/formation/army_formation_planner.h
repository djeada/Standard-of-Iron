#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>
#include <vector>

#include "../units/troop_type.h"
#include "army_formation_types.h"
#include "formation_doctrine.h"

namespace Engine::Core {
class World;
} // namespace Engine::Core

namespace Game::Formation {

struct ArmyFormationMember {
  EntityID entity_id{0};
  Game::Units::TroopType troop_type{Game::Units::TroopType::Swordsman};
  RoleTagSet roles{0U};
  QVector3D current_position;
  float footprint{1.0F};
  FormationDoctrineId doctrine;
};

struct ArmyFormationRequest {
  std::vector<EntityID> members;

  QVector3D anchor;
  float facing{0.0F};
  float frontage{0.0F};

  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};
  FormationDoctrineId doctrine;
  ArmyFormationOptions options;

  float spacing{1.0F};
  bool resolve_terrain{true};
  bool preserve_previous_slots{true};
  FormationGroupID group_id{k_invalid_group};
};

struct ArmyFormationPlan {
  bool valid{false};
  std::string rejection_reason;

  FormationDoctrineId doctrine{k_neutral_doctrine};
  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};

  QVector3D anchor;
  float facing{0.0F};
  float frontage{0.0F};
  float depth{0.0F};
  float spacing{1.0F};

  std::vector<FormationSlot> slot_list;

  int blocked_count{0};
  int adjusted_count{0};

  [[nodiscard]] auto slot_for(EntityID entity) const -> const FormationSlot*;
  [[nodiscard]] auto placed_count() const -> int;
  [[nodiscard]] auto depth_bands() const -> std::vector<int>;
  [[nodiscard]] auto rank_count() const -> int;
  [[nodiscard]] auto file_count() const -> int;
};

struct ArmyFormationLayout {
  bool valid{false};
  std::string rejection_reason;

  FormationDoctrineId doctrine{k_neutral_doctrine};
  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};

  float spacing{1.0F};
  float frontage{0.0F};
  float depth{0.0F};

  std::vector<FormationSlot> slot_list;

  std::uint64_t signature{0U};
};

class ArmyFormationPlanner {
public:
  [[nodiscard]] static auto
  plan(Engine::Core::World& world,
       const ArmyFormationRequest& request) -> ArmyFormationPlan;

  [[nodiscard]] static auto
  plan(const std::vector<ArmyFormationMember>& members,
       const ArmyFormationRequest& request) -> ArmyFormationPlan;

  [[nodiscard]] static auto
  build_layout(const std::vector<ArmyFormationMember>& members,
               const ArmyFormationRequest& request) -> ArmyFormationLayout;

  [[nodiscard]] static auto
  place(const ArmyFormationLayout& layout,
        const ArmyFormationRequest& request) -> ArmyFormationPlan;

  [[nodiscard]] static auto
  layout_signature(const std::vector<ArmyFormationMember>& members,
                   const ArmyFormationRequest& request) -> std::uint64_t;

  [[nodiscard]] static auto
  make_member(EntityID entity_id,
              Game::Units::TroopType troop_type,
              const QVector3D& position,
              const FormationDoctrineId& doctrine) -> ArmyFormationMember;

  [[nodiscard]] static auto collect_members(Engine::Core::World& world,
                                            const std::vector<EntityID>& entities)
      -> std::vector<ArmyFormationMember>;

  [[nodiscard]] static auto
  resolve_doctrine(const std::vector<ArmyFormationMember>& members,
                   const ArmyFormationRequest& request) -> FormationDoctrineId;

  [[nodiscard]] static auto
  combined_roles(const std::vector<ArmyFormationMember>& members) -> RoleTagSet;

  [[nodiscard]] static auto
  plan_local_slots(const std::vector<ArmyFormationMember>& members,
                   const DoctrineIntentTemplate& tmpl,
                   const ArmyFormationOptions& options,
                   float spacing,
                   float requested_frontage,
                   float facing) -> std::vector<FormationSlot>;
};

} // namespace Game::Formation
