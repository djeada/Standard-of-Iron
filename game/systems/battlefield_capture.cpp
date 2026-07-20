#include "battlefield_capture.h"

#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

#include "../core/component.h"
#include "../core/world.h"
#include "../units/factory.h"
#include "../units/unit.h"
#include "ai_system.h"
#include "combat_actions/combat_action_definition.h"
#include "command_service.h"
#include "nation_registry.h"
#include "owner_registry.h"
#include "runtime_system_registry.h"
#include "troop_count_registry.h"

namespace Game::BattlefieldCapture {
namespace {

constexpr int k_player_owner = 1;
constexpr int k_enemy_owner = 2;
constexpr float k_world_extent = 128.0F;

struct ScenarioWorld {
  Engine::Core::World world;
  Game::Units::UnitFactoryRegistry factories;
  std::vector<std::unique_ptr<Game::Units::Unit>> unit_handles;
  std::vector<Engine::Core::EntityID> player_units;
  std::vector<Engine::Core::EntityID> enemy_units;
  std::unordered_map<Engine::Core::EntityID, std::pair<std::uint64_t, int>>
      slot_assignments;
};

auto json_escape(const std::string& value) -> std::string {
  std::string result;
  for (char const ch : value) {
    if (ch == '"' || ch == '\\') {
      result.push_back('\\');
    }
    result.push_back(ch);
  }
  return result;
}

void mix(std::uint64_t& hash, std::uint64_t value) {
  hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6U) + (hash >> 2U);
}

auto animation_name(Engine::Core::CombatAnimationState state) -> const char* {
  using State = Engine::Core::CombatAnimationState;
  switch (state) {
  case State::Idle:
    return "Idle";
  case State::Advance:
    return "Advance";
  case State::WindUp:
    return "WindUp";
  case State::Strike:
    return "Strike";
  case State::Impact:
    return "Impact";
  case State::Recover:
    return "Recover";
  case State::Reposition:
    return "Reposition";
  }
  return "Unknown";
}

auto action_event_name(std::uint8_t type) -> const char* {
  using Event = Game::Systems::CombatActions::CombatActionEventType;
  switch (static_cast<Event>(type)) {
  case Event::WindupStart:
    return "Windup";
  case Event::ActiveStart:
    return "Contact";
  case Event::WeaponTraceStart:
    return "TraceStart";
  case Event::WeaponTraceEnd:
    return "TraceEnd";
  case Event::ProjectileRelease:
    return "Release";
  case Event::RecoveryStart:
    return "Recovery";
  case Event::CancelWindowStart:
    return "CancelStart";
  case Event::CancelWindowEnd:
    return "CancelEnd";
  case Event::ExitSafe:
    return "ExitSafe";
  }
  return {};
}

auto wrap_degrees(float value) -> double {
  double const wrapped = std::fmod(std::abs(static_cast<double>(value)), 360.0);
  return wrapped > 180.0 ? 360.0 - wrapped : wrapped;
}

void configure_registries(bool use_ai) {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.clear();
  owners.register_owner_with_id(
      k_player_owner, Game::Systems::OwnerType::Player, "Verifier Player");
  owners.register_owner_with_id(k_enemy_owner,
                                use_ai ? Game::Systems::OwnerType::AI
                                       : Game::Systems::OwnerType::Player,
                                "Verifier Opponent");
  owners.set_owner_team(k_player_owner, 1);
  owners.set_owner_team(k_enemy_owner, 2);
  owners.set_local_player_id(k_player_owner);

  auto& nations = Game::Systems::NationRegistry::instance();
  nations.initialize_defaults();
  nations.clear_player_assignments();
  nations.set_player_nation(k_player_owner, Game::Systems::NationID::RomanRepublic);
  nations.set_player_nation(k_enemy_owner, Game::Systems::NationID::Carthage);
  Game::Systems::TroopCountRegistry::instance().initialize();
  Game::Systems::CommandService::initialize(static_cast<int>(k_world_extent),
                                            static_cast<int>(k_world_extent));
}

auto spawn_unit(ScenarioWorld& scenario,
                Game::Units::SpawnType type,
                int owner,
                float x,
                float z,
                bool ai_controlled) -> Engine::Core::EntityID {
  Game::Units::SpawnParams params;
  params.position = QVector3D(x, 0.0F, z);
  params.rotation_y = owner == k_player_owner ? 90.0F : -90.0F;
  params.player_id = owner;
  params.spawn_type = type;
  params.ai_controlled = ai_controlled;
  params.nation_id = owner == k_player_owner ? Game::Systems::NationID::RomanRepublic
                                             : Game::Systems::NationID::Carthage;
  auto handle = scenario.factories.create(type, scenario.world, params);
  if (handle == nullptr) {
    throw std::runtime_error("battlefield verifier could not spawn unit");
  }
  auto const id = handle->id();
  scenario.unit_handles.push_back(std::move(handle));
  (owner == k_player_owner ? scenario.player_units : scenario.enemy_units)
      .push_back(id);
  return id;
}

void spawn_line(ScenarioWorld& scenario,
                int owner,
                Game::Units::SpawnType type,
                int count,
                float anchor_x,
                float anchor_z,
                std::uint64_t formation_id,
                bool ai_controlled) {
  int const files = std::min(count, 10);
  for (int index = 0; index < count; ++index) {
    int const rank = index / files;
    int const file = index % files;
    float const rank_direction = owner == k_player_owner ? -1.0F : 1.0F;
    float const x = anchor_x + rank_direction * static_cast<float>(rank) * 1.5F;
    float const z =
        anchor_z +
        (static_cast<float>(file) - static_cast<float>(files - 1) * 0.5F) * 1.35F;
    auto const id = spawn_unit(scenario, type, owner, x, z, ai_controlled);
    auto* entity = scenario.world.get_entity(id);
    auto* mode = entity->add_component<Engine::Core::FormationModeComponent>();
    mode->active = true;
    mode->formation_id = formation_id;
    mode->stable_slot_id = index;
    mode->stable_rank = rank;
    mode->stable_file = file;
    mode->stable_slot_x = x;
    mode->stable_slot_z = z;
    scenario.slot_assignments[id] = {formation_id, index};
  }
}

void issue_opposed_orders(ScenarioWorld& scenario, bool command_enemy) {
  auto move_toward_front = [&](const std::vector<Engine::Core::EntityID>& units,
                               float front_x) {
    std::vector<Game::Systems::CommandService::MoveIntent> intents;
    intents.reserve(units.size());
    for (auto const id : units) {
      auto* entity = scenario.world.get_entity(id);
      auto* transform = entity != nullptr
                            ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      auto* mode = entity != nullptr
                       ? entity->get_component<Engine::Core::FormationModeComponent>()
                       : nullptr;
      if (transform == nullptr) {
        continue;
      }
      float const rank_offset =
          mode != nullptr ? static_cast<float>(mode->stable_rank) * 1.5F : 0.0F;
      float const destination_x =
          front_x + (front_x < 0.0F ? -rank_offset : rank_offset);
      QVector3D const destination(destination_x, 0.0F, transform->position.z);
      intents.push_back({id, destination});
      if (mode != nullptr) {
        mode->stable_slot_x = destination.x();
        mode->stable_slot_z = destination.z();
      }
    }
    Game::Systems::CommandService::move_units(
        scenario.world,
        intents,
        {.kind = Game::Systems::MoveOrderKind::FormationMove,
         .preserve_formation_mode = true});
  };
  move_toward_front(scenario.player_units, -1.0F);
  if (command_enemy) {
    move_toward_front(scenario.enemy_units, 1.0F);
  }
}

auto build_world(ScenarioId id) -> std::unique_ptr<ScenarioWorld> {
  bool const use_ai = id == ScenarioId::BotSkirmish;
  configure_registries(use_ai);
  auto scenario = std::make_unique<ScenarioWorld>();
  Game::Units::register_built_in_units(scenario->factories);

  switch (id) {
  case ScenarioId::InfantryApproach20v20:
    spawn_line(*scenario, 1, Game::Units::SpawnType::Knight, 20, -14, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 20, 14, 0, 2, false);
    break;
  case ScenarioId::ArchersVsInfantry:
    spawn_line(*scenario, 1, Game::Units::SpawnType::Archer, 20, -16, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 20, 16, 0, 2, false);
    break;
  case ScenarioId::MixedFormation:
    spawn_line(*scenario, 1, Game::Units::SpawnType::Knight, 12, -15, -5, 1, false);
    spawn_line(*scenario, 1, Game::Units::SpawnType::Archer, 8, -17, 6, 3, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 20, 15, 0, 2, false);
    break;
  case ScenarioId::CasualtyReflow:

    spawn_line(*scenario, 1, Game::Units::SpawnType::Knight, 20, -8, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 20, 8, 0, 2, false);
    break;
  case ScenarioId::CavalryCharge:
    spawn_line(
        *scenario, 1, Game::Units::SpawnType::MountedKnight, 15, -24, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 20, 8, 0, 2, false);
    break;
  case ScenarioId::NarrowPassage:
    spawn_line(*scenario, 1, Game::Units::SpawnType::Knight, 8, -18, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 8, 18, 0, 2, false);
    break;
  case ScenarioId::CommanderInLine:
    spawn_line(*scenario, 1, Game::Units::SpawnType::Knight, 20, -14, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 20, 14, 0, 2, false);
    break;
  case ScenarioId::BotSkirmish:
    spawn_line(*scenario, 1, Game::Units::SpawnType::Knight, 12, -12, 0, 1, false);
    spawn_line(*scenario, 2, Game::Units::SpawnType::Knight, 12, 12, 0, 2, true);
    break;
  }

  Game::Systems::register_runtime_systems(scenario->world);
  if (auto* ai = scenario->world.get_system<Game::Systems::AISystem>()) {
    ai->set_update_interval(0.1F);
  }
  issue_opposed_orders(*scenario, !use_ai);
  return scenario;
}

void add_overlays(CaptureResult& out, std::uint64_t tick, Engine::Core::World& world) {
  if (tick % 15U != 0U) {
    return;
  }
  for (int const owner : {k_player_owner, k_enemy_owner}) {
    double x = 0.0;
    double z = 0.0;
    int count = 0;
    for (auto* entity : world.get_units_owned_by(owner)) {
      auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->health <= 0) {
        continue;
      }
      x += transform->position.x;
      z += transform->position.z;
      ++count;
    }
    if (count > 0) {
      out.overlays.push_back({tick,
                              "group_anchor",
                              static_cast<std::uint64_t>(owner),
                              {x / count, z / count},
                              "live centroid"});
    }
  }
  for (auto* entity : world.get_entities_with<Engine::Core::FormationModeComponent>()) {
    auto const* mode = entity->get_component<Engine::Core::FormationModeComponent>();
    if (mode == nullptr || !mode->active) {
      continue;
    }
    out.overlays.push_back({tick,
                            "formation_slot",
                            entity->get_id(),
                            {mode->stable_slot_x, mode->stable_slot_z},
                            std::to_string(mode->stable_slot_id)});
  }
}

