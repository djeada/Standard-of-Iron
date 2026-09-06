#include "projectile_system.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <map>

#include "../core/ambient_session.h"
#include "../core/component_gameplay.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "arrow_projectile.h"
#include "combat_system/combat_hit_resolver.h"
#include "stone_projectile.h"

namespace Game::Systems {

namespace {

constexpr float k_min_progress_for_impact = 1.0F - 1.0e-5F;

auto launch_cue_for_kind(ProjectileKind kind,
                         bool is_ballista_bolt,
                         ArrowVisualStyle style = ArrowVisualStyle::Focused) -> const
    char* {
  if (is_ballista_bolt) {
    return "combat.siege_launch";
  }
  switch (kind) {
  case ProjectileKind::Stone:
  case ProjectileKind::FlamingStone:
    return "combat.siege_launch";
  default:

    return style == ArrowVisualStyle::Aimed ||
                   style == ArrowVisualStyle::CommanderSignature
               ? "combat.bow_loose_heavy"
               : "combat.arrow_launch";
  }
}

auto impact_cue_for_kind(ProjectileKind kind, bool is_ballista_bolt) -> const char* {
  if (is_ballista_bolt) {
    return "combat.siege_impact";
  }
  switch (kind) {
  case ProjectileKind::Stone:
  case ProjectileKind::FlamingStone:
    return "combat.siege_impact";
  default:
    return "combat.arrow_flyby";
  }
}
constexpr float k_projectile_escape_radius = 1.5F;

auto target_escaped_impact(const Engine::Core::Entity* target,
                           Projectile* projectile) -> bool {
  if (target == nullptr || projectile == nullptr) {
    return true;
  }

  const auto* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  QVector3D const current_pos(
      target_transform->position.x, 0.0F, target_transform->position.z);
  QVector3D locked_pos = projectile->get_target_locked_position();
  locked_pos.setY(0.0F);
  return (current_pos - locked_pos).length() > k_projectile_escape_radius;
}

[[nodiscard]] auto apply_projectile_damage(Engine::Core::World* world,
                                           Engine::Core::Entity* target,
                                           int damage,
                                           Engine::Core::EntityID attacker_id,
                                           const QVector3D& contact_point,
                                           ProjectileKind projectile_kind) -> bool {
  if (world == nullptr || target == nullptr || damage <= 0) {
    return false;
  }

  auto const result = Game::Systems::Combat::resolve_projectile_impact_hit(
      world,
      {.contact = {.attacker_id = attacker_id,
                   .target_id = target->get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = Engine::Core::CombatAttackFamily::Bow,
                   .attack_direction = Engine::Core::AttackDirection::Thrust,
                   .contact_point = contact_point,
                   .from_projectile = true,
                   .projectile_kind = projectile_kind},
       .explicit_raw_damage = damage});
  return result.applied;
}

[[nodiscard]] auto impact_lifetime(ProjectileKind kind, bool ballista_bolt) -> float {
  if (ballista_bolt) {
    return 0.90F;
  }
  switch (kind) {
  case ProjectileKind::Fireball:
    return 1.10F;
  case ProjectileKind::Stone:
    return 1.25F;
  case ProjectileKind::FlamingStone:
    return 1.45F;
  case ProjectileKind::CursedArrow:
    return 0.80F;
  case ProjectileKind::Arrow:
  default:
    return 0.65F;
  }
}

[[nodiscard]] auto leaves_a_shaft(ProjectileKind kind) -> bool {
  switch (kind) {
  case ProjectileKind::Arrow:
  case ProjectileKind::CursedArrow:
    return true;
  case ProjectileKind::Fireball:
  case ProjectileKind::Stone:
  case ProjectileKind::FlamingStone:
    return false;
  }
  return false;
}

[[nodiscard]] auto scatter_unit(std::uint64_t seed) -> float {
  std::uint64_t value = seed * 0x9e3779b97f4a7c15ULL;
  value ^= value >> 29;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 32;
  return static_cast<float>(value & 0xffffffULL) / 16777215.0F;
}

[[nodiscard]] auto planted_direction(const QVector3D& incoming,
                                     std::uint64_t seed) -> QVector3D {
  QVector3D flat(incoming.x(), 0.0F, incoming.z());
  if (flat.lengthSquared() < 1.0e-6F) {
    float const angle = scatter_unit(seed) * 6.2831853F;
    flat = QVector3D(std::cos(angle), 0.0F, std::sin(angle));
  }
  flat.normalize();
  float const yaw_jitter = (scatter_unit(seed ^ 0x5bf03635ULL) - 0.5F) * 0.9F;
  QVector3D const jittered(
      flat.x() * std::cos(yaw_jitter) - flat.z() * std::sin(yaw_jitter),
      0.0F,
      flat.x() * std::sin(yaw_jitter) + flat.z() * std::cos(yaw_jitter));

  constexpr float k_min_plant_slope = 0.80F;
  constexpr float k_max_plant_slope = 1.90F;
  float const slope = k_min_plant_slope + (k_max_plant_slope - k_min_plant_slope) *
                                              scatter_unit(seed ^ 0x27d4eb2fULL);
  return QVector3D(jittered.x(), -slope, jittered.z()).normalized();
}

[[nodiscard]] auto
valid_direct_target(Engine::Core::World* world,
                    Projectile* projectile) -> Engine::Core::Entity* {
  if (world == nullptr || projectile == nullptr || projectile->get_target_id() == 0) {
    return nullptr;
  }
  auto* target = world->get_entity(projectile->get_target_id());
  if (target == nullptr ||
      target->has_component<Engine::Core::PendingRemovalComponent>() ||
      target_escaped_impact(target, projectile)) {
    return nullptr;
  }
  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return nullptr;
  }
  return target;
}

} // namespace

