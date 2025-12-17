#include "combat_dust_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/systems/camera_visibility_service.h"
#include "../../game/systems/projectile_system.h"
#include "../../game/systems/stone_projectile.h"
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

constexpr float kFlameRadius = 3.0F;
constexpr float kFlameIntensity = 0.8F;
constexpr float kFlameYOffset = 0.5F;
constexpr float kFlameColorR = 1.0F;
constexpr float kFlameColorG = 0.4F;
constexpr float kFlameColorB = 0.1F;
constexpr float kBuildingHealthThreshold = 0.5F;

constexpr float kStoneImpactRadius = 4.0F;
constexpr float kStoneImpactIntensity = 1.2F;
constexpr float kStoneImpactColorR = 0.85F;
constexpr float kStoneImpactColorG = 0.75F;
constexpr float kStoneImpactColorB = 0.55F;
constexpr float kStoneImpactYOffset = 0.15F;
constexpr float kStoneImpactThreshold = 0.95F;
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

    if (!attack->in_melee_lock) {
      continue;
    }

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

  auto buildings = world->get_entities_with<Engine::Core::BuildingComponent>();

  for (auto *building : buildings) {
    if (building->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *transform =
        building->get_component<Engine::Core::TransformComponent>();
    auto *unit_comp = building->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || unit_comp == nullptr) {
      continue;
    }

    if (unit_comp->health <= 0) {
      continue;
    }

    float health_ratio = static_cast<float>(unit_comp->health) /
                         static_cast<float>(unit_comp->max_health);

    if (health_ratio > kBuildingHealthThreshold) {
      continue;
    }

    if (!visibility.is_entity_visible(transform->position.x,
                                      transform->position.z,
                                      kVisibilityCheckRadius)) {
      continue;
    }

    float flame_intensity = kFlameIntensity * (1.0F - health_ratio);

    QVector3D position(transform->position.x, kFlameYOffset,
                       transform->position.z);
    QVector3D color(kFlameColorR, kFlameColorG, kFlameColorB);

    renderer->building_flame(position, color, kFlameRadius, flame_intensity,
                             animation_time);
  }

  auto *projectile_sys = world->get_system<Game::Systems::ProjectileSystem>();
  if (projectile_sys != nullptr) {
    const auto &projectiles = projectile_sys->projectiles();

    for (const auto &projectile : projectiles) {
      if (!projectile->is_active()) {
        continue;
      }

      auto *stone_proj = dynamic_cast<const Game::Systems::StoneProjectile *>(
          projectile.get());
      if (stone_proj == nullptr) {
        continue;
      }

      float progress = stone_proj->get_progress();
      if (progress < kStoneImpactThreshold) {
        continue;
      }

      const QVector3D delta = stone_proj->get_end() - stone_proj->get_start();
      QVector3D impact_pos =
          stone_proj->get_start() + delta * stone_proj->get_progress();

      if (!visibility.is_entity_visible(impact_pos.x(), impact_pos.z(),
                                        kVisibilityCheckRadius)) {
        continue;
      }

      QVector3D position(impact_pos.x(),
                         stone_proj->get_end().y() + kStoneImpactYOffset,
                         impact_pos.z());
      QVector3D color(kStoneImpactColorR, kStoneImpactColorG,
                      kStoneImpactColorB);

      renderer->building_flame(position, color, kStoneImpactRadius,
                               kStoneImpactIntensity, animation_time);
    }
  }
}

} // namespace Render::GL