void add_issue(VerificationReport& report, std::string code, std::string message) {
  report.issues.push_back({std::move(code), std::move(message)});
}

} // namespace

auto scenario_name(ScenarioId id) -> const char* {
  switch (id) {
  case ScenarioId::InfantryApproach20v20:
    return "infantry_20v20";
  case ScenarioId::ArchersVsInfantry:
    return "archers_vs_infantry";
  case ScenarioId::MixedFormation:
    return "mixed_formation";
  case ScenarioId::CasualtyReflow:
    return "casualty_reflow";
  case ScenarioId::CavalryCharge:
    return "cavalry_charge";
  case ScenarioId::NarrowPassage:
    return "narrow_passage";
  case ScenarioId::CommanderInLine:
    return "commander_in_line";
  case ScenarioId::BotSkirmish:
    return "bot_skirmish";
  }
  return "unknown";
}

auto all_scenarios() -> std::vector<ScenarioId> {
  return {ScenarioId::InfantryApproach20v20,
          ScenarioId::ArchersVsInfantry,
          ScenarioId::MixedFormation,
          ScenarioId::CasualtyReflow,
          ScenarioId::CavalryCharge,
          ScenarioId::NarrowPassage,
          ScenarioId::CommanderInLine,
          ScenarioId::BotSkirmish};
}

