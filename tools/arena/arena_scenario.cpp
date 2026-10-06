#include <algorithm>

#include "arena_scenario_internal.h"

namespace Arena {

using namespace scenario_internal;

auto expectation_reads_soldier_samples(ArenaExpectationKind kind) noexcept -> bool {
  switch (kind) {
  case ArenaExpectationKind::GroupIsRendered:
  case ArenaExpectationKind::NoPoseOscillation:
  case ArenaExpectationKind::NoRootTeleport:
  case ArenaExpectationKind::NoUnexpectedFallPose:
  case ArenaExpectationKind::NoPlantedFootSliding:
  case ArenaExpectationKind::NoWeaponTeleport:
  case ArenaExpectationKind::NoLimbOverextension:
  case ArenaExpectationKind::NoPelvisSnap:
  case ArenaExpectationKind::NoLocomotionRestart:
  case ArenaExpectationKind::NoAttackRestart:
  case ArenaExpectationKind::NoBodyPoseSnap:
  case ArenaExpectationKind::HoldPoseMaintained:
  case ArenaExpectationKind::AllLivingSoldiersFight:
  case ArenaExpectationKind::AttackHasTorsoRotation:
  case ArenaExpectationKind::RpgFormationSurvivesLensGap:
    return true;
  default:
    return false;
  }
}

auto scenario_needs_animation_diagnostics(const ArenaScenarioDefinition& definition)
    -> bool {
  return definition.collect_animation_diagnostics ||
         std::any_of(definition.expectations.begin(),
                     definition.expectations.end(),
                     [](const ArenaExpectation& expectation) {
                       return expectation_reads_soldier_samples(expectation.kind);
                     });
}

auto destroy_remaining_gameplay_entities(Engine::Core::World& world) -> std::size_t {
  std::vector<Engine::Core::EntityID> doomed;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity != nullptr) {
      doomed.push_back(entity->get_id());
    }
  }
  for (auto* entity :
       world.collect_entities_with<Engine::Core::WallConstructionSiteComponent>()) {
    if (entity != nullptr) {
      doomed.push_back(entity->get_id());
    }
  }
  std::sort(doomed.begin(), doomed.end());
  doomed.erase(std::unique(doomed.begin(), doomed.end()), doomed.end());
  for (auto const entity_id : doomed) {
    world.destroy_entity(entity_id);
  }
  return doomed.size();
}

ArenaScenarioRunner::ArenaScenarioRunner(Engine::Core::World& world,
                                         ArenaScenarioHost host,
                                         const ArenaScenarioDefinition& definition,
                                         QVector3D world_origin)
    : m_impl(std::make_unique<Impl>(world, std::move(host), definition, world_origin)) {
}

ArenaScenarioRunner::~ArenaScenarioRunner() {
  Game::Systems::Combat::EngagementTrace::instance().set_enabled(false);
}

auto ArenaScenarioRunner::start() -> bool {
  if (m_impl->started) {
    return false;
  }
  auto const validation = validate_scenario(m_impl->scenario);
  if (!validation.empty()) {
    for (auto const& error : validation) {
      qWarning().noquote() << QStringLiteral("Arena scenario '%1' invalid: %2 -- %3")
                                  .arg(m_impl->scenario.id, error.field, error.message);
    }
    return false;
  }
  if (!m_impl->host.spawn_unit) {
    return false;
  }
  m_impl->started = true;

  bool const asserts_frame_budget =
      std::any_of(m_impl->scenario.expectations.begin(),
                  m_impl->scenario.expectations.end(),
                  [](auto const& expectation) {
                    return expectation.kind == ArenaExpectationKind::FrameBudget;
                  });
  if (asserts_frame_budget) {
    Render::Profiling::global_profile().enabled = true;
  }

  Game::Systems::Combat::EngagementTrace::instance().set_enabled(true);
  Game::Systems::Combat::EngagementTrace::instance().clear();
  for (auto const& group : m_impl->scenario.groups) {
    if (group.spawn_at_start) {
      m_impl->spawn_group(group);
    }
  }
  if (m_impl->scenario.rpg_mode) {
    auto const& commanders = m_impl->ids(m_impl->scenario.rpg_commander_group);
    if (commanders.size() != 1U || !m_impl->host.configure_rpg_commander) {
      m_impl->add_issue(
          QStringLiteral("rpg_commander_setup_failed"),
          QStringLiteral("RPG scenario requires one spawned commander and an "
                         "RPG-capable host"));
      return false;
    }
    Engine::Core::EntityID const commander_id = commanders.front();
    m_impl->host.configure_rpg_commander(commander_id);
    auto* commander = m_impl->world.get_entity(commander_id);
    auto const* commander_group =
        m_impl->group_definition(m_impl->scenario.rpg_commander_group);
    if (commander != nullptr && commander_group != nullptr &&
        (commander_group->stamina_override > 0.0F ||
         commander_group->max_stamina_override > 0.0F)) {
      auto* stamina =
          Engine::Core::get_or_add_component<Engine::Core::StaminaComponent>(commander);
      if (stamina != nullptr) {
        if (commander_group->max_stamina_override > 0.0F) {
          stamina->max_stamina = commander_group->max_stamina_override;
        }
        stamina->stamina =
            commander_group->stamina_override > 0.0F
                ? std::min(commander_group->stamina_override, stamina->max_stamina)
                : stamina->max_stamina;
        stamina->regen_delay_remaining = 0.0F;
      }
    }
    auto const* rpg = commander != nullptr
                          ? commander->get_component<Engine::Core::RpgHealthComponent>()
                          : nullptr;
    if (rpg == nullptr || !rpg->active) {
      m_impl->add_issue(QStringLiteral("rpg_commander_setup_failed"),
                        QStringLiteral("RPG host did not activate commander health"),
                        commander_id);
      return false;
    }
    auto const* commander_unit =
        commander->get_component<Engine::Core::UnitComponent>();
    int const commander_health = commander_unit != nullptr ? commander_unit->health : 0;
    m_impl->initial_rpg_health_by_group[m_impl->scenario.rpg_commander_group] =
        commander_health;
    m_impl->minimum_rpg_health_by_group[m_impl->scenario.rpg_commander_group] =
        commander_health;
  }
  for (auto* entity :
       m_impl->world.collect_entities_with<Engine::Core::BuildingComponent>()) {
    if (entity != nullptr) {
      m_impl->initial_building_ids.insert(entity->get_id());
    }
  }
  m_impl->observe_undead_zones();
  m_impl->initialize_battle_sides();
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
  m_impl->observe_undead_zones();
  m_impl->observe_wildlife();
  m_impl->observe_battle();
  for (std::size_t i = 0; i < m_impl->scenario.steps.size(); ++i) {
    auto const& step = m_impl->scenario.steps[i];
    if (!m_impl->steps[i].executed && m_impl->trigger_ready(i, step)) {
      m_impl->execute_step(i, step);
    }
  }
  m_impl->track_rpg_aim();
  m_impl->observe_commander_aura_state();
  m_impl->observe_range_rings();
  m_impl->observe_projectiles();
  for (auto const& expectation : m_impl->scenario.expectations) {
    if (expectation.kind == ArenaExpectationKind::FormationOrderPreserved &&
        m_impl->expectation_active(expectation)) {
      m_impl->check_formation_order(expectation);
    }
  }
  if (m_impl->elapsed + 1.0e-5F >= m_impl->duration_limit ||
      m_impl->battle_decision_ends_scenario()) {
    m_impl->check_end_expectations();
    m_impl->complete = true;
  }
}

