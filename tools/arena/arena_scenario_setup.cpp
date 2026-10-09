#include <QtMath>

#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

namespace {

constexpr float k_default_response_seconds = 0.45F;

auto owner_of(Engine::Core::World& world, Engine::Core::EntityID entity_id) -> int {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  return unit != nullptr ? unit->owner_id : 0;
}

auto command_name(ScenarioCommandKind kind) -> QString {
  switch (kind) {
  case ScenarioCommandKind::Stand:
    return QStringLiteral("Stand");
  case ScenarioCommandKind::Move:
    return QStringLiteral("Move");
  case ScenarioCommandKind::FormationMove:
    return QStringLiteral("FormationMove");
  case ScenarioCommandKind::Run:
    return QStringLiteral("Run");
  case ScenarioCommandKind::FormArmy:
    return QStringLiteral("FormArmy");
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
  case ScenarioCommandKind::TriggerFlagRally:
    return QStringLiteral("TriggerFlagRally");
  case ScenarioCommandKind::RpgPrimaryAttack:
    return QStringLiteral("RpgPrimaryAttack");
  case ScenarioCommandKind::RpgHeavyAttack:
    return QStringLiteral("RpgHeavyAttack");
  case ScenarioCommandKind::RpgAttackHold:
    return QStringLiteral("RpgAttackHold");
  case ScenarioCommandKind::RpgAim:
    return QStringLiteral("RpgAim");
  case ScenarioCommandKind::RpgGuard:
    return QStringLiteral("RpgGuard");
  case ScenarioCommandKind::RpgDodge:
    return QStringLiteral("RpgDodge");
  case ScenarioCommandKind::RpgJump:
    return QStringLiteral("RpgJump");
  case ScenarioCommandKind::RpgSpecial:
    return QStringLiteral("RpgSpecial");
  case ScenarioCommandKind::RpgWeaponSwitch:
    return QStringLiteral("RpgWeaponSwitch");
  case ScenarioCommandKind::RepairStructure:
    return QStringLiteral("RepairStructure");
  case ScenarioCommandKind::DismantleStructure:
    return QStringLiteral("DismantleStructure");
  case ScenarioCommandKind::StartConstruction:
    return QStringLiteral("StartConstruction");
  case ScenarioCommandKind::DeliverToStructure:
    return QStringLiteral("DeliverToStructure");
  case ScenarioCommandKind::HarvestResource:
    return QStringLiteral("HarvestResource");
  case ScenarioCommandKind::AbandonWork:
    return QStringLiteral("AbandonWork");
  case ScenarioCommandKind::SetFarmGrowth:
    return QStringLiteral("SetFarmGrowth");
  case ScenarioCommandKind::RpgMove:
    return QStringLiteral("RpgMove");
  case ScenarioCommandKind::RpgCycleLockOn:
    return QStringLiteral("RpgCycleLockOn");
  case ScenarioCommandKind::ReloadUndeadZoneState:
    return QStringLiteral("ReloadUndeadZoneState");
  case ScenarioCommandKind::TriggerRockfall:
    return QStringLiteral("TriggerRockfall");
  case ScenarioCommandKind::RollStones:
    return QStringLiteral("RollStones");
  case ScenarioCommandKind::CrossByRaft:
    return QStringLiteral("CrossByRaft");
  case ScenarioCommandKind::Marker:
    return QStringLiteral("Marker");
  case ScenarioCommandKind::ShapeMove:
    return QStringLiteral("ShapeMove");
  case ScenarioCommandKind::Wheel:
    return QStringLiteral("Wheel");
  case ScenarioCommandKind::SetWeather:
    return QStringLiteral("SetWeather");
  }
  return QStringLiteral("Unknown");
}

} // namespace

ArenaScenarioRunner::Impl::Impl(Engine::Core::World& world_value,
                                ArenaScenarioHost host_value,
                                const ArenaScenarioDefinition& definition,
                                QVector3D origin)
    : world(world_value)
    , host(std::move(host_value))
    , scenario(definition)
    , world_origin(origin)
    , steps(definition.steps.size())
    , weather_schedule(definition.weather_script.timeline)
    , duration_limit(definition.duration_seconds) {
  report.scenario_id = definition.id;
  for (std::size_t i = 0; i < definition.steps.size(); ++i) {
    auto const& name = definition.steps[i].name;
    if (!name.isEmpty() && !step_index_by_name.contains(name)) {
      step_index_by_name.insert(name, i);
    }
  }
}

auto ArenaScenarioRunner::Impl::named_step_executed_at(const QString& name) const
    -> std::optional<float> {
  auto const found = step_index_by_name.constFind(name);
  if (found == step_index_by_name.cend() || !steps[found.value()].executed) {
    return std::nullopt;
  }
  return steps[found.value()].executed_at;
}

auto ArenaScenarioRunner::Impl::group_strength(const QString& group) const
    -> std::optional<float> {
  auto const initial = initial_health_by_group.constFind(group);
  if (initial == initial_health_by_group.cend() || initial.value() <= 0) {
    return std::nullopt;
  }
  return static_cast<float>(group_health(group)) /
         static_cast<float>(initial.value());
}