ProjectileSystem::ProjectileSystem()
    : m_arrow_config(GameConfig::instance().arrow()) {
}

void ProjectileSystem::spawn_arrow(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   bool is_ballista_bolt,
                                   ProjectileKind kind,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id,
                                   float impact_radius,
                                   float splash_damage_multiplier,
                                   bool friendly_fire,
                                   ArrowVisualStyle visual_style,
                                   std::optional<QVector3D> target_origin_at_launch,
                                   float arc_scale,
                                   float visual_power) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  float arc_height = NAN;
  if (is_ballista_bolt) {
    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist * 0.4F,
                            m_arrow_config.arc_height_min * 0.5F,
                            m_arrow_config.arc_height_max * 0.6F);
  } else {
    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist,
                            m_arrow_config.arc_height_min,
                            m_arrow_config.arc_height_max);
  }
  arc_height *= std::max(0.0F, arc_scale);
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;
  auto visual_profile =
      make_arrow_visual_profile(visual_style, ++m_arrow_spawn_sequence);

  float const power = std::clamp(visual_power, 0.0F, 1.5F);
  visual_profile.scale *= 0.74F + (0.26F * power);
  visual_profile.brightness *= 0.80F + (0.20F * power);
  visual_profile.trail_alpha *= 0.56F + (0.44F * power);

  m_projectiles.push_back(
      std::make_unique<ArrowProjectile>(start,
                                        end,
                                        color,
                                        speed,
                                        arc_height,
                                        inv_dist,
                                        is_ballista_bolt,
                                        kind,
                                        should_apply_damage,
                                        damage,
                                        attacker_id,
                                        target_id,
                                        impact_radius,
                                        splash_damage_multiplier,
                                        friendly_fire,
                                        visual_style,
                                        visual_profile,
                                        target_origin_at_launch.value_or(end)));

  bool const counts_toward_volley = !is_ballista_bolt &&
                                    kind != ProjectileKind::Stone &&
                                    kind != ProjectileKind::FlamingStone;
  m_pending_launch_cues.push_back(
      {.cue_id = launch_cue_for_kind(kind, is_ballista_bolt, visual_style),
       .attacker_id = attacker_id,
       .counts_toward_volley = counts_toward_volley});
}

void ProjectileSystem::spawn_stone(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   float scale,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id,
                                   std::optional<QVector3D> target_origin_at_launch,
                                   ProjectileKind kind) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  constexpr float k_stone_arc_multiplier = 0.35F;
  constexpr float k_stone_arc_min = 1.0F;
  constexpr float k_stone_arc_max = 4.0F;
  float const arc_height =
      std::clamp(k_stone_arc_multiplier * dist, k_stone_arc_min, k_stone_arc_max);
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(
      std::make_unique<StoneProjectile>(start,
                                        end,
                                        color,
                                        speed,
                                        arc_height,
                                        inv_dist,
                                        scale,
                                        should_apply_damage,
                                        damage,
                                        attacker_id,
                                        target_id,
                                        target_origin_at_launch.value_or(end),
                                        kind));

  m_pending_launch_cues.push_back({.cue_id = launch_cue_for_kind(kind, false),
                                   .attacker_id = attacker_id,
                                   .counts_toward_volley = false});
}

