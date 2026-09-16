#include "guard_system.h"

#include <QVector3D>

#include <cmath>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "combat_system/combat_utils.h"

namespace Game::Systems {

void GuardSystem::run(Engine::Core::SystemContext& context) {
  constexpr float k_follow_threshold = 2.0F;
  constexpr float k_follow_goal_tolerance = 0.5F;

  for (auto [entity_id, guard_mode_ref, movement_ref, transform_ref, unit_ref] :
       context.view<Engine::Core::GuardModeComponent,
                    Engine::Core::MovementComponent,
                    const Engine::Core::TransformComponent,
                    const Engine::Core::UnitComponent>()) {
    auto* guard_mode = &guard_mode_ref;
    const auto* movement = &movement_ref;

    if (!guard_mode->active || !guard_mode->has_guard_target || unit_ref.health <= 0) {
      continue;
    }

    const auto* attack_target =
        context.try_get<Engine::Core::AttackTargetComponent>(entity_id);
    if ((attack_target != nullptr) && attack_target->target_id != 0) {
      continue;
    }

    auto* entity = context.world().get_entity(entity_id);
    auto const post = Combat::guard_post_of(entity);
    if (!post.has_value()) {
      continue;
    }

    float threshold = -1.0F;
    if (guard_mode->guarded_entity_id != 0) {
      threshold = k_follow_threshold;
      guard_mode->guard_position_x = post->x();
      guard_mode->guard_position_z = post->z();
      bool const heading_to_a_stale_post =
          movement->get_has_target() &&
          (std::abs(movement->get_goal_x() - post->x()) >= k_follow_goal_tolerance ||
           std::abs(movement->get_goal_y() - post->z()) >= k_follow_goal_tolerance);
      if (heading_to_a_stale_post) {
        guard_mode->returning_to_guard_position = false;
      }
    }
    Combat::send_guard_home(context.world(), entity, threshold);
  }
}

auto GuardSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent,
            TransformComponent,
            AttackTargetComponent,
            BuildingComponent,
            PendingRemovalComponent>{},
      Writes<GuardModeComponent, MovementComponent, AttackComponent>{});
}

} // namespace Game::Systems