auto ArenaScenarioRunner::Impl::ordered_destination(const QString& group) const
    -> QVector3D {
  QVector3D destination;
  bool formed = false;
  for (const auto& step : scenario.steps) {
    bool const moves_group =
        step.group == group && (step.command == ScenarioCommandKind::Move ||
                                step.command == ScenarioCommandKind::FormationMove ||
                                step.command == ScenarioCommandKind::Run ||
                                step.command == ScenarioCommandKind::AttackMove);
    if (moves_group) {
      destination = step.destination;
      formed = false;
    } else if (step.command == ScenarioCommandKind::FormArmy &&
               (step.formation.groups.contains(group) ||
                (step.formation.groups.isEmpty() && step.group == group))) {
      destination = step.formation.anchor;
      formed = true;
    }
  }
  auto const planned = formed_destinations.constFind(group);
  if (formed && planned != formed_destinations.cend()) {
    return planned.value();
  }
  return destination;
}

auto ArenaScenarioRunner::Impl::group_definition(const QString& name) const
    -> const ArenaScenarioGroup* {
  auto const found =
      std::find_if(scenario.groups.begin(),
                   scenario.groups.end(),
                   [&](auto const& candidate) { return candidate.name == name; });
  return found == scenario.groups.end() ? nullptr : &*found;
}

auto ArenaScenarioRunner::Impl::ids(const QString& group) const
    -> const std::vector<Engine::Core::EntityID>& {
  static const std::vector<Engine::Core::EntityID> empty;
  auto const found = groups.constFind(group);
  return found == groups.cend() ? empty : found.value();
}

auto ArenaScenarioRunner::Impl::step_entities(const ArenaScenarioStep& step) const
    -> std::vector<Engine::Core::EntityID> {
  if (step.zone_id.isEmpty()) {
    return ids(step.group);
  }
  auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
  auto const shrine_id = undead != nullptr ? undead->anchor_entity(step.zone_id) : 0U;
  if (shrine_id == 0U) {
    return {};
  }
  return {shrine_id};
}

auto ArenaScenarioRunner::Impl::entity_alive(Engine::Core::EntityID entity_id) const
    -> bool {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  return unit != nullptr && unit->health > 0 &&
         !world.has<Engine::Core::PendingRemovalComponent>(entity_id);
}

auto ArenaScenarioRunner::Impl::group_health(const QString& group) const -> int {
  int total = 0;
  for (auto entity_id : ids(group)) {
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
    if (unit != nullptr) {
      total += std::max(0, unit->health);
    }
  }
  return total;
}

auto ArenaScenarioRunner::Impl::commander_frames() const
    -> std::vector<const TraceFrame*> {
  std::vector<const TraceFrame*> frames;
  frames.reserve(trace.size());
  for (auto const& frame : trace) {
    if (frame.commander.valid) {
      frames.push_back(&frame);
    }
  }
  return frames;
}

auto ArenaScenarioRunner::Impl::group_destroyed(const QString& group) const -> bool {
  auto const& group_ids = ids(group);
  return !group_ids.empty() &&
         std::none_of(group_ids.begin(), group_ids.end(), [&](auto entity_id) {
           return entity_alive(entity_id);
         });
}

auto ArenaScenarioRunner::Impl::centroid(const QString& group) const
    -> std::optional<QVector3D> {
  QVector3D total;
  int count = 0;
  for (auto const entity_id : ids(group)) {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform == nullptr || !entity_alive(entity_id)) {
      continue;
    }
    total += vector_from_transform(*transform);
    ++count;
  }
  return count > 0 ? std::optional<QVector3D>(total / static_cast<float>(count))
                   : std::nullopt;
}

void ArenaScenarioRunner::Impl::track_rpg_aim() {
  if (rpg_aim_shooter_group.isEmpty() || rpg_aim_target_group.isEmpty()) {
    return;
  }
  auto const target = centroid(rpg_aim_target_group);
  if (!target.has_value()) {

    rpg_aim_shooter_group.clear();
    rpg_aim_target_group.clear();
    return;
  }

  constexpr float k_chest_height = 1.15F;
  QVector3D const aim_point = *target + QVector3D(0.0F, k_chest_height, 0.0F);
  for (auto entity_id : ids(rpg_aim_shooter_group)) {
    if (host.aim_rpg_view_at) {
      host.aim_rpg_view_at(entity_id, aim_point);
      continue;
    }
    if (!host.set_rpg_view_yaw || !host.set_rpg_view_pitch) {
      continue;
    }
    auto const* shooter = world.get_entity(entity_id);
    auto const* shooter_transform =
        shooter != nullptr ? shooter->get_component<Engine::Core::TransformComponent>()
                           : nullptr;
    if (shooter_transform == nullptr) {
      continue;
    }
    constexpr float k_eye_height = 1.55F;
    float const dx = aim_point.x() - shooter_transform->position.x;
    float const dz = aim_point.z() - shooter_transform->position.z;
    float const flat = std::sqrt((dx * dx) + (dz * dz));
    host.set_rpg_view_yaw(entity_id,
                          std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>);
    host.set_rpg_view_pitch(
        entity_id,
        std::atan2(aim_point.y() - (shooter_transform->position.y + k_eye_height),
                   std::max(flat, 0.01F)) *
            180.0F / std::numbers::pi_v<float>);
  }
}

