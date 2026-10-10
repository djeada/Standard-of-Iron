#pragma once

// Headless, deterministic playthrough of a campaign mission.
//
// A mission is loaded the way GameEngine loads one (LevelOrchestrator::load_skirmish
// followed by GameEngine::configure_loaded_match), with no renderer, and then ticked
// the way GameEngine::simulate ticks it: per fixed 60 Hz step, the mission waves and
// timed events, the mission stages, World::update (every runtime system, including the
// AI, rockfall, rafts and undead zones) and finally the VictoryService.
//
// A scripted player plan issues orders through the same entry points the UI uses
// (App::Utils::submit_ground_move, App::Utils::issue_attack_command, the command
// queue), so every order is validated and drained by the command system on the next
// tick exactly as a mouse click would be.

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/mission/mission_runtime.h"
#include "app/orders/movement_utils.h"
#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "app/session/skirmish_loader.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "core/component_combat.h"
#include "core/component_commander.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/component_structures.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "game/command/command_queue.h"
#include "game/core/event_manager.h"
#include "game/game_config.h"
#include "game/map/map_context.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/default_content.h"
#include "game/systems/economy/production_service.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/victory_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "tests/support/ai_quiesce.h"

namespace Playthrough {

using Engine::Core::EntityID;
using Game::Units::SpawnType;
using Ids = std::vector<EntityID>;

constexpr int k_local_owner = 1;

// A point in the mission/map authoring space (the numbers in the JSON files).
struct G {
  float x = 0.0F;
  float z = 0.0F;
};

struct Report {
  QString outcome;
  float seconds = 0.0F;
  QString defeat_rule;
  QString defeat_description;
  int companies = 0;
  int men = 0;
  int start_companies = 0;
  int start_men = 0;
  int enemy_commanders = 0;
  int stages_complete = 0;
  int stage_count = 0;
  // Progress, for missions the plan does not win.
  int commanders_killed = 0;
  int camps_captured = 0;
  int lost_by_5min = -1;
  int lost_by_10min = -1;
  QString last_active_stage;
  QStringList objectives;
  double wall_seconds = 0.0;

  [[nodiscard]] auto line() const -> std::string {
    char buffer[512];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "outcome=%s t=%.0fs (%d:%02d) defeat_rule=%s companies=%d/%d "
                  "men=%d/%d enemy_commanders=%d killed_commanders=%d captured=%d "
                  "lost@5m=%d lost@10m=%d stages=%d/%d active_stage=%s wall=%ds",
                  outcome.isEmpty() ? "none" : outcome.toStdString().c_str(),
                  seconds,
                  static_cast<int>(seconds) / 60,
                  static_cast<int>(seconds) % 60,
                  defeat_rule.isEmpty() ? "-" : defeat_rule.toStdString().c_str(),
                  companies,
                  start_companies,
                  men,
                  start_men,
                  enemy_commanders,
                  commanders_killed,
                  camps_captured,
                  lost_by_5min,
                  lost_by_10min,
                  stages_complete,
                  stage_count,
                  last_active_stage.isEmpty() ? "-"
                                              : last_active_stage.toStdString().c_str(),
                  static_cast<int>(wall_seconds));
    return buffer;
  }
};

class Mission;

// One line of a plan: when `when` first holds, run `act`. With `every` > 0 the step
// keeps firing at that interval for as long as `when` holds.
struct Step {
  std::string label;
  std::function<bool(Mission&)> when;
  std::function<void(Mission&)> act;
  float every = 0.0F;
  float next_due = 0.0F;
  bool fired = false;
};
using Plan = std::vector<Step>;

