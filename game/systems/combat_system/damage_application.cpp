#include "damage_application.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "../../core/ambient_session.h"
#include "../../core/component.h"
#include "../../core/death_sequence.h"
#include "../../core/event_manager.h"
#include "../../core/world.h"
#include "../../formation/army_formation_registry.h"
#include "../../units/spawn_type.h"
#include "../building_collision_registry.h"
#include "../combat_rules.h"
#include "../defensive_unit_layout_service.h"
#include "../formation_combat_geometry.h"
#include "../movement/order_service.h"
#include "../navigation/wall_network_service.h"
#include "combat_utils.h"
#include "formation_casualties.h"
#include "game/core/presentation_coverage.h"
#include "structure_combat.h"
#include "threat_alert.h"

namespace Game::Systems::Combat {

namespace {

auto is_valid_retaliation_attacker(Engine::Core::Entity* attacker) -> bool {
  if (attacker == nullptr) {
    return false;
  }
  auto const* unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }
  if (is_building(attacker)) {
    return unit->spawn_type == Game::Units::SpawnType::DefenseTower;
  }
  return true;
}

auto note_wildlife_aggressor(Engine::Core::Entity* target,
                             Engine::Core::EntityID attacker_id) -> bool {
  auto* wildlife = target->get_component<Engine::Core::WildlifeComponent>();
  if (wildlife == nullptr) {
    return false;
  }
  wildlife->aggressor_id = attacker_id;
  wildlife->hostile_timer = Game::Wildlife::k_hostility_duration;
  wildlife->think_cooldown = 0.0F;
  return true;
}

void assign_retaliation_target_if_needed(Engine::Core::World* world,
                                         Engine::Core::Entity* target,
                                         Engine::Core::Entity* attacker) {
  if ((target == nullptr) || (attacker == nullptr) || (world == nullptr)) {
    return;
  }

  if (!is_valid_retaliation_attacker(attacker)) {
    return;
  }

  if (note_wildlife_aggressor(target, attacker->get_id())) {
    return;
  }

  answer_attacker(world, target, attacker, AnswerPolicy::KeepCurrentFight);
}

void queue_structure_impact(Engine::Core::Entity& target,
                            const Engine::Core::Entity* attacker,
                            const std::optional<QVector3D>& contact_point) {
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return;
  }

  QVector3D source(target_transform->position.x,
                   target_transform->position.y,
                   target_transform->position.z - 2.0F);
  if (auto const* attacker_transform =
          attacker != nullptr
              ? attacker->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      attacker_transform != nullptr) {
    source = {attacker_transform->position.x,
              attacker_transform->position.y,
              attacker_transform->position.z};
  }

  auto const profile = structure_attack_profile(attacker);
  auto const surface = closest_structure_surface(target, source);
  QVector3D const point = contact_point.value_or(
      structure_impact_point(target, source, 0.0F, profile.impact_height));

  float radius = 0.38F;
  float intensity = 1.0F;
  float lifetime = 0.75F;
  switch (profile.impact_style) {
  case StructureImpactStyle::LightMelee:
    break;
  case StructureImpactStyle::HeavyMelee:
    radius = 0.48F;
    intensity = 1.25F;
    lifetime = 0.90F;
    break;
  case StructureImpactStyle::Elephant:
    radius = 0.78F;
    intensity = 2.2F;
    lifetime = 1.25F;
    break;
  case StructureImpactStyle::Ballista:
    radius = 0.55F;
    intensity = 1.7F;
    lifetime = 1.0F;
    break;
  case StructureImpactStyle::Catapult:
    radius = 0.95F;
    intensity = 2.6F;
    lifetime = 1.4F;
    break;
  case StructureImpactStyle::Magic:
    radius = 0.68F;
    intensity = 1.9F;
    lifetime = 1.15F;
    break;
  }