auto ArenaScenarioRunner::Impl::groups_distance(const QString& lhs,
                                                const QString& rhs) const -> float {
  float closest = std::numeric_limits<float>::max();
  for (auto const lhs_id : ids(lhs)) {
    auto const* lhs_transform = world.try_get<Engine::Core::TransformComponent>(lhs_id);
    if (lhs_transform == nullptr || !entity_alive(lhs_id)) {
      continue;
    }
    for (auto const rhs_id : ids(rhs)) {
      auto const* rhs_transform =
          world.try_get<Engine::Core::TransformComponent>(rhs_id);
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

void ArenaScenarioRunner::Impl::add_issue(QString code,
                                          QString message,
                                          Engine::Core::EntityID entity_id,
                                          int soldier_index) {
  QString const key =
      QStringLiteral("%1:%2:%3").arg(code).arg(entity_id).arg(soldier_index);
  if (issue_keys.contains(key)) {
    return;
  }
  issue_keys.insert(key);
  report.issues.push_back(
      {std::move(code), std::move(message), elapsed, entity_id, soldier_index});
}

void ArenaScenarioRunner::Impl::spawn_group(const ArenaScenarioGroup& group) {
  if (!ids(group.name).empty() || !host.spawn_unit) {
    return;
  }
  auto& spawned = groups[group.name];
  bool const explicit_positions = !group.positions.empty();
  int const member_count =
      explicit_positions ? static_cast<int>(group.positions.size()) : group.count;
  spawned.reserve(static_cast<std::size_t>(member_count));
  float const center = (static_cast<float>(group.count) - 1.0F) * 0.5F;
  for (int index = 0; index < member_count; ++index) {
    QVector3D const position =
        explicit_positions
            ? world_origin + group.positions[static_cast<std::size_t>(index)]
            : world_origin + group.origin +
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
    auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
    if (unit != nullptr) {
      initial_health_by_group[group.name] += unit->health;
    }
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform != nullptr) {
      entity_states[entity_id] = {vector_from_transform(*transform), elapsed, 0, 1.0F};
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
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    projections.push_back(
        transform != nullptr
            ? QVector3D::dotProduct(vector_from_transform(*transform), axis)
            : 0.0F);
  }
}

auto ArenaScenarioRunner::Impl::trigger_ready(
    std::size_t index, const ArenaScenarioStep& step) const -> bool {
  auto const& trigger = step.trigger;
  if (!trigger.after_step.isEmpty() &&
      !named_step_executed_at(trigger.after_step).has_value()) {
    return false;
  }
  if (trigger.fallback_seconds >= 0.0F &&
      elapsed + 1.0e-5F >= trigger.fallback_seconds) {
    return true;
  }
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
  case ScenarioTriggerKind::GroupStrengthBelow: {
    auto const strength = group_strength(trigger.group);
    return strength.has_value() && *strength < trigger.threshold;
  }
  case ScenarioTriggerKind::StepExecuted: {
    auto const executed_at = named_step_executed_at(trigger.step);
    return executed_at.has_value() &&
           elapsed + 1.0e-5F >= *executed_at + trigger.time_seconds;
  }
  }
  return false;
}

void ArenaScenarioRunner::Impl::shape_move_group(const ArenaScenarioStep& step) {
  auto const center = centroid(step.group);
  if (!center.has_value()) {
    return;
  }
  QVector3D offset = (world_origin + step.destination) - *center;
  offset.setY(0.0F);
  std::vector<Game::Systems::CommandService::MoveIntent> intents;
  for (auto entity_id : ids(step.group)) {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform == nullptr || !entity_alive(entity_id)) {
      continue;
    }
    Game::Systems::CommandService::MoveIntent intent;
    intent.unit_id = entity_id;
    intent.target = vector_from_transform(*transform) + offset;
    intent.target.setY(0.0F);
    intent.facing_angle = transform->rotation.y;
    intents.push_back(intent);
  }
  if (!intents.empty()) {
    Game::Systems::CommandService::move_units(world, intents);
  }
  arm_response(step.group, command_name(step.command));
}

void ArenaScenarioRunner::Impl::wheel_group(const ArenaScenarioStep& step) {
  std::vector<std::pair<Engine::Core::EntityID, QVector3D>> members;
  for (auto entity_id : ids(step.group)) {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform != nullptr && entity_alive(entity_id)) {
      members.emplace_back(entity_id, vector_from_transform(*transform));
    }
  }
  if (members.empty()) {
    return;
  }
  QVector3D pivot = world_origin + step.destination;
  if (!step.pivot_side.isEmpty()) {
    QVector3D sum;
    float yaw_sum = 0.0F;
    for (auto const& [entity_id, position] : members) {
      sum += position;
      auto const* transform =
          world.try_get<Engine::Core::TransformComponent>(entity_id);
      yaw_sum += transform != nullptr ? transform->rotation.y : 0.0F;
    }
    QVector3D const center = sum / static_cast<float>(members.size());
    pivot = center;
    if (step.pivot_side != QStringLiteral("center")) {
      float const yaw = qDegreesToRadians(yaw_sum / static_cast<float>(members.size()));
      QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
      QVector3D const right(-forward.z(), 0.0F, forward.x());
      bool const want_right = step.pivot_side == QStringLiteral("right");
      float best = want_right ? -std::numeric_limits<float>::max()
                              : std::numeric_limits<float>::max();
      for (auto const& [entity_id, position] : members) {
        (void)entity_id;
        float const along = QVector3D::dotProduct(position - center, right);
        if ((want_right && along > best) || (!want_right && along < best)) {
          best = along;
          pivot = center + right * along;
        }
      }
    }
  }
  float const radians = qDegreesToRadians(step.angle_degrees);
  float const cos_a = std::cos(radians);
  float const sin_a = std::sin(radians);
  std::vector<Game::Systems::CommandService::MoveIntent> intents;
  intents.reserve(members.size());
  for (auto const& [entity_id, position] : members) {
    QVector3D const local = position - pivot;
    QVector3D const rotated(local.x() * cos_a + local.z() * sin_a,
                            0.0F,
                            -local.x() * sin_a + local.z() * cos_a);
    Game::Systems::CommandService::MoveIntent intent;
    intent.unit_id = entity_id;
    intent.target = pivot + rotated;
    intent.target.setY(0.0F);
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    intent.facing_angle =
        (transform != nullptr ? transform->rotation.y : 0.0F) + step.angle_degrees;
    intents.push_back(intent);
  }
  Game::Systems::CommandService::move_units(world, intents);
  arm_response(step.group, command_name(step.command));
}

