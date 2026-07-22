#include "arena_scenario.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_system/mounted_charge_processor.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/units/unit.h"
#include "render/profiling/combat_animation_diagnostics.h"

namespace Arena {
namespace {

constexpr float k_default_response_seconds = 0.45F;
constexpr float k_default_idle_seconds = 1.25F;
constexpr float k_default_engagement_distance = 7.0F;
constexpr float k_default_root_step = 0.8F;
constexpr float k_default_frame_budget_ms = 33.34F;
constexpr float k_default_fall_up_y = 0.72F;

auto vector_from_transform(const Engine::Core::TransformComponent& transform)
    -> QVector3D {
  return {transform.position.x, transform.position.y, transform.position.z};
}

auto horizontal_distance(const QVector3D& lhs, const QVector3D& rhs) -> float {
  QVector3D delta = lhs - rhs;
  delta.setY(0.0F);
  return delta.length();
}

auto soldier_key(Engine::Core::EntityID entity_id, int soldier_index) -> std::uint64_t {
  return (static_cast<std::uint64_t>(entity_id) << 32U) |
         static_cast<std::uint32_t>(std::max(0, soldier_index));
}

auto trigger_name(ScenarioTriggerKind kind) -> QString {
  switch (kind) {
  case ScenarioTriggerKind::AtTime:
    return QStringLiteral("AtTime");
  case ScenarioTriggerKind::GroupDestroyed:
    return QStringLiteral("GroupDestroyed");
  case ScenarioTriggerKind::FirstContact:
    return QStringLiteral("FirstContact");
  case ScenarioTriggerKind::GroupsWithinDistance:
    return QStringLiteral("GroupsWithinDistance");
  case ScenarioTriggerKind::GroupEnteredArea:
    return QStringLiteral("GroupEnteredArea");
  case ScenarioTriggerKind::PreviousStepComplete:
    return QStringLiteral("PreviousStepComplete");
  }
  return QStringLiteral("Unknown");
}

auto command_name(ScenarioCommandKind kind) -> QString {
  switch (kind) {
  case ScenarioCommandKind::Stand:
    return QStringLiteral("Stand");
  case ScenarioCommandKind::Move:
    return QStringLiteral("Move");
  case ScenarioCommandKind::FormationMove:
    return QStringLiteral("FormationMove");
  case ScenarioCommandKind::Charge:
    return QStringLiteral("Charge");
  case ScenarioCommandKind::Attack:
    return QStringLiteral("Attack");
  case ScenarioCommandKind::AttackMove:
    return QStringLiteral("AttackMove");
  case ScenarioCommandKind::Hold:
    return QStringLiteral("Hold");
  case ScenarioCommandKind::Guard:
    return QStringLiteral("Guard");
  case ScenarioCommandKind::Stop:
    return QStringLiteral("Stop");
  case ScenarioCommandKind::SpawnAmbush:
    return QStringLiteral("SpawnAmbush");
  case ScenarioCommandKind::ReleaseReserve:
    return QStringLiteral("ReleaseReserve");
  case ScenarioCommandKind::SetCamera:
    return QStringLiteral("SetCamera");
  case ScenarioCommandKind::SetHealth:
    return QStringLiteral("SetHealth");
  case ScenarioCommandKind::ApplyDamage:
    return QStringLiteral("ApplyDamage");
  case ScenarioCommandKind::MeleeLock:
    return QStringLiteral("MeleeLock");
  case ScenarioCommandKind::SetFullCreatureLod:
    return QStringLiteral("SetFullCreatureLod");
  case ScenarioCommandKind::TriggerCommanderAura:
    return QStringLiteral("TriggerCommanderAura");
  }
  return QStringLiteral("Unknown");
}

auto expectation_name(ArenaExpectationKind kind) -> QString {
  switch (kind) {
  case ArenaExpectationKind::AllGroupsRespondWithin:
    return QStringLiteral("AllGroupsRespondWithin");
  case ArenaExpectationKind::NoEligibleTroopIdleDuringCombat:
    return QStringLiteral("NoEligibleTroopIdleDuringCombat");
  case ArenaExpectationKind::NoPoseOscillation:
    return QStringLiteral("NoPoseOscillation");
  case ArenaExpectationKind::NoRootTeleport:
    return QStringLiteral("NoRootTeleport");
  case ArenaExpectationKind::NoUnexpectedFallPose:
    return QStringLiteral("NoUnexpectedFallPose");
  case ArenaExpectationKind::NoLimbOverextension:
    return QStringLiteral("NoLimbOverextension");
  case ArenaExpectationKind::MovementIsContinuous:
    return QStringLiteral("MovementIsContinuous");
  case ArenaExpectationKind::FormationOrderPreserved:
    return QStringLiteral("FormationOrderPreserved");
  case ArenaExpectationKind::FormationEngagementIsStable:
    return QStringLiteral("FormationEngagementIsStable");
  case ArenaExpectationKind::FormationBodyOverlapObserved:
    return QStringLiteral("FormationBodyOverlapObserved");
  case ArenaExpectationKind::CombatIndicatorIsContinuous:
    return QStringLiteral("CombatIndicatorIsContinuous");
  case ArenaExpectationKind::AllLivingSoldiersFight:
    return QStringLiteral("AllLivingSoldiersFight");
  case ArenaExpectationKind::MovementAnimationObserved:
    return QStringLiteral("MovementAnimationObserved");
  case ArenaExpectationKind::AttackAnimationObserved:
    return QStringLiteral("AttackAnimationObserved");
  case ArenaExpectationKind::HoldPoseMaintained:
    return QStringLiteral("HoldPoseMaintained");
  case ArenaExpectationKind::RepeatedAttackAnimationObserved:
    return QStringLiteral("RepeatedAttackAnimationObserved");
  case ArenaExpectationKind::AttackHasVisibleContact:
    return QStringLiteral("AttackHasVisibleContact");
  case ArenaExpectationKind::AttackRecoveryObserved:
    return QStringLiteral("AttackRecoveryObserved");
  case ArenaExpectationKind::NoActiveCombatAtEnd:
    return QStringLiteral("NoActiveCombatAtEnd");
  case ArenaExpectationKind::HitReactionObserved:
    return QStringLiteral("HitReactionObserved");
  case ArenaExpectationKind::DeathAnimationObserved:
    return QStringLiteral("DeathAnimationObserved");
  case ArenaExpectationKind::LaunchedCasualtyObserved:
    return QStringLiteral("LaunchedCasualtyObserved");
  case ArenaExpectationKind::NoLaunchedCasualtyObserved:
    return QStringLiteral("NoLaunchedCasualtyObserved");
  case ArenaExpectationKind::ChargeImpactPrecedesMeleeLock:
    return QStringLiteral("ChargeImpactPrecedesMeleeLock");
  case ArenaExpectationKind::TargetRetakenAfterDeath:
    return QStringLiteral("TargetRetakenAfterDeath");
  case ArenaExpectationKind::BotIssuesUsefulCommand:
    return QStringLiteral("BotIssuesUsefulCommand");
  case ArenaExpectationKind::FrameBudget:
    return QStringLiteral("FrameBudget");
  case ArenaExpectationKind::GroupIsRendered:
    return QStringLiteral("GroupIsRendered");
  case ArenaExpectationKind::GroupExists:
    return QStringLiteral("GroupExists");
  case ArenaExpectationKind::GroupDestroyed:
    return QStringLiteral("GroupDestroyed");
  case ArenaExpectationKind::GroupReachedDestination:
    return QStringLiteral("GroupReachedDestination");
  case ArenaExpectationKind::BridgeTraversalObserved:
    return QStringLiteral("BridgeTraversalObserved");
  case ArenaExpectationKind::BridgeCenterlineAligned:
    return QStringLiteral("BridgeCenterlineAligned");
  case ArenaExpectationKind::ElevationGainObserved:
    return QStringLiteral("ElevationGainObserved");
  case ArenaExpectationKind::OwnerCompletesConstruction:
    return QStringLiteral("OwnerCompletesConstruction");
  case ArenaExpectationKind::OwnerHarvestsResource:
    return QStringLiteral("OwnerHarvestsResource");
  case ArenaExpectationKind::CommanderAuraActivated:
    return QStringLiteral("CommanderAuraActivated");
  case ArenaExpectationKind::CommanderAuraBuffObserved:
    return QStringLiteral("CommanderAuraBuffObserved");
  case ArenaExpectationKind::CommanderAuraExpired:
    return QStringLiteral("CommanderAuraExpired");
  case ArenaExpectationKind::NoCommanderAuraBuffObserved:
    return QStringLiteral("NoCommanderAuraBuffObserved");
  }
  return QStringLiteral("Unknown");
}

auto json_vector(const QVector3D& value) -> QJsonArray {
  return {value.x(), value.y(), value.z()};
}

} // namespace

auto validate_scenario(const ArenaScenarioDefinition& definition)
    -> std::vector<ArenaScenarioValidationError> {
  std::vector<ArenaScenarioValidationError> errors;
  if (definition.id.trimmed().isEmpty()) {
    errors.push_back({QStringLiteral("id"), QStringLiteral("scenario id is empty")});
  }
  if (!(definition.duration_seconds > 0.0F)) {
    errors.push_back({QStringLiteral("duration_seconds"),
                      QStringLiteral("duration must be positive")});
  }
  if (definition.groups.empty()) {
    errors.push_back(
        {QStringLiteral("groups"), QStringLiteral("at least one group is required")});
  }
  if (definition.expectations.empty()) {
    errors.push_back({QStringLiteral("expectations"),
                      QStringLiteral("scenario must declare acceptance expectations")});
  }

  QSet<QString> group_names;
  for (std::size_t i = 0; i < definition.groups.size(); ++i) {
    auto const& group = definition.groups[i];
    QString const field = QStringLiteral("groups[%1]").arg(i);
    if (group.name.trimmed().isEmpty()) {
      errors.push_back({field, QStringLiteral("group name is empty")});
    } else if (group_names.contains(group.name)) {
      errors.push_back({field, QStringLiteral("duplicate group '%1'").arg(group.name)});
    } else {
      group_names.insert(group.name);
    }
    if (group.count <= 0) {
      errors.push_back({field, QStringLiteral("group count must be positive")});
    }
    if (group.individuals_per_unit < 0) {
      errors.push_back(
          {field, QStringLiteral("individuals_per_unit cannot be negative")});
    }
  }

  auto check_group = [&](const QString& value, const QString& field, bool required) {
    if (value.isEmpty()) {
      if (required) {
        errors.push_back({field, QStringLiteral("group reference is required")});
      }
      return;
    }
    if (!group_names.contains(value)) {
      errors.push_back(
          {field, QStringLiteral("unknown group reference '%1'").arg(value)});
    }
  };

  for (std::size_t i = 0; i < definition.steps.size(); ++i) {
    auto const& step = definition.steps[i];
    QString const field = QStringLiteral("steps[%1]").arg(i);
    if (step.trigger.time_seconds < 0.0F) {
      errors.push_back({field, QStringLiteral("trigger time cannot be negative")});
    }
    bool const command_needs_group =
        step.command != ScenarioCommandKind::SetCamera &&
        step.command != ScenarioCommandKind::SetFullCreatureLod;
    check_group(step.group, field + QStringLiteral(".group"), command_needs_group);
    bool const command_needs_target =
        step.command == ScenarioCommandKind::Attack ||
        step.command == ScenarioCommandKind::AttackMove ||
        step.command == ScenarioCommandKind::Charge ||
        step.command == ScenarioCommandKind::Guard ||
        step.command == ScenarioCommandKind::ReleaseReserve ||
        step.command == ScenarioCommandKind::MeleeLock;
    check_group(step.target_group,
                field + QStringLiteral(".target_group"),
                command_needs_target);
    if (step.trigger.kind != ScenarioTriggerKind::AtTime &&
        step.trigger.kind != ScenarioTriggerKind::PreviousStepComplete) {
      check_group(step.trigger.group, field + QStringLiteral(".trigger.group"), true);
    }
    if (step.trigger.kind == ScenarioTriggerKind::FirstContact ||
        step.trigger.kind == ScenarioTriggerKind::GroupsWithinDistance) {
      check_group(step.trigger.target_group,
                  field + QStringLiteral(".trigger.target_group"),
                  true);
    }
  }

  for (std::size_t i = 0; i < definition.expectations.size(); ++i) {
    auto const& expectation = definition.expectations[i];
    QString const field = QStringLiteral("expectations[%1]").arg(i);
    check_group(expectation.group, field + QStringLiteral(".group"), false);
    check_group(
        expectation.target_group, field + QStringLiteral(".target_group"), false);
    if (expectation.end_seconds > 0.0F &&
        expectation.end_seconds < expectation.start_seconds) {
      errors.push_back({field, QStringLiteral("expectation end precedes its start")});
    }
  }
  return errors;
}

auto ArenaScenarioReport::summary() const -> QString {
  if (passed()) {
    return QStringLiteral("PASS %1: %2 frames, %3 rendered soldier samples, %4 s")
        .arg(scenario_id)
        .arg(rendered_frames)
        .arg(rendered_soldier_samples)
        .arg(elapsed_seconds, 0, 'f', 2);
  }
  return QStringLiteral("FAIL %1: %2 issue(s); first: %3")
      .arg(scenario_id)
      .arg(issues.size())
      .arg(issues.front().message);
}

struct ArenaScenarioRunner::Impl {
  struct StepRuntime {
    bool executed{false};
    float executed_at{0.0F};
  };

