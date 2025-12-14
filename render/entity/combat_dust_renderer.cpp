#include "combat_dust_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/systems/camera_visibility_service.h"
#include "../scene_renderer.h"

namespace Render::GL {

namespace {
constexpr float kDustRadius = 2.0F;
constexpr float kDustIntensity = 0.6F;
constexpr float kDustYOffset = 0.05F;
constexpr float kDustColorR = 0.6F;
constexpr float kDustColorG = 0.55F;
constexpr float kDustColorB = 0.45F;
constexpr float kVisibilityCheckRadius = 3.0F;
} // namespace

void render_combat_dust(Renderer *renderer, ResourceManager *,
                        Engine::Core::World *world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float animation_time = renderer->get_animation_time();
  auto &visibility = Game::Systems::CameraVisibilityService::instance();

  auto units = world->get_entities_with<Engine::Core::AttackComponent>();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *transform = unit->get_component<Engine::Core::TransformComponent>();
    auto *attack = unit->get_component<Engine::Core::AttackComponent>();
    auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || attack == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    // Only render dust for units in active melee combat
    if (!attack->in_melee_lock) {
      continue;
    }

    // Skip units not visible to the camera
    if (!visibility.is_entity_visible(transform->position.x,
                                       transform->position.z,
                                       kVisibilityCheckRadius)) {
      continue;
    }

    QVector3D position(transform->position.x, kDustYOffset,
                       transform->position.z);
    QVector3D color(kDustColorR, kDustColorG, kDustColorB);

    renderer->combat_dust(position, color, kDustRadius, kDustIntensity,
                          animation_time);
  }
}

} // namespace Render::GL