void ProjectileSystem::flush_launch_cues(Engine::Core::World* world) {

  std::map<int, int> arrows_launched_by_owner;
  for (const auto& pending : m_pending_launch_cues) {
    const auto* attacker =
        world != nullptr
            ? world->try_get<Engine::Core::UnitComponent>(pending.attacker_id)
            : nullptr;
    int const owner_id = attacker != nullptr ? attacker->owner_id : 0;
    Engine::Core::EventManager::instance().publish(
        Engine::Core::AudioCueEvent::for_owner(owner_id, pending.cue_id));
    if (pending.counts_toward_volley) {
      ++arrows_launched_by_owner[owner_id];
    }
  }
  m_pending_launch_cues.clear();

  for (const auto& [owner_id, launched] : arrows_launched_by_owner) {
    if (launched >= k_volley_arrow_threshold) {
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AudioCueEvent::for_owner(owner_id, "combat.arrow_volley"));
    }
  }
}

void ProjectileSystem::update(Engine::Core::World* world, float delta_time) {
  flush_launch_cues(world);

  for (auto& impact : m_impacts) {
    impact.age += std::max(0.0F, delta_time);
  }
  std::erase_if(m_impacts,
                [](auto const& impact) { return impact.age >= impact.lifetime; });

  for (auto& spent : m_spent) {
    spent.age += std::max(0.0F, delta_time);
  }
  std::erase_if(m_spent, [](auto const& spent) { return spent.age >= spent.lifetime; });

  for (auto& projectile : m_projectiles) {
    if (projectile == nullptr || !projectile->is_active()) {
      continue;
    }
    projectile->update(delta_time);
    if (projectile->get_progress() >= k_min_progress_for_impact) {
      auto const resolution = resolve_impact(world, projectile.get());
      publish_impact(world, *projectile, resolution);
      projectile->deactivate();
    }
  }

  m_projectiles.erase(
      std::remove_if(m_projectiles.begin(),
                     m_projectiles.end(),
                     [](const ProjectilePtr& p) { return !p->is_active(); }),
      m_projectiles.end());
}

auto ProjectileSystem::resolve_impact(Engine::Core::World* world,
                                      Projectile* projectile) -> ImpactResolution {
  ImpactResolution resolution;
  if (projectile == nullptr || world == nullptr) {
    return resolution;
  }

  if (auto* arrow_projectile = dynamic_cast<ArrowProjectile*>(projectile);
      arrow_projectile != nullptr) {
    if (arrow_projectile->get_kind() == ProjectileKind::Fireball) {
      resolution.hit_target = valid_direct_target(world, projectile) != nullptr;
      if (projectile->should_apply_damage()) {
        auto const area_result =
            Game::Systems::Combat::resolve_projectile_area_impact_hit(
                world,
                {.attacker_id = projectile->get_attacker_id(),
                 .primary_target_id = projectile->get_target_id(),
                 .impact_point = projectile->get_end(),
                 .projectile_kind = arrow_projectile->get_kind(),
                 .explicit_raw_damage = projectile->get_damage(),
                 .radius = arrow_projectile->impact_radius(),
                 .splash_damage_multiplier =
                     arrow_projectile->splash_damage_multiplier(),
                 .friendly_fire = arrow_projectile->friendly_fire()});
        resolution.damage_applied =
            area_result.applied_hits > 0 || area_result.fire_patch_spawned;
      }
      return resolution;
    }
  }

  auto* target = valid_direct_target(world, projectile);
  resolution.hit_target = target != nullptr;
  if (target != nullptr && projectile->should_apply_damage()) {
    resolution.damage_applied = apply_projectile_damage(world,
                                                        target,
                                                        projectile->get_damage(),
                                                        projectile->get_attacker_id(),
                                                        projectile->get_end(),
                                                        projectile->get_kind());
    if (resolution.damage_applied) {
      confirm_commander_hit(world, projectile->get_attacker_id(), *target);
    }
  }
  return resolution;
}

void ProjectileSystem::confirm_commander_hit(Engine::Core::World* world,
                                             Engine::Core::EntityID attacker_id,
                                             const Engine::Core::Entity& target) {
  if (world == nullptr || attacker_id == 0) {
    return;
  }
  auto* attacker = world->get_entity(attacker_id);
  if (attacker == nullptr) {
    return;
  }
  auto const* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled) {
    return;
  }
  auto* targets =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
          attacker);
  if (targets == nullptr) {
    return;
  }
  auto const* target_unit = target.get_component<Engine::Core::UnitComponent>();
  targets->recent_hit_target_id = target.get_id();
  targets->recent_hit_soldier_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  targets->recent_hit_timer = 0.28F;
  targets->recent_hit_killed = target_unit != nullptr && target_unit->health <= 0;
  ++targets->hit_confirm_sequence;
}

