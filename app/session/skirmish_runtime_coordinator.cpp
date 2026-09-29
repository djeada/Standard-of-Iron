#include "app/session/skirmish_runtime_coordinator.h"

#include <QDebug>
#include <QVector3D>

#include <chrono>
#include <cstdint>

#include "app/session/level_orchestrator.h"
#include "app/session/loading_overlay.h"
#include "game/core/component_core.h"
#include "game/core/startup_profiler.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/map_context.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/difficulty_forces.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/units/spawn_type.h"
#include "scene/camera.h"

namespace App::Core {

auto SkirmishRuntimeCoordinator::perform_load(
    const PerformSkirmishLoadContext& ctx) const -> PerformSkirmishLoadEffects {
  LevelOrchestrator orchestrator;
  const LevelLoadResult load_result =
      orchestrator.load_skirmish(ctx.map_path,
                                 ctx.player_configs,
                                 ctx.selected_player_id,
                                 ctx.world,
                                 ctx.scene,
                                 ctx.level,
                                 ctx.entity_cache,
                                 ctx.victory_service,
                                 ctx.minimap_manager,
                                 ctx.visibility_coordinator,
                                 ctx.emit_owner_info_changed,
                                 ctx.allow_default_player_barracks,
                                 ctx.defer_ai_initialization,
                                 ctx.loading_progress_tracker);

  return {.success = load_result.success,
          .error = load_result.error_message,
          .updated_player_id = load_result.updated_player_id,
          .selected_player_changed =
              load_result.updated_player_id != ctx.selected_player_id,
          .resolved_player_configs = load_result.resolved_player_configs};
}

void SkirmishRuntimeCoordinator::center_camera_on_local_forces(
    const CenterCameraOnLocalForcesContext& ctx) const {
  if (ctx.world == nullptr || ctx.camera == nullptr) {
    return;
  }

  QVector3D troops_sum(0.0F, 0.0F, 0.0F);
  QVector3D structures_sum(0.0F, 0.0F, 0.0F);
  int troops_count = 0;
  int structures_count = 0;

  for (auto* entity : ctx.world->collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        unit->owner_id != ctx.local_owner_id) {
      continue;
    }

    const QVector3D pos(
        transform->position.x, transform->position.y, transform->position.z);
    if (Game::Units::is_troop_spawn(unit->spawn_type)) {
      troops_sum += pos;
      ++troops_count;
    } else {
      structures_sum += pos;
      ++structures_count;
    }
  }

  QVector3D focus;
  if (troops_count > 0) {
    focus = troops_sum / static_cast<float>(troops_count);
  } else if (structures_count > 0) {
    focus = structures_sum / static_cast<float>(structures_count);
  } else {
    return;
  }

  const QVector3D current_target = ctx.camera->get_target();
  const QVector3D current_position = ctx.camera->get_position();
  const QVector3D offset = current_position - current_target;
  if (offset.lengthSquared() < 1e-6F) {
    const auto framing = Game::GameConfig::instance().camera_reset_framing();
    ctx.camera->set_rts_view(focus, framing.distance, framing.pitch, framing.yaw);
    return;
  }

  ctx.camera->look_at(focus + offset, focus, ctx.camera->get_up_vector());
}

void SkirmishRuntimeCoordinator::initialize_player_resources(
    const InitializePlayerResourcesContext& ctx) const {
  auto& resources = ctx.session.economy();
  resources.clear();

  auto& owner_registry = ctx.session.owners();
  for (const auto& owner : owner_registry.get_all_owners()) {
    resources.ensure_owner(owner.owner_id);
  }

  const bool is_mission = ctx.mission_definition != nullptr;

  Game::Systems::ResourceAmounts stock = ctx.level.starting_resources;
  if (is_mission) {
    ctx.mission_definition->player_setup.starting_resources.apply_to(stock);
  } else {
    stock.set(Game::Systems::ResourceType::Gold,
              Game::GameConfig::instance().get_starting_gold());
  }

  const auto endow = [&](int owner_id, float multiplier) {
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      const int authored = stock.get(type);
      resources.set(
          owner_id, type, Game::Mission::scaled_resource_amount(authored, multiplier));
    }
  };
  for (const auto& owner_id : owner_registry.get_player_owner_ids()) {
    endow(owner_id, 1.0F);
  }
  for (const auto& owner_id : owner_registry.get_ai_owner_ids()) {
    const bool eligible =
        ctx.difficulty != nullptr &&
        Game::Mission::difficulty_applies_to(
            *ctx.difficulty, owner_registry, owner_id, ctx.local_owner_id);
    endow(owner_id,
          eligible ? ctx.difficulty->profile_for(owner_id).resource_multiplier : 1.0F);
  }
}

