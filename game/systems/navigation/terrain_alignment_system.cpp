#include "terrain_alignment_system.h"

#include <QVector3D>

#include <algorithm>

#include "core/ambient_session.h"
#include "core/component_commander.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/system_context.h"
#include "core/world.h"
#include "map/terrain_service.h"

namespace Game::Systems {

void TerrainAlignmentSystem::run(Engine::Core::SystemContext& context) {
  auto& terrain_service = *Game::Session::services_for(context.world()).terrain;

  if (!terrain_service.is_initialized()) {
    return;
  }

  for (auto [entity_id, transform] : context.view<Engine::Core::TransformComponent>()) {
    auto* launch = context.try_get<Engine::Core::CombatLaunchComponent>(entity_id);
    float const launch_height =
        launch != nullptr ? std::max(0.0F, transform.position.y - launch->ground_y)
                          : 0.0F;
    align_transform_to_terrain(transform, terrain_service);
    if (launch != nullptr) {
      launch->ground_y = transform.position.y;
      transform.position.y += launch_height;
    }
    if (const auto* walker =
            context.try_get<Engine::Core::WallWalkerComponent>(entity_id)) {
      transform.position.y += walker->elevation;
    }
    if (const auto* rider =
            context.try_get<Engine::Core::RaftRiderComponent>(entity_id)) {
      transform.position.y = std::max(transform.position.y, rider->deck_y);
    }
  }
}

void TerrainAlignmentSystem::align_transform_to_terrain(
    Engine::Core::TransformComponent& transform,
    Game::Map::TerrainService& terrain_service) {
  QVector3D const aligned = terrain_service.resolve_surface_world_position(
      transform.position.x, transform.position.z, 0.0F, transform.position.y);
  transform.position.y = aligned.y();
}

auto TerrainAlignmentSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<WallWalkerComponent, RaftRiderComponent>{},
                               Writes<TransformComponent, CombatLaunchComponent>{});
}

} // namespace Game::Systems