void ProjectileSystem::publish_impact(Engine::Core::World* world,
                                      const Projectile& projectile,
                                      const ImpactResolution& resolution) {
  bool ballista_bolt = false;
  bool aimed_shot = false;
  if (auto const* arrow = dynamic_cast<const ArrowProjectile*>(&projectile)) {
    ballista_bolt = arrow->is_ballista_bolt();
    aimed_shot = arrow->visual_style() == ArrowVisualStyle::Aimed;
  }
  QVector3D incoming_direction = projectile.get_end() - projectile.get_start();
  if (incoming_direction.lengthSquared() > 1.0e-6F) {
    incoming_direction.normalize();
  } else {
    incoming_direction = QVector3D(0.0F, -1.0F, 0.0F);
  }
  m_impacts.push_back({
      .sequence = ++m_impact_sequence,
      .position = projectile.get_end(),
      .incoming_direction = incoming_direction,
      .color = projectile.get_color(),
      .kind = projectile.get_kind(),
      .age = 0.0F,
      .lifetime =
          aimed_shot ? 0.95F : impact_lifetime(projectile.get_kind(), ballista_bolt),
      .scale = projectile.get_scale(),
      .ballista_bolt = ballista_bolt,
      .aimed_shot = aimed_shot,
      .hit_target = resolution.hit_target,
      .damage_applied = resolution.damage_applied,
      .attacker_id = projectile.get_attacker_id(),
      .target_id = projectile.get_target_id(),
  });

  const auto* impact_attacker =
      world != nullptr
          ? world->try_get<Engine::Core::UnitComponent>(projectile.get_attacker_id())
          : nullptr;
  const auto* impact_target =
      world != nullptr
          ? world->try_get<Engine::Core::UnitComponent>(projectile.get_target_id())
          : nullptr;
  int const impact_owner_id =
      impact_attacker != nullptr
          ? impact_attacker->owner_id
          : (impact_target != nullptr ? impact_target->owner_id : 0);
  Engine::Core::EventManager::instance().publish(Engine::Core::AudioCueEvent::for_owner(
      impact_owner_id,
      (aimed_shot && resolution.hit_target)
          ? "combat.hit.arrow"
          : impact_cue_for_kind(projectile.get_kind(), ballista_bolt)));

  record_spent_projectile(world, projectile, incoming_direction, ballista_bolt);
}

void ProjectileSystem::record_spent_projectile(Engine::Core::World* world,
                                               const Projectile& projectile,
                                               const QVector3D& incoming_direction,
                                               bool ballista_bolt) {
  if (!leaves_a_shaft(projectile.get_kind())) {
    return;
  }

  std::uint64_t const seed = m_impact_sequence;
  QVector3D const impact = projectile.get_end();
  float const ground_y =
      world != nullptr
          ? Game::Session::services_for(*world).terrain->get_terrain_height(impact.x(),
                                                                            impact.z())
          : 0.0F;

  constexpr float k_scatter_radius = 0.34F;
  float const scatter_angle = scatter_unit(seed ^ 0x94d049bbULL) * 6.2831853F;
  float const scatter_dist = k_scatter_radius * scatter_unit(seed ^ 0x1b873593ULL);

  SpentProjectile spent;
  spent.position = QVector3D(impact.x() + std::cos(scatter_angle) * scatter_dist,
                             ground_y,
                             impact.z() + std::sin(scatter_angle) * scatter_dist);
  spent.direction = planted_direction(incoming_direction, seed);
  spent.color = projectile.get_color();
  spent.kind = projectile.get_kind();
  spent.roll_deg = scatter_unit(seed ^ 0x85ebca6bULL) * 360.0F;
  spent.scale = projectile.get_scale();
  spent.embed = ballista_bolt ? 0.34F : 0.24F;
  spent.lifetime = m_arrow_config.spent_lifetime_seconds *
                   (0.82F + 0.36F * scatter_unit(seed ^ 0xc2b2ae35ULL));
  spent.ballista_bolt = ballista_bolt;

  if (m_spent.size() >= k_max_spent_projectiles) {
    m_spent.erase(m_spent.begin());
  }
  m_spent.push_back(spent);
}

} // namespace Game::Systems
