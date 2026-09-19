#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>
#include <vector>

#include "../units/troop_type.h"
#include "army_formation_types.h"
#include "formation_doctrine.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Formation {

struct ArmyFormationMember {
  EntityID entity_id{0};
  Game::Units::TroopType troop_type{Game::Units::TroopType::Swordsman};
  RoleTagSet roles{0U};
  QVector3D current_position;
  float footprint{1.0F};

  float half_width{0.5F};
  float half_depth{0.5F};

  int individuals{1};
  int files{1};
  float soldier_file_step{1.0F};
  float soldier_rank_step{1.0F};
  float soldier_body_radius{0.5F};
  bool heavy{false};
  std::vector<std::pair<float, float>> extents_by_files;
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
  bool allow_anchor_shift{true};
  bool assign_nearest{false};
  bool preserve_previous_slots{false};
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

  float slot_spacing{1.0F};
  float footprint_gap{0.0F};
  MovementPolicy movement_policy{MovementPolicy::ReformAtDestination};

  std::vector<FormationSlot> slot_list;
  std::vector<float> slot_clearance;
  std::vector<float> slot_half_width;
  std::vector<float> slot_half_depth;

  std::vector<int> slot_files;

  int blocked_count{0};
  int adjusted_count{0};
  float displacement{0.0F};
  bool narrowed{false};

  [[nodiscard]] auto keeps_shape() const -> bool;
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
  float slot_spacing{1.0F};
  float frontage{0.0F};
  float depth{0.0F};
  float footprint_gap{0.0F};
  MovementPolicy movement_policy{MovementPolicy::ReformAtDestination};

  std::vector<FormationSlot> slot_list;

  std::vector<float> slot_clearance;
  std::vector<float> slot_half_width;
  std::vector<float> slot_half_depth;
  std::vector<int> slot_files;

  std::vector<QVector3D> slot_start;
  std::vector<std::uint64_t> slot_kind;
  bool assign_by_distance{false};

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
  plan(const std::vector<ArmyFormationMember>& members,
       const ArmyFormationRequest& request,
       const ArmyFormation* previous_group) -> ArmyFormationPlan;

  [[nodiscard]] static auto
  fit_to_ground(const ArmyFormationLayout& first_layout,
                const std::vector<ArmyFormationMember>& members,
                const ArmyFormationRequest& request,
                const ArmyFormation* previous_group) -> ArmyFormationPlan;

  [[nodiscard]] static auto
  layout_from_reference(const ArmyFormation& formation) -> ArmyFormationLayout;

  [[nodiscard]] static auto
  min_cost_assignment(const std::vector<std::vector<float>>& cost) -> std::vector<int>;

  static void fold_onto_reference(ArmyFormationPlan& plan,
                                  const std::vector<FormationSlot>& reference);

  [[nodiscard]] static auto
  resolve_movement_policy(MovementPolicy requested,
                          const DoctrineIntentTemplate& tmpl) -> MovementPolicy;

  [[nodiscard]] static auto
  footprints_overlap(const FormationSlot& a, const FormationSlot& b, float gap) -> bool;

  [[nodiscard]] static auto first_overlap(const std::vector<FormationSlot>& slot_list,
                                          float gap) -> std::pair<int, int>;

  [[nodiscard]] static auto
  build_layout(const std::vector<ArmyFormationMember>& members,
               const ArmyFormationRequest& request,
               const ArmyFormation* previous_group = nullptr) -> ArmyFormationLayout;

  [[nodiscard]] static auto
  place(const ArmyFormationLayout& layout,
        const ArmyFormationRequest& request) -> ArmyFormationPlan;

  [[nodiscard]] static auto scatter_offsets(int count,
                                            float spacing) -> std::vector<QVector3D>;

  [[nodiscard]] static auto
  scatter_layout(const std::vector<ArmyFormationMember>& members,
                 float spacing) -> ArmyFormationLayout;

  [[nodiscard]] static auto
  layout_signature(const std::vector<ArmyFormationMember>& members,
                   const ArmyFormationRequest& request,
                   const ArmyFormation* previous_group = nullptr) -> std::uint64_t;

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
                   float facing,
                   float* slot_spacing_out = nullptr,
                   int row_cap_override = 0,
                   int* row_cap_used = nullptr) -> std::vector<FormationSlot>;

  static void measure_footprint(const Engine::Core::Entity& entity,
                                float fallback_radius,
                                ArmyFormationMember& member);

  static void shape_member_for_intent(ArmyFormationMember& member, float aspect);
};

} // namespace Game::Formation