auto parse_scenario(const std::string& name, ScenarioId& result) -> bool {
  for (auto const id : all_scenarios()) {
    if (name == scenario_name(id)) {
      result = id;
      return true;
    }
  }
  return false;
}

auto run(const RunnerConfig& config, const TickObserver& observer) -> CaptureResult {
  if (!(config.fixed_step_seconds > 0.0) || config.duration_seconds < 0.0) {
    throw std::invalid_argument("invalid capture timing");
  }
  auto const started = std::chrono::steady_clock::now();
  CaptureResult out;
  out.config = config;
  out.deterministic_digest = 1469598103934665603ULL;
  auto scenario = build_world(config.scenario);
  auto const tick_count = static_cast<std::uint64_t>(
      std::ceil(config.duration_seconds / config.fixed_step_seconds));
  std::unordered_map<Engine::Core::EntityID, int> previous_health;
  std::unordered_map<Engine::Core::EntityID, float> previous_yaw;
  std::unordered_map<Engine::Core::EntityID, float> prior_state_change_time;
  std::unordered_map<Engine::Core::EntityID, Engine::Core::MotionPresentationState>
      prior_motion_state;
  std::unordered_map<Engine::Core::EntityID, int> previous_action_damage;

  for (std::uint64_t tick = 0; tick < tick_count; ++tick) {
    auto const tick_started = std::chrono::steady_clock::now();
    scenario->world.update(static_cast<float>(config.fixed_step_seconds));
    if (config.scenario == ScenarioId::BotSkirmish) {
      std::this_thread::yield();
    }
    double const tick_ms = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - tick_started)
                               .count();
    out.performance.slowest_tick_ms =
        std::max(out.performance.slowest_tick_ms, tick_ms);
    add_overlays(out, tick, scenario->world);

    std::vector<Engine::Core::Entity*> entities;
    for (auto* entity :
         scenario->world.get_entities_with<Engine::Core::UnitComponent>()) {
      entities.push_back(entity);
    }
    std::sort(entities.begin(), entities.end(), [](auto* lhs, auto* rhs) {
      return lhs->get_id() < rhs->get_id();
    });
    std::uint64_t live_count = 0;
    for (auto* entity : entities) {
      auto const id = entity->get_id();
      auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr) {
        continue;
      }
      bool const dead = unit->health <= 0;
      if (!dead) {
        ++live_count;
      }
      auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
      auto const* motion =
          entity->get_component<Engine::Core::MotionPresentationComponent>();
      auto const* target = entity->get_component<Engine::Core::AttackTargetComponent>();
      auto const* action =
          entity->get_component<Engine::Core::RpgCommanderActionComponent>();
      auto const* combat = entity->get_component<Engine::Core::CombatStateComponent>();
      auto const slot_it = scenario->slot_assignments.find(id);

      double const actual_vx = motion != nullptr ? motion->velocity_x : 0.0;
      double const actual_vz = motion != nullptr ? motion->velocity_z : 0.0;
      double const step =
          motion != nullptr ? std::hypot(motion->displacement_x, motion->displacement_z)
                            : 0.0;
      out.quality.max_position_step = std::max(out.quality.max_position_step, step);
      double const allowed_step = std::max(
          0.05, static_cast<double>(unit->speed) * config.fixed_step_seconds * 1.75);
      if (step > allowed_step) {
        ++out.quality.excessive_position_steps;
      }
      if (!std::isfinite(transform->position.x) ||
          !std::isfinite(transform->position.z) || !std::isfinite(actual_vx) ||
          !std::isfinite(actual_vz)) {
        ++out.quality.non_finite_samples;
      }
      if (auto it = previous_yaw.find(id); it != previous_yaw.end()) {
        out.quality.max_yaw_step_degrees =
            std::max(out.quality.max_yaw_step_degrees,
                     wrap_degrees(transform->rotation.y - it->second));
      }
      previous_yaw[id] = transform->rotation.y;

      if (motion != nullptr) {
        auto const state = motion->state;
        if (auto it = prior_motion_state.find(id);
            it != prior_motion_state.end() && it->second != state) {
          auto const now = static_cast<float>(tick * config.fixed_step_seconds);
          float const last_change = prior_state_change_time[id];
          if (now - last_change < 0.10F) {
            ++out.quality.locomotion_flickers;
          }
          prior_state_change_time[id] = now;
        }
        prior_motion_state[id] = state;
        if (out.quality.first_response_seconds < 0.0 &&
            unit->owner_id == k_player_owner && motion->speed > 0.05F) {
          out.quality.first_response_seconds = (tick + 1U) * config.fixed_step_seconds;
        }
      }

      std::string event;
      if (action != nullptr && action->last_event_valid) {
        event = action_event_name(action->last_event_type);
        ++out.performance.action_events;
      }
      if (action != nullptr && action->last_damage > 0 &&
          previous_action_damage[id] != action->last_damage) {
        ++out.performance.damage_events;
        if (!action->action_running && !action->action_completed) {
          ++out.quality.damage_without_visible_action;
        }
      }
      previous_action_damage[id] = action != nullptr ? action->last_damage : 0;
      if (auto it = previous_health.find(id);
          it != previous_health.end() && it->second > 0 && dead) {
        ++out.performance.deaths;
      }
      previous_health[id] = unit->health;
      if (out.quality.first_combat_seconds < 0.0 &&
          (action != nullptr && action->combat_action_id != 0U)) {
        out.quality.first_combat_seconds = tick * config.fixed_step_seconds;
      }

      TickRecord const record{
          tick,
          id,
          slot_it != scenario->slot_assignments.end()
              ? slot_it->second.first
              : static_cast<std::uint64_t>(unit->owner_id),
          slot_it != scenario->slot_assignments.end() ? slot_it->second.second : -1,
          target != nullptr ? target->target_id : 0U,
          action != nullptr ? action->combat_action_id : 0U,
          transform->position.x,
          transform->position.z,
          transform->rotation.y,
          movement != nullptr ? movement->get_vx() : 0.0,
          movement != nullptr ? movement->get_vz() : 0.0,
          actual_vx,
          actual_vz,
          static_cast<double>(unit->health),
          std::move(event),
          combat != nullptr
              ? animation_name(combat->animation_state)
              : (motion != nullptr && motion->has_locomotion() ? "Walk" : "Idle"),
          dead};
      out.ticks.push_back(record);
      if (observer) {
        observer(record);
      }
      mix(out.deterministic_digest, tick);
      mix(out.deterministic_digest, id);
      mix(out.deterministic_digest,
          static_cast<std::uint64_t>(std::llround(record.x * 10000.0)));
      mix(out.deterministic_digest,
          static_cast<std::uint64_t>(std::max(0.0, record.health)));
      ++out.performance.entity_updates;
    }

    std::unordered_map<std::uint64_t, std::vector<Engine::Core::Entity*>> formations;
    for (auto* entity : entities) {
      auto const* mode = entity->get_component<Engine::Core::FormationModeComponent>();
      auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (mode != nullptr && mode->active && mode->formation_id != 0 &&
          unit != nullptr && unit->health > 0) {
        formations[mode->formation_id].push_back(entity);
      }
    }
    for (auto& [formation_id, members] : formations) {
      (void)formation_id;
      int front_rank = std::numeric_limits<int>::max();
      int owner = 0;
      for (auto* member : members) {
        auto const* mode =
            member->get_component<Engine::Core::FormationModeComponent>();
        auto const* unit = member->get_component<Engine::Core::UnitComponent>();
        front_rank = std::min(front_rank, mode->stable_rank);
        owner = unit->owner_id;
      }
      double front_progress = owner == k_player_owner
                                  ? -std::numeric_limits<double>::infinity()
                                  : std::numeric_limits<double>::infinity();
      for (auto* member : members) {
        auto const* mode =
            member->get_component<Engine::Core::FormationModeComponent>();
        auto const* transform =
            member->get_component<Engine::Core::TransformComponent>();
        if (mode->stable_rank != front_rank) {
          continue;
        }
        if (owner == k_player_owner) {
          front_progress =
              std::max(front_progress, static_cast<double>(transform->position.x));
        } else {
          front_progress =
              std::min(front_progress, static_cast<double>(transform->position.x));
        }
      }
      for (auto* member : members) {
        auto const* mode =
            member->get_component<Engine::Core::FormationModeComponent>();
        auto const* transform =
            member->get_component<Engine::Core::TransformComponent>();
        if (mode->stable_rank <= front_rank) {
          continue;
        }
        bool const bypassed = owner == k_player_owner
                                  ? transform->position.x > front_progress + 0.25
                                  : transform->position.x < front_progress - 0.25;
        if (bypassed) {
          ++out.quality.rear_rank_bypasses;
        }
      }
      std::sort(members.begin(),
                members.end(),
                [](Engine::Core::Entity* lhs, Engine::Core::Entity* rhs) {
                  auto const* lhs_mode =
                      lhs->get_component<Engine::Core::FormationModeComponent>();
                  auto const* rhs_mode =
                      rhs->get_component<Engine::Core::FormationModeComponent>();
                  return std::pair{lhs_mode->stable_rank, lhs_mode->stable_file} <
                         std::pair{rhs_mode->stable_rank, rhs_mode->stable_file};
                });
      for (std::size_t i = 1; i < members.size(); ++i) {
        auto const* previous_mode =
            members[i - 1]->get_component<Engine::Core::FormationModeComponent>();
        auto const* mode =
            members[i]->get_component<Engine::Core::FormationModeComponent>();
        if (mode->stable_rank != previous_mode->stable_rank) {
          continue;
        }
        auto const* previous_transform =
            members[i - 1]->get_component<Engine::Core::TransformComponent>();
        auto const* transform =
            members[i]->get_component<Engine::Core::TransformComponent>();
        if (transform->position.z + 0.20F < previous_transform->position.z) {
          ++out.quality.friendly_order_inversions;
        }
      }
    }
    out.performance.max_live_entities =
        std::max(out.performance.max_live_entities, live_count);
    ++out.performance.ticks;
  }

  if (auto* ai = scenario->world.get_system<Game::Systems::AISystem>()) {
    ai->update(&scenario->world, 0.0F);
    out.performance.ai_decisions = ai->completed_decision_count();
    out.performance.ai_commands = ai->applied_command_count();
  }
  out.performance.simulated_seconds = tick_count * config.fixed_step_seconds;
  out.performance.wall_seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  return out;
}