auto SkirmishRuntimeCoordinator::finalize_load(
    const FinalizeSkirmishLoadContext& ctx) const -> FinalizeSkirmishLoadEffects {
  ctx.runtime_loading = false;
  ctx.loading_overlay.arm_after_load();
  ctx.loading_overlay.set_show_objectives_after_loading(ctx.is_mission_match);
  return {};
}

auto SkirmishRuntimeCoordinator::loading_tip_hints(const CampaignManager* campaign)
    -> LoadingTipHints {
  LoadingTipHints hints;
  if (campaign == nullptr || !campaign->current_mission_definition().has_value()) {
    return hints;
  }
  const auto& mission = *campaign->current_mission_definition();
  hints.mission_id = mission.id;
  hints.mission_has_undead = mission.include_ambient_undead;
  for (const auto& condition : mission.victory_conditions) {
    if (condition.type.contains(QStringLiteral("undead")) ||
        condition.type == QStringLiteral("purify_shrine")) {
      hints.mission_has_undead = true;
    }
  }
  return hints;
}

void SkirmishRuntimeCoordinator::apply_difficulty_forces(
    Engine::Core::World& world,
    const Game::Mission::MatchDifficulty& difficulty,
    int local_owner_id) {
  const Engine::Core::ScopedStartupPhase phase("mission.difficulty_forces");
  const auto forces =
      Game::Mission::apply_starting_force_difficulty(world, difficulty, local_owner_id);
  (void)Game::Mission::apply_undead_wave_difficulty(world, difficulty);
  if (forces.units_added != 0 || forces.units_withdrawn != 0) {
    qInfo() << "Difficulty:" << difficulty.baseline_id() << "reinforced"
            << forces.owners_scaled << "opponent(s) by" << forces.units_added
            << "unit(s) and withdrew" << forces.units_withdrawn;
  }
}

void SkirmishRuntimeCoordinator::record_startup_counters(Engine::Core::World& world) {
  const auto map_statistics = Game::Map::MapContextStore::statistics();
  auto& profiler = Engine::Core::StartupProfiler::instance();
  profiler.add_counter("map.requests",
                       static_cast<std::int64_t>(map_statistics.requests));
  profiler.add_counter("map.parses", static_cast<std::int64_t>(map_statistics.parses));
  profiler.add_counter("map.reuses", static_cast<std::int64_t>(map_statistics.reuses));
  if (Engine::Core::StartupProfiler::reporting_enabled()) {
    std::int64_t unit_count = 0;
    for ([[maybe_unused]] auto entry : world.view<Engine::Core::UnitComponent>()) {
      ++unit_count;
    }
    profiler.add_counter("world.units", unit_count);
  }
}

void SkirmishRuntimeCoordinator::prepare_ai_state(
    Engine::Core::World* world, Game::Session::SessionContext* session) {
  if (world == nullptr || session == nullptr) {
    return;
  }

  auto* ai_system = world->get_system<Game::Systems::AISystem>();
  if (ai_system == nullptr) {
    return;
  }

  const Engine::Core::ScopedStartupPhase phase("ai.initial_preparation");

  const auto& ai_owner_ids = session->owners().get_ai_owner_ids();
  if (ai_system->ai_player_count() != ai_owner_ids.size()) {
    ai_system->reinitialize();
  }

  ai_system->prepare_initial_decisions(*world);

  constexpr auto k_initial_decision_budget = std::chrono::milliseconds(1500);
  if (!ai_system->await_initial_decisions(k_initial_decision_budget)) {
    qWarning() << "Mission startup: AI initial decisions were still running after"
               << k_initial_decision_budget.count() << "ms";
  }

  Engine::Core::StartupProfiler::instance().add_counter(
      "ai.owners", static_cast<std::int64_t>(ai_system->ai_player_count()));
}

} // namespace App::Core
