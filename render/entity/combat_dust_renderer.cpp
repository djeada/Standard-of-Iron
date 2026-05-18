#include "combat_dust_renderer.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../../game/map/render_visibility_rules.h"
#include "../../game/map/visibility_service.h"
#include "../../game/systems/camera_visibility_service.h"
#include "../../game/systems/combat_rules.h"
#include "../../game/systems/projectile_system.h"
#include "../../game/systems/stone_projectile.h"
#include "../combat_dust_defaults.h"
#include "../scene_renderer.h"

namespace Render::GL {

namespace {
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

constexpr float k_blood_y_offset = 0.02F;

constexpr float k_stone_impact_radius = 0.6F;
constexpr float k_elephant_stomp_impact_radius = 0.52F;
constexpr float k_stone_impact_intensity = 1.5F;
constexpr float k_stone_impact_color_r = 0.75F;
constexpr float k_stone_impact_color_g = 0.65F;
constexpr float k_stone_impact_color_b = 0.45F;
constexpr float k_stone_impact_y_offset = 0.1F;
constexpr float k_stone_impact_duration = 10.0F;
constexpr float k_stone_impact_trigger_progress = 0.99F;

auto blood_alpha_scale(float elapsed_time, float lifetime) -> float {
  if (lifetime <= 0.0F) {
    return 0.0F;
  }

  float const fade_window = std::max(1.0F, lifetime * 0.25F);
  float const remaining_time = lifetime - elapsed_time;
  if (remaining_time >= fade_window) {
    return 1.0F;
  }
  return std::clamp(remaining_time / fade_window, 0.0F, 1.0F);
}

std::unordered_set<const void*> g_tracked_projectiles;
} // namespace

auto StoneImpactTracker::instance() -> StoneImpactTracker& {
  static StoneImpactTracker instance;
  return instance;
}

void StoneImpactTracker::add_impact(const QVector3D& position,
                                    float current_time,
                                    float radius,
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
  m_impacts.erase(std::remove_if(m_impacts.begin(),
                                 m_impacts.end(),
                                 [current_time](const StoneImpactEffect& impact) {
                                   return (current_time - impact.start_time) >
                                          impact.duration;
                                 }),
                  m_impacts.end());
}

void render_combat_dust(Renderer* renderer,
                        ResourceManager*,
                        Engine::Core::World* world) {
  if (renderer == nullptr || world == nullptr) {
    return;
  }

  float const animation_time = renderer->get_animation_time();
  auto& visibility = Game::Systems::CameraVisibilityService::instance();
  auto& fog_of_war = Game::Map::VisibilityService::instance();
  auto fog_snapshot = fog_of_war.is_initialized() ? fog_of_war.snapshot_ptr() : nullptr;
  auto is_fog_visible = [&fog_snapshot](float world_x, float world_z) -> bool {
    return fog_snapshot == nullptr ||
           Game::Map::should_render_combat_effect(*fog_snapshot, world_x, world_z);
  };

  auto units = world->get_entities_with<Engine::Core::AttackComponent>();

  for (auto* unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = unit->get_component<Engine::Core::TransformComponent>();
    auto* attack = unit->get_component<Engine::Core::AttackComponent>();
    auto* unit_comp = unit->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || attack == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    if (!attack->in_melee_lock ||
        !Game::Systems::CombatRules::participates_in_rts_melee_lock(unit)) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    QVector3D const position(
        transform->position.x, k_dust_y_offset, transform->position.z);
    QVector3D const color(k_dust_color_r, k_dust_color_g, k_dust_color_b);

    renderer->combat_dust(position,
                          color,
                          CombatDustDefaults::k_radius,
                          CombatDustDefaults::k_intensity,
                          animation_time);
  }

  auto builders = world->get_entities_with<Engine::Core::BuilderProductionComponent>();

  for (auto* builder : builders) {
    if (builder->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = builder->get_component<Engine::Core::TransformComponent>();
    auto* production =
        builder->get_component<Engine::Core::BuilderProductionComponent>();
    auto* unit_comp = builder->get_component<Engine::Core::UnitComponent>();

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

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    QVector3D const position(
        transform->position.x, k_dust_y_offset, transform->position.z);
    QVector3D const color(k_dust_color_r, k_dust_color_g, k_dust_color_b);

    renderer->combat_dust(position,
                          color,
                          CombatDustDefaults::k_radius,
                          CombatDustDefaults::k_intensity,
                          animation_time);
  }

  auto buildings = world->get_entities_with<Engine::Core::BuildingComponent>();

  for (auto* building : buildings) {
    if (building->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = building->get_component<Engine::Core::TransformComponent>();
    auto* unit_comp = building->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || unit_comp == nullptr) {
      continue;
    }

    if (unit_comp->health <= 0) {
      continue;
    }

    float const health_ratio = static_cast<float>(unit_comp->health) /
                               static_cast<float>(unit_comp->max_health);

    if (health_ratio > k_building_health_threshold) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, k_visibility_check_radius)) {
      continue;
    }

    float const flame_intensity = k_flame_intensity * (1.0F - health_ratio);

    QVector3D const position(
        transform->position.x, k_flame_y_offset, transform->position.z);
    QVector3D const color(k_flame_color_r, k_flame_color_g, k_flame_color_b);

    renderer->building_flame(
        position, color, k_flame_radius, flame_intensity, animation_time);
  }