auto verify(const CaptureResult& result) -> VerificationReport {
  VerificationReport report;
  report.metrics = result.quality;
  if (result.quality.first_response_seconds < 0.0 ||
      result.quality.first_response_seconds > 0.20) {
    add_issue(report, "input_response", "player units did not respond within 200 ms");
  }
  if (result.quality.non_finite_samples != 0U) {
    add_issue(report, "non_finite_motion", "position or velocity became non-finite");
  }
  if (result.quality.excessive_position_steps != 0U) {
    add_issue(report, "position_jump", "unit displacement exceeded its speed envelope");
  }
  if (result.quality.max_yaw_step_degrees > 50.0) {
    add_issue(report,
              "yaw_snap",
              "visible facing changed by more than 50 degrees in one tick");
  }
  if (result.quality.locomotion_flickers > result.performance.entity_updates / 100U) {
    add_issue(report, "locomotion_flicker", "idle/walk state flickered repeatedly");
  }
  if (result.quality.damage_without_visible_action != 0U) {
    add_issue(report, "invisible_damage", "damage occurred without a visible action");
  }
  if (result.quality.rear_rank_bypasses != 0U) {
    add_issue(
        report, "rear_rank_bypass", "a reserve crossed the occupied combat front");
  }
  if (result.quality.friendly_order_inversions != 0U) {
    add_issue(report, "formation_crossing", "friendly rank/file order inverted");
  }
  if (result.config.duration_seconds >= 12.0 &&
      result.quality.first_combat_seconds < 0.0) {
    add_issue(report, "no_engagement", "opposed forces never reached visible combat");
  }
  if (result.performance.slowest_tick_ms > 33.0) {
    add_issue(report, "simulation_hitch", "a simulation tick exceeded 33 ms");
  }
  if (result.config.scenario == ScenarioId::BotSkirmish &&
      (result.performance.ai_decisions == 0U || result.performance.ai_commands == 0U)) {
    add_issue(report, "idle_bot", "AI produced no gameplay command during the soak");
  }
  report.passed = report.issues.empty();
  return report;
}

