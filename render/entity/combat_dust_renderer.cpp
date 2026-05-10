#include "combat_dust_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/camera_visibility_service.h"
#include "../../game/systems/projectile_system.h"
#include "../../game/systems/stone_projectile.h"
#include "../scene_renderer.h"
#include <algorithm>
#include <unordered_set>

namespace Render::GL {

namespace {
constexpr float k_dust_radius = 2.0F;
constexpr float k_dust_intensity = 0.6F;
constexpr float k_dust_y_offset = 0.05F;
constexpr float k_dust_color_r = 0.6F;
constexpr float k_dust_color_g = 0.55F;
constexpr float k_dust_color_b = 0.45F;
constexpr float k_visibility_check_radius = 3.0F;

constexpr float k_flame_radius = 3.0F;
constexpr float k_flame_intensity = 0.8F;
constexpr float k_flame_y_offset = 0.5F;
constexpr float k_flame_color_r = 1.0F;
constexpr float k_flame_color_g = 0.4F;
constexpr float k_flame_color_b = 0.1F;
constexpr float k_building_health_threshold = 0.5F;

constexpr float k_stone_impact_radius = 0.6F;
constexpr float k_elephant_stomp_impact_radius = 0.52F;
constexpr float k_stone_impact_intensity = 1.5F;
constexpr float k_stone_impact_color_r = 0.75F;
constexpr float k_stone_impact_color_g = 0.65F;
constexpr float k_stone_impact_color_b = 0.45F;
constexpr float k_stone_impact_y_offset = 0.1F;
constexpr float k_stone_impact_duration = 10.0F;
constexpr float k_stone_impact_trigger_progress = 0.99F;

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
  effect.duration = k_stone_impact_duration;
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
  auto &fog_of_war = Game::Map::VisibilityService::instance();
  Game::Map::VisibilityService::Snapshot fog_snapshot;
  if (fog_of_war.is_initialized()) {
    fog_snapshot = fog_of_war.snapshot();
  }
  auto is_fog_visible = [&fog_snapshot](float world_x, float world_z) -> bool {
    return fog_snapshot.is_visible_world(world_x, world_z);
  };

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

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(transform->position.x,
                                      transform->position.z,
                                      k_visibility_check_radius)) {
      continue;
    }

    QVector3D position(transform->position.x, k_dust_y_offset,
                       transform->position.z);
    QVector3D color(k_dust_color_r, k_dust_color_g, k_dust_color_b);

    renderer->combat_dust(position, color, k_dust_radius, k_dust_intensity,
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

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(transform->position.x,
                                      transform->position.z,
                                      k_visibility_check_radius)) {
      continue;
    }

    QVector3D position(transform->position.x, k_dust_y_offset,
                       transform->position.z);
    QVector3D color(k_dust_color_r, k_dust_color_g, k_dust_color_b);

    renderer->combat_dust(position, color, k_dust_radius, k_dust_intensity,
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

    if (health_ratio > k_building_health_threshold) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(transform->position.x,
                                      transform->position.z,
                                      k_visibility_check_radius)) {
      continue;
    }

    float flame_intensity = k_flame_intensity * (1.0F - health_ratio);

    QVector3D position(transform->position.x, k_flame_y_offset,
                       transform->position.z);
    QVector3D color(k_flame_color_r, k_flame_color_g, k_flame_color_b);

    renderer->building_flame(position, color, k_flame_radius, flame_intensity,
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
      if (progress < k_stone_impact_trigger_progress) {
        continue;
      }

      const void *proj_ptr = static_cast<const void *>(stone_proj);
      if (g_tracked_projectiles.find(proj_ptr) != g_tracked_projectiles.end()) {
        continue;
      }

      g_tracked_projectiles.insert(proj_ptr);

      QVector3D impact_pos = stone_proj->get_end();

      if (!is_fog_visible(impact_pos.x(), impact_pos.z())) {
        continue;
      }

      if (!visibility.is_entity_visible(impact_pos.x(), impact_pos.z(),
                                        k_visibility_check_radius * 2.0F)) {
        continue;
      }

      QVector3D position(impact_pos.x(),
                         impact_pos.y() + k_stone_impact_y_offset,
                         impact_pos.z());

      impact_tracker.add_impact(position, animation_time,
                                k_elephant_stomp_impact_radius,
                                k_stone_impact_intensity);
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

  auto elephants =
      world->get_entities_with<Engine::Core::ElephantStompImpactComponent>();
  for (auto *elephant : elephants) {
    auto *stomp_impact =
        elephant->get_component<Engine::Core::ElephantStompImpactComponent>();
    auto *transform =
        elephant->get_component<Engine::Core::TransformComponent>();

    if (stomp_impact == nullptr || transform == nullptr) {
      continue;
    }

    for (auto &impact : stomp_impact->impacts) {
      if (impact.time < 0.0F) {
        continue;
      }

      QVector3D position(
          impact.x, transform->position.y + k_stone_impact_y_offset, impact.z);

      if (!is_fog_visible(position.x(), position.z())) {
        impact.time = -1.0F;
        continue;
      }

      if (!visibility.is_entity_visible(position.x(), position.z(),
                                        k_visibility_check_radius * 2.0F)) {
        impact.time = -1.0F;
        continue;
      }

      impact_tracker.add_impact(position, animation_time,
                                k_elephant_stomp_impact_radius,
                                k_stone_impact_intensity);
      impact.time = -1.0F;
    }

    stomp_impact->impacts.erase(
        std::remove_if(
            stomp_impact->impacts.begin(), stomp_impact->impacts.end(),
            [](const Engine::Core::ElephantStompImpactComponent::ImpactRecord
                   &impact) { return impact.time < 0.0F; }),
        stomp_impact->impacts.end());
  }

  impact_tracker.update(animation_time);

  QVector3D color(k_stone_impact_color_r, k_stone_impact_color_g,
                  k_stone_impact_color_b);
  for (const auto &impact : impact_tracker.impacts()) {
    if (!is_fog_visible(impact.position.x(), impact.position.z())) {
      continue;
    }

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