  auto blood_stains = world->get_entities_with<Engine::Core::BloodStainComponent>();
  for (auto* blood_entity : blood_stains) {
    if (blood_entity == nullptr ||
        blood_entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* transform = blood_entity->get_component<Engine::Core::TransformComponent>();
    auto* blood_stain =
        blood_entity->get_component<Engine::Core::BloodStainComponent>();
    if (transform == nullptr || blood_stain == nullptr) {
      continue;
    }

    float const alpha_scale =
        blood_alpha_scale(blood_stain->elapsed_time, blood_stain->lifetime);
    if (alpha_scale <= 0.0F) {
      continue;
    }

    if (!is_fog_visible(transform->position.x, transform->position.z)) {
      continue;
    }

    float const visibility_radius =
        std::max(k_visibility_check_radius, blood_stain->radius);
    if (!visibility.is_entity_visible(
            transform->position.x, transform->position.z, visibility_radius)) {
      continue;
    }

    QVector3D const position(transform->position.x,
                             transform->position.y + k_blood_y_offset,
                             transform->position.z);
    renderer->blood_pool(position,
                         blood_stain->radius,
                         alpha_scale,
                         blood_stain->rotation,
                         blood_stain->aspect_ratio,
                         blood_stain->seed);
  }

  auto* projectile_sys = world->get_system<Game::Systems::ProjectileSystem>();
  auto& impact_tracker = StoneImpactTracker::instance();

  if (projectile_sys != nullptr) {
    const auto& projectiles = projectile_sys->projectiles();

    for (const auto& projectile : projectiles) {
      const auto* stone_proj =
          dynamic_cast<const Game::Systems::StoneProjectile*>(projectile.get());
      if (stone_proj == nullptr) {
        continue;
      }

      float const progress = stone_proj->get_progress();
      if (progress < k_stone_impact_trigger_progress) {
        continue;
      }

      const void* proj_ptr = static_cast<const void*>(stone_proj);
      if (g_tracked_projectiles.find(proj_ptr) != g_tracked_projectiles.end()) {
        continue;
      }

      g_tracked_projectiles.insert(proj_ptr);

      QVector3D const impact_pos = stone_proj->get_end();

      if (!is_fog_visible(impact_pos.x(), impact_pos.z())) {
        continue;
      }

      if (!visibility.is_entity_visible(
              impact_pos.x(), impact_pos.z(), k_visibility_check_radius * 2.0F)) {
        continue;
      }

      QVector3D const position(
          impact_pos.x(), impact_pos.y() + k_stone_impact_y_offset, impact_pos.z());

      impact_tracker.add_impact(position,
                                animation_time,
                                k_elephant_stomp_impact_radius,
                                k_stone_impact_intensity);
    }
  }

  std::erase_if(g_tracked_projectiles, [projectile_sys](const void* ptr) {
    if (projectile_sys == nullptr) {
      return true;
    }
    const auto& projectiles = projectile_sys->projectiles();
    for (const auto& p : projectiles) {
      if (static_cast<const void*>(p.get()) == ptr) {
        return false;
      }
    }
    return true;
  });

  auto elephants =
      world->get_entities_with<Engine::Core::ElephantStompImpactComponent>();
  for (auto* elephant : elephants) {
    auto* stomp_impact =
        elephant->get_component<Engine::Core::ElephantStompImpactComponent>();
    auto* transform = elephant->get_component<Engine::Core::TransformComponent>();

    if (stomp_impact == nullptr || transform == nullptr) {
      continue;
    }

    for (auto& impact : stomp_impact->impacts) {
      if (impact.time < 0.0F) {
        continue;
      }

      QVector3D const position(
          impact.x, transform->position.y + k_stone_impact_y_offset, impact.z);

      if (!is_fog_visible(position.x(), position.z())) {
        impact.time = -1.0F;
        continue;
      }

      if (!visibility.is_entity_visible(
              position.x(), position.z(), k_visibility_check_radius * 2.0F)) {
        impact.time = -1.0F;
        continue;
      }

      impact_tracker.add_impact(position,
                                animation_time,
                                k_elephant_stomp_impact_radius,
                                k_stone_impact_intensity);
      impact.time = -1.0F;
    }

    stomp_impact->impacts.erase(
        std::remove_if(
            stomp_impact->impacts.begin(),
            stomp_impact->impacts.end(),
            [](const Engine::Core::ElephantStompImpactComponent::ImpactRecord& impact) {
              return impact.time < 0.0F;
            }),
        stomp_impact->impacts.end());
  }

  impact_tracker.update(animation_time);

  QVector3D const color(
      k_stone_impact_color_r, k_stone_impact_color_g, k_stone_impact_color_b);
  for (const auto& impact : impact_tracker.impacts()) {
    if (!is_fog_visible(impact.position.x(), impact.position.z())) {
      continue;
    }

    if (!visibility.is_entity_visible(
            impact.position.x(), impact.position.z(), impact.radius)) {
      continue;
    }

    float const impact_time = animation_time - impact.start_time;

    renderer->stone_impact(
        impact.position, color, impact.radius, impact.intensity, impact_time);
  }
}

} // namespace Render::GL