void write_json_lines(const CaptureResult& result, std::ostream& output) {
  output << std::setprecision(10);
  for (auto const& r : result.ticks) {
    output << R"({"type":"tick","tick":)" << r.tick << ",\"entity\":" << r.entity_id
           << ",\"group\":" << r.group_id << ",\"slot\":" << r.slot_id
           << ",\"target\":" << r.target_id << ",\"action\":" << r.action_id
           << ",\"transform\":[" << r.x << ',' << r.z << ',' << r.yaw
           << "],\"preferred_velocity\":[" << r.preferred_vx << ',' << r.preferred_vz
           << "],\"actual_velocity\":[" << r.actual_vx << ',' << r.actual_vz
           << "],\"health\":" << r.health << R"(,"event":")"
           << json_escape(r.action_event) << R"(","animation":")"
           << json_escape(r.animation_state) << R"(","dead":)"
           << (r.dead ? "true" : "false") << "}\n";
  }
  for (auto const& r : result.overlays) {
    output << R"({"type":"overlay","tick":)" << r.tick << R"(,"kind":")" << r.kind
           << R"(","owner":)" << r.owner_id << ",\"geometry\":[";
    for (std::size_t i = 0; i < r.geometry.size(); ++i) {
      if (i != 0U) {
        output << ',';
      }
      output << r.geometry[i];
    }
    output << R"(],"label":")" << json_escape(r.label) << "\"}\n";
  }
  output << R"({"type":"summary","scenario":")" << scenario_name(result.config.scenario)
         << R"(","seed":)" << result.config.seed
         << ",\"fixed_step\":" << result.config.fixed_step_seconds
         << ",\"ticks\":" << result.performance.ticks
         << ",\"entity_updates\":" << result.performance.entity_updates
         << ",\"action_events\":" << result.performance.action_events
         << ",\"damage_events\":" << result.performance.damage_events
         << ",\"deaths\":" << result.performance.deaths
         << ",\"ai_decisions\":" << result.performance.ai_decisions
         << ",\"ai_commands\":" << result.performance.ai_commands
         << ",\"slowest_tick_ms\":" << result.performance.slowest_tick_ms
         << ",\"wall_seconds\":" << result.performance.wall_seconds
         << ",\"digest\":" << result.deterministic_digest << "}\n";
}

void write_acceptance_manifest(const CaptureResult& result, std::ostream& output) {
  output << "scenario=" << scenario_name(result.config.scenario)
         << "\nseed=" << result.config.seed
         << "\nsimulation_hz=" << std::llround(1.0 / result.config.fixed_step_seconds)
         << "\nrender_fps=30,60" << "\nrender_frame_policy=interpolate_same_replay"
         << "\nproduction_system_registry=true"
         << "\ndigest=" << result.deterministic_digest << '\n';
}

void write_verification_report(const VerificationReport& report, std::ostream& output) {
  output << (report.passed ? "PASS" : "FAIL") << '\n';
  output << "first_response_seconds=" << report.metrics.first_response_seconds << '\n';
  output << "first_combat_seconds=" << report.metrics.first_combat_seconds << '\n';
  output << "max_position_step=" << report.metrics.max_position_step << '\n';
  output << "max_yaw_step_degrees=" << report.metrics.max_yaw_step_degrees << '\n';
  output << "locomotion_flickers=" << report.metrics.locomotion_flickers << '\n';
  for (auto const& issue : report.issues) {
    output << "issue=" << issue.code << ": " << issue.message << '\n';
  }
}

} // namespace Game::BattlefieldCapture
