#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "formation_roles.h"

namespace Game::Formation {

using FormationGroupID = std::uint64_t;
using FormationDoctrineId = std::string;
using EntityID = std::uint64_t;

inline constexpr FormationGroupID k_invalid_group = 0U;
inline constexpr int k_invalid_slot = -1;

inline const FormationDoctrineId k_neutral_doctrine = "neutral";

enum class ArmyFormationIntent : std::uint8_t {
  FactionDefault,
  Line,
  Column,
  Defensive,
  Assault,
  Encirclement,
  SiegeEscort
};

enum class FlankPreference : std::uint8_t {
  Balanced,
  StrongLeft,
  StrongRight,
  Split
};

enum class MovementPolicy : std::uint8_t {
  ReformAtDestination,
  MaintainFormation
};

enum class RangedPlacement : std::uint8_t {
  Front,
  Rear,
  Skirmish
};

enum class MixedDoctrinePolicy : std::uint8_t {
  CompositeByRole,
  SeparateContingents,
  CommanderDoctrine,
  MajorityDoctrine
};

enum class SlotStatus : std::uint8_t {
  Valid,
  Adjusted,
  Blocked
};

enum class FormationPhase : std::uint8_t {

  Reforming = 0,
  Formed = 1,
  Disrupted = 2,
  Opening = 3,
  Traversing = 4,
  Arrived = 5
};

[[nodiscard]] auto intent_to_string(ArmyFormationIntent intent) -> const char*;
[[nodiscard]] auto intent_display_name(ArmyFormationIntent intent) -> QString;
[[nodiscard]] auto
try_parse_intent(const QString& value) -> std::optional<ArmyFormationIntent>;
[[nodiscard]] auto all_intents() -> std::vector<ArmyFormationIntent>;

[[nodiscard]] auto flank_preference_to_string(FlankPreference pref) -> const char*;
[[nodiscard]] auto
try_parse_flank_preference(const QString& value) -> std::optional<FlankPreference>;

[[nodiscard]] auto movement_policy_to_string(MovementPolicy policy) -> const char*;
[[nodiscard]] auto
try_parse_movement_policy(const QString& value) -> std::optional<MovementPolicy>;

[[nodiscard]] auto ranged_placement_to_string(RangedPlacement placement) -> const char*;
[[nodiscard]] auto
try_parse_ranged_placement(const QString& value) -> std::optional<RangedPlacement>;

[[nodiscard]] auto mixed_policy_to_string(MixedDoctrinePolicy policy) -> const char*;
[[nodiscard]] auto
try_parse_mixed_policy(const QString& value) -> std::optional<MixedDoctrinePolicy>;

[[nodiscard]] auto phase_to_string(FormationPhase phase) -> const char*;
[[nodiscard]] auto phase_display_name(FormationPhase phase) -> QString;

struct FormationSlot {
  int id{k_invalid_slot};
  ArmyRole role{ArmyRole::Centre};

  QVector3D local_offset;
  QVector3D world_position;
  float facing{0.0F};

  int rank{0};
  int file{0};

  SlotStatus status{SlotStatus::Valid};
  EntityID occupant{0};

  [[nodiscard]] auto is_occupied() const noexcept -> bool { return occupant != 0U; }
  [[nodiscard]] auto is_placeable() const noexcept -> bool {
    return status != SlotStatus::Blocked;
  }
};

struct FormationMovePlan {
  std::vector<QVector3D> corridor;
  std::size_t corridor_index{0};

  QVector3D formation_center;
  QVector3D facing_direction{0.0F, 0.0F, 1.0F};

  bool active{false};

  [[nodiscard]] auto has_corridor() const noexcept -> bool {
    return active && corridor_index < corridor.size();
  }

  [[nodiscard]] auto next_waypoint() const noexcept -> QVector3D {
    return has_corridor() ? corridor[corridor_index] : formation_center;
  }

  void clear() noexcept {
    corridor.clear();
    corridor_index = 0;
    active = false;
  }
};

struct ArmyFormationShape {
  float frontage{0.0F};
  float depth{0.0F};
  float spacing{1.0F};
  int reserve_rows{0};
};

struct ArmyFormationOptions {
  FlankPreference flank_preference{FlankPreference::Balanced};
  MovementPolicy movement_policy{MovementPolicy::ReformAtDestination};
  RangedPlacement ranged_placement{RangedPlacement::Rear};
  MixedDoctrinePolicy mixed_policy{MixedDoctrinePolicy::MajorityDoctrine};

  float frontage_scale{1.0F};
  float depth_scale{1.0F};
  float spacing_scale{1.0F};
  int reserve_rows{-1};

  bool preserve_member_order{false};
  bool doctrine_locked{false};
};

struct ArmyFormation {
  FormationGroupID id{k_invalid_group};
  FormationDoctrineId doctrine{k_neutral_doctrine};
  ArmyFormationIntent intent{ArmyFormationIntent::FactionDefault};

  QVector3D anchor;
  float facing{0.0F};
  float frontage{0.0F};
  float depth{0.0F};
  float spacing{1.0F};

  ArmyFormationOptions options;
  FormationPhase phase{FormationPhase::Reforming};

  float cohesion{1.0F};
  float cohesion_pace{0.0F};

  std::vector<EntityID> members;
  std::vector<FormationSlot> slot_list;

  QVector3D destination;
  bool has_destination{false};
  float advance_progress{0.0F};

  FormationMovePlan move_plan;

  std::uint32_t plan_revision{0U};
  bool needs_replan{true};

  bool moves_pending{false};

  [[nodiscard]] auto maintains_formation() const -> bool {
    return options.movement_policy == MovementPolicy::MaintainFormation;
  }

  [[nodiscard]] auto is_formed() const -> bool {
    return phase == FormationPhase::Formed || phase == FormationPhase::Arrived;
  }

  [[nodiscard]] auto find_slot(int slot_id) const -> const FormationSlot*;
  [[nodiscard]] auto find_slot_for(EntityID entity) const -> const FormationSlot*;
  [[nodiscard]] auto has_member(EntityID entity) const -> bool;
  [[nodiscard]] auto blocked_slot_count() const -> int;
  [[nodiscard]] auto slot_error(const QVector3D& position,
                                EntityID entity) const -> float;
};

} // namespace Game::Formation
