#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Game::Battlefield {

using DefinitionId = std::string;

enum class FormationShape : std::uint8_t {
  Line
};
enum class SpacingClass : std::uint8_t {
  Close,
  Standard,
  Open
};
enum class TurnStyle : std::uint8_t {
  Wheeling,
  Reform
};
enum class ReflowPolicy : std::uint8_t {
  FrontToRear
};
enum class MovementClass : std::uint8_t {
  Infantry,
  Cavalry,
  LargeBody
};
enum class EngagementRole : std::uint8_t {
  Melee,
  Ranged,
  Support
};
enum class FireMode : std::uint8_t {
  FreeFire,
  RippleVolley,
  SynchronizedVolley,
  HoldFire
};
enum class ActionEventKind : std::uint8_t {
  Windup,
  Release,
  TraceStart,
  TraceEnd,
  Impact,
  Recover
};
enum class Capability : std::uint8_t {
  Melee,
  Ranged,
  FormationMember
};

struct DefinitionHeader {
  DefinitionId id;
  std::uint32_t version{1};
};

struct FormationProfile {
  DefinitionHeader header;
  FormationShape shape{FormationShape::Line};
  std::uint16_t preferred_frontage{10};
  SpacingClass spacing{SpacingClass::Standard};
  TurnStyle turn_style{TurnStyle::Wheeling};
  ReflowPolicy reflow{ReflowPolicy::FrontToRear};
};

struct MovementProfile {
  DefinitionHeader header;
  MovementClass movement_class{MovementClass::Infantry};
  DefinitionId animation_graph_id;
  float speed{2.2F};
};

struct EngagementDoctrine {
  DefinitionHeader header;
  std::uint8_t active_ranks{1};
  float reach{1.5F};
  bool replace_casualties{true};
  bool permits_disengagement{true};
};

struct FireDoctrine {
  DefinitionHeader header;
  FireMode mode{FireMode::HoldFire};
  std::uint8_t eligible_ranks{0};
};

struct ActionEventDefinition {
  ActionEventKind kind{ActionEventKind::Windup};
  float normalized_time{};
};

struct ActionDefinition {
  DefinitionHeader header;
  DefinitionId clip_id;
  float duration_seconds{1.0F};
  bool melee_contact_required{false};
  bool projectile_release_required{false};
  std::vector<ActionEventDefinition> events;
};

struct UnitArchetype {
  DefinitionHeader header;
  DefinitionId movement_profile_id;
  DefinitionId formation_profile_id;
  DefinitionId engagement_doctrine_id;
  DefinitionId fire_doctrine_id;
  std::vector<DefinitionId> action_ids;
  std::vector<Capability> capabilities;
  float max_health{100.0F};
  float attack_damage{25.0F};
};

struct ValidationIssue {
  DefinitionId definition_id;
  std::string message;
};

class DefinitionRegistry {
public:
  void add(FormationProfile definition);
  void add(MovementProfile definition);
  void add(EngagementDoctrine definition);
  void add(FireDoctrine definition);
  void add(ActionDefinition definition);
  void add(UnitArchetype definition);

  [[nodiscard]] auto formation(std::string_view id) const -> const FormationProfile*;
  [[nodiscard]] auto movement(std::string_view id) const -> const MovementProfile*;
  [[nodiscard]] auto engagement(std::string_view id) const -> const EngagementDoctrine*;
  [[nodiscard]] auto fire(std::string_view id) const -> const FireDoctrine*;
  [[nodiscard]] auto action(std::string_view id) const -> const ActionDefinition*;
  [[nodiscard]] auto archetype(std::string_view id) const -> const UnitArchetype*;
  [[nodiscard]] auto validate() const -> std::vector<ValidationIssue>;
  [[nodiscard]] auto
  debug_description(std::string_view archetype_id) const -> std::string;

private:
  std::unordered_map<DefinitionId, FormationProfile> formations_;
  std::unordered_map<DefinitionId, MovementProfile> movements_;
  std::unordered_map<DefinitionId, EngagementDoctrine> engagements_;
  std::unordered_map<DefinitionId, FireDoctrine> fires_;
  std::unordered_map<DefinitionId, ActionDefinition> actions_;
  std::unordered_map<DefinitionId, UnitArchetype> archetypes_;
};

[[nodiscard]] auto standard_infantry_definitions() -> DefinitionRegistry;

} // namespace Game::Battlefield