  auto* presentation = Engine::Core::get_or_add_component<
      Engine::Core::StructureDamagePresentationComponent>(&target);
  if (presentation == nullptr) {
    return;
  }
  constexpr std::size_t k_max_structure_impacts = 16U;
  if (presentation->impacts.size() >= k_max_structure_impacts) {
    presentation->impacts.erase(presentation->impacts.begin());
  }
  presentation->impacts.push_back({
      .x = point.x(),
      .y = point.y(),
      .z = point.z(),
      .normal_x = surface.outward_normal.x(),
      .normal_z = surface.outward_normal.z(),
      .age = 0.0F,
      .lifetime = lifetime,
      .radius = radius,
      .intensity = intensity,
      .style = static_cast<std::uint8_t>(profile.impact_style),
  });
}

[[nodiscard]] auto apply_defensive_unit_layout_damage_scaling(
    const Engine::Core::Entity& target,
    const Engine::Core::Entity* attacker,
    const std::optional<QVector3D>& contact_point,
    int damage) -> int {
  if (damage <= 0) {
    return damage;
  }

  Game::Systems::DefensiveUnitLayoutDamageContext context{};
  if (attacker != nullptr) {
    if (const auto* attacker_unit =
            attacker->get_component<Engine::Core::UnitComponent>()) {
      context.is_cavalry_impact = Game::Units::is_cavalry(attacker_unit->spawn_type);
    }
    if (const auto* attack = attacker->get_component<Engine::Core::AttackComponent>()) {
      context.is_missile =
          attack->current_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
    }
    if (const auto* attacker_transform =
            attacker->get_component<Engine::Core::TransformComponent>()) {
      context.attack_origin = QVector3D(attacker_transform->position.x,
                                        attacker_transform->position.y,
                                        attacker_transform->position.z);
    }
  }
  if (contact_point.has_value() && attacker == nullptr) {
    context.attack_origin = *contact_point;
  }

  float multiplier =
      Game::Systems::DefensiveUnitLayoutService::damage_multiplier(target, context);
  multiplier *= Game::Formation::ArmyFormationRuntime::damage_taken_multiplier(target);
  if (attacker != nullptr) {
    multiplier *=
        Game::Systems::DefensiveUnitLayoutService::attack_output_multiplier(*attacker);
  }

  if (multiplier >= 0.999F && multiplier <= 1.001F) {
    return damage;
  }
  return std::max(
      1, static_cast<int>(std::lround(static_cast<float>(damage) * multiplier)));
}

struct AttackerInfo {
  Engine::Core::Entity* entity{nullptr};
  int owner_id{0};
  std::optional<Game::Units::SpawnType> spawn_type;
};

[[nodiscard]] auto
resolve_attacker(Engine::Core::World* world,
                 Engine::Core::EntityID attacker_id) -> AttackerInfo {
  AttackerInfo info;
  if (attacker_id == 0 || world == nullptr) {
    return info;
  }
  info.entity = world->get_entity(attacker_id);
  if (const auto* attacker_unit =
          world->try_get<Engine::Core::UnitComponent>(attacker_id);
      attacker_unit != nullptr) {
    info.owner_id = attacker_unit->owner_id;
    info.spawn_type = attacker_unit->spawn_type;
  }
  return info;
}

void drop_dead_preferred_slot(std::optional<std::uint16_t>& preferred_soldier_slot,
                              const FormationCombat::FormationLayout& layout) {
  if (!preferred_soldier_slot.has_value()) {
    return;
  }
  bool const slot_is_live = std::any_of(layout.live_slots.begin(),
                                        layout.live_slots.end(),
                                        [preferred_soldier_slot](auto const& slot) {
                                          return slot.index == *preferred_soldier_slot;
                                        });
  if (!slot_is_live) {
    preferred_soldier_slot.reset();
  }
}