namespace {

using Engine::Core::CombatAnimationState;

constexpr float k_telegraph_range = 7.5F;
constexpr float k_ring_y_offset = 0.025F;

constexpr QVector3D k_danger_red{1.0F, 0.12F, 0.03F};
constexpr QVector3D k_warning_orange{1.0F, 0.48F, 0.05F};
constexpr QVector3D k_stagger_cyan{0.2F, 0.85F, 1.0F};
constexpr QVector3D k_flash_white{1.0F, 0.75F, 0.35F};
constexpr QVector3D k_lock_gold{1.0F, 0.80F, 0.15F};
constexpr QVector3D k_lock_white{0.95F, 1.0F, 0.90F};

inline float pulse(float t, float hz, float phase = 0.0F) {
  return 0.5F + 0.5F * std::sin(t * 6.2832F * hz + phase);
}

inline void submit_ring(Render::GL::Renderer* renderer,
                        float px,
                        float py,
                        float pz,
                        float radius,
                        float alpha_inner,
                        float alpha_outer,
                        const QVector3D& color) {
  QMatrix4x4 model;
  model.translate(px, py + k_ring_y_offset, pz);
  model.scale(radius, 1.0F, radius);
  renderer->selection_ring(model, alpha_inner, alpha_outer, color);
}

} // namespace

