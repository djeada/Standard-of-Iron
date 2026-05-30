#include "skirmish_runtime_coordinator.h"

#include <QVector3D>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/units/spawn_type.h"
#include "level_orchestrator.h"
#include "render/gl/camera.h"

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
                                 ctx.loading_progress_tracker);

  return {.success = load_result.success,
          .error = load_result.error_message,
          .updated_player_id = load_result.updated_player_id,
          .selected_player_changed =
              load_result.updated_player_id != ctx.selected_player_id};
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

  for (auto* entity : ctx.world->get_entities_with<Engine::Core::UnitComponent>()) {
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
    const auto& cam_config = Game::GameConfig::instance().camera();
    ctx.camera->set_rts_view(focus,
                             cam_config.default_distance,
                             cam_config.default_pitch,
                             cam_config.default_yaw);
    return;
  }

  ctx.camera->look_at(focus + offset, focus, ctx.camera->get_up_vector());
}

void SkirmishRuntimeCoordinator::initialize_player_resources(
    const InitializePlayerResourcesContext& ctx) const {
  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  resources.clear();

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  for (const auto& owner : owner_registry.get_all_owners()) {
    resources.ensure_owner(owner.owner_id);
  }

  if (ctx.mission_definition != nullptr) {
    const auto& mission_resources =
        ctx.mission_definition->player_setup.starting_resources;
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      resources.set(ctx.local_owner_id, type, mission_resources.get(type));
    }
    return;
  }

  const auto& map_resources = ctx.level.starting_resources;
  for (const auto& owner_id : owner_registry.get_player_owner_ids()) {
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      resources.set(owner_id, type, map_resources.get(type));
    }
  }
  for (const auto& owner_id : owner_registry.get_ai_owner_ids()) {
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      resources.set(owner_id, type, map_resources.get(type));
    }
  }
}

auto SkirmishRuntimeCoordinator::finalize_load(
    const FinalizeSkirmishLoadContext& ctx) const -> FinalizeSkirmishLoadEffects {
  ctx.runtime_loading = false;
  ctx.loading_overlay_wait_for_first_frame.store(true, std::memory_order_release);
  ctx.loading_overlay_frames_remaining = 5;
  ctx.loading_overlay_min_duration_ms = 1000;
  ctx.loading_overlay_timer.restart();
  ctx.finalize_progress_after_overlay = true;
  ctx.show_objectives_after_loading = ctx.is_campaign_mission;
  return {};
}

} // namespace App::Core