  struct CommandResponse {
    float issued_at{0.0F};
    float deadline{0.0F};
    QVector3D initial_position;
    QString command;
    bool observed{false};
    bool reported{false};
  };

  struct EntityState {
    QVector3D position;
    float observed_at{0.0F};
    int health{0};
    float yaw{0.0F};
    Engine::Core::EntityID melee_lock_target_id{0};
    bool melee_lock{false};
    bool initialized{false};
  };

  struct SoldierState {
    QVector3D root_position;
    float observed_at{0.0F};
    float fight_idle_since{-1.0F};
    float terminal_pose_since{-1.0F};
    bool initialized{false};
    bool culled{false};
    bool attacking{false};
    bool completed_attack_phase{false};
  };

  struct TraceUnit {
    Engine::Core::EntityID entity_id{0};
    QString group;
    QVector3D position;
    int health{0};
    std::uint32_t target_id{0};
    QString motion;
    QString combat_mode;
    int mounted_charge_state{-1};
    int mounted_charge_cancel_reason{-1};
    int combat_action_id{0};
    bool melee_lock{false};
    std::uint32_t melee_lock_target_id{0};
    bool combat_indicator_submitted{false};
    float yaw{0.0F};
    bool movement_target{false};
    float movement_vx{0.0F};
    float movement_vz{0.0F};
    float movement_goal_x{0.0F};
    float movement_goal_z{0.0F};
    bool formation_contact{false};
    float formation_surface_gap{0.0F};
    std::vector<std::uint16_t> engaged_soldiers;
    std::vector<Engine::Core::FormationEngagementPair> engagement_pairs;
    QString construction_type;
    bool construction_site{false};
    bool construction_in_progress{false};
    float construction_time_remaining{0.0F};
    bool commander_aura_active{false};
    bool commander_aura_buffed{false};
  };

  struct TraceSoldier {
    Engine::Core::EntityID entity_id{0};
    int soldier_index{0};
    QVector3D root_position;
    float root_up_y{1.0F};
    float submitted_body_up_y{1.0F};
    float submitted_max_arm_reach{0.0F};
    bool submitted_body_pose_valid{false};
    QString declared_action;
    int declared_target_slot{-1};
    float declared_surface_gap{0.0F};
    QString animation;
    QString visual;
    float attack_phase{0.0F};
    std::uint32_t transitions{0};
    bool culled{false};
  };

  struct TraceFrame {
    float time_seconds{0.0F};
    double frame_time_ms{0.0};
    ArenaRenderedFrameTimings timings;
    std::vector<TraceUnit> units;
    std::vector<TraceSoldier> soldiers;
  };

  struct BridgeAlignmentObservation {
    bool sampled{false};
    float midpoint_distance{std::numeric_limits<float>::infinity()};
    float lateral_offset{std::numeric_limits<float>::infinity()};
  };

  Engine::Core::World& world;
  ArenaScenarioHost host;
  ArenaScenarioDefinition scenario;
  QVector3D world_origin;
  QHash<QString, std::vector<Engine::Core::EntityID>> groups;
  QHash<Engine::Core::EntityID, QString> entity_groups;
  std::vector<StepRuntime> steps;
  QHash<Engine::Core::EntityID, CommandResponse> responses;
  QHash<Engine::Core::EntityID, EntityState> entity_states;
  QHash<std::uint64_t, SoldierState> soldier_states;
  QHash<Engine::Core::EntityID, float> idle_since;
  QHash<QString, bool> visible_attacks;
  QHash<QString, bool> visible_movement;
  QHash<QString, bool> visible_attack_recoveries;
  QHash<QString, bool> visible_hit_reactions;
  QHash<QString, bool> visible_deaths;
  QHash<QString, bool> launched_casualties;
  QHash<QString, bool> charge_impacts;
  QHash<QString, bool> melee_locks_after_charge;
  QHash<QString, bool> paired_visible_attacks;
  QHash<QString, QSet<std::uint64_t>> living_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> engaged_soldiers_by_group;
  QHash<QString, QSet<std::uint64_t>> attacking_soldiers_by_group;
  QHash<std::uint64_t, int> attack_entries_by_soldier;
  QHash<QString, bool> staggered_attack_phases;
  QHash<QString, bool> damage_seen;
  QHash<QString, float> minimum_formation_surface_gap;
  QHash<QString, bool> bridge_traversal_seen;
  QHash<QString, BridgeAlignmentObservation> bridge_alignment;
  QHash<QString, float> initial_elevation;
  QHash<QString, float> maximum_elevation;
  QHash<QString, bool> useful_bot_action;
  QHash<QString, bool> commander_aura_active_seen;
  QHash<QString, bool> commander_aura_expired_seen;
  QHash<QString, bool> commander_aura_buff_seen;
  QHash<int, int> completed_construction_by_owner;
  QHash<int, int> completed_harvest_by_owner;
  QSet<Engine::Core::EntityID> latched_builder_completions;
  QSet<Engine::Core::EntityID> initial_building_ids;
  QSet<Engine::Core::EntityID> observed_constructed_building_ids;
  QHash<QString, std::uint64_t> rendered_by_group;
  QHash<QString, std::vector<float>> initial_formation_projection;
  QSet<QString> issue_keys;
  std::vector<TraceFrame> trace;
  ArenaScenarioReport report;
  float elapsed{0.0F};
  float duration_limit{0.0F};
  bool started{false};
  bool complete{false};
  bool end_expectations_checked{false};

  Impl(Engine::Core::World& world_value,
       ArenaScenarioHost host_value,
       const ArenaScenarioDefinition& definition,
       QVector3D origin)
      : world(world_value)
      , host(std::move(host_value))
      , scenario(definition)
      , world_origin(origin)
      , steps(definition.steps.size())
      , duration_limit(definition.duration_seconds) {
    report.scenario_id = definition.id;
  }

  [[nodiscard]] auto
  group_definition(const QString& name) const -> const ArenaScenarioGroup* {
    auto const found =
        std::find_if(scenario.groups.begin(),
                     scenario.groups.end(),
                     [&](auto const& candidate) { return candidate.name == name; });
    return found == scenario.groups.end() ? nullptr : &*found;
  }

  [[nodiscard]] auto
  ids(const QString& group) const -> const std::vector<Engine::Core::EntityID>& {
    static const std::vector<Engine::Core::EntityID> empty;
    auto const found = groups.constFind(group);
    return found == groups.cend() ? empty : found.value();
  }

  [[nodiscard]] auto entity_alive(Engine::Core::EntityID entity_id) const -> bool {
    auto* entity = world.get_entity(entity_id);
    auto const* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    return unit != nullptr && unit->health > 0 &&
           !entity->has_component<Engine::Core::PendingRemovalComponent>();
  }

  [[nodiscard]] auto group_destroyed(const QString& group) const -> bool {
    auto const& group_ids = ids(group);
    return !group_ids.empty() &&
           std::none_of(group_ids.begin(), group_ids.end(), [&](auto entity_id) {
             return entity_alive(entity_id);
           });
  }

