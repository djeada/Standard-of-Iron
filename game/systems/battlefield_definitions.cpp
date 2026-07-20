#include "battlefield_definitions.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Game::Battlefield {
namespace {

template <typename T>
void insert_unique(std::unordered_map<DefinitionId, T>& destination, T definition) {
  if (definition.header.id.empty())
    throw std::invalid_argument("definition ID must not be empty");
  if (definition.header.version == 0)
    throw std::invalid_argument("definition version must be positive");
  const auto id = definition.header.id;
  if (!destination.emplace(id, std::move(definition)).second)
    throw std::invalid_argument("duplicate definition ID: " + id);
}

template <typename T>
auto find(const std::unordered_map<DefinitionId, T>& source,
          std::string_view id) -> const T* {
  const auto found = source.find(std::string(id));
  return found == source.end() ? nullptr : &found->second;
}

void issue(std::vector<ValidationIssue>& issues,
           const DefinitionId& id,
           bool invalid,
           const char* message) {
  if (invalid)
    issues.push_back({id, message});
}

} // namespace

void DefinitionRegistry::add(FormationProfile d) {
  insert_unique(formations_, std::move(d));
}
void DefinitionRegistry::add(MovementProfile d) {
  insert_unique(movements_, std::move(d));
}
void DefinitionRegistry::add(EngagementDoctrine d) {
  insert_unique(engagements_, std::move(d));
}
void DefinitionRegistry::add(FireDoctrine d) {
  insert_unique(fires_, std::move(d));
}
void DefinitionRegistry::add(ActionDefinition d) {
  insert_unique(actions_, std::move(d));
}
void DefinitionRegistry::add(UnitArchetype d) {
  insert_unique(archetypes_, std::move(d));
}

auto DefinitionRegistry::formation(std::string_view id) const
    -> const FormationProfile* {
  return find(formations_, id);
}
auto DefinitionRegistry::movement(std::string_view id) const -> const MovementProfile* {
  return find(movements_, id);
}
auto DefinitionRegistry::engagement(std::string_view id) const
    -> const EngagementDoctrine* {
  return find(engagements_, id);
}
auto DefinitionRegistry::fire(std::string_view id) const -> const FireDoctrine* {
  return find(fires_, id);
}
auto DefinitionRegistry::action(std::string_view id) const -> const ActionDefinition* {
  return find(actions_, id);
}
auto DefinitionRegistry::archetype(std::string_view id) const -> const UnitArchetype* {
  return find(archetypes_, id);
}

auto DefinitionRegistry::validate() const -> std::vector<ValidationIssue> {
  std::vector<ValidationIssue> issues;
  for (const auto& [id, value] : formations_)
    issue(issues,
          id,
          value.preferred_frontage == 0,
          "preferred frontage must be positive");
  for (const auto& [id, value] : movements_) {
    issue(issues, id, value.speed <= 0.0F, "speed must be positive");
    issue(
        issues, id, value.animation_graph_id.empty(), "animation graph ID is required");
  }
  for (const auto& [id, value] : engagements_) {
    issue(issues, id, value.active_ranks == 0, "at least one active rank is required");
    issue(issues, id, value.reach <= 0.0F, "weapon reach must be positive");
  }
  for (const auto& [id, value] : actions_) {
    issue(issues, id, value.clip_id.empty(), "clip ID is required");
    issue(issues, id, value.duration_seconds <= 0.0F, "duration must be positive");
    float previous = -1.0F;
    bool has_contact = false;
    bool has_release = false;
    for (const auto& event : value.events) {
      issue(issues,
            id,
            event.normalized_time < 0.0F || event.normalized_time > 1.0F,
            "event time must be normalized");
      issue(
          issues, id, event.normalized_time < previous, "events must be time ordered");
      previous = event.normalized_time;
      has_contact |= event.kind == ActionEventKind::Impact ||
                     event.kind == ActionEventKind::TraceStart;
      has_release |= event.kind == ActionEventKind::Release;
    }
    issue(issues,
          id,
          value.melee_contact_required && !has_contact,
          "melee action requires a contact event");
    issue(issues,
          id,
          value.projectile_release_required && !has_release,
          "projectile action requires a release event");
  }
  for (const auto& [id, value] : archetypes_) {
    issue(issues,
          id,
          movement(value.movement_profile_id) == nullptr,
          "unknown movement profile");
    issue(issues,
          id,
          formation(value.formation_profile_id) == nullptr,
          "unknown formation profile");
    issue(issues,
          id,
          engagement(value.engagement_doctrine_id) == nullptr,
          "unknown engagement doctrine");
    issue(issues, id, fire(value.fire_doctrine_id) == nullptr, "unknown fire doctrine");
    issue(issues, id, value.action_ids.empty(), "at least one action is required");
    for (const auto& action_id : value.action_ids)
      issue(issues, id, action(action_id) == nullptr, "unknown action definition");
    issue(issues, id, value.max_health <= 0.0F, "max health must be positive");
  }
  return issues;
}

auto DefinitionRegistry::debug_description(std::string_view archetype_id) const
    -> std::string {
  const auto* value = archetype(archetype_id);
  if (!value)
    return "unresolved archetype: " + std::string(archetype_id);
  std::ostringstream out;
  out << value->header.id << "@" << value->header.version
      << " movement=" << value->movement_profile_id
      << " formation=" << value->formation_profile_id
      << " engagement=" << value->engagement_doctrine_id
      << " fire=" << value->fire_doctrine_id << " actions=";
  for (std::size_t index = 0; index < value->action_ids.size(); ++index) {
    if (index)
      out << ',';
    out << value->action_ids[index];
  }
  return out.str();
}

auto standard_infantry_definitions() -> DefinitionRegistry {
  DefinitionRegistry result;
  result.add(FormationProfile{{"line_standard", 1},
                              FormationShape::Line,
                              10,
                              SpacingClass::Standard,
                              TurnStyle::Wheeling,
                              ReflowPolicy::FrontToRear});
  result.add(MovementProfile{
      {"infantry_standard", 1}, MovementClass::Infantry, "humanoid_locomotion", 2.2F});
  result.add(EngagementDoctrine{{"sword_line", 1}, 1, 1.55F, true, true});
  result.add(FireDoctrine{{"hold_fire", 1}, FireMode::HoldFire, 0});
  result.add(ActionDefinition{{"sword_slash", 1},
                              "infantry_slash_a",
                              1.2F,
                              true,
                              false,
                              {{ActionEventKind::Windup, 0.0F},
                               {ActionEventKind::TraceStart, 0.38F},
                               {ActionEventKind::Impact, 0.46F},
                               {ActionEventKind::TraceEnd, 0.55F},
                               {ActionEventKind::Recover, 0.72F}}});
  result.add(UnitArchetype{{"sword_infantry", 1},
                           "infantry_standard",
                           "line_standard",
                           "sword_line",
                           "hold_fire",
                           {"sword_slash"},
                           {Capability::Melee, Capability::FormationMember},
                           100.0F,
                           25.0F});
  return result;
}

} // namespace Game::Battlefield