namespace Render::GL {

void RpgTelegraphRenderer::clear() {
  m_cache.clear();
  m_strike_flashes.clear();
}

void RpgTelegraphRenderer::render(Renderer* renderer,
                                  Engine::Core::World* world,
                                  Engine::Core::EntityID commander_id,
                                  Engine::Core::EntityID locked_target_id,
                                  float anim_time) {
  using Engine::Core::CombatStateComponent;
  using Engine::Core::StaggerComponent;
  using Engine::Core::TransformComponent;

  auto* commander_ent = world->get_entity(commander_id);
  if (commander_ent == nullptr) {
    return;
  }
  auto* cmd_tf = commander_ent->get_component<TransformComponent>();
  if (cmd_tf == nullptr) {
    return;
  }
  const float cx = cmd_tf->position.x;
  const float cz = cmd_tf->position.z;

  std::vector<Engine::Core::EntityID> seen;
  seen.reserve(16);

  for (auto* entity : world->get_entities_with<CombatStateComponent>()) {
    const auto id = entity->get_id();
    if (id == commander_id) {
      continue;
    }
    auto* csc = entity->get_component<CombatStateComponent>();
    auto* tf = entity->get_component<TransformComponent>();
    if (csc == nullptr || tf == nullptr) {
      continue;
    }
    const float ex = tf->position.x;
    const float ez = tf->position.z;
    const float ey = tf->position.y;
    const float dx = ex - cx;
    const float dz = ez - cz;
    if (dx * dx + dz * dz > k_telegraph_range * k_telegraph_range) {
      continue;
    }

    const auto cur_state = csc->animation_state;
    auto it = m_cache.find(id);

    if (cur_state == CombatAnimationState::WindUp) {
      seen.push_back(id);
      if (it == m_cache.end()) {
        m_cache[id] = TelegraphEntry{ex, ez, ey, CombatAnimationState::WindUp};
      } else {

        constexpr float k_pos_epsilon = 0.01F;
        if (std::abs(it->second.last_pos_x - ex) > k_pos_epsilon ||
            std::abs(it->second.last_pos_z - ez) > k_pos_epsilon) {
          it->second.last_pos_x = ex;
          it->second.last_pos_z = ez;
          it->second.base_y = ey;
        }
        it->second.prev_state = CombatAnimationState::WindUp;
      }
    } else if (cur_state == CombatAnimationState::Strike && it != m_cache.end() &&
               it->second.prev_state == CombatAnimationState::WindUp) {

      m_strike_flashes.push_back(StrikeFlash{{ex, ey, ez}, anim_time});
      m_cache.erase(it);
    }
  }

  for (auto it = m_cache.begin(); it != m_cache.end();) {
    const bool still_seen =
        std::find(seen.begin(), seen.end(), it->first) != seen.end();
    if (!still_seen) {
      it = m_cache.erase(it);
    } else {
      ++it;
    }
  }

  m_strike_flashes.erase(std::remove_if(m_strike_flashes.begin(),
                                        m_strike_flashes.end(),
                                        [anim_time](const StrikeFlash& f) {
                                          return (anim_time - f.start_time) >=
                                                 StrikeFlash::k_duration;
                                        }),
                         m_strike_flashes.end());

  for (const auto& [id, entry] : m_cache) {
    auto* entity = world->get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    auto* csc = entity->get_component<CombatStateComponent>();
    if (csc == nullptr) {
      continue;
    }
    const float progress = std::clamp(
        csc->state_time / CombatStateComponent::k_wind_up_duration, 0.0F, 1.0F);

    const float inner_r = 0.55F + 0.33F * progress;

    constexpr float outer_r = 1.20F;

    const float inner_alpha = 0.55F + 0.20F * pulse(anim_time, 4.0F);

    const float outer_alpha = 0.20F + 0.10F * pulse(anim_time, 2.0F, 1.047F);

    submit_ring(renderer,
                entry.last_pos_x,
                entry.base_y,
                entry.last_pos_z,
                inner_r,
                inner_alpha,
                inner_alpha * 0.55F,
                k_danger_red);
    submit_ring(renderer,
                entry.last_pos_x,
                entry.base_y,
                entry.last_pos_z,
                outer_r,
                outer_alpha,
                outer_alpha * 0.4F,
                k_warning_orange);
  }

  for (auto* entity : world->get_entities_with<StaggerComponent>()) {
    if (entity->get_id() == commander_id) {
      continue;
    }
    auto* tf = entity->get_component<TransformComponent>();
    auto* sc = entity->get_component<StaggerComponent>();
    if (tf == nullptr || sc == nullptr) {
      continue;
    }
    const float ex = tf->position.x;
    const float ez = tf->position.z;
    const float dx = ex - cx;
    const float dz = ez - cz;
    if (dx * dx + dz * dz > k_telegraph_range * k_telegraph_range) {
      continue;
    }

    const float fade = std::clamp(sc->remaining / 0.5F, 0.0F, 1.0F);
    const float stagger_alpha = 0.55F * fade * (0.7F + 0.3F * pulse(anim_time, 6.0F));
    submit_ring(renderer,
                ex,
                tf->position.y,
                ez,
                0.70F,
                stagger_alpha,
                stagger_alpha * 0.35F,
                k_stagger_cyan);
  }

  for (const auto& flash : m_strike_flashes) {
    const float elapsed = anim_time - flash.start_time;
    const float t = elapsed / StrikeFlash::k_duration;

    const float flash_r = 0.8F + 0.6F * t;
    const float flash_alpha = (1.0F - t) * 0.75F;
    submit_ring(renderer,
                flash.pos.x(),
                flash.pos.y(),
                flash.pos.z(),
                flash_r,
                flash_alpha,
                flash_alpha * 0.3F,
                k_flash_white);
  }

  if (locked_target_id != 0) {
    auto* lock_ent = world->get_entity(locked_target_id);
    if (lock_ent != nullptr) {
      auto* lock_tf = lock_ent->get_component<TransformComponent>();
      auto* lock_unit = lock_ent->get_component<Engine::Core::UnitComponent>();
      if (lock_tf != nullptr && lock_unit != nullptr && lock_unit->health > 0) {
        const float lx = lock_tf->position.x;
        const float lz = lock_tf->position.z;
        const float ly = lock_tf->position.y;
        const float lock_alpha = 0.55F + 0.20F * pulse(anim_time, 3.0F);
        const float lock_outer = 0.22F + 0.08F * pulse(anim_time, 3.0F, 1.047F);
        submit_ring(
            renderer, lx, ly, lz, 0.90F, lock_alpha, lock_alpha * 0.40F, k_lock_gold);
        submit_ring(
            renderer, lx, ly, lz, 1.10F, lock_outer, lock_outer * 0.30F, k_lock_white);
      }
    }
  }
}

} // namespace Render::GL