  [[nodiscard]] auto centroid(const QString& group) const -> std::optional<QVector3D> {
    QVector3D total;
    int count = 0;
    for (auto const entity_id : ids(group)) {
      auto* entity = world.get_entity(entity_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      if (transform == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      total += vector_from_transform(*transform);
      ++count;
    }
    return count > 0 ? std::optional<QVector3D>(total / static_cast<float>(count))
                     : std::nullopt;
  }

  [[nodiscard]] auto groups_distance(const QString& lhs,
                                     const QString& rhs) const -> float {
    float closest = std::numeric_limits<float>::max();
    for (auto const lhs_id : ids(lhs)) {
      auto* lhs_entity = world.get_entity(lhs_id);
      auto const* lhs_transform =
          lhs_entity != nullptr
              ? lhs_entity->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      if (lhs_transform == nullptr || !entity_alive(lhs_id)) {
        continue;
      }
      for (auto const rhs_id : ids(rhs)) {
        auto* rhs_entity = world.get_entity(rhs_id);
        auto const* rhs_transform =
            rhs_entity != nullptr
                ? rhs_entity->get_component<Engine::Core::TransformComponent>()
                : nullptr;
        if (rhs_transform == nullptr || !entity_alive(rhs_id)) {
          continue;
        }
        closest = std::min(closest,
                           horizontal_distance(vector_from_transform(*lhs_transform),
                                               vector_from_transform(*rhs_transform)));
      }
    }
    return closest;
  }

  void add_issue(QString code,
                 QString message,
                 Engine::Core::EntityID entity_id = 0,
                 int soldier_index = -1) {
    QString const key =
        QStringLiteral("%1:%2:%3").arg(code).arg(entity_id).arg(soldier_index);
    if (issue_keys.contains(key)) {
      return;
    }
    issue_keys.insert(key);
    report.issues.push_back(
        {std::move(code), std::move(message), elapsed, entity_id, soldier_index});
  }

  void spawn_group(const ArenaScenarioGroup& group) {
    if (!ids(group.name).empty() || !host.spawn_unit) {
      return;
    }
    auto& spawned = groups[group.name];
    spawned.reserve(static_cast<std::size_t>(group.count));
    float const center = (static_cast<float>(group.count) - 1.0F) * 0.5F;
    for (int index = 0; index < group.count; ++index) {
      QVector3D const position = world_origin + group.origin +
                                 group.spacing * (static_cast<float>(index) - center);
      Engine::Core::EntityID const entity_id = host.spawn_unit(group, position);
      if (entity_id == 0U) {
        add_issue(
            QStringLiteral("spawn_failed"),
            QStringLiteral("%1: failed to spawn member %2").arg(group.name).arg(index));
        continue;
      }
      spawned.push_back(entity_id);
      entity_groups.insert(entity_id, group.name);
      auto* entity = world.get_entity(entity_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      if (transform != nullptr) {
        entity_states[entity_id] = {
            vector_from_transform(*transform), elapsed, 0, 1.0F};
      }
    }
    QVector3D axis = group.spacing;
    axis.setY(0.0F);
    if (axis.lengthSquared() < 0.0001F) {
      axis = QVector3D(1.0F, 0.0F, 0.0F);
    }
    axis.normalize();
    auto& projections = initial_formation_projection[group.name];
    for (auto entity_id : spawned) {
      auto* entity = world.get_entity(entity_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      projections.push_back(
          transform != nullptr
              ? QVector3D::dotProduct(vector_from_transform(*transform), axis)
              : 0.0F);
    }
  }

  [[nodiscard]] auto trigger_ready(std::size_t index,
                                   const ArenaScenarioStep& step) const -> bool {
    auto const& trigger = step.trigger;
    switch (trigger.kind) {
    case ScenarioTriggerKind::AtTime:
      return elapsed + 1.0e-5F >= trigger.time_seconds;
    case ScenarioTriggerKind::GroupDestroyed:
      return group_destroyed(trigger.group);
    case ScenarioTriggerKind::FirstContact:
    case ScenarioTriggerKind::GroupsWithinDistance: {
      float const threshold = trigger.distance > 0.0F ? trigger.distance : 2.5F;
      return groups_distance(trigger.group, trigger.target_group) <= threshold;
    }
    case ScenarioTriggerKind::GroupEnteredArea: {
      auto const group_center = centroid(trigger.group);
      float const radius = trigger.distance > 0.0F ? trigger.distance : 1.0F;
      return group_center.has_value() &&
             horizontal_distance(*group_center, world_origin + trigger.position) <=
                 radius;
    }
    case ScenarioTriggerKind::PreviousStepComplete:
      return index == 0 || steps[index - 1].executed;
    }
    return false;
  }

  void arm_response(const QString& group, const QString& command) {
    float threshold = k_default_response_seconds;
    bool response_required = false;
    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind == ArenaExpectationKind::AllGroupsRespondWithin &&
          (expectation.group.isEmpty() || expectation.group == group) &&
          expectation_active(expectation)) {
        response_required = true;
        if (expectation.threshold > 0.0F) {
          threshold = expectation.threshold;
        }
      }
    }
    if (!response_required) {
      return;
    }
    for (auto entity_id : ids(group)) {
      auto* entity = world.get_entity(entity_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      responses[entity_id] = {elapsed,
                              elapsed + threshold,
                              transform != nullptr ? vector_from_transform(*transform)
                                                   : QVector3D{},
                              command,
                              false,
                              false};
    }
  }

  void stop_group(const QString& group, bool clear_attack) {
    for (auto entity_id : ids(group)) {
      auto* entity = world.get_entity(entity_id);
      if (entity == nullptr) {
        continue;
      }
      if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
        movement->stop();
      }
      if (clear_attack) {
        entity->remove_component<Engine::Core::AttackTargetComponent>();
      }
    }
  }

  void attack_group(const QString& group, const QString& target_group, bool chase) {
    auto const& attackers = ids(group);
    auto const& targets = ids(target_group);
    if (targets.empty()) {
      add_issue(
          QStringLiteral("command_target_missing"),
          QStringLiteral("%1 cannot attack empty group %2").arg(group, target_group));
      return;
    }
    std::size_t target_index = 0;
    for (auto attacker : attackers) {
      while (target_index < targets.size() && !entity_alive(targets[target_index])) {
        ++target_index;
      }
      if (target_index >= targets.size()) {
        target_index = 0;
      }
      auto const target = targets[target_index % targets.size()];
      Game::Systems::CommandService::attack_target(world, {attacker}, target, chase);
      target_index = (target_index + 1) % targets.size();
    }
  }

  void execute_step(std::size_t index, const ArenaScenarioStep& step) {
    auto& runtime = steps[index];
    runtime.executed = true;
    runtime.executed_at = elapsed;

    switch (step.command) {
    case ScenarioCommandKind::Stand:
    case ScenarioCommandKind::Stop:
      stop_group(step.group, true);
      break;
    case ScenarioCommandKind::Move: {
      auto const& group_ids = ids(step.group);
      auto plan = Game::Systems::CommandService::plan_ground_move(
          world, group_ids, world_origin + step.destination);
      Game::Systems::CommandService::move_units(world, group_ids, plan.positions);
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::FormationMove: {
      auto const& group_ids = ids(step.group);
      auto plan = Game::Systems::CommandService::plan_ground_move(
          world, group_ids, world_origin + step.destination, true);
      Game::Systems::CommandService::issue_ground_move(world, group_ids, plan);
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::Charge:
      for (auto entity_id : ids(step.group)) {
        if (auto* entity = world.get_entity(entity_id)) {
          (void)Game::Systems::Combat::request_mounted_charge(
              *entity, Engine::Core::MountedChargeIntentSource::Player);
        }
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_run_mode(true);
          }
        }
      }
      attack_group(step.group, step.target_group, true);
      arm_response(step.group, command_name(step.command));
      break;
    case ScenarioCommandKind::Attack:
    case ScenarioCommandKind::AttackMove:
    case ScenarioCommandKind::ReleaseReserve:
      attack_group(step.group,
                   step.target_group,
                   step.command == ScenarioCommandKind::AttackMove ||
                       step.command == ScenarioCommandKind::ReleaseReserve ||
                       step.chase);
      arm_response(step.group, command_name(step.command));
      break;
    case ScenarioCommandKind::Hold:
      for (auto entity_id : ids(step.group)) {
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_hold_mode(step.enabled);
          }
        }
      }
      break;
    case ScenarioCommandKind::Guard: {
      auto const& targets = ids(step.target_group);
      for (auto entity_id : ids(step.group)) {
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_guard_mode(step.enabled);
            if (step.enabled && !targets.empty()) {
              unit->set_guard_target(targets.front());
            } else if (!step.enabled) {
              unit->clear_guard_mode();
            }
          }
        }
      }
      break;
    }
    case ScenarioCommandKind::SpawnAmbush:
      if (auto const* group = group_definition(step.group)) {
        spawn_group(*group);
      }
      if (!step.target_group.isEmpty()) {
        attack_group(step.group, step.target_group, true);
        arm_response(step.group, command_name(step.command));
      }
      break;
    case ScenarioCommandKind::SetCamera:
      if (host.set_camera) {
        host.set_camera(all_entities(),
                        {step.camera_distance, step.camera_angle, step.camera_yaw});
      }
      break;
    case ScenarioCommandKind::SetHealth:
      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
        if (unit != nullptr) {
          unit->health = std::clamp(step.value, 0, unit->max_health);
        }
      }
      break;
    case ScenarioCommandKind::ApplyDamage:
      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
        if (unit != nullptr) {
          unit->health = std::max(0, unit->health - std::max(0, step.value));
        }
      }
      break;
    case ScenarioCommandKind::MeleeLock: {
      attack_group(step.group, step.target_group, false);
      auto const& targets = ids(step.target_group);
      if (!targets.empty()) {
        for (auto entity_id : ids(step.group)) {
          auto* entity = world.get_entity(entity_id);
          auto* attack = entity != nullptr
                             ? entity->get_component<Engine::Core::AttackComponent>()
                             : nullptr;
          if (attack != nullptr) {
            attack->in_melee_lock = true;
            attack->melee_lock_target_id = targets.front();
            attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
            attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
          }
        }
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    case ScenarioCommandKind::SetFullCreatureLod:
      if (host.set_force_full_creature_lod) {
        host.set_force_full_creature_lod(step.enabled);
      }
      break;
    case ScenarioCommandKind::TriggerCommanderAura:
      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* commander =
            entity != nullptr
                ? entity->get_component<Engine::Core::CommanderComponent>()
                : nullptr;
        if (commander == nullptr) {
          add_issue(QStringLiteral("aura_commander_missing"),
                    QStringLiteral("%1 contains no commander for aura activation")
                        .arg(step.group),
                    entity_id);
          continue;
        }
        if (step.value > 0) {
          commander->aura_ability_duration = static_cast<float>(step.value);
        }
        commander->request_aura_ability();
      }
      break;
    }
  }

  void observe_commander_aura_state() {
    for (auto const& group : scenario.groups) {
      for (auto entity_id : ids(group.name)) {
        auto* entity = world.get_entity(entity_id);
        if (entity == nullptr) {
          continue;
        }
        if (auto const* commander =
                entity->get_component<Engine::Core::CommanderComponent>()) {
          if (commander->aura_ability_active) {
            commander_aura_active_seen[group.name] = true;
          } else if (commander_aura_active_seen.value(group.name, false) &&
                     commander->aura_ability_cooldown_remaining > 0.0F) {
            commander_aura_expired_seen[group.name] = true;
          }
        }
        if (auto const* buff =
                entity->get_component<Engine::Core::CommanderAuraBuffComponent>();
            buff != nullptr && buff->active) {
          commander_aura_buff_seen[group.name] = true;
        }
      }
    }
  }

  [[nodiscard]] auto all_entities() const -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> result;
    for (auto const& group : scenario.groups) {
      auto const& group_ids = ids(group.name);
      result.insert(result.end(), group_ids.begin(), group_ids.end());
    }
    return result;
  }

  [[nodiscard]] auto
  expectation_active(const ArenaExpectation& expectation) const -> bool {
    if (elapsed + 1.0e-5F < expectation.start_seconds) {
      return false;
    }
    return expectation.end_seconds <= 0.0F ||
           elapsed <= expectation.end_seconds + 1.0e-5F;
  }

  [[nodiscard]] auto applies_to(const ArenaExpectation& expectation,
                                const QString& group) const -> bool {
    return expectation.group.isEmpty() || expectation.group == group;
  }

  void observe_entity(Engine::Core::EntityID entity_id,
                      const QString& group,
                      TraceFrame& frame) {
    auto* entity = world.get_entity(entity_id);
    auto const* transform =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    auto const* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (transform == nullptr || unit == nullptr) {
      return;
    }
    QVector3D const position = vector_from_transform(*transform);
    if (!initial_elevation.contains(group)) {
      initial_elevation[group] = position.y();
      maximum_elevation[group] = position.y();
    } else {
      maximum_elevation[group] = std::max(maximum_elevation.value(group), position.y());
    }
    if (Game::Map::TerrainService::instance().is_on_bridge(position.x(),
                                                           position.z())) {
      bridge_traversal_seen[group] = true;
    }
    auto const* target = entity->get_component<Engine::Core::AttackTargetComponent>();
    auto const* motion =
        entity->get_component<Engine::Core::MotionPresentationComponent>();
    auto const* formation_contact =
        entity->get_component<Engine::Core::FormationContactComponent>();
    auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
    auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
    auto const* mounted_charge =
        entity->get_component<Engine::Core::MountedChargeComponent>();
    auto const* combat_action =
        entity->get_component<Engine::Core::RpgCommanderActionComponent>();
    if (auto const* casualties =
            entity->get_component<Engine::Core::SoldierCasualtyAnimationComponent>();
        casualties != nullptr &&
        std::any_of(casualties->entries.begin(),
                    casualties->entries.end(),
                    [](auto const& entry) { return entry.launched; })) {
      launched_casualties[group] = true;
    }
    auto const* builder =
        entity->get_component<Engine::Core::BuilderProductionComponent>();
    if (builder != nullptr && builder->construction_complete &&
        !latched_builder_completions.contains(entity_id)) {
      if (builder->product_type == "cut_tree" ||
          builder->product_type == "collect_stone" ||
          builder->product_type == "collect_iron_ore") {
        completed_harvest_by_owner[unit->owner_id]++;
      }
      latched_builder_completions.insert(entity_id);
    } else if (builder != nullptr && !builder->construction_complete) {
      latched_builder_completions.remove(entity_id);
    }
    bool const mounted_charge_impact =
        combat_action != nullptr &&
        combat_action->combat_action_id ==
            static_cast<std::uint8_t>(
                Game::Systems::CombatActions::CombatActionId::MountedChargeImpact);
    if (mounted_charge_impact) {
      charge_impacts[group] = true;
    }
    if (charge_impacts.value(group, false) && attack != nullptr &&
        attack->in_melee_lock && !mounted_charge_impact) {
      melee_locks_after_charge[group] = true;
    }
    bool const combat_indicator_submitted =
        Render::Profiling::CombatAnimationDiagnostics::instance()
            .mode_indicator_submitted(entity_id);
    QString combat_mode = QStringLiteral("none");
    if (attack != nullptr) {
      combat_mode =
          attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged
              ? QStringLiteral("ranged")
              : (attack->current_mode ==
                         Engine::Core::AttackComponent::CombatMode::Melee
                     ? QStringLiteral("melee")
                     : QStringLiteral("auto"));
    }
    QString motion_name = QStringLiteral("idle");
    if (motion != nullptr) {
      motion_name = motion->is_run_state()
                        ? QStringLiteral("run")
                        : (motion->is_walk_state() ? QStringLiteral("walk")
                                                   : QStringLiteral("idle"));
    }
    if (formation_contact != nullptr && formation_contact->in_contact) {
      auto const current = minimum_formation_surface_gap.find(group);
      if (current == minimum_formation_surface_gap.end() ||
          formation_contact->surface_gap < current.value()) {
        minimum_formation_surface_gap[group] = formation_contact->surface_gap;
      }
    }
    frame.units.push_back(
        {entity_id,
         group,
         position,
         unit->health,
         target != nullptr ? target->target_id : 0U,
         motion_name,
         combat_mode,
         mounted_charge != nullptr ? static_cast<int>(mounted_charge->state) : -1,
         mounted_charge != nullptr
             ? static_cast<int>(mounted_charge->last_cancel_reason)
             : -1,
         combat_action != nullptr ? combat_action->combat_action_id : 0,
         attack != nullptr && attack->in_melee_lock,
         attack != nullptr ? attack->melee_lock_target_id : 0U,
         combat_indicator_submitted,
         transform->rotation.y,
         movement != nullptr && movement->get_has_target(),
         movement != nullptr ? movement->get_vx() : 0.0F,
         movement != nullptr ? movement->get_vz() : 0.0F,
         movement != nullptr ? movement->get_goal_x() : 0.0F,
         movement != nullptr ? movement->get_goal_y() : 0.0F,
         formation_contact != nullptr && formation_contact->in_contact,
         formation_contact != nullptr ? formation_contact->surface_gap : 0.0F,
         formation_contact != nullptr ? formation_contact->engaged_soldier_indices
                                      : std::vector<std::uint16_t>{},
         formation_contact != nullptr
             ? formation_contact->engagement_pairs
             : std::vector<Engine::Core::FormationEngagementPair>{},
         builder != nullptr ? QString::fromStdString(builder->product_type) : QString{},
         builder != nullptr && builder->has_construction_site,
         builder != nullptr && builder->in_progress,
         builder != nullptr ? builder->time_remaining : 0.0F,
         [&]() {
           auto const* commander =
               entity->get_component<Engine::Core::CommanderComponent>();
           return commander != nullptr && commander->aura_ability_active;
         }(),
         [&]() {
           auto const* buff =
               entity->get_component<Engine::Core::CommanderAuraBuffComponent>();
           return buff != nullptr && buff->active;
         }()});

    auto& previous = entity_states[entity_id];

    if (attack != nullptr && attack->in_melee_lock && !combat_indicator_submitted) {
      for (auto const& expectation : scenario.expectations) {
        if (expectation.kind == ArenaExpectationKind::CombatIndicatorIsContinuous &&
            expectation_active(expectation) && applies_to(expectation, group)) {
          add_issue(QStringLiteral("missing_combat_indicator"),
                    QStringLiteral("%1 entity %2 had a melee lock but submitted no "
                                   "fight-mode indicator in the rendered frame")
                        .arg(group)
                        .arg(entity_id),
                    entity_id);
        }
      }
    }
    float const sample_dt =
        previous.initialized ? elapsed - previous.observed_at : 0.0F;
    if (previous.initialized && sample_dt > 0.0F) {
      float const step = horizontal_distance(position, previous.position);
      for (auto const& expectation : scenario.expectations) {
        if (!expectation_active(expectation) || !applies_to(expectation, group)) {
          continue;
        }
        if (expectation.kind == ArenaExpectationKind::MovementIsContinuous) {
          float const multiplier =
              expectation.threshold > 0.0F ? expectation.threshold : 2.5F;
          float const allowed = std::max(0.25F, unit->speed * sample_dt * multiplier);
          if (step > allowed) {
            add_issue(QStringLiteral("movement_discontinuity"),
                      QStringLiteral("%1 entity %2 moved %3 m in one rendered frame "
                                     "(allowed %4 m)")
                          .arg(group)
                          .arg(entity_id)
                          .arg(step, 0, 'f', 2)
                          .arg(allowed, 0, 'f', 2),
                      entity_id);
          }
        }
        if (expectation.kind == ArenaExpectationKind::FormationEngagementIsStable &&
            previous.melee_lock && entity_alive(previous.melee_lock_target_id)) {
          Engine::Core::EntityID const current_lock_target =
              attack != nullptr && attack->in_melee_lock ? attack->melee_lock_target_id
                                                         : 0U;
          if (current_lock_target != previous.melee_lock_target_id) {
            add_issue(QStringLiteral("melee_engagement_restarted"),
                      QStringLiteral("%1 entity %2 released or changed a living "
                                     "melee opponent during the engagement")
                          .arg(group)
                          .arg(entity_id),
                      entity_id);
          }
          bool const locomoting = motion != nullptr && motion->has_locomotion();
          bool const navigation_active =
              movement != nullptr &&
              (movement->get_has_target() || std::abs(movement->get_vx()) > 0.001F ||
               std::abs(movement->get_vz()) > 0.001F);
          if (locomoting || navigation_active || step > 0.005F) {
            add_issue(QStringLiteral("movement_during_melee_engagement"),
                      QStringLiteral("%1 entity %2 walked while its melee opponent "
                                     "was still alive")
                          .arg(group)
                          .arg(entity_id),
                      entity_id);
          }
          float const yaw_delta = std::abs(
              std::fmod(transform->rotation.y - previous.yaw + 540.0F, 360.0F) -
              180.0F);
          if (yaw_delta > 0.05F) {
            add_issue(QStringLiteral("rotation_during_melee_engagement"),
                      QStringLiteral("%1 entity %2 changed facing by %3 degrees "
                                     "while melee-locked")
                          .arg(group)
                          .arg(entity_id)
                          .arg(yaw_delta, 0, 'f', 2),
                      entity_id);
          }
        }
      }
      if (unit->health < previous.health) {
        damage_seen[group] = true;
      }
    }
    previous = {position,
                elapsed,
                unit->health,
                transform->rotation.y,
                attack != nullptr ? attack->melee_lock_target_id : 0U,
                attack != nullptr && attack->in_melee_lock,
                true};

    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::FormationEngagementIsStable ||
          !expectation_active(expectation) || !applies_to(expectation, group) ||
          formation_contact == nullptr || !formation_contact->in_contact ||
          attack == nullptr || !attack->in_melee_lock) {
        continue;
      }
      auto const* formation =
          entity->get_component<Engine::Core::FormationPresentationComponent>();
      std::size_t const living =
          formation != nullptr ? static_cast<std::size_t>(std::count_if(
                                     formation->soldiers.begin(),
                                     formation->soldiers.end(),
                                     [](auto const& soldier) { return soldier.alive; }))
                               : 0U;
      QSet<std::uint16_t> paired_slots;
      for (auto const& pair : formation_contact->engagement_pairs) {
        paired_slots.insert(pair.attacker_slot);
      }
      if (living > 0U && paired_slots.size() != static_cast<qsizetype>(living)) {
        add_issue(QStringLiteral("incomplete_formation_engagement"),
                  QStringLiteral("%1 entity %2 engaged %3 of %4 living soldiers")
                      .arg(group)
                      .arg(entity_id)
                      .arg(paired_slots.size())
                      .arg(living),
                  entity_id);
      }
      auto* lock_target = world.get_entity(attack->melee_lock_target_id);
      if (lock_target != nullptr) {
        auto const geometry =
            Game::Systems::FormationCombat::contact_geometry(*entity, *lock_target);
        bool const visibly_interpenetrating =
            geometry.surface_gap <= -(geometry.contact_tolerance + 0.001F);
        if (!previous.melee_lock && geometry.formation_overlap_required &&
            geometry.center_distance > geometry.engagement_center_distance + 0.01F &&
            !visibly_interpenetrating) {
          add_issue(QStringLiteral("insufficient_formation_overlap"),
                    QStringLiteral("%1 entity %2 locked at center distance %3 m "
                                   "instead of the %4 m overlap contract")
                        .arg(group)
                        .arg(entity_id)
                        .arg(geometry.center_distance, 0, 'f', 3)
                        .arg(geometry.engagement_center_distance, 0, 'f', 3),
                    entity_id);
        }
      }
    }

    if (auto response = responses.find(entity_id);
        response != responses.end() && !response->observed) {
      bool const moved =
          horizontal_distance(position, response->initial_position) > 0.03F;
      bool const visually_active = motion != nullptr && motion->has_locomotion();
      auto const* combat = entity->get_component<Engine::Core::CombatStateComponent>();
      bool const combat_active =
          combat != nullptr &&
          combat->animation_state != Engine::Core::CombatAnimationState::Idle;
      auto const* hold = entity->get_component<Engine::Core::HoldModeComponent>();
      bool const stance_exit_accepted =
          response->command == QStringLiteral("ReleaseReserve") && hold != nullptr &&
          !hold->active;
      response->observed =
          moved || visually_active || combat_active || stance_exit_accepted;
      if (!response->observed && elapsed > response->deadline && !response->reported) {
        response->reported = true;
        add_issue(QStringLiteral("command_response_timeout"),
                  QStringLiteral("%1 entity %2 did not visibly respond to %3 within "
                                 "%4 s")
                      .arg(group)
                      .arg(entity_id)
                      .arg(response->command)
                      .arg(response->deadline - response->issued_at, 0, 'f', 2),
                  entity_id);
      }
    }

    for (auto const& expectation : scenario.expectations) {
      if (expectation.kind != ArenaExpectationKind::NoEligibleTroopIdleDuringCombat ||
          !expectation_active(expectation) || !applies_to(expectation, group) ||
          expectation.target_group.isEmpty() || unit->health <= 0) {
        continue;
      }
      float const distance = groups_distance(group, expectation.target_group);
      float const eligible_distance = expectation.distance > 0.0F
                                          ? expectation.distance
                                          : k_default_engagement_distance;
      bool const active_order = target != nullptr && target->target_id != 0U;
      bool const active_motion = motion != nullptr && motion->has_locomotion();
      bool const eligible_idle =
          distance <= eligible_distance && !active_order && !active_motion;
      if (!eligible_idle) {
        idle_since.remove(entity_id);
        continue;
      }
      if (!idle_since.contains(entity_id)) {
        idle_since.insert(entity_id, elapsed);
      }
      float const allowed_idle =
          expectation.threshold > 0.0F ? expectation.threshold : k_default_idle_seconds;
      if (elapsed - idle_since.value(entity_id) > allowed_idle) {
        add_issue(QStringLiteral("eligible_troop_idle"),
                  QStringLiteral("%1 entity %2 stayed idle for %3 s while an eligible "
                                 "enemy was %4 m away")
                      .arg(group)
                      .arg(entity_id)
                      .arg(elapsed - idle_since.value(entity_id), 0, 'f', 2)
                      .arg(distance, 0, 'f', 2),
                  entity_id);
      }
    }

    if (motion != nullptr && motion->has_locomotion()) {
      useful_bot_action[group] = true;
    }
    if (target != nullptr && target->target_id != 0U) {
      useful_bot_action[group] = true;
    }
  }

  void observe_bridge_centerline_alignment(const QString& group) {
    QVector3D centroid;
    int living = 0;
    for (auto entity_id : ids(group)) {
      auto* entity = world.get_entity(entity_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      if (transform == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      centroid += vector_from_transform(*transform);
      ++living;
    }
    if (living == 0) {
      return;
    }
    centroid /= static_cast<float>(living);

    auto const* height_map = Game::Map::TerrainService::instance().get_height_map();
    if (height_map == nullptr) {
      return;
    }
    for (auto const& bridge : height_map->get_bridges()) {
      QVector3D direction = bridge.end - bridge.start;
      direction.setY(0.0F);
      float const length = direction.length();
      if (length < 0.01F) {
        continue;
      }
      direction /= length;
      QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
      QVector3D offset = centroid - bridge.start;
      offset.setY(0.0F);
      float const along = QVector3D::dotProduct(offset, direction);
      float const lateral = std::abs(QVector3D::dotProduct(offset, perpendicular));
      if (along < 0.0F || along > length || lateral > bridge.width * 0.5F) {
        continue;
      }

      float const midpoint_distance = std::abs(along - length * 0.5F);
      auto& observation = bridge_alignment[group];
      if (!observation.sampled || midpoint_distance < observation.midpoint_distance) {
        observation.sampled = true;
        observation.midpoint_distance = midpoint_distance;
        observation.lateral_offset = lateral;
      }
    }
  }

  void observe_soldiers(Engine::Core::EntityID entity_id,
                        const QString& group,
                        TraceFrame& frame) {
    auto* entity = world.get_entity(entity_id);
    auto const* presentation =
        entity != nullptr
            ? entity->get_component<Engine::Core::FormationPresentationComponent>()
            : nullptr;
    auto const* debug =
        Render::Profiling::CombatAnimationDiagnostics::instance().find_unit(entity_id);
    if (debug == nullptr) {
      return;
    }
    bool const formation_fight_active =
        presentation != nullptr && presentation->melee_ordered &&
        presentation->target_alive &&
        std::any_of(
            presentation->soldiers.begin(),
            presentation->soldiers.end(),
            [](auto const& soldier) {
              return soldier.alive &&
                     (soldier.action ==
                          Engine::Core::FormationSoldierAction::MeleeEngaged ||
                      soldier.action ==
                          Engine::Core::FormationSoldierAction::MeleeFollowThrough);
            });
    float minimum_attack_phase = std::numeric_limits<float>::max();
    float maximum_attack_phase = std::numeric_limits<float>::lowest();
    QSet<int> attack_phase_bins;
    int visible_attack_count = 0;
    for (auto const& soldier : debug->soldiers) {
      Engine::Core::FormationSoldierPresentation const* directive = nullptr;
      if (presentation != nullptr && soldier.soldier_index >= 0) {
        auto const slot = static_cast<std::size_t>(soldier.soldier_index);
        if (slot < presentation->soldiers.size() &&
            presentation->soldiers[slot].slot_index == slot) {
          directive = &presentation->soldiers[slot];
        }
      }
      QString declared_action = QStringLiteral("single_body");
      if (directive != nullptr) {
        switch (directive->action) {
        case Engine::Core::FormationSoldierAction::FollowUnit:
          declared_action = QStringLiteral("follow_unit");
          break;
        case Engine::Core::FormationSoldierAction::MeleeReady:
          declared_action = QStringLiteral("melee_ready");
          break;
        case Engine::Core::FormationSoldierAction::MeleeEngaged:
          declared_action = QStringLiteral("melee_engaged");
          break;
        case Engine::Core::FormationSoldierAction::MeleeFollowThrough:
          declared_action = QStringLiteral("melee_follow_through");
          break;
        }
      }
      ++report.rendered_soldier_samples;
      ++rendered_by_group[group];
      bool const culled =
          soldier.cull_reason != Render::Profiling::SoldierCullReason::None;
      bool const observed_attack =
          soldier.visual_state == Render::Profiling::SoldierVisualState::Attack &&
          !culled;
      bool const observed_movement =
          (soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
           soldier.visual_state == Render::Profiling::SoldierVisualState::Run) &&
          !culled;
      if (observed_movement) {
        visible_movement[group] = true;
      }
      if (soldier.visual_state == Render::Profiling::SoldierVisualState::HitReaction &&
          !culled) {
        visible_hit_reactions[group] = true;
      }
      if ((soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
           soldier.visual_state == Render::Profiling::SoldierVisualState::Dead) &&
          !culled) {
        visible_deaths[group] = true;
      }
      frame.soldiers.push_back(
          {entity_id,
           soldier.soldier_index,
           soldier.root_position,
           soldier.root_up_y,
           soldier.submitted_body_up_y,
           soldier.submitted_max_arm_reach,
           soldier.submitted_body_pose_valid,
           declared_action,
           directive != nullptr &&
                   (directive->action ==
                        Engine::Core::FormationSoldierAction::MeleeEngaged ||
                    directive->action ==
                        Engine::Core::FormationSoldierAction::MeleeFollowThrough)
               ? static_cast<int>(directive->target_slot)
               : -1,
           directive != nullptr ? directive->engagement_surface_gap : 0.0F,
           QString::fromLatin1(
               Render::Profiling::animation_state_name(soldier.animation_state)),
           QString::fromLatin1(
               Render::Profiling::soldier_visual_state_name(soldier.visual_state)),
           soldier.attack_phase,
           soldier.transitions_last_second,
           culled});

      std::uint64_t const key = soldier_key(entity_id, soldier.soldier_index);
      if (directive != nullptr && directive->alive) {
        living_soldiers_by_group[group].insert(key);
        if (directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged) {
          engaged_soldiers_by_group[group].insert(key);
        }
      }
      auto& previous = soldier_states[key];
      if ((observed_attack && (!previous.initialized || !previous.attacking)) ||
          soldier.attack_phase_reset) {
        ++attack_entries_by_soldier[key];
      }
      bool const recovered_to_controlled_pose =
          soldier.visual_state == Render::Profiling::SoldierVisualState::Idle ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Hold ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Walk ||
          soldier.visual_state == Render::Profiling::SoldierVisualState::Run;
      if (!culled && previous.completed_attack_phase && recovered_to_controlled_pose) {
        visible_attack_recoveries[group] = true;
      }
      if (observed_attack && soldier.attack_phase >= 0.85F) {
        previous.completed_attack_phase = true;
      }
      bool const living_formation_fighter =
          formation_fight_active && directive != nullptr && directive->alive && !culled;
      if (!living_formation_fighter) {
        previous.fight_idle_since = -1.0F;
        previous.terminal_pose_since = -1.0F;
      }
      float const step =
          previous.initialized
              ? horizontal_distance(soldier.root_position, previous.root_position)
              : 0.0F;
      for (auto const& expectation : scenario.expectations) {
        if (!expectation_active(expectation) || !applies_to(expectation, group)) {
          continue;
        }
        if (expectation.kind == ArenaExpectationKind::NoPoseOscillation &&
            soldier.churn_flagged) {
          add_issue(QStringLiteral("pose_oscillation"),
                    QStringLiteral("%1 entity %2 soldier %3 changed visual state %4 "
                                   "times in one second")
                        .arg(group)
                        .arg(entity_id)
                        .arg(soldier.soldier_index)
                        .arg(soldier.transitions_last_second),
                    entity_id,
                    soldier.soldier_index);
        }
        if (expectation.kind == ArenaExpectationKind::NoRootTeleport &&
            previous.initialized && !previous.culled && !culled &&
            elapsed - previous.observed_at <= 0.05F) {
          float const allowed = expectation.threshold > 0.0F ? expectation.threshold
                                                             : k_default_root_step;
          if (step > allowed) {
            add_issue(QStringLiteral("render_root_teleport"),
                      QStringLiteral("%1 entity %2 soldier %3 render root jumped %4 m "
                                     "between frames")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(step, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::NoUnexpectedFallPose && !culled) {
          float const minimum_up = expectation.threshold > 0.0F ? expectation.threshold
                                                                : k_default_fall_up_y;
          bool const legitimate_fall =
              soldier.visual_state == Render::Profiling::SoldierVisualState::Dying ||
              soldier.visual_state == Render::Profiling::SoldierVisualState::Dead;
          float const observed_up = soldier.submitted_body_pose_valid
                                        ? soldier.submitted_body_up_y
                                        : soldier.root_up_y;
          if (observed_up < minimum_up && !legitimate_fall) {
            add_issue(
                QStringLiteral("unexpected_fall_pose"),
                QStringLiteral("%1 entity %2 soldier %3 submitted body up-vector was "
                               "%4 while alive")
                    .arg(group)
                    .arg(entity_id)
                    .arg(soldier.soldier_index)
                    .arg(observed_up, 0, 'f', 2),
                entity_id,
                soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::NoLimbOverextension && !culled &&
            soldier.submitted_body_pose_valid) {
          float const maximum_reach =
              expectation.threshold > 0.0F ? expectation.threshold : 0.60F;
          if (soldier.submitted_max_arm_reach > maximum_reach) {
            add_issue(QStringLiteral("limb_overextension"),
                      QStringLiteral("%1 entity %2 soldier %3 submitted arm reach was "
                                     "%4 m")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(soldier.submitted_max_arm_reach, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::AllLivingSoldiersFight &&
            living_formation_fighter) {
          if (observed_attack) {
            previous.fight_idle_since = -1.0F;
          } else if (previous.fight_idle_since < 0.0F) {
            previous.fight_idle_since = elapsed;
          } else {
            float const allowed_idle =
                expectation.threshold > 0.0F ? expectation.threshold : 0.35F;
            if (elapsed - previous.fight_idle_since > allowed_idle) {
              add_issue(QStringLiteral("living_soldier_idle_in_fight"),
                        QStringLiteral("%1 entity %2 soldier %3 stayed out of its "
                                       "unit fight animation for %4 s")
                            .arg(group)
                            .arg(entity_id)
                            .arg(soldier.soldier_index)
                            .arg(elapsed - previous.fight_idle_since, 0, 'f', 2),
                        entity_id,
                        soldier.soldier_index);
            }
          }
          bool const terminal_attack_pose =
              observed_attack && soldier.attack_phase >= 0.99F;
          if (!terminal_attack_pose) {
            previous.terminal_pose_since = -1.0F;
          } else if (previous.terminal_pose_since < 0.0F) {
            previous.terminal_pose_since = elapsed;
          } else if (elapsed - previous.terminal_pose_since > 0.25F) {
            add_issue(QStringLiteral("fight_animation_terminal_stall"),
                      QStringLiteral("%1 entity %2 soldier %3 held its terminal "
                                     "fight pose for %4 s")
                          .arg(group)
                          .arg(entity_id)
                          .arg(soldier.soldier_index)
                          .arg(elapsed - previous.terminal_pose_since, 0, 'f', 2),
                      entity_id,
                      soldier.soldier_index);
          }
        }
        if (expectation.kind == ArenaExpectationKind::HoldPoseMaintained && !culled) {
          auto const* hold = entity->get_component<Engine::Core::HoldModeComponent>();
          if (hold != nullptr && hold->active &&
              soldier.animation_state != Render::Creature::AnimationStateId::Hold) {
            add_issue(
                QStringLiteral("hold_pose_replaced"),
                QStringLiteral("%1 entity %2 soldier %3 rendered %4 while Hold was "
                               "active")
                    .arg(group)
                    .arg(entity_id)
                    .arg(soldier.soldier_index)
                    .arg(QString::fromLatin1(Render::Profiling::animation_state_name(
                        soldier.animation_state))),
                entity_id,
                soldier.soldier_index);
          }
        }
      }
      if (observed_attack) {
        attacking_soldiers_by_group[group].insert(key);
        minimum_attack_phase = std::min(minimum_attack_phase, soldier.attack_phase);
        maximum_attack_phase = std::max(maximum_attack_phase, soldier.attack_phase);
        attack_phase_bins.insert(
            std::clamp(static_cast<int>(soldier.attack_phase * 12.0F), 0, 11));
        ++visible_attack_count;
        visible_attacks[group] = true;
        useful_bot_action[group] = true;
        if (presentation != nullptr && presentation->melee_ordered) {
          bool const physical_strike =
              directive != nullptr &&
              directive->action == Engine::Core::FormationSoldierAction::MeleeEngaged;
          bool const declared_follow_through =
              directive != nullptr &&
              directive->action ==
                  Engine::Core::FormationSoldierAction::MeleeFollowThrough;
          bool const continuing_visible_attack =
              declared_follow_through && previous.initialized && previous.attacking;

          if (physical_strike || continuing_visible_attack) {
            paired_visible_attacks[group] = true;
          }
        } else {

          paired_visible_attacks[group] = true;
        }
        if (auto response = responses.find(entity_id); response != responses.end()) {
          response->observed = true;
        }
      }
      previous.root_position = soldier.root_position;
      previous.observed_at = elapsed;
      previous.initialized = true;
      previous.culled = culled;
      previous.attacking = observed_attack;
    }
    bool const small_group_staggered =
        visible_attack_count >= 2 && visible_attack_count < 8 &&
        maximum_attack_phase - minimum_attack_phase >= 0.02F;
    bool const formation_staggered =
        visible_attack_count >= 8 && attack_phase_bins.size() >= 4 &&
        maximum_attack_phase - minimum_attack_phase >= 0.20F;
    if (small_group_staggered || formation_staggered) {
      staggered_attack_phases[group] = true;
    }
  }

  void check_formation_order(const ArenaExpectation& expectation) {
    auto const* definition = group_definition(expectation.group);
    auto const& group_ids = ids(expectation.group);
    if (definition == nullptr || group_ids.size() < 2) {
      return;
    }
    QVector3D axis = definition->spacing;
    axis.setY(0.0F);
    if (axis.lengthSquared() < 0.0001F) {
      return;
    }
    axis.normalize();
    float const tolerance =
        expectation.threshold > 0.0F ? expectation.threshold : 0.75F;
    float previous_projection = -std::numeric_limits<float>::max();
    for (auto entity_id : group_ids) {
      auto* entity = world.get_entity(entity_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      if (transform == nullptr || !entity_alive(entity_id)) {
        continue;
      }
      float const projection =
          QVector3D::dotProduct(vector_from_transform(*transform), axis);
      if (projection + tolerance < previous_projection) {
        add_issue(QStringLiteral("formation_order_inversion"),
                  QStringLiteral("%1 formation members crossed their stable order")
                      .arg(expectation.group),
                  entity_id);
        return;
      }
      previous_projection = projection;
    }
  }

  void check_end_expectations() {
    if (end_expectations_checked) {
      return;
    }
    end_expectations_checked = true;
    for (auto const& expectation : scenario.expectations) {
      switch (expectation.kind) {
      case ArenaExpectationKind::AttackAnimationObserved:
        if (!visible_attacks.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_attack"),
                    QStringLiteral("%1 never produced a visible attack animation")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::RepeatedAttackAnimationObserved: {
        int const required_cycles =
            expectation.threshold > 0.0F
                ? std::max(2, static_cast<int>(expectation.threshold))
                : 2;
        int missing = 0;
        for (auto const soldier : living_soldiers_by_group.value(expectation.group)) {
          if (attack_entries_by_soldier.value(soldier, 0) < required_cycles) {
            ++missing;
          }
        }
        if (missing > 0) {
          add_issue(QStringLiteral("soldiers_did_not_repeat_attack"),
                    QStringLiteral("%1 had %2 living soldiers with fewer than %3 "
                                   "visible attack cycles")
                        .arg(expectation.group)
                        .arg(missing)
                        .arg(required_cycles));
        }
        break;
      }
      case ArenaExpectationKind::MovementAnimationObserved:
        if (!visible_movement.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_movement"),
                    QStringLiteral("%1 never produced a visible movement animation")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::AttackHasVisibleContact:
        if (!visible_attacks.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_attack"),
                    QStringLiteral("%1 never produced a visible attack animation")
                        .arg(expectation.group));
        } else if (!paired_visible_attacks.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_paired_visible_attack"),
                    QStringLiteral("%1 never attacked through a physical soldier "
                                   "engagement pair")
                        .arg(expectation.group));
        } else if (!expectation.target_group.isEmpty() &&
                   !damage_seen.value(expectation.target_group, false) &&
                   !group_destroyed(expectation.target_group)) {
          add_issue(QStringLiteral("no_visible_contact_result"),
                    QStringLiteral("%1 attacked but %2 never registered contact damage")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      case ArenaExpectationKind::FormationBodyOverlapObserved: {
        float const required_overlap =
            expectation.threshold > 0.0F ? expectation.threshold : 0.20F;
        auto const observed = minimum_formation_surface_gap.find(expectation.group);
        if (observed == minimum_formation_surface_gap.end() ||
            observed.value() > -required_overlap) {
          add_issue(
              QStringLiteral("formation_body_overlap_not_observed"),
              observed == minimum_formation_surface_gap.end()
                  ? QStringLiteral("%1 never established visible formation contact")
                        .arg(expectation.group)
                  : QStringLiteral("%1 reached only %2 m of body overlap (required "
                                   "%3 m)")
                        .arg(expectation.group)
                        .arg(-observed.value(), 0, 'f', 2)
                        .arg(required_overlap, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::DeathAnimationObserved:
        if (!visible_deaths.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_death"),
                    QStringLiteral("%1 never produced a visible death animation")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::LaunchedCasualtyObserved:
        if (!launched_casualties.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_launched_casualty"),
                    QStringLiteral("%1 never produced an impact-launched casualty")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoLaunchedCasualtyObserved:
        if (launched_casualties.value(expectation.group, false)) {
          add_issue(QStringLiteral("unexpected_launched_casualty"),
                    QStringLiteral("%1 produced a launched casualty despite its "
                                   "braced hold")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::ChargeImpactPrecedesMeleeLock:
        if (!charge_impacts.value(expectation.group, false)) {
          add_issue(QStringLiteral("charge_impact_not_observed"),
                    QStringLiteral("%1 never entered its one-shot mounted charge "
                                   "impact action")
                        .arg(expectation.group));
        } else if (!melee_locks_after_charge.value(expectation.group, false)) {
          add_issue(QStringLiteral("melee_did_not_follow_charge"),
                    QStringLiteral("%1 did not enter ordinary melee lock after its "
                                   "mounted charge impact")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::AttackRecoveryObserved:
        if (!visible_attack_recoveries.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_attack_recovery"),
                    QStringLiteral("%1 never completed an attack and recovered to a "
                                   "controlled movement or idle pose")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoActiveCombatAtEnd: {
        bool active_combat = false;
        for (auto entity_id : ids(expectation.group)) {
          auto* entity = world.get_entity(entity_id);
          if (entity == nullptr || !entity_alive(entity_id)) {
            continue;
          }
          auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
          auto const* target =
              entity->get_component<Engine::Core::AttackTargetComponent>();
          auto const* combat =
              entity->get_component<Engine::Core::CombatStateComponent>();
          auto const* action =
              entity->get_component<Engine::Core::RpgCommanderActionComponent>();
          auto const* presentation =
              entity->get_component<Engine::Core::CreaturePresentationComponent>();
          active_combat =
              active_combat || (attack != nullptr && attack->in_melee_lock) ||
              (target != nullptr && target->target_id != 0) ||
              (combat != nullptr &&
               combat->animation_state != Engine::Core::CombatAnimationState::Idle) ||
              (action != nullptr &&
               (action->action_running || action->action_active)) ||
              (presentation != nullptr &&
               (presentation->is_attacking || presentation->combat_active));
        }
        if (active_combat) {
          add_issue(QStringLiteral("combat_continued_without_opponent"),
                    QStringLiteral("%1 retained attack state at scenario end")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::HitReactionObserved:
        if (!visible_hit_reactions.value(expectation.group, false)) {
          add_issue(QStringLiteral("no_visible_hit_reaction"),
                    QStringLiteral("%1 never produced a visible hit reaction")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::AllLivingSoldiersFight: {
        auto missing = living_soldiers_by_group.value(expectation.group);
        missing.subtract(attacking_soldiers_by_group.value(expectation.group));
        if (!missing.isEmpty()) {
          add_issue(QStringLiteral("soldiers_never_fought"),
                    QStringLiteral("%1 had %2 living soldiers that never "
                                   "produced a fight animation")
                        .arg(expectation.group)
                        .arg(missing.size()));
        }
        if (!engaged_soldiers_by_group.value(expectation.group).isEmpty() &&
            !staggered_attack_phases.value(expectation.group, false)) {
          add_issue(QStringLiteral("synchronized_formation_attacks"),
                    QStringLiteral("%1 never produced visibly staggered per-soldier "
                                   "fight phases")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::TargetRetakenAfterDeath: {
        bool retaken = false;
        auto const& valid_targets = ids(expectation.target_group);
        for (auto entity_id : ids(expectation.group)) {
          auto* entity = world.get_entity(entity_id);
          auto const* target =
              entity != nullptr
                  ? entity->get_component<Engine::Core::AttackTargetComponent>()
                  : nullptr;
          retaken = retaken || (target != nullptr &&
                                std::find(valid_targets.begin(),
                                          valid_targets.end(),
                                          target->target_id) != valid_targets.end());
        }
        if (!retaken) {
          add_issue(QStringLiteral("target_not_retaken"),
                    QStringLiteral("%1 did not retarget %2 after its first target died")
                        .arg(expectation.group, expectation.target_group));
        }
        break;
      }
      case ArenaExpectationKind::BotIssuesUsefulCommand:
        if (!useful_bot_action.value(expectation.group, false)) {
          add_issue(QStringLiteral("bot_inactive"),
                    QStringLiteral("AI group %1 never moved or acquired a target")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupIsRendered:
        if (rendered_by_group.value(expectation.group, 0U) == 0U) {
          add_issue(QStringLiteral("group_not_rendered"),
                    QStringLiteral("%1 produced no rendered soldier observations")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupExists:
        if (ids(expectation.group).empty() || group_destroyed(expectation.group)) {
          add_issue(QStringLiteral("group_missing"),
                    QStringLiteral("%1 did not retain a live scenario entity")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupDestroyed:
        if (!group_destroyed(expectation.group)) {
          add_issue(QStringLiteral("group_not_destroyed"),
                    QStringLiteral("%1 retained a living scenario entity")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::GroupReachedDestination: {
        QVector3D centroid;
        int living = 0;
        for (auto entity_id : ids(expectation.group)) {
          auto* entity = world.get_entity(entity_id);
          auto const* transform =
              entity != nullptr
                  ? entity->get_component<Engine::Core::TransformComponent>()
                  : nullptr;
          if (transform != nullptr && entity_alive(entity_id)) {
            centroid += vector_from_transform(*transform);
            ++living;
          }
        }
        float const tolerance =
            expectation.distance > 0.0F ? expectation.distance : 2.5F;
        QVector3D const destination = world_origin + expectation.position;
        if (living == 0 ||
            horizontal_distance(centroid / static_cast<float>(std::max(living, 1)),
                                destination) > tolerance) {
          add_issue(QStringLiteral("group_missed_destination"),
                    QStringLiteral("%1 did not reach its scenario destination")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::BridgeTraversalObserved:
        if (!bridge_traversal_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("bridge_not_traversed"),
                    QStringLiteral("%1 never occupied the bridge deck")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::BridgeCenterlineAligned: {
        auto const observation = bridge_alignment.value(expectation.group);
        float const tolerance =
            expectation.distance > 0.0F ? expectation.distance : 0.50F;
        if (!observation.sampled || observation.lateral_offset > tolerance) {
          add_issue(
              QStringLiteral("bridge_centerline_missed"),
              observation.sampled
                  ? QStringLiteral("%1 crossed %2 m from the bridge centerline at "
                                   "midspan (allowed %3 m)")
                        .arg(expectation.group)
                        .arg(observation.lateral_offset, 0, 'f', 2)
                        .arg(tolerance, 0, 'f', 2)
                  : QStringLiteral("%1 produced no bridge-midspan alignment sample")
                        .arg(expectation.group));
        }
        break;
      }
      case ArenaExpectationKind::ElevationGainObserved: {
        float const required_gain =
            expectation.threshold > 0.0F ? expectation.threshold : 1.0F;
        float const gain = maximum_elevation.value(expectation.group) -
                           initial_elevation.value(expectation.group);
        if (gain < required_gain) {
          add_issue(QStringLiteral("elevation_gain_not_observed"),
                    QStringLiteral("%1 climbed only %2 m (required %3 m)")
                        .arg(expectation.group)
                        .arg(gain, 0, 'f', 2)
                        .arg(required_gain, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::OwnerCompletesConstruction: {
        int owner_id = 0;
        if (!ids(expectation.group).empty()) {
          auto* anchor = world.get_entity(ids(expectation.group).front());
          auto const* unit = anchor != nullptr
                                 ? anchor->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
          owner_id = unit != nullptr ? unit->owner_id : 0;
        }
        const int required = std::max(1, static_cast<int>(expectation.threshold));
        if (owner_id <= 0 ||
            completed_construction_by_owner.value(owner_id) < required) {
          add_issue(QStringLiteral("economy_no_construction"),
                    QStringLiteral("%1 completed %2 of %3 required AI constructions")
                        .arg(expectation.group)
                        .arg(completed_construction_by_owner.value(owner_id))
                        .arg(required));
        }
        break;
      }
      case ArenaExpectationKind::OwnerHarvestsResource: {
        int owner_id = 0;
        if (!ids(expectation.group).empty()) {
          auto* anchor = world.get_entity(ids(expectation.group).front());
          auto const* unit = anchor != nullptr
                                 ? anchor->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
          owner_id = unit != nullptr ? unit->owner_id : 0;
        }
        const int required = std::max(1, static_cast<int>(expectation.threshold));
        if (owner_id <= 0 || completed_harvest_by_owner.value(owner_id) < required) {
          add_issue(QStringLiteral("economy_no_harvest"),
                    QStringLiteral("%1 completed %2 of %3 required resource harvests")
                        .arg(expectation.group)
                        .arg(completed_harvest_by_owner.value(owner_id))
                        .arg(required));
        }
        break;
      }
      case ArenaExpectationKind::FrameBudget: {
        std::vector<double> samples;
        samples.reserve(trace.size());
        for (auto const& frame : trace) {
          bool const after_start =
              frame.time_seconds + 1.0e-5F >= expectation.start_seconds;
          bool const before_end =
              expectation.end_seconds <= 0.0F ||
              frame.time_seconds <= expectation.end_seconds + 1.0e-5F;
          if (after_start && before_end) {
            samples.push_back(frame.frame_time_ms);
          }
        }
        if (samples.empty()) {
          break;
        }
        std::sort(samples.begin(), samples.end());
        double const budget = expectation.threshold > 0.0F ? expectation.threshold
                                                           : k_default_frame_budget_ms;
        std::size_t const p95_index = std::min<std::size_t>(
            samples.size() - 1U, ((samples.size() * 95U) + 99U) / 100U - 1U);
        double const p95 = samples[p95_index];
        double const maximum = samples.back();

        if (p95 > budget || maximum > budget * 10.0) {
          add_issue(QStringLiteral("frame_budget_exceeded"),
                    QStringLiteral("render frame p95/max was %1/%2 ms (budget %3 ms)")
                        .arg(p95, 0, 'f', 2)
                        .arg(maximum, 0, 'f', 2)
                        .arg(budget, 0, 'f', 2));
        }
        break;
      }
      case ArenaExpectationKind::CommanderAuraActivated:
        if (!commander_aura_active_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_not_activated"),
                    QStringLiteral("%1 never entered its timed command aura state")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::CommanderAuraBuffObserved:
        if (!commander_aura_buff_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_buff_not_observed"),
                    QStringLiteral("%1 never received the nearby commander bonus")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::CommanderAuraExpired:
        if (!commander_aura_expired_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_not_expired"),
                    QStringLiteral("%1 command aura did not expire into cooldown")
                        .arg(expectation.group));
        }
        break;
      case ArenaExpectationKind::NoCommanderAuraBuffObserved:
        if (commander_aura_buff_seen.value(expectation.group, false)) {
          add_issue(QStringLiteral("commander_aura_leaked_outside_radius"),
                    QStringLiteral("%1 received a commander bonus outside the aura")
                        .arg(expectation.group));
        }
        break;
      default:
        break;
      }
    }
  }
};

ArenaScenarioRunner::ArenaScenarioRunner(Engine::Core::World& world,
                                         ArenaScenarioHost host,
                                         const ArenaScenarioDefinition& definition,
                                         QVector3D world_origin)
    : m_impl(std::make_unique<Impl>(world, std::move(host), definition, world_origin)) {
}

ArenaScenarioRunner::~ArenaScenarioRunner() = default;

auto ArenaScenarioRunner::start() -> bool {
  if (m_impl->started || !validate_scenario(m_impl->scenario).empty() ||
      !m_impl->host.spawn_unit) {
    return false;
  }
  m_impl->started = true;
  for (auto const& group : m_impl->scenario.groups) {
    if (group.spawn_at_start) {
      m_impl->spawn_group(group);
    }
  }
  for (auto* entity :
       m_impl->world.get_entities_with<Engine::Core::BuildingComponent>()) {
    if (entity != nullptr) {
      m_impl->initial_building_ids.insert(entity->get_id());
    }
  }
  if (m_impl->host.set_camera) {
    m_impl->host.set_camera(m_impl->all_entities(), m_impl->scenario.camera);
  }
  for (std::size_t i = 0; i < m_impl->scenario.steps.size(); ++i) {
    auto const& step = m_impl->scenario.steps[i];
    if (!m_impl->steps[i].executed && m_impl->trigger_ready(i, step)) {
      m_impl->execute_step(i, step);
    }
  }
  return true;
}

void ArenaScenarioRunner::update(float simulation_dt) {
  if (!m_impl->started || m_impl->complete || simulation_dt <= 0.0F) {
    return;
  }
  m_impl->elapsed += simulation_dt;
  m_impl->report.elapsed_seconds = m_impl->elapsed;
  for (std::size_t i = 0; i < m_impl->scenario.steps.size(); ++i) {
    auto const& step = m_impl->scenario.steps[i];
    if (!m_impl->steps[i].executed && m_impl->trigger_ready(i, step)) {
      m_impl->execute_step(i, step);
    }
  }
  m_impl->observe_commander_aura_state();
  for (auto const& expectation : m_impl->scenario.expectations) {
    if (expectation.kind == ArenaExpectationKind::FormationOrderPreserved &&
        m_impl->expectation_active(expectation)) {
      m_impl->check_formation_order(expectation);
    }
  }
  if (m_impl->elapsed + 1.0e-5F >= m_impl->duration_limit) {
    m_impl->check_end_expectations();
    m_impl->complete = true;
  }
}

void ArenaScenarioRunner::observe_rendered_frame(double frame_time_ms) {
  ArenaRenderedFrameTimings timings;
  timings.total_ms = frame_time_ms;
  observe_rendered_frame(timings);
}

void ArenaScenarioRunner::observe_rendered_frame(
    const ArenaRenderedFrameTimings& timings) {
  if (!m_impl->started) {
    return;
  }
  ++m_impl->report.rendered_frames;
  Impl::TraceFrame frame;
  frame.time_seconds = m_impl->elapsed;
  frame.frame_time_ms = timings.total_ms;
  frame.timings = timings;
  for (auto* entity :
       m_impl->world.get_entities_with<Engine::Core::BuildingComponent>()) {
    if (entity == nullptr || m_impl->initial_building_ids.contains(entity->get_id()) ||
        m_impl->observed_constructed_building_ids.contains(entity->get_id())) {
      continue;
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->health > 0) {
      m_impl->observed_constructed_building_ids.insert(entity->get_id());
      m_impl->completed_construction_by_owner[unit->owner_id]++;
    }
  }
  for (auto const& group : m_impl->scenario.groups) {
    for (auto entity_id : m_impl->ids(group.name)) {
      m_impl->observe_entity(entity_id, group.name, frame);
      m_impl->observe_soldiers(entity_id, group.name, frame);
    }
    m_impl->observe_bridge_centerline_alignment(group.name);
  }
  m_impl->trace.push_back(std::move(frame));
}

void ArenaScenarioRunner::set_duration_limit(float duration_seconds) {
  if (duration_seconds > 0.0F) {
    m_impl->duration_limit = duration_seconds;
  }
}

auto ArenaScenarioRunner::definition() const noexcept
    -> const ArenaScenarioDefinition& {
  return m_impl->scenario;
}

auto ArenaScenarioRunner::elapsed_seconds() const noexcept -> float {
  return m_impl->elapsed;
}

auto ArenaScenarioRunner::finished() const noexcept -> bool {
  return m_impl->complete;
}

auto ArenaScenarioRunner::report() const noexcept -> const ArenaScenarioReport& {
  return m_impl->report;
}

auto ArenaScenarioRunner::group_entities(const QString& group) const
    -> const std::vector<Engine::Core::EntityID>& {
  return m_impl->ids(group);
}

auto ArenaScenarioRunner::all_entities() const -> std::vector<Engine::Core::EntityID> {
  return m_impl->all_entities();
}

auto ArenaScenarioRunner::issue_revision() const noexcept -> std::size_t {
  return m_impl->report.issues.size();
}

auto ArenaScenarioRunner::write_artifacts(const QString& directory,
                                          QString* error) const -> bool {
  QDir const dir;
  if (!dir.mkpath(directory)) {
    if (error != nullptr) {
      *error = QStringLiteral("failed to create artifact directory %1").arg(directory);
    }
    return false;
  }

  QJsonObject report_object;
  report_object.insert(QStringLiteral("scenario"), m_impl->report.scenario_id);
  report_object.insert(QStringLiteral("completed"), m_impl->complete);
  report_object.insert(QStringLiteral("passed"),
                       m_impl->complete && m_impl->report.passed());
  report_object.insert(QStringLiteral("elapsed_seconds"),
                       m_impl->report.elapsed_seconds);
  report_object.insert(QStringLiteral("rendered_frames"),
                       static_cast<qint64>(m_impl->report.rendered_frames));
  report_object.insert(QStringLiteral("rendered_soldier_samples"),
                       static_cast<qint64>(m_impl->report.rendered_soldier_samples));
  QJsonArray issues;
  for (auto const& issue : m_impl->report.issues) {
    issues.append(
        QJsonObject{{QStringLiteral("code"), issue.code},
                    {QStringLiteral("message"), issue.message},
                    {QStringLiteral("time_seconds"), issue.time_seconds},
                    {QStringLiteral("entity_id"), static_cast<qint64>(issue.entity_id)},
                    {QStringLiteral("soldier_index"), issue.soldier_index}});
  }
  report_object.insert(QStringLiteral("issues"), issues);

  QFile report_file(QDir(directory).filePath(QStringLiteral("report.json")));
  if (!report_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error != nullptr) {
      *error = report_file.errorString();
    }
    return false;
  }
  report_file.write(QJsonDocument(report_object).toJson(QJsonDocument::Indented));
  report_file.close();

  QFile trace_file(QDir(directory).filePath(QStringLiteral("trace.jsonl")));
  if (!trace_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (error != nullptr) {
      *error = trace_file.errorString();
    }
    return false;
  }
  for (auto const& frame : m_impl->trace) {
    QJsonArray units;
    for (auto const& unit : frame.units) {
      QJsonArray engaged_soldiers;
      for (auto index : unit.engaged_soldiers) {
        engaged_soldiers.append(static_cast<int>(index));
      }
      QJsonArray engagement_pairs;
      for (auto const& pair : unit.engagement_pairs) {
        engagement_pairs.append(
            QJsonObject{{QStringLiteral("attacker_slot"), pair.attacker_slot},
                        {QStringLiteral("target_slot"), pair.target_slot},
                        {QStringLiteral("root_distance"), pair.root_distance},
                        {QStringLiteral("surface_gap"), pair.surface_gap}});
      }
      units.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(unit.entity_id)},
          {QStringLiteral("group"), unit.group},
          {QStringLiteral("position"), json_vector(unit.position)},
          {QStringLiteral("health"), unit.health},
          {QStringLiteral("target_id"), static_cast<qint64>(unit.target_id)},
          {QStringLiteral("motion"), unit.motion},
          {QStringLiteral("combat_mode"), unit.combat_mode},
          {QStringLiteral("mounted_charge_state"), unit.mounted_charge_state},
          {QStringLiteral("mounted_charge_cancel_reason"),
           unit.mounted_charge_cancel_reason},
          {QStringLiteral("combat_action_id"), unit.combat_action_id},
          {QStringLiteral("melee_lock"), unit.melee_lock},
          {QStringLiteral("melee_lock_target_id"),
           static_cast<qint64>(unit.melee_lock_target_id)},
          {QStringLiteral("combat_indicator_submitted"),
           unit.combat_indicator_submitted},
          {QStringLiteral("yaw"), unit.yaw},
          {QStringLiteral("movement_target"), unit.movement_target},
          {QStringLiteral("movement_velocity"),
           QJsonArray{unit.movement_vx, unit.movement_vz}},
          {QStringLiteral("movement_goal"),
           QJsonArray{unit.movement_goal_x, unit.movement_goal_z}},
          {QStringLiteral("formation_contact"), unit.formation_contact},
          {QStringLiteral("formation_surface_gap"), unit.formation_surface_gap},
          {QStringLiteral("engaged_soldiers"), engaged_soldiers},
          {QStringLiteral("engagement_pairs"), engagement_pairs},
          {QStringLiteral("construction_type"), unit.construction_type},
          {QStringLiteral("construction_site"), unit.construction_site},
          {QStringLiteral("construction_in_progress"), unit.construction_in_progress},
          {QStringLiteral("construction_time_remaining"),
           unit.construction_time_remaining},
          {QStringLiteral("commander_aura_active"), unit.commander_aura_active},
          {QStringLiteral("commander_aura_buffed"), unit.commander_aura_buffed}});
    }
    QJsonArray soldiers;
    for (auto const& soldier : frame.soldiers) {
      soldiers.append(QJsonObject{
          {QStringLiteral("entity_id"), static_cast<qint64>(soldier.entity_id)},
          {QStringLiteral("soldier_index"), soldier.soldier_index},
          {QStringLiteral("root_position"), json_vector(soldier.root_position)},
          {QStringLiteral("root_up_y"), soldier.root_up_y},
          {QStringLiteral("submitted_body_up_y"), soldier.submitted_body_up_y},
          {QStringLiteral("submitted_max_arm_reach"), soldier.submitted_max_arm_reach},
          {QStringLiteral("submitted_body_pose_valid"),
           soldier.submitted_body_pose_valid},
          {QStringLiteral("declared_action"), soldier.declared_action},
          {QStringLiteral("declared_target_slot"), soldier.declared_target_slot},
          {QStringLiteral("declared_surface_gap"), soldier.declared_surface_gap},
          {QStringLiteral("animation"), soldier.animation},
          {QStringLiteral("visual"), soldier.visual},
          {QStringLiteral("attack_phase"), soldier.attack_phase},
          {QStringLiteral("transitions_last_second"),
           static_cast<qint64>(soldier.transitions)},
          {QStringLiteral("culled"), soldier.culled}});
    }
    QJsonObject const line{
        {QStringLiteral("time_seconds"), frame.time_seconds},
        {QStringLiteral("frame_time_ms"), frame.frame_time_ms},
        {QStringLiteral("frame_breakdown_ms"),
         QJsonObject{
             {QStringLiteral("simulation"), frame.timings.simulation_ms},
             {QStringLiteral("terrain_submit"), frame.timings.terrain_submit_ms},
             {QStringLiteral("world_submit"), frame.timings.world_submit_ms},
             {QStringLiteral("effects_submit"), frame.timings.effects_submit_ms},
             {QStringLiteral("playback"), frame.timings.playback_ms},
             {QStringLiteral("overlays"), frame.timings.overlays_ms},
             {QStringLiteral("humanoid_preparation"),
              frame.timings.humanoid_preparation_ms},
             {QStringLiteral("animation_sampling"),
              frame.timings.animation_sampling_ms},
             {QStringLiteral("bpat_playback"), frame.timings.bpat_playback_ms},
             {QStringLiteral("layout_generation"),
              frame.timings.layout_generation_ms}}},
        {QStringLiteral("visible_soldiers"),
         static_cast<qint64>(frame.timings.visible_soldiers)},
        {QStringLiteral("draw_calls"), static_cast<qint64>(frame.timings.draw_calls)},
        {QStringLiteral("units"), units},
        {QStringLiteral("soldiers"), soldiers}};
    trace_file.write(QJsonDocument(line).toJson(QJsonDocument::Compact));
    trace_file.write("\n");
  }
  return true;
}

} // namespace Arena
