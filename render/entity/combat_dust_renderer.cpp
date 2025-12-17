#include "combat_dust_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/systems/camera_visibility_service.h"
#include "../../game/systems/projectile_system.h"
#include "../../game/systems/stone_projectile.h"
#include "../scene_renderer.h"
#include <algorithm>
#include <unordered_set>

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

constexpr float kStoneImpactRadius = 0.6F;
constexpr float kStoneImpactIntensity = 1.5F;
constexpr float kStoneImpactColorR = 0.75F;
constexpr float kStoneImpactColorG = 0.65F;
constexpr float kStoneImpactColorB = 0.45F;
constexpr float kStoneImpactYOffset = 0.1F;
constexpr float kStoneImpactDuration = 10.0F;
constexpr float kStoneImpactTriggerProgress = 0.99F;

std::unordered_set<const void *> g_tracked_projectiles;
} // namespace

auto StoneImpactTracker::instance() -> StoneImpactTracker & {
  static StoneImpactTracker instance;
  return instance;
}

void StoneImpactTracker::add_impact(const QVector3D &position,
                                    float current_time, float radius,
                                    float intensity) {
  StoneImpactEffect effect;
  effect.position = position;
  effect.start_time = current_time;
  effect.duration = kStoneImpactDuration;
  effect.radius = radius;
  effect.intensity = intensity;
  m_impacts.push_back(effect);
}

void StoneImpactTracker::update(float current_time) {
  m_impacts.erase(
      std::remove_if(m_impacts.begin(), m_impacts.end(),
                     [current_time](const StoneImpactEffect &impact) {
                       return (current_time - impact.start_time) >
                              impact.duration;
                     }),
      m_impacts.end());
}

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

  auto builders =
      world->get_entities_with<Engine::Core::BuilderProductionComponent>();

  for (auto *builder : builders) {
    if (builder->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *transform =
        builder->get_component<Engine::Core::TransformComponent>();
    auto *production =
        builder->get_component<Engine::Core::BuilderProductionComponent>();
    auto *unit_comp = builder->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || production == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!production->in_progress) {
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
  auto &impact_tracker = StoneImpactTracker::instance();

  if (projectile_sys != nullptr) {
    const auto &projectiles = projectile_sys->projectiles();

    for (const auto &projectile : projectiles) {
      auto *stone_proj = dynamic_cast<const Game::Systems::StoneProjectile *>(
          projectile.get());
      if (stone_proj == nullptr) {
        continue;
      }

      float progress = stone_proj->get_progress();
      if (progress < kStoneImpactTriggerProgress) {
        continue;
      }

      const void *proj_ptr = static_cast<const void *>(stone_proj);
      if (g_tracked_projectiles.find(proj_ptr) != g_tracked_projectiles.end()) {
        continue;
      }

      g_tracked_projectiles.insert(proj_ptr);

      QVector3D impact_pos = stone_proj->get_end();

      if (!visibility.is_entity_visible(impact_pos.x(), impact_pos.z(),
                                        kVisibilityCheckRadius * 2.0F)) {
        continue;
      }

      QVector3D position(impact_pos.x(), impact_pos.y() + kStoneImpactYOffset,
                         impact_pos.z());

      impact_tracker.add_impact(position, animation_time, kStoneImpactRadius,
                                kStoneImpactIntensity);
    }
  }

  std::erase_if(g_tracked_projectiles, [projectile_sys](const void *ptr) {
    if (projectile_sys == nullptr) {
      return true;
    }
    const auto &projectiles = projectile_sys->projectiles();
    for (const auto &p : projectiles) {
      if (static_cast<const void *>(p.get()) == ptr) {
        return false;
      }
    }
    return true;
  });

  impact_tracker.update(animation_time);

  QVector3D color(kStoneImpactColorR, kStoneImpactColorG, kStoneImpactColorB);
  for (const auto &impact : impact_tracker.impacts()) {
    if (!visibility.is_entity_visible(impact.position.x(), impact.position.z(),
                                      impact.radius)) {
      continue;
    }

    float impact_time = animation_time - impact.start_time;

    renderer->stone_impact(impact.position, color, impact.radius,
                           impact.intensity, impact_time);
  }
}

} // namespace Render::GL