inline auto narrating() -> bool {
  const char* value = std::getenv("SOI_PLAYTHROUGH_NARRATE");
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

class Mission {
public:
  Mission() = default;
  ~Mission() { TestSupport::quiesce_ai(m_world); }
  Mission(const Mission&) = delete;
  Mission(Mission&&) = delete;
  auto operator=(const Mission&) -> Mission& = delete;
  auto operator=(Mission&&) -> Mission& = delete;

  // Mirrors LevelOrchestrator::load_skirmish + GameEngine::configure_loaded_match for a
  // campaign mission.
  auto load(const QString& campaign_id,
            const QString& mission_id,
            const QString& difficulty = QStringLiteral("normal")) -> bool {
    // GameEngine composes its session with the default content (nations, troop
    // catalogue, formations); without it every nation lookup falls back to Rome.
    Game::Systems::initialize_default_content(m_session->nations());
    m_world.set_presentation_enabled(false);
    Game::Systems::register_runtime_systems(m_world);

    int selected_player_id = k_local_owner;
    m_campaign.start_campaign_mission(
        QStringLiteral("%1/%2").arg(campaign_id, mission_id),
        selected_player_id,
        difficulty);
    if (!m_campaign.current_mission_definition().has_value()) {
      m_error = QStringLiteral("mission did not load");
      return false;
    }
    const auto& mission = *m_campaign.current_mission_definition();
    const QString map_path = mission.map_path.startsWith(QStringLiteral(":/"))
                                 ? mission.map_path.mid(2)
                                 : mission.map_path;
    const QVariantList player_configs = build_campaign_player_configs(mission);

    // --- LevelOrchestrator::load_skirmish -------------------------------------
    App::Core::SkirmishLoader loader(m_world, m_renderer, m_camera);
    int updated_player_id = selected_player_id;
    const auto load_result = loader.start(
        map_path, player_configs, selected_player_id, false, updated_player_id);
    if (!load_result.ok) {
      m_error = load_result.error_message;
      return false;
    }
    selected_player_id = updated_player_id;
    m_level.map_path = map_path;
    m_level.map_name = load_result.map_name;
    m_level.player_unit_id = load_result.player_unit_id;
    m_level.max_troops_per_player = load_result.max_troops_per_player;
    m_level.grid_width = load_result.grid_width;
    m_level.grid_height = load_result.grid_height;
    m_level.tile_size = load_result.tile_size;
    m_level.is_spectator_mode = load_result.is_spectator_mode;
    Game::GameConfig::instance().set_max_troops_per_player(
        load_result.max_troops_per_player);

    QString map_error;
    m_map_context = Game::Map::MapContextStore::acquire(map_path, &map_error);
    if (!m_map_context.valid()) {
      m_error = map_error;
      return false;
    }
    m_level.starting_resources = m_map_context.definition()->starting_resources;
    Game::Session::configure_map_systems(
        m_world, *m_map_context.definition(), &m_victory);
    m_session->terrain().seal();
    {
      auto& troops = m_session->troop_counts();
      troops.rebuild_from_world(m_world);
      auto& stats = m_session->stats();
      stats.rebuild_from_world(m_world);
      for (const auto& owner : m_session->owners().get_all_owners()) {
        if (owner.type == Game::Systems::OwnerType::Player ||
            owner.type == Game::Systems::OwnerType::AI) {
          stats.mark_game_start(owner.owner_id);
        }
      }
    }
    Game::Systems::NavGrid::prewarm();

    // --- GameEngine::configure_loaded_match ------------------------------------
    m_runtime.set_difficulty(App::Mission::MissionRuntime::resolve_difficulty(
        &m_campaign,
        load_result.resolved_player_configs.isEmpty()
            ? player_configs
            : load_result.resolved_player_configs));
    App::Core::SkirmishRuntimeCoordinator::apply_difficulty_forces(
        m_world, m_runtime.difficulty(), k_local_owner);
    m_runtime.apply_skirmish_commander_setup(binding(), player_configs);
    if (!m_runtime.bind_setup(binding(), selected_player_id).has_value()) {
      m_error = QStringLiteral("mission setup refused");
      return false;
    }
    (void)m_runtime.configure_stages(binding());
    App::Core::SkirmishRuntimeCoordinator runtime;
    runtime.initialize_player_resources(
        {*m_session, m_level, k_local_owner, &mission, &m_runtime.difficulty()});
    m_campaign.configure_mission_victory_conditions(&m_victory, k_local_owner);
    m_victory.set_spectator_mode(m_level.is_spectator_mode);
    App::Core::SkirmishRuntimeCoordinator::prepare_ai_state(&m_world, m_session.get());

    m_victory.set_victory_callback([this](const QString& state) {
      if (m_outcome.isEmpty()) {
        m_outcome = state;
      }
    });

    m_grid_origin = to_world({0.0F, 0.0F});
    const QVector3D unit_x = to_world({1.0F, 0.0F}) - m_grid_origin;
    const QVector3D unit_z = to_world({0.0F, 1.0F}) - m_grid_origin;
    m_grid_scale_x = std::abs(unit_x.x()) > 1e-4F ? unit_x.x() : 1.0F;
    m_grid_scale_z = std::abs(unit_z.z()) > 1e-4F ? unit_z.z() : 1.0F;
    return true;
  }

  [[nodiscard]] auto error() const -> QString { return m_error; }

  // ---- coordinates ------------------------------------------------------------
  [[nodiscard]] auto to_world(G point) const -> QVector3D {
    return Game::Mission::mission_position_to_world(
        Game::Mission::Position{.x = point.x, .z = point.z},
        m_map_context.definition(),
        m_level);
  }
  [[nodiscard]] auto to_grid(const QVector3D& world) const -> G {
    return {(world.x() - m_grid_origin.x()) / m_grid_scale_x,
            (world.z() - m_grid_origin.z()) / m_grid_scale_z};
  }

  // ---- queries -------------------------------------------------------------------
  [[nodiscard]] auto world() -> Engine::Core::World& { return m_world; }
  [[nodiscard]] auto seconds() const -> float { return m_runtime.waves().elapsed(); }
  [[nodiscard]] auto outcome() const -> QString { return m_outcome; }

  [[nodiscard]] auto unit(EntityID id) -> const Engine::Core::UnitComponent* {
    auto* entity = m_world.get_entity(id);
    return entity == nullptr ? nullptr
                             : entity->get_component<Engine::Core::UnitComponent>();
  }
  [[nodiscard]] auto alive(EntityID id) -> bool {
    auto* entity = m_world.get_entity(id);
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      return false;
    }
    const auto* u = entity->get_component<Engine::Core::UnitComponent>();
    return u != nullptr && u->health > 0;
  }
  [[nodiscard]] auto position(EntityID id) -> G {
    auto* entity = m_world.get_entity(id);
    const auto* transform =
        entity == nullptr ? nullptr
                          : entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      return {};
    }
    return to_grid(QVector3D(transform->position.x, 0.0F, transform->position.z));
  }
  [[nodiscard]] static auto distance(G a, G b) -> float {
    return std::hypot(a.x - b.x, a.z - b.z);
  }
  [[nodiscard]] auto is_commander(EntityID id) -> bool {
    auto* entity = m_world.get_entity(id);
    return entity != nullptr &&
           entity->has_component<Engine::Core::CommanderComponent>();
  }
  [[nodiscard]] auto is_enemy_owner(int owner_id) -> bool {
    return owner_id != k_local_owner && !Game::Core::is_neutral_owner(owner_id) &&
           !m_session->owners().are_allies(owner_id, k_local_owner);
  }
  [[nodiscard]] static auto men_in(const Engine::Core::UnitComponent& u) -> int {
    return Engine::Core::resolve_surviving_individual_count(
        u.health,
        u.max_health,
        Game::Units::TroopConfig::instance().get_individuals_per_unit(u.spawn_type));
  }

  // Every living unit/structure matching `filter`.
  template <typename Filter>
  auto collect(Filter filter) -> Ids {
    Ids ids;
    for (auto* entity : m_world.collect_entities_with<Engine::Core::UnitComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const EntityID id = entity->get_id();
      if (!alive(id)) {
        continue;
      }
      const auto& u = *entity->get_component<Engine::Core::UnitComponent>();
      if (filter(id, u)) {
        ids.push_back(id);
      }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
  }

  // The local fighting troops: no commander, no builders, no civilians.
  auto army() -> Ids {
    return collect([this](EntityID id, const Engine::Core::UnitComponent& u) {
      return u.owner_id == k_local_owner &&
             Game::Units::is_combatant_spawn(u.spawn_type) && !is_commander(id);
    });
  }
  auto commander() -> Ids {
    return collect([this](EntityID id, const Engine::Core::UnitComponent& u) {
      return u.owner_id == k_local_owner && is_commander(id);
    });
  }
  auto local_of_type(std::initializer_list<SpawnType> types) -> Ids {
    const std::set<SpawnType> wanted(types);
    return collect([&](EntityID, const Engine::Core::UnitComponent& u) {
      return u.owner_id == k_local_owner && wanted.count(u.spawn_type) > 0;
    });
  }
  auto except_types(const Ids& ids, std::initializer_list<SpawnType> types) -> Ids {
    const std::set<SpawnType> drop(types);
    Ids out;
    for (const auto id : ids) {
      const auto* u = unit(id);
      if (u != nullptr && drop.count(u->spawn_type) == 0) {
        out.push_back(id);
      }
    }
    return out;
  }
  auto near(const Ids& ids, G centre, float radius) -> Ids {
    Ids out;
    for (const auto id : ids) {
      if (distance(position(id), centre) <= radius) {
        out.push_back(id);
      }
    }
    return out;
  }
  auto idle(const Ids& ids) -> Ids {
    Ids out;
    for (const auto id : ids) {
      auto* entity = m_world.get_entity(id);
      if (entity == nullptr) {
        continue;
      }
      const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
      const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
      const bool moving = movement != nullptr && movement->get_has_target();
      const bool fighting =
          attack != nullptr && attack->target_id != 0 && alive(attack->target_id);
      if (!moving && !fighting) {
        out.push_back(id);
      }
    }
    return out;
  }

  // Named groups, frozen when tagged; `group` returns the members still alive.
  void tag(const std::string& name, const Ids& ids) { m_groups[name] = ids; }
  auto group(const std::string& name) -> Ids {
    Ids out;
    for (const auto id : m_groups[name]) {
      if (alive(id)) {
        out.push_back(id);
      }
    }
    return out;
  }
  auto all_but(const Ids& ids, const Ids& excluded) -> Ids {
    Ids out;
    for (const auto id : ids) {
      if (std::find(excluded.begin(), excluded.end(), id) == excluded.end()) {
        out.push_back(id);
      }
    }
    return out;
  }

  auto barracks() -> Ids {
    return collect([](EntityID, const Engine::Core::UnitComponent& u) {
      return u.spawn_type == SpawnType::Barracks;
    });
  }
  auto enemy_barracks() -> Ids {
    return collect([this](EntityID, const Engine::Core::UnitComponent& u) {
      return u.spawn_type == SpawnType::Barracks && u.owner_id != k_local_owner;
    });
  }
  auto enemy_commanders() -> Ids {
    return collect([this](EntityID id, const Engine::Core::UnitComponent& u) {
      return is_enemy_owner(u.owner_id) && is_commander(id);
    });
  }
  auto commander_alive(int owner_id) -> bool {
    for (const auto id : enemy_commanders()) {
      if (unit(id)->owner_id == owner_id) {
        return true;
      }
    }
    return false;
  }
  auto enemy_troops() -> Ids {
    return collect([this](EntityID, const Engine::Core::UnitComponent& u) {
      return is_enemy_owner(u.owner_id) &&
             Game::Units::is_combatant_spawn(u.spawn_type);
    });
  }
  auto nearest(const Ids& ids, G from) -> std::optional<EntityID> {
    std::optional<EntityID> best;
    float best_distance = std::numeric_limits<float>::max();
    for (const auto id : ids) {
      const float d = distance(position(id), from);
      if (d < best_distance) {
        best_distance = d;
        best = id;
      }
    }
    return best;
  }
  auto centroid(const Ids& ids) -> G {
    if (ids.empty()) {
      return {};
    }
    G sum;
    for (const auto id : ids) {
      const G p = position(id);
      sum.x += p.x;
      sum.z += p.z;
    }
    return {sum.x / static_cast<float>(ids.size()),
            sum.z / static_cast<float>(ids.size())};
  }

  [[nodiscard]] auto
  stage(const QString& id) const -> const Game::Mission::StageStatus* {
    for (const auto& status : m_runtime.stages().stages()) {
      if (status.id == id) {
        return &status;
      }
    }
    return nullptr;
  }
  [[nodiscard]] auto stage_active(const QString& id) const -> bool {
    const auto* status = stage(id);
    return status != nullptr && status->active && !status->complete;
  }
  [[nodiscard]] auto stage_complete(const QString& id) const -> bool {
    const auto* status = stage(id);
    return status != nullptr && status->complete;
  }
  // Camps the VictoryService counts as captured: local barracks of a foreign build.
  [[nodiscard]] auto captured_from_enemy() -> int {
    const auto* local = m_session->nations().get_nation_for_player(k_local_owner);
    int count = 0;
    for (const auto id : barracks()) {
      const auto* building =
          m_world.get_entity(id)->get_component<Engine::Core::BuildingComponent>();
      if (unit(id)->owner_id == k_local_owner && building != nullptr &&
          local != nullptr && building->original_nation_id != local->id) {
        ++count;
      }
    }
    return count;
  }
  [[nodiscard]] auto captured_barracks() -> int {
    int count = 0;
    for (const auto id : barracks()) {
      if (unit(id)->owner_id == k_local_owner &&
          m_initial_foreign_barracks.count(id) > 0) {
        ++count;
      }
    }
    return count;
  }

  // ---- orders (the UI's own entry points) -----------------------------------------
  auto attack_move(const Ids& ids, G target) -> bool {
    return ground_move(ids, target, Game::Systems::MoveOrderKind::AttackMove);
  }
  auto move(const Ids& ids, G target) -> bool {
    return ground_move(ids, target, Game::Systems::MoveOrderKind::FormationMove);
  }
  auto attack(const Ids& ids, EntityID target) -> bool {
    if (ids.empty()) {
      return false;
    }
    const auto outcome =
        App::Utils::issue_attack_command(&m_world, ids, target, k_local_owner);
    note_order(
        "attack", ids.size(), position(target), outcome.accepted(), outcome.reason);
    return outcome.accepted();
  }
  auto hold(const Ids& ids, bool active = true) -> bool {
    if (ids.empty()) {
      return false;
    }
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Hold;
    request.payload = Game::Command::SetHold{.units = ids, .active = active};
    return App::Core::submit_player_order(m_world, k_local_owner, std::move(request))
        .accepted();
  }
  // Builders' "auto gather" with a priority product (cut_tree, collect_stone, ...).
  auto auto_gather(const Ids& ids, const char* product) -> bool {
    if (ids.empty()) {
      return false;
    }
    App::Core::OrderRequest request;
    request.kind = App::Core::OrderKind::Gather;
    request.payload = Game::Command::SetAutoGather{
        .units = ids, .active = true, .priority_product_type = product};
    const bool ok =
        App::Core::submit_player_order(m_world, k_local_owner, std::move(request))
            .accepted();
    note_order(product, ids.size(), centroid(ids), ok, QStringLiteral("refused"));
    return ok;
  }
  [[nodiscard]] auto harvested(Game::Systems::ResourceType type) -> int {
    return m_session->economy().get_harvested_all(k_local_owner).get(type);
  }

  // Queue one company of `product` at every local camp that can start it now (the
  // HUD's recruit button: ProductionService decides cost, manpower and queue room).
  auto recruit(Game::Units::TroopType product) -> int {
    int queued = 0;
    for (const auto camp : barracks()) {
      if (unit(camp)->owner_id != k_local_owner) {
        continue;
      }
      if (Game::Systems::ProductionService::can_start_production(
              m_world, camp, product) != Game::Systems::ProductionResult::Success) {
        continue;
      }
      Game::Command::submit(
          m_world,
          Game::Command::Source::LocalPlayer,
          k_local_owner,
          Game::Command::Produce{.building = camp, .product = product});
      ++queued;
    }
    return queued;
  }

  auto cross_by_raft(const Ids& ids) -> bool {
    auto units = App::Core::filter_selected_units_for_action(
        &m_world, ids, QStringLiteral("cross_raft"));
    if (units.empty()) {
      note_order(
          "raft", ids.size(), {}, false, QStringLiteral("no unit can raft here"));
      return false;
    }
    const std::size_t count = units.size();
    Game::Command::submit(m_world,
                          Game::Command::Source::LocalPlayer,
                          k_local_owner,
                          Game::Command::CrossByRaft{.units = std::move(units)});
    note_order("raft", count, {}, true, {});
    return true;
  }

  // ---- running ---------------------------------------------------------------------
  auto run(Plan plan, float cap_seconds) -> Report {
    const auto wall_start = std::chrono::steady_clock::now();
    for (const auto id : barracks()) {
      if (unit(id)->owner_id != k_local_owner) {
        m_initial_foreign_barracks.insert(id);
      }
    }
    Report report;
    tally(report.start_companies, report.start_men);
    const int start_enemy_commanders = static_cast<int>(enemy_commanders().size());
    if (narrating()) {
      survey();
    }

    auto& session = *m_session;
    const double tick = session.clock().tick_seconds();
    float next_plan_check = 0.0F;
    float next_narration = 0.0F;
    QString last_stage;
    while (m_outcome.isEmpty() && seconds() < cap_seconds) {
      if (seconds() >= next_plan_check) {
        next_plan_check = seconds() + 0.5F;
        m_peak_captured = std::max(m_peak_captured, captured_from_enemy());
        for (auto& step : plan) {
          if ((step.fired && step.every <= 0.0F) || seconds() < step.next_due) {
            continue;
          }
          if (step.when && !step.when(*this)) {
            continue;
          }
          if (narrating() && !step.fired) {
            std::printf("  [%5.0fs] plan: %s\n", seconds(), step.label.c_str());
          }
          step.fired = true;
          step.next_due = seconds() + step.every;
          step.act(*this);
        }
      }
      if (narrating()) {
        const QString active = active_stage_id();
        if (active != last_stage) {
          std::printf(
              "  [%5.0fs] stage -> %s\n", seconds(), active.toStdString().c_str());
          last_stage = active;
        }
        if (seconds() >= next_narration) {
          next_narration = seconds() + 60.0F;
          narrate();
        }
        if (const char* dump_at = std::getenv("SOI_PT_DUMP_AT");
            dump_at != nullptr && !m_dumped && seconds() >= std::atof(dump_at)) {
          m_dumped = true;
          dump_units();
        }
      }

      if (report.lost_by_5min < 0 && seconds() >= 300.0F) {
        report.lost_by_5min = m_losses;
      }
      if (report.lost_by_10min < 0 && seconds() >= 600.0F) {
        report.lost_by_10min = m_losses;
      }
      // GameEngine::simulate -> RuntimeFrameOrchestrator::advance_simulation.
      session.advance(tick,
                      8,
                      Game::Session::SessionContext::OverloadPolicy::DiscardBacklog,
                      [this](float step) {
                        if (m_outcome.isEmpty()) {
                          (void)m_runtime.advance_waves(binding(), step, false, [] {});
                        }
                        m_runtime.advance_stages(binding(), step);
                        m_world.update(step);
                        m_victory.update(m_world, step);
                      });
    }

    report.outcome = m_outcome;
    report.seconds = seconds();
    report.defeat_description = m_victory.get_defeat_description();
    if (m_outcome == QStringLiteral("defeat")) {
      for (const auto& condition :
           m_campaign.current_mission_definition()->defeat_conditions) {
        if (condition.description == report.defeat_description) {
          report.defeat_rule = condition.type;
        }
      }
    }
    tally(report.companies, report.men);
    report.enemy_commanders = static_cast<int>(enemy_commanders().size());
    report.commanders_killed =
        std::max(0, start_enemy_commanders - report.enemy_commanders);
    report.camps_captured = m_peak_captured;
    if (report.lost_by_5min < 0) {
      report.lost_by_5min = m_losses;
    }
    if (report.lost_by_10min < 0) {
      report.lost_by_10min = m_losses;
    }
    for (const auto& status : m_runtime.stages().stages()) {
      ++report.stage_count;
      if (status.complete) {
        ++report.stages_complete;
      }
    }
    report.last_active_stage = active_stage_id();
    for (const auto& objective : m_victory.objectives()) {
      report.objectives << QStringLiteral("%1 %2/%3%4")
                               .arg(objective.id)
                               .arg(objective.progress)
                               .arg(objective.required)
                               .arg(objective.complete ? QStringLiteral(" done")
                                                       : QString());
    }
    report.wall_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start)
            .count();
    if (narrating()) {
      narrate();
      std::printf("  objectives: %s\n",
                  report.objectives.join(QStringLiteral(", ")).toStdString().c_str());
      std::printf("  %s\n", report.line().c_str());
    }
    return report;
  }

  [[nodiscard]] auto active_stage_id() const -> QString {
    for (const auto& status : m_runtime.stages().stages()) {
      if (status.active && !status.complete) {
        return status.id;
      }
    }
    return QStringLiteral("(none)");
  }

  // Start-of-mission layout, in map coordinates, so plans can be written against it.
  void survey() {
    std::map<int, std::map<std::string, std::vector<std::string>>> by_owner;
    for (auto* entity : m_world.collect_entities_with<Engine::Core::UnitComponent>()) {
      const EntityID id = entity->get_id();
      if (!alive(id)) {
        continue;
      }
      const auto& u = *entity->get_component<Engine::Core::UnitComponent>();
      if (Game::Units::is_wildlife_spawn(u.spawn_type) ||
          u.spawn_type == SpawnType::WallSegment ||
          u.spawn_type == SpawnType::WallGate || u.spawn_type == SpawnType::Home ||
          u.spawn_type == SpawnType::Farm) {
        continue;
      }
      const G p = position(id);
      char buffer[96];
      std::snprintf(buffer,
                    sizeof(buffer),
                    "(%.0f,%.0f)%s",
                    p.x,
                    p.z,
                    is_commander(id) ? "*CMD" : "");
      std::string type(Game::Units::spawn_type_name(u.spawn_type));
      by_owner[u.owner_id][type].push_back(buffer);
    }
    std::printf("  survey (map coords):\n");
    for (const auto& [owner, types] : by_owner) {
      std::printf("   owner %d (%s%s):\n",
                  owner,
                  m_session->owners().get_owner_name(owner).c_str(),
                  owner == k_local_owner ? ", local"
                                         : (is_enemy_owner(owner) ? ", enemy" : ""));
      for (const auto& [type, spots] : types) {
        std::string joined;
        for (const auto& spot : spots) {
          joined += spot + " ";
        }
        std::printf(
            "     %-18s x%-3zu %s\n", type.c_str(), spots.size(), joined.c_str());
      }
    }
    for (const auto& status : m_runtime.stages().stages()) {
      const G target = to_grid(status.target);
      std::printf("   stage %-22s %s target=(%.0f,%.0f)\n",
                  status.id.toStdString().c_str(),
                  status.has_target ? "has" : "no ",
                  target.x,
                  target.z);
    }
  }

  // Per-company state of the local army and of every enemy within 40 of it.
  void dump_units() {
    auto line = [this](EntityID id) {
      auto* e = m_world.get_entity(id);
      const auto* u = unit(id);
      const auto* mv = e->get_component<Engine::Core::MovementComponent>();
      const auto* at = e->get_component<Engine::Core::AttackTargetComponent>();
      const auto* morale = e->get_component<Engine::Core::MoraleComponent>();
      const G p = position(id);
      const G goal = mv != nullptr && mv->get_has_target()
                         ? to_grid(QVector3D(mv->get_goal_x(), 0.0F, mv->get_goal_y()))
                         : G{-1, -1};
      std::printf(
          "     o%d %-16s (%.0f,%.0f) hp %d/%d men %d goal (%.0f,%.0f) target %s%s%s "
          "morale %.0f%s\n",
          u->owner_id,
          std::string(Game::Units::spawn_type_name(u->spawn_type)).c_str(),
          p.x,
          p.z,
          u->health,
          u->max_health,
          men_in(*u),
          goal.x,
          goal.z,
          at != nullptr && at->target_id != 0 && alive(at->target_id)
              ? std::string(
                    Game::Units::spawn_type_name(unit(at->target_id)->spawn_type))
                    .c_str()
              : "-",
          e->get_component<Engine::Core::HoldModeComponent>() != nullptr &&
                  e->get_component<Engine::Core::HoldModeComponent>()->active
              ? " HOLD"
              : "",
          e->has_component<Engine::Core::AIControlledComponent>() ? " AI" : "",
          morale != nullptr ? morale->morale : -1.0F,
          morale != nullptr && morale->routing ? " ROUT" : "");
    };
    std::printf("  [%5.0fs] unit dump\n", seconds());
    const Ids troops = army();
    for (const auto id : troops) {
      line(id);
    }
    for (const auto id : enemy_troops()) {
      for (const auto own : troops) {
        if (distance(position(id), position(own)) < 40.0F) {
          line(id);
          break;
        }
      }
    }
  }

  void narrate() {
    int companies = 0;
    int men = 0;
    tally(companies, men);
    std::map<int, std::pair<int, int>> enemy;
    for (const auto id : enemy_troops()) {
      const auto* u = unit(id);
      enemy[u->owner_id].first += 1;
      enemy[u->owner_id].second += men_in(*u);
    }
    std::string enemies;
    for (const auto& [owner, counts] : enemy) {
      enemies += std::to_string(owner) + ":" + std::to_string(counts.first) + "c/" +
                 std::to_string(counts.second) + "m ";
    }
    int own_barracks = 0;
    for (const auto id : barracks()) {
      if (unit(id)->owner_id == k_local_owner) {
        ++own_barracks;
      }
    }
    const auto cmd = commander();
    const G cmd_at = cmd.empty() ? G{} : position(cmd.front());
    std::printf("  [%5.0fs] stage=%s own=%dc/%dm barracks=%d captured=%d "
                "cmd=%s(%.0f,%.0f) enemy_cmd=%zu enemy={%s} waves_cleared=%d\n",
                seconds(),
                active_stage_id().toStdString().c_str(),
                companies,
                men,
                own_barracks,
                captured_barracks(),
                cmd.empty() ? "DEAD" : "ok",
                cmd_at.x,
                cmd_at.z,
                enemy_commanders().size(),
                enemies.c_str(),
                m_runtime.waves().director().cleared_wave_count());
    std::string camps;
    for (const auto id : barracks()) {
      const auto* u = unit(id);
      const G p = position(id);
      char buffer[64];
      std::snprintf(buffer,
                    sizeof(buffer),
                    "(%.0f,%.0f)o%d/%d%% ",
                    p.x,
                    p.z,
                    u->owner_id,
                    100 * u->health / std::max(1, u->max_health));
      camps += buffer;
    }
    std::string commanders;
    for (const auto id : enemy_commanders()) {
      const G p = position(id);
      char buffer[64];
      std::snprintf(
          buffer, sizeof(buffer), "o%d(%.0f,%.0f) ", unit(id)->owner_id, p.x, p.z);
      commanders += buffer;
    }
    const Ids troops = army();
    const G at = centroid(troops);
    float spread = 0.0F;
    for (const auto id : troops) {
      spread = std::max(spread, distance(position(id), at));
    }
    std::string recruiting;
    for (const auto id : barracks()) {
      if (unit(id)->owner_id == k_local_owner) {
        recruiting += std::to_string(static_cast<int>(
                          Game::Systems::ProductionService::can_start_production(
                              m_world, id, Game::Units::TroopType::Spearman))) +
                      " ";
      }
    }
    std::printf(
        "          recruit-ruling %s gold %d food %d\n",
        recruiting.c_str(),
        m_session->economy().get(k_local_owner, Game::Systems::ResourceType::Gold),
        m_session->economy().get(k_local_owner, Game::Systems::ResourceType::Food));
    std::printf(
        "          companies killed %d lost %d | camps: %s| enemy commanders: %s| "
        "army at (%.0f,%.0f) spread %.0f | harvested w%d s%d i%d\n",
        m_kills,
        m_losses,
        camps.c_str(),
        commanders.c_str(),
        at.x,
        at.z,
        spread,
        harvested(Game::Systems::ResourceType::Wood),
        harvested(Game::Systems::ResourceType::Stone),
        harvested(Game::Systems::ResourceType::Iron));
    std::fflush(stdout);
  }