void ArenaScenarioRunner::observe_rendered_frame(double frame_time_ms) {
  ArenaRenderedFrameTimings timings;
  timings.total_ms = frame_time_ms;
  observe_rendered_frame(timings);
}

void ArenaScenarioRunner::set_animation_time(float seconds) {
  m_impl->animation_time = seconds;
}

void ArenaScenarioRunner::observe_commander_presentation(
    const App::Core::CommanderPresentationTrace& trace) {
  m_impl->commander_trace = trace;
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
  frame.commander = m_impl->commander_trace;
  frame.animation_time = m_impl->animation_time;
  m_impl->record_animals(frame);
  for (auto* entity :
       m_impl->world.collect_entities_with<Engine::Core::BuildingComponent>()) {
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
      m_impl->observe_narrow_layout(group.name, entity_id);
      m_impl->observe_building_clearance(entity_id, group.name);
      m_impl->observe_soldiers(entity_id, group.name, frame);
      m_impl->observe_motion_quality(entity_id, group.name);
    }
    m_impl->observe_bridge_centerline_alignment(group.name);
  }
  for (auto [animal_id, wildlife] :
       m_impl->world.view<const Engine::Core::WildlifeComponent>()) {
    (void)wildlife;
    m_impl->observe_motion_quality(animal_id, QString::fromLatin1(k_wildlife_group));
  }
  int raft_riders = 0;
  for (auto [rider_id, rider] :
       m_impl->world.view<const Engine::Core::RaftRiderComponent>()) {
    (void)rider;
    ++raft_riders;
    for (auto const& group : m_impl->scenario.groups) {
      auto const& members = m_impl->ids(group.name);
      if (std::find(members.begin(), members.end(), rider_id) != members.end()) {
        m_impl->raft_riders_by_group[group.name].insert(rider_id);
      }
    }
  }
  m_impl->most_raft_riders = std::max(m_impl->most_raft_riders, raft_riders);
  if (!m_impl->wall_walker_seen) {
    auto walkers = m_impl->world.view<const Engine::Core::WallWalkerComponent>();
    m_impl->wall_walker_seen = walkers.begin() != walkers.end();
  }
  m_impl->observe_rpg_locomotion_presentation(frame);
  m_impl->observe_rpg_swing_cadence(frame);
  m_impl->observe_rpg_travel(frame);
  m_impl->observe_group_pair_proximity(frame);
  m_impl->trace.push_back(std::move(frame));
}

void ArenaScenarioRunner::report_external_issue(QString code, QString message) {
  if (!m_impl->started || m_impl->complete) {
    return;
  }
  m_impl->add_issue(std::move(code), std::move(message));
}

void ArenaScenarioRunner::set_duration_limit(float duration_seconds) {
  if (duration_seconds > 0.0F) {
    m_impl->duration_limit = duration_seconds;
  }
}

void ArenaScenarioRunner::set_environment_snapshot(
    const ArenaEnvironmentSnapshot& snapshot) {
  m_impl->environment_snapshot = snapshot;
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

auto ArenaScenarioRunner::live_battle_sides() const
    -> std::vector<ArenaBattleSideResult> {
  return m_impl->live_sides();
}

auto ArenaScenarioRunner::battle_decided() const noexcept -> bool {
  return m_impl->battle_decided;
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

} // namespace Arena