void ArenaScenarioRunner::Impl::arm_response(const QString& group,
                                             const QString& command) {
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
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    responses[entity_id] = {elapsed,
                            elapsed + threshold,
                            transform != nullptr ? vector_from_transform(*transform)
                                                 : QVector3D{},
                            transform != nullptr ? transform->rotation.y : 0.0F,
                            command,
                            false,
                            false};
  }
}

void ArenaScenarioRunner::Impl::stop_group(const QString& group, bool clear_attack) {
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

void ArenaScenarioRunner::Impl::attack_group(const QString& group,
                                             const QString& target_group,
                                             bool chase) {
  auto const& attackers = ids(group);
  auto const& targets = ids(target_group);
  if (targets.empty()) {
    add_issue(
        QStringLiteral("command_target_missing"),
        QStringLiteral("%1 cannot attack empty group %2").arg(group, target_group));
    return;
  }
  std::vector<Engine::Core::EntityID> living;
  living.reserve(targets.size());
  for (auto target : targets) {
    if (entity_alive(target)) {
      living.push_back(target);
    }
  }
  if (living.empty()) {
    add_issue(
        QStringLiteral("command_target_missing"),
        QStringLiteral("%1 cannot attack empty group %2").arg(group, target_group));
    return;
  }

  const std::size_t share = (attackers.size() + living.size() - 1U) / living.size();
  std::unordered_map<Engine::Core::EntityID, std::size_t> assigned;

  const auto position_of =
      [&](Engine::Core::EntityID entity_id) -> std::optional<QVector3D> {
    auto const* transform = world.try_get<Engine::Core::TransformComponent>(entity_id);
    if (transform == nullptr) {
      return std::nullopt;
    }
    return vector_from_transform(*transform);
  };

  for (auto attacker : attackers) {
    auto const from = position_of(attacker);
    Engine::Core::EntityID chosen = living.front();
    float best = std::numeric_limits<float>::max();
    bool found = false;
    for (bool honour_share : {true, false}) {
      for (auto target : living) {
        if (honour_share && assigned[target] >= share) {
          continue;
        }
        auto const to = position_of(target);
        float const distance = (from.has_value() && to.has_value())
                                   ? horizontal_distance(*from, *to)
                                   : 0.0F;
        if (!found || distance < best) {
          best = distance;
          chosen = target;
          found = true;
        }
      }
      if (found) {
        break;
      }
    }
    Game::Systems::CommandService::attack_target(world, {attacker}, chosen, chase);
    ++assigned[chosen];
  }
}

void ArenaScenarioRunner::Impl::form_army(const ArenaScenarioStep& step) {
  std::vector<Engine::Core::EntityID> members;
  QStringList sources = step.formation.groups;
  if (sources.isEmpty()) {
    sources.append(step.group);
  }
  for (auto const& name : sources) {
    auto const& group_ids = ids(name);
    members.insert(members.end(), group_ids.begin(), group_ids.end());
  }
  members.erase(
      std::remove_if(members.begin(),
                     members.end(),
                     [&](auto entity_id) { return !entity_alive(entity_id); }),
      members.end());
  if (members.empty()) {
    add_issue(
        QStringLiteral("formation_members_missing"),
        QStringLiteral("FormArmy step '%1' resolved no living units").arg(step.name));
    return;
  }

  Game::Formation::ArmyFormationRequest request;
  request.members = members;
  request.anchor = world_origin + step.formation.anchor;
  request.facing = step.formation.facing_degrees;
  request.frontage = step.formation.frontage;
  request.intent = step.formation.intent;
  request.options = step.formation.options;
  if (!step.formation.doctrine.isEmpty()) {
    request.doctrine = step.formation.doctrine.toStdString();
    request.options.doctrine_locked = true;
  }
  if (step.formation.spacing > 0.0F) {
    request.spacing = step.formation.spacing;
  }

  auto const result = Game::Formation::ArmyFormationService::commit(world, request);
  if (!result.valid) {
    add_issue(QStringLiteral("formation_rejected"),
              QStringLiteral("FormArmy step '%1' was rejected: %2")
                  .arg(step.name, QString::fromStdString(result.rejection_reason)));
    return;
  }
  if (result.positions.size() != members.size()) {
    add_issue(QStringLiteral("formation_plan_mismatch"),
              QStringLiteral("FormArmy step '%1' planned %2 slots for %3 units")
                  .arg(step.name)
                  .arg(result.positions.size())
                  .arg(members.size()));
    return;
  }

  Game::Systems::CommandService::march_into_formation(world, members, result);

  for (auto const& name : sources) {
    QVector3D sum;
    int count = 0;
    for (std::size_t i = 0; i < members.size(); ++i) {
      if (ids(name).end() !=
          std::find(ids(name).begin(), ids(name).end(), members[i])) {
        sum += result.positions[i] - world_origin;
        ++count;
      }
    }
    if (count > 0) {
      formed_destinations.insert(name, sum / static_cast<float>(count));
    }
  }

  for (auto const& name : sources) {
    arm_response(name, command_name(step.command));
  }
}

void ArenaScenarioRunner::Impl::execute_step(std::size_t index,
                                             const ArenaScenarioStep& step) {
  auto& runtime = steps[index];
  runtime.executed = true;
  runtime.executed_at = elapsed;
  if (!step.event.isEmpty()) {
    report.events.push_back({step.event, elapsed});
  }

  switch (step.command) {
  case ScenarioCommandKind::Marker:
    break;
  case ScenarioCommandKind::ShapeMove:
    shape_move_group(step);
    break;
  case ScenarioCommandKind::Wheel:
    wheel_group(step);
    break;
  case ScenarioCommandKind::SetWeather:
    if (step.weather_change >= 0) {
      weather_schedule.push_back({elapsed, step.weather_change});
    }
    break;
  case ScenarioCommandKind::Stand:
  case ScenarioCommandKind::Stop:
    stop_group(step.group, true);
    break;
  case ScenarioCommandKind::Move: {
    auto const& group_ids = ids(step.group);
    auto plan = Game::Systems::CommandService::plan_ground_move(
        world, group_ids, world_origin + step.destination);
    if (plan.fully_placeable_for(group_ids)) {
      Game::Systems::CommandService::move_units(
          world, group_ids, plan.target_positions());
    }
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
  case ScenarioCommandKind::Run: {
    auto const& group_ids = ids(step.group);
    auto plan = Game::Systems::CommandService::plan_ground_move(
        world, group_ids, world_origin + step.destination, true);
    Game::Systems::CommandService::issue_ground_move(world, group_ids, plan);
    for (auto entity_id : group_ids) {
      if (host.find_unit) {
        if (auto* unit = host.find_unit(entity_id)) {
          unit->set_run_mode(step.enabled);
        }
      }
    }
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::FormArmy:
    form_army(step);
    break;
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
                     step.command == ScenarioCommandKind::ReleaseReserve || step.chase);
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
    auto const& subjects = ids(step.group);

    const int guard_owner_id = subjects.empty() ? 0 : owner_of(world, subjects.front());

    Game::Command::Command guard_command{};
    guard_command.source = Game::Command::Source::Script;
    guard_command.owner_id = guard_owner_id;
    Game::Command::SetGuard payload{};
    payload.units = subjects;
    payload.active = step.enabled;
    if (step.enabled && !targets.empty()) {
      if (auto const target_centre = centroid(step.target_group)) {
        payload.anchor = *target_centre;
        payload.has_anchor = true;
      }
    }
    guard_command.payload = std::move(payload);
    Game::Command::dispatch(world, guard_command);

    if (step.enabled && !targets.empty()) {
      for (auto entity_id : subjects) {
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
            unit->set_guard_target(targets.front());
          }
        }
      }
    } else if (!step.enabled) {
      for (auto entity_id : subjects) {
        if (host.find_unit) {
          if (auto* unit = host.find_unit(entity_id)) {
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
  case ScenarioCommandKind::RepairStructure: {
    auto const& structures = ids(step.target_group);
    auto const workers = ids(step.group);
    if (structures.empty() || workers.empty()) {
      break;
    }
    Game::Command::dispatch(
        world,
        Game::Command::Command{.source = Game::Command::Source::Script,
                               .owner_id = owner_of(world, workers.front()),
                               .payload = Game::Command::RepairStructure{
                                   .units = workers, .structure = structures.front()}});
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::DismantleStructure: {
    auto const& structures = ids(step.target_group);
    auto const workers = ids(step.group);
    if (structures.empty() || workers.empty()) {
      break;
    }
    Game::Command::dispatch(
        world,
        Game::Command::Command{.source = Game::Command::Source::Script,
                               .owner_id = owner_of(world, workers.front()),
                               .payload = Game::Command::DismantleStructure{
                                   .units = workers, .structure = structures.front()}});
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::StartConstruction: {
    auto const workers = ids(step.group);
    if (workers.empty() || step.construction_type.isEmpty()) {
      break;
    }
    const int owner = owner_of(world, workers.front());
    const std::string type = step.construction_type.toStdString();

    auto& economy = Game::Session::session_for(world).economy();
    const auto costs = Game::Systems::construction_cost_info(type).resource_costs;
    for (const auto resource : Game::Systems::k_all_resource_types) {
      economy.add(owner, resource, costs.get(resource));
    }
    Game::Command::dispatch(
        world,
        Game::Command::Command{.source = Game::Command::Source::Script,
                               .owner_id = owner,
                               .payload = Game::Command::StartConstruction{
                                   .units = workers,
                                   .construction_type = type,
                                   .site = step.destination,
                                   .rotation_y = step.construction_rotation_degrees}});
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::DeliverToStructure: {
    auto const& structures = ids(step.target_group);
    auto const carriers = ids(step.group);
    if (structures.empty() || carriers.empty()) {
      break;
    }
    Game::Command::dispatch(
        world,
        Game::Command::Command{.source = Game::Command::Source::Script,
                               .owner_id = owner_of(world, carriers.front()),
                               .payload = Game::Command::DeliverCivilians{
                                   .units = carriers, .barracks = structures.front()}});
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::SetFarmGrowth:
    for (auto entity_id : step_entities(step)) {
      auto* entity = world.get_entity(entity_id);
      auto* farm = entity != nullptr
                       ? entity->get_component<Engine::Core::FarmComponent>()
                       : nullptr;
      if (farm != nullptr) {
        farm->growth = std::clamp(static_cast<float>(step.value) / 100.0F, 0.0F, 1.0F);
      }
    }
    break;
  case ScenarioCommandKind::HarvestResource: {
    if (host.terrain == nullptr) {
      break;
    }
    auto& terrain = *host.terrain;
    const QString kind = step.resource_kind;
    if (kind == QStringLiteral("grain") || kind == QStringLiteral("sheep")) {
      const std::string_view product =
          kind == QStringLiteral("grain")
              ? Game::Systems::k_builder_product_harvest_grain
              : Game::Systems::k_builder_product_slaughter_sheep;
      for (auto entity_id : ids(step.group)) {
        auto* entity = world.get_entity(entity_id);
        auto* transform =
            entity != nullptr
                ? entity->get_component<Engine::Core::TransformComponent>()
                : nullptr;
        if (transform == nullptr) {
          continue;
        }
        const int owner_id = owner_of(world, entity_id);
        const auto target = Game::Systems::find_food_target_near(world,
                                                                 product,
                                                                 owner_id,
                                                                 transform->position.x,
                                                                 transform->position.z,
                                                                 0.0F,
                                                                 entity_id);
        if (!target.has_value()) {
          continue;
        }
        Game::Command::dispatch(
            world,
            Game::Command::Command{.source = Game::Command::Source::Script,
                                   .owner_id = owner_id,
                                   .payload = Game::Command::StartHarvest{
                                       .units = {entity_id},
                                       .construction_type = std::string(product),
                                       .resource_target = target->id,
                                       .site = QVector3D(target->x, 0.0F, target->z)}});
      }
      arm_response(step.group, command_name(step.command));
      break;
    }
    const auto matches = [&kind](Game::Map::WorldProp::Type type) {
      if (kind == QStringLiteral("tree")) {
        return Game::Map::is_tree_world_prop_type(type);
      }
      if (kind == QStringLiteral("boulder")) {
        return Game::Map::is_boulder_world_prop_type(type);
      }
      if (kind == QStringLiteral("iron_ore")) {
        return Game::Map::is_iron_ore_world_prop_type(type);
      }
      return false;
    };
    const std::string product =
        kind == QStringLiteral("boulder")
            ? std::string(Game::Systems::k_builder_product_collect_stone)
        : kind == QStringLiteral("iron_ore")
            ? std::string(Game::Systems::k_builder_product_collect_iron_ore)
            : std::string(Game::Systems::k_builder_product_cut_tree);

    for (auto entity_id : ids(step.group)) {
      auto* entity = world.get_entity(entity_id);
      auto* builder =
          entity != nullptr
              ? entity->get_component<Engine::Core::BuilderProductionComponent>()
              : nullptr;
      auto* transform = entity != nullptr
                            ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      if (builder == nullptr || transform == nullptr) {
        continue;
      }

      const Game::Map::WorldProp* best = nullptr;
      float best_distance_sq = std::numeric_limits<float>::infinity();
      for (const auto& candidate : terrain.world_props()) {
        if (!matches(candidate.type) || terrain.is_world_prop_reserved(candidate.id)) {
          continue;
        }
        const QVector3D at = terrain.world_prop_world_position(candidate);
        const float dx = at.x() - transform->position.x;
        const float dz = at.z() - transform->position.z;
        const float distance_sq = dx * dx + dz * dz;
        if (distance_sq < best_distance_sq) {
          best_distance_sq = distance_sq;
          best = &candidate;
        }
      }
      if (best == nullptr) {
        continue;
      }

      const QVector3D at = terrain.world_prop_world_position(*best);
      Game::Command::dispatch(
          world,
          Game::Command::Command{
              .source = Game::Command::Source::Script,
              .owner_id = owner_of(world, entity_id),
              .payload = Game::Command::StartHarvest{.units = {entity_id},
                                                     .construction_type = product,
                                                     .resource_target = best->id,
                                                     .site = at}});
    }
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::AbandonWork: {

    for (auto entity_id : ids(step.group)) {
      auto* entity = world.get_entity(entity_id);
      auto* builder =
          entity != nullptr
              ? entity->get_component<Engine::Core::BuilderProductionComponent>()
              : nullptr;
      if (builder == nullptr) {
        continue;
      }
      builder->report_fault(Engine::Core::BuilderTaskFault::Interrupted,
                            std::max(1.0F, static_cast<float>(step.value)));
      builder->in_progress = false;
      builder->at_construction_site = false;
    }
    auto const& group_ids = ids(step.group);
    auto plan = Game::Systems::CommandService::plan_ground_move(
        world, group_ids, world_origin + step.destination);
    if (plan.fully_placeable_for(group_ids)) {
      Game::Systems::CommandService::move_units(
          world, group_ids, plan.target_positions());
    }
    arm_response(step.group, command_name(step.command));
    break;
  }
  case ScenarioCommandKind::SetCamera:
    if (host.set_camera) {
      host.set_camera(all_entities(),
                      {step.camera_distance, step.camera_angle, step.camera_yaw});
    }
    break;
  case ScenarioCommandKind::SetHealth:
    for (auto entity_id : step_entities(step)) {
      auto* entity = world.get_entity(entity_id);
      auto* unit = entity != nullptr
                       ? entity->get_component<Engine::Core::UnitComponent>()
                       : nullptr;
      if (unit != nullptr) {
        unit->health = std::clamp(step.value, 0, unit->max_health);
      }
    }
    break;
  case ScenarioCommandKind::ApplyDamage: {
    Engine::Core::EntityID const attacker_id =
        !ids(step.target_group).empty() ? ids(step.target_group).front() : 0U;
    for (auto entity_id : step_entities(step)) {
      auto* entity = world.get_entity(entity_id);
      (void)Game::Systems::Combat::apply_unit_damage(
          &world, entity, std::max(0, step.value), attacker_id);
    }
    break;
  }
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
      auto* commander = entity != nullptr
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
  case ScenarioCommandKind::TriggerFlagRally:
    for (auto entity_id : ids(step.group)) {
      auto* entity = world.get_entity(entity_id);
      auto* commander = entity != nullptr
                            ? entity->get_component<Engine::Core::CommanderComponent>()
                            : nullptr;
      if (commander == nullptr) {
        add_issue(
            QStringLiteral("flag_rally_commander_missing"),
            QStringLiteral("%1 contains no commander for a flag rally").arg(step.group),
            entity_id);
        continue;
      }
      auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (transform == nullptr) {
        add_issue(QStringLiteral("flag_rally_transform_missing"),
                  QStringLiteral("%1 has no transform to plant a standard on")
                      .arg(step.group),
                  entity_id);
        continue;
      }

      if (step.value < 0) {
        commander->cancel_flag_rally();
        continue;
      }

      commander->begin_flag_rally(transform->position.x, transform->position.z, true);
    }
    break;
  case ScenarioCommandKind::RpgPrimaryAttack:
    for (auto entity_id : ids(step.group)) {
      if (!host.rpg_primary_attack || !host.rpg_primary_attack(entity_id)) {
        add_issue(
            QStringLiteral("rpg_attack_failed"),
            QStringLiteral("%1 could not start an RPG primary attack").arg(step.group),
            entity_id);
      }
    }
    break;
  case ScenarioCommandKind::RpgHeavyAttack:
    for (auto entity_id : ids(step.group)) {
      if (!host.rpg_heavy_attack || !host.rpg_heavy_attack(entity_id)) {
        add_issue(
            QStringLiteral("rpg_heavy_attack_failed"),
            QStringLiteral("%1 could not start an RPG heavy attack").arg(step.group),
            entity_id);
      }
    }
    break;
  case ScenarioCommandKind::RpgAttackHold:
    for (auto entity_id : ids(step.group)) {
      if (!host.set_rpg_attack_held) {
        add_issue(
            QStringLiteral("rpg_attack_hold_unavailable"),
            QStringLiteral("%1 has no RPG attack-hold host callback").arg(step.group),
            entity_id);
        continue;
      }
      host.set_rpg_attack_held(entity_id, step.enabled);
    }
    break;
  case ScenarioCommandKind::RpgAim:
    if (!step.target_group.isEmpty()) {

      rpg_aim_shooter_group = step.group;
      rpg_aim_target_group = step.target_group;
      track_rpg_aim();
      break;
    }
    rpg_aim_shooter_group.clear();
    rpg_aim_target_group.clear();
    for (auto entity_id : ids(step.group)) {
      if (!host.set_rpg_view_yaw || !host.set_rpg_view_pitch) {
        add_issue(QStringLiteral("rpg_aim_unavailable"),
                  QStringLiteral("%1 has no RPG view host callbacks").arg(step.group),
                  entity_id);
        continue;
      }
      host.set_rpg_view_yaw(entity_id, step.rpg_view_yaw_degrees.value_or(0.0F));
      host.set_rpg_view_pitch(entity_id, step.rpg_view_pitch_degrees.value_or(0.0F));
    }
    break;
  case ScenarioCommandKind::RpgGuard:
    for (auto entity_id : ids(step.group)) {
      if (!host.set_rpg_guard) {
        add_issue(QStringLiteral("rpg_guard_unavailable"),
                  QStringLiteral("%1 has no RPG guard host callback").arg(step.group),
                  entity_id);
        continue;
      }
      host.set_rpg_guard(entity_id, step.enabled);
    }
    break;
  case ScenarioCommandKind::RpgDodge:
    for (auto entity_id : ids(step.group)) {
      if (!host.request_rpg_dodge) {
        add_issue(QStringLiteral("rpg_dodge_unavailable"),
                  QStringLiteral("%1 has no RPG dodge host callback").arg(step.group),
                  entity_id);
        continue;
      }
      host.request_rpg_dodge(entity_id, step.destination);
    }
    break;
  case ScenarioCommandKind::RpgJump:
    for (auto entity_id : ids(step.group)) {
      if (!host.request_rpg_jump) {
        add_issue(QStringLiteral("rpg_jump_unavailable"),
                  QStringLiteral("%1 has no RPG jump host callback").arg(step.group),
                  entity_id);
        continue;
      }
      host.request_rpg_jump(entity_id);
    }
    break;
  case ScenarioCommandKind::RpgSpecial:
    for (auto entity_id : ids(step.group)) {
      if (!host.request_rpg_special) {
        add_issue(QStringLiteral("rpg_special_unavailable"),
                  QStringLiteral("%1 has no RPG special host callback").arg(step.group),
                  entity_id);
        continue;
      }
      host.request_rpg_special(entity_id);
    }
    break;
  case ScenarioCommandKind::RpgWeaponSwitch:
    for (auto entity_id : ids(step.group)) {
      if (!host.request_rpg_weapon_switch) {
        add_issue(
            QStringLiteral("rpg_weapon_switch_unavailable"),
            QStringLiteral("%1 has no RPG weapon-switch host callback").arg(step.group),
            entity_id);
        continue;
      }
      host.request_rpg_weapon_switch(entity_id);
    }
    break;
  case ScenarioCommandKind::RpgCycleLockOn:
    for (auto entity_id : ids(step.group)) {
      if (!host.cycle_rpg_lock_on) {
        add_issue(QStringLiteral("rpg_lock_on_unavailable"),
                  QStringLiteral("%1 has no RPG lock-on host callback").arg(step.group),
                  entity_id);
        continue;
      }
      host.cycle_rpg_lock_on(entity_id);
    }
    break;
  case ScenarioCommandKind::RpgMove:
    for (auto entity_id : ids(step.group)) {
      if (!host.set_rpg_move_input) {
        add_issue(QStringLiteral("rpg_move_unavailable"),
                  QStringLiteral("%1 has no RPG move host callback").arg(step.group),
                  entity_id);
        continue;
      }
      if (step.rpg_view_yaw_degrees.has_value() && host.set_rpg_view_yaw) {
        host.set_rpg_view_yaw(entity_id, *step.rpg_view_yaw_degrees);
      }

      for (float const axis : {step.destination.x(), step.destination.z()}) {
        if (std::abs(axis) > 0.01F && std::abs(axis) <= 0.5F) {
          add_issue(QStringLiteral("rpg_move_axis_ignored"),
                    QStringLiteral("%1 RpgMove axis %2 is below the 0.5 key "
                                   "threshold and does nothing")
                        .arg(step.group)
                        .arg(axis),
                    entity_id);
        }
      }
      host.set_rpg_move_input(entity_id, step.destination, step.value != 0);
    }
    break;
  case ScenarioCommandKind::ReloadUndeadZoneState: {
    auto* undead = world.get_system<Game::Systems::UndeadAwakeningSystem>();
    if (undead == nullptr) {
      add_issue(QStringLiteral("undead_zone_reload_unavailable"),
                QStringLiteral("the scene has no undead awakening system to "
                               "save and restore"));
      break;
    }
    undead->restore_state(undead->serialize_state());
    break;
  }
  case ScenarioCommandKind::RollStones: {
    auto* rockfall = world.get_system<Game::Systems::RockfallSystem>();
    bool rolled = false;
    for (auto const id : ids(step.group)) {
      rolled = (rockfall != nullptr && rockfall->order_release(world, id)) || rolled;
    }
    if (!rolled) {
      add_issue(
          QStringLiteral("roll_stones_failed"),
          QStringLiteral("%1 stands beside no stone cache it holds").arg(step.group));
    }
    break;
  }
  case ScenarioCommandKind::CrossByRaft: {
    auto* rafts = world.get_system<Game::Systems::RaftSystem>();
    bool queued = false;
    for (auto const id : ids(step.group)) {
      queued = (rafts != nullptr && rafts->order_crossing(world, id)) || queued;
    }
    if (!queued) {
      add_issue(QStringLiteral("raft_order_failed"),
                QStringLiteral("%1 stands near no raft landing").arg(step.group));
    }
    break;
  }
  case ScenarioCommandKind::TriggerRockfall: {
    auto* rockfall = world.get_system<Game::Systems::RockfallSystem>();
    if (rockfall == nullptr || !rockfall->trigger(step.zone_id)) {
      add_issue(QStringLiteral("rockfall_trigger_failed"),
                QStringLiteral("rockfall trap '%1' is missing or not armed")
                    .arg(step.zone_id));
    }
    break;
  }
  }
}

} // namespace Arena