private:
  auto binding() -> App::Mission::MissionBinding {
    return {.world = &m_world,
            .session = m_session.get(),
            .campaign = &m_campaign,
            .level = &m_level,
            .victory_service = &m_victory,
            .minimap = nullptr,
            .local_owner_id = k_local_owner};
  }

  void tally(int& companies, int& men) {
    companies = 0;
    men = 0;
    for (const auto id : collect([](EntityID, const Engine::Core::UnitComponent& u) {
           return u.owner_id == k_local_owner &&
                  Game::Units::is_troop_spawn(u.spawn_type);
         })) {
      ++companies;
      men += men_in(*unit(id));
    }
  }

  auto
  ground_move(const Ids& ids, G target, Game::Systems::MoveOrderKind kind) -> bool {
    if (ids.empty()) {
      return false;
    }
    const auto outcome = App::Utils::submit_ground_move(
        m_world, ids, to_world(target), k_local_owner, kind);
    note_order(kind == Game::Systems::MoveOrderKind::AttackMove ? "attack-move"
                                                                : "move",
               ids.size(),
               target,
               outcome.accepted(),
               outcome.reason);
    return outcome.accepted();
  }

  void note_order(const char* what,
                  std::size_t count,
                  G target,
                  bool accepted,
                  const QString& reason) {
    if (!narrating() && accepted) {
      return;
    }
    if (!accepted || narrating()) {
      std::printf("  [%5.0fs]   %s %zu unit(s) -> (%.0f,%.0f) %s%s\n",
                  seconds(),
                  what,
                  count,
                  target.x,
                  target.z,
                  accepted ? "ok" : "REFUSED: ",
                  accepted ? "" : reason.toStdString().c_str());
    }
  }

  std::unique_ptr<Game::Session::SessionContext> m_session{
      std::make_unique<Game::Session::SessionContext>()};
  Game::Session::ScopedSession m_scope{*m_session};
  Engine::Core::World& m_world{m_session->world()};
  Render::GL::Renderer m_renderer{Render::ShaderQuality::None};
  Render::GL::Camera m_camera;
  CampaignManager m_campaign;
  Game::Systems::LevelSnapshot m_level;
  Game::Map::MapContext m_map_context;
  Game::Systems::VictoryService m_victory{
      Game::Systems::VictoryService::Services{.stats = m_session->stats(),
                                              .owners = m_session->owners(),
                                              .nations = m_session->nations(),
                                              .economy = m_session->economy()}};
  App::Mission::MissionRuntime m_runtime{nullptr, nullptr, [](const QString& text) {
                                           if (narrating()) {
                                             std::printf("          \"%s\"\n",
                                                         text.toStdString().c_str());
                                           }
                                         }};
  // Captures and commander deaths, narrated as they happen.
  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent> m_captured{
      [this](const Engine::Core::BarrackCapturedEvent& e) {
        if (narrating()) {
          const G p = position(e.barrack_id);
          std::printf("  [%5.0fs] ** barracks (%.0f,%.0f) owner %d -> %d\n",
                      seconds(),
                      p.x,
                      p.z,
                      e.previous_owner_id,
                      e.new_owner_id);
        }
      }};
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent> m_died{
      [this](const Engine::Core::UnitDiedEvent& e) {
        if (Game::Units::is_combatant_spawn(e.spawn_type)) {
          if (e.owner_id == k_local_owner) {
            ++m_losses;
          } else if (is_enemy_owner(e.owner_id)) {
            ++m_kills;
          }
        }
        if (!narrating()) {
          return;
        }
        const bool commander = is_commander(e.unit_id);
        const bool structure = Game::Units::is_building_spawn(e.spawn_type) &&
                               e.spawn_type != SpawnType::WallSegment &&
                               e.spawn_type != SpawnType::Home &&
                               e.spawn_type != SpawnType::Farm;
        if (commander || structure || e.owner_id == k_local_owner) {
          const G p = position(e.unit_id);
          const auto* killer = unit(e.killer_id);
          std::printf(
              "  [%5.0fs] ** %s%s of owner %d died at (%.0f,%.0f), killed by %s of "
              "owner %d\n",
              seconds(),
              std::string(Game::Units::spawn_type_name(e.spawn_type)).c_str(),
              commander ? " (COMMANDER)" : "",
              e.owner_id,
              p.x,
              p.z,
              killer == nullptr
                  ? "?"
                  : std::string(Game::Units::spawn_type_name(killer->spawn_type))
                        .c_str(),
              e.killer_owner_id);
        }
      }};
  bool m_dumped = false;
  int m_peak_captured = 0;
  int m_kills = 0;
  int m_losses = 0;
  std::map<std::string, Ids> m_groups;
  std::set<EntityID> m_initial_foreign_barracks;
  QVector3D m_grid_origin;
  float m_grid_scale_x = 1.0F;
  float m_grid_scale_z = 1.0F;
  QString m_outcome;
  QString m_error;
};

// ---- plan vocabulary --------------------------------------------------------------
inline auto at(float seconds) -> std::function<bool(Mission&)> {
  return [seconds](Mission& m) {
    return m.seconds() >= seconds;
  };
}
inline auto when_stage(const char* id) -> std::function<bool(Mission&)> {
  return [id = QString::fromLatin1(id)](Mission& m) {
    return m.stage_active(id);
  };
}
inline auto after_stage(const char* id) -> std::function<bool(Mission&)> {
  return [id = QString::fromLatin1(id)](Mission& m) {
    return m.stage_complete(id);
  };
}
inline auto always() -> std::function<bool(Mission&)> {
  return [](Mission&) {
    return true;
  };
}

} // namespace Playthrough