void react_wildlife_to_hit(Engine::Core::Entity& target, int health) {
  auto* const registry = target.registry();
  auto* wildlife = registry->try_get<Engine::Core::WildlifeComponent>(target.get_id());
  if (wildlife == nullptr) {
    return;
  }
  if (wildlife->flinch_timer <= 0.0F) {
    wildlife->flinch_timer =
        Engine::Core::WildlifeComponent::k_flinch_animation_seconds;
  }
  wildlife->watched_health = health;
  if (wildlife->species == Game::Wildlife::Species::Sheep) {
    wildlife->held_timer = std::max(wildlife->held_timer, 0.22F);
    if (auto* movement =
            registry->try_get<Engine::Core::MovementComponent>(target.get_id())) {
      movement->stop();
    }
  }
}

void release_melee_lock_partner(Engine::Core::World* world,
                                Engine::Core::Entity& target,
                                Engine::Core::AttackComponent* target_atk) {
  if ((target_atk == nullptr) || !target_atk->in_melee_lock ||
      target_atk->melee_lock_target_id == 0 || world == nullptr) {
    return;
  }
  auto* lock_partner = world->get_entity(target_atk->melee_lock_target_id);
  auto* partner_atk =
      (lock_partner != nullptr &&
       !lock_partner->has_component<Engine::Core::PendingRemovalComponent>())
          ? lock_partner->get_component<Engine::Core::AttackComponent>()
          : nullptr;
  if ((partner_atk != nullptr) &&
      partner_atk->melee_lock_target_id == target.get_id()) {
    partner_atk->release_melee_lock();
  }
}

struct HitContext {
  Engine::Core::World* world{nullptr};
  Engine::Core::Entity* target{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::EntityID attacker_id{0};
  AttackerInfo attacker;
  std::optional<Game::Units::SpawnType> sourceless_hit_sound_as;
  bool structure{false};
  int effective_damage{0};
  bool killing_blow{false};
};

void present_formation_hit(const HitContext& hit,
                           DamageApplicationResult& result,
                           std::optional<std::uint16_t> preferred_soldier_slot,
                           const FormationCombat::FormationLayout& previous_layout) {
  drop_dead_preferred_slot(preferred_soldier_slot, previous_layout);
  auto const preferred_hit_slot =
      preferred_soldier_slot.has_value()
          ? preferred_soldier_slot
          : preferred_formation_hit_slot(hit.target, hit.attacker.entity);
  publish_formation_hit(*hit.target,
                        hit.attacker_id,
                        preferred_hit_slot,
                        Engine::Core::HitReactionKind::Flinch,
                        hit.world);
  result.queued_soldier_casualties = begin_soldier_casualties(hit.target,
                                                              hit.attacker.entity,
                                                              result.previous_health,
                                                              result.new_health,
                                                              preferred_hit_slot,
                                                              previous_layout);
  if (result.queued_soldier_casualties > 0 && !hit.killing_blow && !hit.structure) {
    spawn_blood_stain(hit.world, hit.target);
  }
}

void publish_hit_event(const HitContext& hit) {
  Game::Units::SpawnType const attacker_type =
      hit.attacker.spawn_type.value_or(
          hit.sourceless_hit_sound_as.value_or(Game::Units::SpawnType::Swordsman));
  Engine::Core::EventManager::instance().publish(
      Engine::Core::CombatHitEvent(hit.attacker_id,
                                   hit.target->get_id(),
                                   hit.effective_damage,
                                   attacker_type,
                                   hit.killing_blow,
                                   hit.attacker.owner_id,
                                   hit.unit->owner_id,
                                   hit.structure));
}

void react_to_survived_hit(const HitContext& hit,
                           const std::optional<QVector3D>& contact_point,
                           float impact_speed) {
  react_wildlife_to_hit(*hit.target, hit.unit->health);
  apply_hit_feedback(hit.target,
                     hit.attacker_id,
                     hit.world,
                     Engine::Core::HitReactionKind::Flinch,
                     {.contact_point = contact_point, .weapon_speed = impact_speed});
  assign_retaliation_target_if_needed(hit.world, hit.target, hit.attacker.entity);
}

void resolve_death(const HitContext& hit) {
  auto* target = hit.target;
  auto* world = hit.world;
  fill_formation_front_vacancy(world, target);
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(target->get_id(),
                                  hit.unit->owner_id,
                                  hit.unit->spawn_type,
                                  hit.attacker_id,
                                  hit.attacker.owner_id));
  if (Game::Units::is_building_spawn(hit.unit->spawn_type)) {
    Engine::Core::note_coverage(Engine::Core::CoverageEvent::StructureDestroyed);
  }

  auto* target_atk = target->get_component<Engine::Core::AttackComponent>();
  release_melee_lock_partner(world, *target, target_atk);

  if (world != nullptr && hit.structure) {
    Game::Session::services_for(*world).building_collision->unregister_building(
        target->get_id());
  }
  if (world != nullptr &&
      target->get_component<Engine::Core::WallSegmentComponent>() != nullptr) {
    WallNetworkService::refresh_world(*world);
  }
  if (auto* movement = target->get_component<Engine::Core::MovementComponent>()) {
    movement->stop();
  }
  Game::Systems::OrderService::exit_hold_mode(target);
  if (target_atk != nullptr) {
    target_atk->release_melee_lock();
  }
  auto* target_selector = target->get_component<Engine::Core::AttackTargetComponent>();
  if (target_selector != nullptr) {
    target_selector->target_id = 0;
    target_selector->should_chase = false;
  }
  if (!hit.structure && hit.killing_blow) {
    spawn_blood_stain(world, target);
  }
  begin_death_sequence(target, hit.attacker.entity);
}

} // namespace

DamageApplicationResult
apply_unit_damage(Engine::Core::World* world,
                  Engine::Core::Entity* target,
                  int damage,
                  Engine::Core::EntityID attacker_id,
                  std::optional<QVector3D> contact_point,
                  std::optional<std::uint16_t> preferred_soldier_slot,
                  float impact_speed,
                  std::optional<Game::Units::SpawnType> sourceless_hit_sound_as) {
  DamageApplicationResult result;
  if (target == nullptr || damage <= 0) {
    return result;
  }
  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return result;
  }

  HitContext hit{.world = world,
                 .target = target,
                 .unit = unit,
                 .attacker_id = attacker_id,
                 .attacker = resolve_attacker(world, attacker_id),
                 .sourceless_hit_sound_as = sourceless_hit_sound_as};
  hit.structure = is_building(target);
  int const raw_damage =
      hit.structure ? resolve_structure_damage(hit.attacker.entity, damage) : damage;
  hit.effective_damage =
      hit.structure ? raw_damage
                    : apply_defensive_unit_layout_damage_scaling(
                          *target, hit.attacker.entity, contact_point, raw_damage);
  result.previous_health = unit->health;
  result.new_health = result.previous_health;
  if (hit.effective_damage <= 0 || result.previous_health <= 0) {
    return result;
  }
  result.applied_damage = hit.effective_damage;
  result.new_health = std::max(0, result.previous_health - hit.effective_damage);
  result.killed = result.new_health <= 0;
  hit.killing_blow = result.previous_health <= hit.effective_damage;
  auto const previous_layout = FormationCombat::resolve_layout(*target);

  unit->health = result.new_health;

  present_formation_hit(hit, result, preferred_soldier_slot, previous_layout);
  publish_hit_event(hit);
  if (hit.structure) {
    queue_structure_impact(*target, hit.attacker.entity, contact_point);
  }
  if (unit->health > 0) {
    react_to_survived_hit(hit, contact_point, impact_speed);
    if (hit.structure) {
      Engine::Core::EventManager::instance().publish(
          Engine::Core::BuildingAttackedEvent(target->get_id(),
                                              unit->owner_id,
                                              unit->spawn_type,
                                              attacker_id,
                                              hit.attacker.owner_id,
                                              hit.effective_damage));
    }
  } else {
    resolve_death(hit);
  }
  return result;
}

} // namespace Game::Systems::Combat
